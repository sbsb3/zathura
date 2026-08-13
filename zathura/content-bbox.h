/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_CONTENT_BBOX_H
#define ZATHURA_CONTENT_BBOX_H

#include <stddef.h>
#include <stdbool.h>

#include "document.h"

/**
 * Computes the Pth percentile of a set of values using linear interpolation
 * between closest ranks (numpy/R "type 7", the common default). Degrades
 * gracefully for small n: n=1 always returns the single value regardless of
 * p.
 *
 * @param values Array of values. Not modified; a sorted copy is made
 *    internally.
 * @param n Number of values. Must be > 0.
 * @param p Percentile in [0, 100]
 * @return The interpolated percentile value
 */
double percentile_linear(const double* values, size_t n, double p);

/**
 * Aggregates a set of per-page content bounding boxes (e.g. sampled across a
 * document) into a single representative bounding box, trimming outliers.
 * Each edge is aggregated independently, with the tail direction chosen so
 * that the result stays a well-formed rectangle: x1/y1 (the near edges) use
 * the (100-percentile)th percentile (trims stray-far-left/top pages), x2/y2
 * (the far edges) use the percentile-th percentile (trims stray-wide/tall
 * pages). The resulting content width/height are always derived from the
 * aggregated edges, never computed separately, so the rectangle is always
 * self-consistent.
 *
 * @param rects Array of per-page content bboxes, raw unrotated page-point
 *    space. Must contain only well-formed rectangles (x2>x1, y2>y1).
 * @param n Number of rectangles. Must be > 0.
 * @param percentile The percentile tunable in [50, 100] (the
 *    "smart-width-percentile" setting, e.g. 90)
 * @return The aggregated bounding box
 */
zathura_rectangle_t content_bbox_aggregate(const zathura_rectangle_t* rects, size_t n, double percentile);

/**
 * Ensures the document's smart-width content bbox cache is populated,
 * computing it on first call by sampling up to 10 evenly-spaced pages and
 * asking the active plugin for each page's content bbox
 * (zathura_page_get_content_bbox()). Idempotent: a no-op after the first
 * successful (or unsuccessful) call for a given document. The cache is
 * naturally reset because reload creates a brand-new zathura_document_t.
 *
 * @param zathura The zathura instance
 * @param document The document
 * @return true if a usable content bbox is now cached
 *    (zathura_document_get_smart_width_available()), false if none could be
 *    determined (e.g. plugin doesn't implement it, or the document has no
 *    text layer) and callers should fall back to plain width-fit
 */
bool content_bbox_ensure_computed(zathura_t* zathura, zathura_document_t* document);

/**
 * Returns the cached smart-width content bbox's horizontal extent, in
 * viewport-pixel space at the document's *current* zoom/rotation, for the
 * given page. Does not trigger computation -- call
 * content_bbox_ensure_computed() first.
 *
 * @param zathura The zathura instance
 * @param page_id The page to compute the pixel extent for
 * @param x1_px Set to the left edge in pixels on success
 * @param x2_px Set to the right edge in pixels on success
 * @return true on success, false if no content bbox is cached/available or
 *    the page doesn't exist
 */
bool content_bbox_get_extent_px(zathura_t* zathura, unsigned int page_id, double* x1_px, double* x2_px);

/**
 * Computes the document-relative position_x ratio (as stored on
 * zathura_document_t / consumed by position_set()) that aligns the cached
 * content bbox's left edge with the viewport's left edge, offset by the
 * page-h-padding setting. Falls back to the given value verbatim if no
 * content bbox is available.
 *
 * @param zathura The zathura instance
 * @param page_id The current page
 * @param fallback_pos_x Value to return unchanged if no content bbox is
 *    available (typically the caller's own default/computed position_x)
 * @return The position_x ratio
 */
double content_bbox_adjust_position_x(zathura_t* zathura, unsigned int page_id, double fallback_pos_x);

#endif // ZATHURA_CONTENT_BBOX_H
