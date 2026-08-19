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
 * x1/y1 (the origin) use the (100-percentile)th percentile, trimming
 * stray-far-left/top pages; the width and height use the percentile-th
 * percentile, trimming stray-wide/tall pages. x2/y2 are then derived from
 * origin + extent, so the result is always a well-formed rectangle.
 *
 * Note that the extents are aggregated, not the far edges: every page is drawn
 * shifted so that its own content starts at the column's left edge (see
 * content_bbox_page_align_offset_px()), so the column only has to be as wide as
 * the widest content, not as wide as the span from the leftmost to the
 * rightmost. Aggregating x2 directly would fold each book's mirrored-margin
 * offset into the column and waste that much of the viewport.
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
 * Returns how far, in widget pixels at the current zoom, a page has to be
 * drawn horizontally shifted for its content to line up with the document's
 * shared content column.
 *
 * Books typically use mirrored inner/outer margins, so the text sits ~18pt
 * further right on recto pages than on verso ones. Without this shift the text
 * appears to jump left, right, left, right as you page through the document,
 * and the column has to be widened by the mirror offset to keep both sides
 * from being clipped. Shifting each page instead lets every page share one
 * horizontal scroll position.
 *
 * Returns 0 (no shift) unless the document is in smart-width mode with a usable
 * content column, and the rotation is 0 or 180.
 *
 * @param zathura The zathura instance
 * @param page_id The page to compute the shift for
 * @return The horizontal shift in widget pixels; 0 if the page should not be
 *    shifted
 */
double content_bbox_page_align_offset_px(zathura_t* zathura, unsigned int page_id);

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
