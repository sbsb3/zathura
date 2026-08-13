/* SPDX-License-Identifier: Zlib */

#include "content-bbox.h"

#include <glib.h>

#include <girara/log.h>
#include <girara-gtk/settings.h>

#include "adjustment.h"
#include "document-widget.h"
#include "internal.h"
#include "page.h"
#include "utils.h"
#include "zathura.h"

bool content_bbox_ensure_page_computed(zathura_t* zathura, zathura_document_t* document, unsigned int page_id) {
  g_return_val_if_fail(zathura != NULL, false);
  g_return_val_if_fail(document != NULL, false);

  if (zathura_document_smart_width_page_bbox_known(document, page_id) == true) {
    zathura_rectangle_t cached = {0, 0, 0, 0};
    return zathura_document_get_smart_width_page_bbox(document, page_id, &cached);
  }

  if (zathura_document_get_smart_width_supported_known(document) == true &&
      zathura_document_get_smart_width_supported(document) == false) {
    /* Plugin doesn't implement content-bbox extraction at all -- no point
     * asking it again for every other page. */
    return false;
  }

  zathura_page_t* page = zathura_document_get_page(document, page_id);
  if (page == NULL) {
    return false;
  }

  zathura_rectangle_t bbox = {0, 0, 0, 0};
  const zathura_error_t rc = zathura_page_get_content_bbox(page, &bbox);

  if (rc == ZATHURA_ERROR_NOT_IMPLEMENTED) {
    girara_debug("smart-width: plugin does not implement content bbox extraction");
    zathura_document_set_smart_width_supported_known(document, true);
    zathura_document_set_smart_width_supported(document, false);
    return false;
  }

  zathura_document_set_smart_width_supported_known(document, true);
  zathura_document_set_smart_width_supported(document, true);

  if (rc != ZATHURA_ERROR_OK || bbox.x2 <= bbox.x1 || bbox.y2 <= bbox.y1) {
    /* Degenerate rect: blank page, scanned page without OCR text, ... Cache
     * the negative result so we don't re-query this page every time. */
    zathura_document_set_smart_width_page_bbox(document, page_id, NULL);
    return false;
  }

  girara_debug("smart-width: content bbox for page %u: (%.2f,%.2f)-(%.2f,%.2f)", page_id, bbox.x1, bbox.y1, bbox.x2,
              bbox.y2);
  zathura_document_set_smart_width_page_bbox(document, page_id, &bbox);

  return true;
}

bool content_bbox_get_extent_px(zathura_t* zathura, unsigned int page_id, double* x1_px, double* x2_px) {
  g_return_val_if_fail(zathura != NULL, false);
  g_return_val_if_fail(x1_px != NULL && x2_px != NULL, false);

  zathura_document_t* document = zathura_get_document(zathura);
  if (document == NULL || content_bbox_ensure_page_computed(zathura, document, page_id) == false) {
    return false;
  }

  zathura_page_t* page = zathura_document_get_page(document, page_id);
  zathura_rectangle_t raw = {0, 0, 0, 0};
  if (page == NULL || zathura_document_get_smart_width_page_bbox(document, page_id, &raw) == false) {
    return false;
  }

  const zathura_rectangle_t px = recalc_rectangle(page, raw);

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

  unsigned int cell_height = 0, cell_width = 0, doc_height = 0, doc_width = 0;
  zathura_document_widget_get_cell_size(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget), page_id, &cell_height,
                                        &cell_width);
  zathura_document_widget_get_document_size(ZATHURA_DOCUMENT_WIDGET(zathura->ui.document_widget), &doc_height,
                                            &doc_width);
  if (doc_width == 0) {
    return fallback_pos_x;
  }

  /* Base position: page's top-left aligned with the viewport's top-left,
   * then shift right by the content bbox's own left offset within the page
   * cell -- same idiom link_goto_dest() uses for link targets. */
  double pos_x = 0, unused_pos_y = 0;
  page_number_to_position(zathura, page_id, 0.0, 0.0, &pos_x, &unused_pos_y);

  return pos_x + shiftx_px * (double)cell_width / (double)doc_width;
}
