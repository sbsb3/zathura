/* SPDX-License-Identifier: Zlib */

#include "content-bbox.h"

#include <stdlib.h>
#include <math.h>
#include <glib.h>

#include <girara/log.h>
#include <girara-gtk/settings.h>

#include "adjustment.h"
#include "document-widget.h"
#include "internal.h"
#include "page.h"
#include "utils.h"
#include "zathura.h"

/* Maximum number of pages sampled to build the aggregated content bbox. Kept
 * small since bbox extraction runs synchronously on the main thread. */
#define CONTENT_BBOX_SAMPLE_LIMIT 10

static int compare_double(const void* a, const void* b) {
  const double da = *(const double*)a;
  const double db = *(const double*)b;
  return (da > db) - (da < db);
}

double percentile_linear(const double* values, size_t n, double p) {
  if (values == NULL || n == 0) {
    return 0.0;
  }

  if (n == 1) {
    return values[0];
  }

  g_autofree double* sorted = g_memdup2(values, n * sizeof(double));
  qsort(sorted, n, sizeof(double), compare_double);

  const double clamped_p = CLAMP(p, 0.0, 100.0);
  const double rank      = clamped_p / 100.0 * (double)(n - 1);
  const size_t lo        = (size_t)floor(rank);
  const size_t hi        = (size_t)ceil(rank);
  const double frac      = rank - (double)lo;

  return sorted[lo] + frac * (sorted[hi] - sorted[lo]);
}

zathura_rectangle_t content_bbox_aggregate(const zathura_rectangle_t* rects, size_t n, double percentile) {
  zathura_rectangle_t result = {0, 0, 0, 0};
  if (rects == NULL || n == 0) {
    return result;
  }

  g_autofree double* x1s     = g_new(double, n);
  g_autofree double* y1s     = g_new(double, n);
  g_autofree double* widths  = g_new(double, n);
  g_autofree double* heights = g_new(double, n);

  for (size_t i = 0; i < n; i++) {
    x1s[i]     = rects[i].x1;
    y1s[i]     = rects[i].y1;
    widths[i]  = rects[i].x2 - rects[i].x1;
    heights[i] = rects[i].y2 - rects[i].y1;
  }

  /* Near edges: trim the stray-far-left/top tail by taking the low percentile.
   * Extents: trim the stray-wide/tall tail by taking the high percentile.
   *
   * The far edges are *derived* from origin + extent rather than aggregated
   * directly, because every page is drawn shifted so its own content starts at
   * the column's left edge (see content_bbox_page_align_offset_px()). What the
   * column has to be wide enough for is therefore the widest content, not the
   * rightmost -- aggregating x2 directly would add the mirrored-margin offset
   * to the column width and waste that much of the viewport. */
  result.x1 = percentile_linear(x1s, n, 100.0 - percentile);
  result.y1 = percentile_linear(y1s, n, 100.0 - percentile);
  result.x2 = result.x1 + percentile_linear(widths, n, percentile);
  result.y2 = result.y1 + percentile_linear(heights, n, percentile);

  return result;
}

/**
 * Looks up a single page's content bbox, computing and caching it on first
 * use. Cheap enough to call from the draw path once cached.
 */
static bool content_bbox_page_bbox(zathura_document_t* document, unsigned int page_id, zathura_rectangle_t* bbox,
                                   zathura_error_t* error) {
  if (error != NULL) {
    *error = ZATHURA_ERROR_OK;
  }

  if (zathura_document_smart_width_page_bbox_known(document, page_id) == true) {
    return zathura_document_get_smart_width_page_bbox(document, page_id, bbox);
  }

  zathura_page_t* page = zathura_document_get_page(document, page_id);
  if (page == NULL) {
    return false;
  }

  zathura_rectangle_t computed = {0, 0, 0, 0};
  const zathura_error_t rc     = zathura_page_get_content_bbox(page, &computed);
  if (error != NULL) {
    *error = rc;
  }

  if (rc == ZATHURA_ERROR_NOT_IMPLEMENTED) {
    /* not a per-page property: the plugin has no such hook at all, so don't
     * poison the cache with a negative result for this one page */
    return false;
  }

  if (rc != ZATHURA_ERROR_OK || computed.x2 <= computed.x1 || computed.y2 <= computed.y1) {
    /* degenerate: blank page, scanned page without a text layer, ... */
    zathura_document_set_smart_width_page_bbox(document, page_id, NULL);
    return false;
  }

  zathura_document_set_smart_width_page_bbox(document, page_id, &computed);
  if (bbox != NULL) {
    *bbox = computed;
  }

  return true;
}

