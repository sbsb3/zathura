/* SPDX-License-Identifier: Zlib */

#ifndef ZATHURA_CONTENT_BBOX_H
#define ZATHURA_CONTENT_BBOX_H

#include <stdbool.h>

#include "document.h"

/**
 * Ensures a page's smart-width content bbox cache entry is populated,
 * computing it on first call by asking the active plugin for that page's
 * content bbox (zathura_page_get_content_bbox()). Idempotent: a no-op after
 * the first successful (or unsuccessful) call for a given page. Fit is
 * deliberately computed per page rather than aggregated across the document,
 * since page layout (chapter openers, code listings, figures, ...) routinely
 * varies within a single book -- aggregating would fit some pages against
 * bounds that don't describe their actual content. The cache is naturally
 * reset because reload creates a brand-new zathura_document_t.
 *
 * @param zathura The zathura instance
 * @param document The document
 * @param page_id The page to compute the content bbox for
 * @return true if a usable content bbox is now cached for this page, false
 *    if none could be determined (e.g. plugin doesn't implement it, or the
 *    page has no text layer) and callers should fall back to plain width-fit
 */
bool content_bbox_ensure_page_computed(zathura_t* zathura, zathura_document_t* document, unsigned int page_id);

/**
 * Returns the cached smart-width content bbox's horizontal extent, in
 * viewport-pixel space at the document's *current* zoom/rotation, for the
 * given page. Computes and caches the bbox first if not already cached.
 *
 * @param zathura The zathura instance
 * @param page_id The page to compute the pixel extent for
 * @param x1_px Set to the left edge in pixels on success
 * @param x2_px Set to the right edge in pixels on success
 * @return true on success, false if no content bbox is available for this
 *    page or the page doesn't exist
 */
bool content_bbox_get_extent_px(zathura_t* zathura, unsigned int page_id, double* x1_px, double* x2_px);

/**
 * Computes the document-relative position_x ratio (as stored on
 * zathura_document_t / consumed by position_set()) that aligns the page's
 * content bbox's left edge with the viewport's left edge, offset by the
 * page-h-padding setting. Falls back to the given value verbatim if no
 * content bbox is available for this page.
 *
 * @param zathura The zathura instance
 * @param page_id The current page
 * @param fallback_pos_x Value to return unchanged if no content bbox is
 *    available (typically the caller's own default/computed position_x)
 * @return The position_x ratio
 */
double content_bbox_adjust_position_x(zathura_t* zathura, unsigned int page_id, double fallback_pos_x);

#endif // ZATHURA_CONTENT_BBOX_H
