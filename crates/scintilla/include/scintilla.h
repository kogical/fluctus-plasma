/* SPDX-FileCopyrightText: 2026 Kogical
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 * C ABI for the `scintilla` connected-device discovery and battery crate.
 */
#ifndef SCINTILLA_H
#define SCINTILLA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScintillaClient ScintillaClient;

/**
 * Creates a new scintilla client with its own internal Tokio runtime.
 * Returns NULL if initialization fails.
 */
ScintillaClient *scintilla_client_new(bool include_stubs, bool enable_vendor_probing);

/**
 * Frees a client created by scintilla_client_new. Passing NULL is a no-op.
 */
void scintilla_client_free(ScintillaClient *client);

/**
 * Performs a oneshot discovery and returns a UTF-8 JSON array of devices.
 * The returned string must be freed with scintilla_string_free().
 * Returns NULL on error.
 */
char *scintilla_client_discover_json(ScintillaClient *client);

/**
 * Frees a string returned by scintilla_client_discover_json(). Passing NULL is a no-op.
 */
void scintilla_string_free(char *ptr);

#ifdef __cplusplus
}
#endif

#endif /* SCINTILLA_H */