bool content_bbox_ensure_computed(zathura_t* zathura, zathura_document_t* document) {
  g_return_val_if_fail(zathura != NULL, false);
  g_return_val_if_fail(document != NULL, false);

  if (zathura_document_get_smart_width_computed(document) == true) {
    return zathura_document_get_smart_width_available(document);
  }

  const unsigned int number_of_pages = zathura_document_get_number_of_pages(document);
  if (number_of_pages == 0) {
    zathura_document_set_smart_width_computed(document, true);
    zathura_document_set_smart_width_available(document, false);
    return false;
  }

  unsigned int percentile_setting = 90;
  girara_setting_get(zathura->ui.session, "smart-width-percentile", &percentile_setting);

  const unsigned int sample_count = MIN(number_of_pages, CONTENT_BBOX_SAMPLE_LIMIT);
  g_autofree zathura_rectangle_t* samples = g_new(zathura_rectangle_t, sample_count);
  unsigned int num_samples                = 0;

  for (unsigned int i = 0; i < sample_count; i++) {
    const unsigned int page_index = (sample_count == 1)
                                        ? 0
                                        : (unsigned int)((double)i * (number_of_pages - 1) / (double)(sample_count - 1) +
                                                         0.5);

    zathura_rectangle_t bbox = {0, 0, 0, 0};
    zathura_error_t rc       = ZATHURA_ERROR_OK;
    if (content_bbox_page_bbox(document, page_index, &bbox, &rc) == false) {
      if (rc == ZATHURA_ERROR_NOT_IMPLEMENTED) {
        /* Plugin doesn't support this at all -- no point sampling further. */
        break;
      }
      continue;
    }

    samples[num_samples++] = bbox;
  }

  zathura_document_set_smart_width_computed(document, true);

  if (num_samples == 0) {
    girara_debug("smart-width: no usable content bbox found for document");
    zathura_document_set_smart_width_available(document, false);
    return false;
  }

  const zathura_rectangle_t aggregated = content_bbox_aggregate(samples, num_samples, (double)percentile_setting);
  girara_debug("smart-width: aggregated content bbox from %u/%u sampled pages: (%.2f,%.2f)-(%.2f,%.2f)", num_samples,
              sample_count, aggregated.x1, aggregated.y1, aggregated.x2, aggregated.y2);

  zathura_document_set_smart_width_bbox(document, aggregated);
  zathura_document_set_smart_width_available(document, true);

  return true;
}

double content_bbox_page_align_offset_px(zathura_t* zathura, unsigned int page_id) {
  g_return_val_if_fail(zathura != NULL, 0.0);

  zathura_document_t* document = zathura_get_document(zathura);
  if (document == NULL || zathura_document_get_adjust_mode(document) != ZATHURA_ADJUST_SMARTWIDTH ||
      zathura_document_get_smart_width_available(document) == false) {
    return 0.0;
  }

  /* At 90/270 the horizontal extent comes from the bbox's y edges and the
   * shift would have to move vertically instead; not handled, the mode still
   * zooms correctly there (recalc_rectangle() rotates), it just doesn't
   * realign the columns. */
  const unsigned int rotation = zathura_document_get_rotation(document);
  if (rotation != 0 && rotation != 180) {
    return 0.0;
  }

  zathura_rectangle_t bbox = {0, 0, 0, 0};
  if (content_bbox_page_bbox(document, page_id, &bbox, NULL) == false) {
    return 0.0;
  }

  const zathura_rectangle_t column = zathura_document_get_smart_width_bbox(document);
  const double column_width        = column.x2 - column.x1;
  const double page_width          = bbox.x2 - bbox.x1;
  if (column_width <= 0 || page_width <= 0) {
    return 0.0;
  }

  /* Align this page's content with the shared column. Pages wider than the
   * column can't fit whatever we do, so centre the overflow instead of letting
   * it all fall off one side. */
  double shift = (rotation == 180) ? (bbox.x2 - column.x2) : (column.x1 - bbox.x1);
  if (page_width > column_width) {
    shift -= (page_width - column_width) / 2.0;
  }

  return shift * zathura_document_get_scale(document);
}

bool content_bbox_get_extent_px(zathura_t* zathura, unsigned int page_id, double* x1_px, double* x2_px) {
  g_return_val_if_fail(zathura != NULL, false);
  g_return_val_if_fail(x1_px != NULL && x2_px != NULL, false);

  zathura_document_t* document = zathura_get_document(zathura);
  if (document == NULL || zathura_document_get_smart_width_available(document) == false) {
    return false;
  }

  zathura_page_t* page = zathura_document_get_page(document, page_id);
  if (page == NULL) {
    return false;
  }

  const zathura_rectangle_t raw = zathura_document_get_smart_width_bbox(document);
  const zathura_rectangle_t px  = recalc_rectangle(page, raw);

  *x1_px = MIN(px.x1, px.x2);
  *x2_px = MAX(px.x1, px.x2);

  return true;
}

double content_bbox_adjust_position_x(zathura_t* zathura, unsigned int page_id, double fallback_pos_x) {
  g_return_val_if_fail(zathura != NULL, fallback_pos_x);

  double x1_px = 0, x2_px = 0;
  if (content_bbox_get_extent_px(zathura, page_id, &x1_px, &x2_px) == false) {
    return fallback_pos_x;
  }

  int page_h_padding = 1;
  girara_setting_get(zathura->ui.session, "page-h-padding", &page_h_padding);

  const double shiftx_px = x1_px - (double)page_h_padding;

  unsigned int doc_height = 0, doc_width = 0;
  zathura_document_widget_get_document_size(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget), &doc_height,
                                            &doc_width);
  if (doc_width == 0) {
    return fallback_pos_x;
  }

  /* Base position: page's left edge aligned with the viewport's left edge,
   * then shift right by the content column's own left offset within the page.
   * position_x is a fraction of the document width, so pixels convert by
   * dividing by doc_width -- the same conversion link_goto_dest() makes for
   * link targets. */
  double pos_x = 0, unused_pos_y = 0;
  page_number_to_position(zathura, page_id, 0.0, 0.0, &pos_x, &unused_pos_y);

  girara_debug("smart-width: aligning page %u, content column starts at %.2fpx of a %upx wide document", page_id, x1_px,
               doc_width);

  return pos_x + shiftx_px / (double)doc_width;
}
