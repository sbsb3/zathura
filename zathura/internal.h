/* SPDX-License-Identifier: Zlib */

#ifndef INTERNAL_H
#define INTERNAL_H

#include "zathura.h"
#include "plugin.h"

/**
 * Zathura password dialog
 */
typedef struct zathura_password_dialog_info_s {
  char* path;          /**< Path to the file */
  char* uri;           /**< URI to the file */
  zathura_t* zathura;  /**< Zathura session */
  gulong hide_handler; /**< id of inputbar "hide" handler, 0 if none */
} zathura_password_dialog_info_t;

struct zathura_document_information_entry_s {
  zathura_document_information_type_t type; /**< Type of the information */
  char* value;                              /**< Value */
};

/**
 * Returns the associated plugin
 *
 * @param document The document
 * @return The plugin or NULL
 */
const zathura_plugin_t* zathura_document_get_plugin(zathura_document_t* document);

/**
 * Whether this page's content bbox has already been looked up (whether or not
 * a usable one was found). Distinguishes "cached negative result" from "never
 * queried", which zathura_document_get_smart_width_page_bbox() alone cannot.
 *
 * @param document The document
 * @param page_id The page index
 * @return true if the page has already been queried
 */
bool zathura_document_smart_width_page_bbox_known(zathura_document_t* document, unsigned int page_id);

/**
 * Returns a single page's cached content bbox, in raw unrotated page-point
 * space. Used by smart-width to shift each page so that its content column
 * lines up with every other page's -- books with mirrored inner/outer margins
 * otherwise make the text jump left and right on alternating pages.
 *
 * @param document The document
 * @param page_id The page index
 * @param bbox Set to the cached bbox on success; may be NULL
 * @return true if a usable bbox is cached for this page
 */
bool zathura_document_get_smart_width_page_bbox(zathura_document_t* document, unsigned int page_id,
                                                zathura_rectangle_t* bbox);

/**
 * Caches a single page's content bbox. Pass NULL to cache a negative result
 * (no usable content found), so the page isn't queried again.
 *
 * @param document The document
 * @param page_id The page index
 * @param bbox The bbox to cache, or NULL
 */
void zathura_document_set_smart_width_page_bbox(zathura_document_t* document, unsigned int page_id,
                                                const zathura_rectangle_t* bbox);

/**
 * Cached content bounding box used by the smart-width adjust mode. Stored in
 * raw, unrotated page-point space and shared across all pages of the
 * document. See content-bbox.h for how it is computed.
 *
 * @param document The document
 * @return The cached content bbox, or an all-zero rectangle if none is cached
 */
zathura_rectangle_t zathura_document_get_smart_width_bbox(zathura_document_t* document);

/**
 * Sets the cached smart-width content bbox.
 *
 * @param document The document
 * @param bbox The bbox to cache
 */
void zathura_document_set_smart_width_bbox(zathura_document_t* document, zathura_rectangle_t bbox);

/**
 * Whether the smart-width content bbox has already been computed (whether or
 * not a usable bbox was found) for this document.
 *
 * @param document The document
 * @return true if content_bbox_ensure_computed() has already run
 */
bool zathura_document_get_smart_width_computed(zathura_document_t* document);

/**
 * Marks whether the smart-width content bbox has been computed.
 *
 * @param document The document
 * @param computed The new value
 */
void zathura_document_set_smart_width_computed(zathura_document_t* document, bool computed);

/**
 * Whether a usable smart-width content bbox was found (false means: no
 * content-bbox-capable plugin function, or every sampled page was degenerate
 * -- callers should fall back to plain width-fit behavior).
 *
 * @param document The document
 * @return true if the cached bbox is usable
 */
bool zathura_document_get_smart_width_available(zathura_document_t* document);

/**
 * Marks whether a usable smart-width content bbox is available.
 *
 * @param document The document
 * @param available The new value
 */
void zathura_document_set_smart_width_available(zathura_document_t* document, bool available);

/**
 * Whether the one-time "smart-width unavailable" statusbar notice has
 * already fired for this document (avoids spamming on every resize-triggered
 * adjust_view() call).
 *
 * @param document The document
 * @return true if the notice has already fired
 */
bool zathura_document_get_smart_width_notified(zathura_document_t* document);

/**
 * Marks whether the smart-width fallback notice has fired.
 *
 * @param document The document
 * @param notified The new value
 */
void zathura_document_set_smart_width_notified(zathura_document_t* document, bool notified);

#endif // INTERNAL_H
