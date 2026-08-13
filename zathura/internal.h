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
 * Per-page content bbox cache used by the smart-width adjust mode. Each page
 * gets its own bbox (raw, unrotated page-point space) -- fit is computed
 * against the *current* page's own content, not an aggregate across the
 * document, since page layout (chapter openers, code listings, figures, ...)
 * routinely varies within a single book. See content-bbox.h for how it is
 * computed.
 *
 * @param document The document
 * @param page_id The page index
 * @return true if this page has already been queried (positive or negative
 *    result cached)
 */
bool zathura_document_smart_width_page_bbox_known(zathura_document_t* document, unsigned int page_id);

/**
 * Retrieves the cached content bbox for a page.
 *
 * @param document The document
 * @param page_id The page index
 * @param bbox Set to the cached bbox on success
 * @return true if a usable bbox is cached for this page, false if unknown or
 *    known-negative (no usable content found on that page)
 */
bool zathura_document_get_smart_width_page_bbox(zathura_document_t* document, unsigned int page_id,
                                                zathura_rectangle_t* bbox);

/**
 * Caches the content bbox for a page.
 *
 * @param document The document
 * @param page_id The page index
 * @param bbox The bbox to cache, or NULL to cache a negative result (no
 *    usable content found on that page)
 */
void zathura_document_set_smart_width_page_bbox(zathura_document_t* document, unsigned int page_id,
                                                const zathura_rectangle_t* bbox);

/**
 * Whether we've learned yet if the active plugin implements content-bbox
 * extraction at all (learned lazily from the first page ever queried).
 *
 * @param document The document
 * @return true if known
 */
bool zathura_document_get_smart_width_supported_known(zathura_document_t* document);

/**
 * Marks whether it's known if the plugin implements content-bbox extraction.
 *
 * @param document The document
 * @param known The new value
 */
void zathura_document_set_smart_width_supported_known(zathura_document_t* document, bool known);

/**
 * Whether the active plugin implements content-bbox extraction at all. Only
 * meaningful once zathura_document_get_smart_width_supported_known() is true;
 * lets callers short-circuit and stop querying entirely (rather than just
 * per-page) once a plugin has proven not to implement the hook.
 *
 * @param document The document
 * @return true if the plugin implements it
 */
bool zathura_document_get_smart_width_supported(zathura_document_t* document);

/**
 * Marks whether the plugin implements content-bbox extraction.
 *
 * @param document The document
 * @param supported The new value
 */
void zathura_document_set_smart_width_supported(zathura_document_t* document, bool supported);

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
