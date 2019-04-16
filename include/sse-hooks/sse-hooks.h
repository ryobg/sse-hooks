/**
 * @file sse-hooks.h
 * @brief Public C API for users of SSE Hooks
 * @internal
 *
 * This file is part of SSE Hooks project (aka SSEH).
 *
 *   SSEH is free software: you can redistribute it and/or modify it
 *   under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   SSEH is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with SSEH. If not, see <http://www.gnu.org/licenses/>.
 *
 * @endinternal
 *
 * @ingroup Public API
 *
 * @note This API is thread-safe.
 * @note Unless mentioned, all strings are null-terminated and in UTF-8.
 *
 * @details
 * This file encompass all the functions which are presented to the users of
 * SSE Hooks. The interface targets to be maximum compatible and portable
 * across the expected build and usage scenarios. This file uses generic C, but
 * is compatible with C++. As the methods are to be exported in the DLL, this
 * lib interface can be also accessed from other languages too.
 *
 * Basic flow goes as follow:
 *
 * 1. Initialize the library #sseh_init()
 * 2. Load a pre-defined JSON configuration file by calling #sseh_load()
 * 3. Update the configuration as needed by calling #sseh_merge_patch()
 * 4. Enable and apply SSEH changes upon the process memory, call #sseh_apply()
 * 5. Fetch any fields of interest like the computed calls to the original non-
 * patched functions by calling #sseh_identify()
 * 6. When SSEH and its hooks are no longer needed - #sseh_uninit()
 */

#ifndef SSEH_SSEHOOKS_H
#define SSEH_SSEHOOKS_H

#include <stdint.h>
#include <sse-hooks/platform.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

/**
 * Run-time version of this API and the implementation details.
 *
 * This function can be used to detect in run-time what kind of feature & fixes
 * on the API, the loaded SSEH is compiled with. This function is the only one,
 * which is guaranteed to preseve through the whole lifecycle of this product.
 *
 * The @param api tells what version is the current API. Any version different
 * than the one expected guarantees a broken interface. Most likely it will
 * mean that a function is missing or its prototype is different.
 *
 * The @param maj describes major, but compatible changes within the API. Maybe
 * a new function is added or the behaviour of an old one was extended in
 * compatible way i.e. it won't break the callee.
 *
 * The @param imp is an implementation detail, in most cases may not be of
 * interest. It is reserved for patches, bug fixes, maybe documentation updates
 * re-release of something and etc. It is used mostly as kind of timestamp.
 *
 * @param[out] api (optional) non-portable
 * @param[out] maj (optional) new features and enhancements
 * @param[out] imp (optional) patches
 * @param[out] build (optional) ISO timestamp
 */

SSEH_API void SSEH_CCONV
sseh_version (int* api, int* maj, int* imp, const char** timestamp);

/** @see #sseh_version() */

typedef void (SSEH_CCONV* sseh_version_t) (int*, int*, int*, const char**);

/******************************************************************************/

/**
 * Report the last, for this thread, message in more human-readable form.
 *
 * @param[in,out] size in bytes of @param message, on exit how many bytes were
 * actually written (excluding the terminating null) or how many bytes are
 * needed in order to get the full message. Can be zero, if there is no error.
 *
 * @param[out] message in human readable form, can be nullptr if @param size is
 * needed to pre-allocate a buffer.
 */

SSEH_API void SSEH_CCONV
sseh_last_error (size_t* size, char* message);

/** @see #sseh_last_error() */

typedef void (SSEH_CCONV* sseh_last_error_t) (size_t*, char*);

/******************************************************************************/

/**
 * Initialize SSEH.
 *
 * This function must be called first before any further usage (excluding
 * #sseh_version() and #sseh_last_error()).
 *
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_init ();

/** @see #sseh_init() */

typedef int (SSEH_CCONV* sseh_init_t) ();

/**
 * Uninitialize (pre-init state) SSEH.
 */

SSEH_API void SSEH_CCONV
sseh_uninit ();

/** @see #sseh_uninit() */

typedef void (SSEH_CCONV* sseh_uninit_t) ();

/******************************************************************************/

/**
 * Search for function name, optionally in given module.
 *
 * @see ::GetModuleHandle* and ::GetProcAddress*
 *
 * @param[in] module name of the library to search, or nullptr for this process.
 * @param[in] name of the function to search for
 * @param[out] address found, or nullptr on error
 * @returns non-zero on success, otherwise see #sseh_last_error()
 */

SSEH_API int SSEH_CCONV
sseh_find_address (const char* module, const char* name, uintptr_t* address);

/** @see #sseh_find_address() */

typedef int (SSEH_CCONV* sseh_find_address_t)
    (const char*, const char*, uintptr_t*);

/******************************************************************************/

/**
 * Load from file and replace the JSON configuration.
 *
 * This is a shorthand for fancy combination of reading a file and
 * #sseh_merge_patch() calls. The configuration still needs to be applied
 * through #sseh_apply() later.
 *
 * @param[in] filepath to read from
 */

SSEH_API int SSEH_CCONV
sseh_load (const char* filepath);

/** @see #sseh_load() */

typedef int (SSEH_CCONV* sseh_load_t) (const char*);

/******************************************************************************/

/**
 * Report a JSON at given location.
 *
 * This function allows basic retrieval of information from the internal
 * storage based on the JSON pointer reference. The current JSON structure
 * supported by SSEH is described in separate document.
 *
 * @see https://tools.ietf.org/html/rfc6901
 *
 * @param[in] pointer to use (e.g. "/hooks")
 * @param[in,out] size number of bytes of the incoming @param json array. On
 * exit reports how many bytes were actually used (w/o the terminating null)
 * or how many bytes are needed to store the whole result (again, w/o the
 * terminating null).
 * @param[in] json to store the reported result
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_identify (const char* pointer, size_t* size, char* json);

/** @see #sseh_identify() */

typedef int (SSEH_CCONV* sseh_identify_t) (const char*, size_t*, char*);

/**
 * Merge a JSON patch in the internal configuration.
 *
 * Any functional changes like enabling or disabling a hook, adding a new
 * detour or else have to be applied through a call to #sseh_apply(). Changes
 * which does not have a direct effect upon the process memory do not need a
 * call to #sseh_apply() (e.g. custom user fields, renaming a function, etc.)
 *
 * @see https://tools.ietf.org/html/rfc6902
 *
 * @param[in] json to merge in
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_merge_patch (const char* json);

/** @see #sseh_merge_patch() */

typedef int (SSEH_CCONV* sseh_merge_patch_t) (const char*);

/******************************************************************************/

/**
 * Applies the SSEH configuration upon the process memory.
 *
 * @note Certain fields like the addresses of "original" functions trampoline.
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_apply ();

/** @see #sseh_apply() */

typedef int (SSEH_CCONV* sseh_apply_t) ();

/******************************************************************************/

/**
 * Execute custom command.
 *
 * This is highly implementation specific and may change any moment. It is like
 * patch hole for development use.
 *
 * @param[in] command identifier
 * @param[in,out] arg to pass in or out data
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_execute (const char* command, void* arg);

/** @see #sseh_execute() */

typedef int (SSEH_CCONV* sseh_execute_t) (const char*, void*);

/******************************************************************************/

/**
 * Set of function pointers as found in this file.
 *
 * Compatible changes are function pointers appened to the end of this
 * structure.
 */

struct sseh_api_v1
{
	/** @see #sseh_version() */
	sseh_version_t version;
	/** @see #sseh_last_error() */
	sseh_last_error_t last_error;
	/** @see #sseh_init() */
	sseh_init_t init;
	/** @see #sseh_uninit() */
	sseh_uninit_t uninit;
	/** @see #sseh_find_address() */
	sseh_find_address_t find_address;
	/** @see #sseh_load() */
	sseh_load_t load;
	/** @see #sseh_identify() */
	sseh_identify_t identify;
    /** @see #sseh_merge_patch() */
    sseh_merge_patch_t merge_patch;
	/** @see #sseh_apply() */
	sseh_apply_t apply;
	/** @see #sseh_execute() */
	sseh_execute_t execute;
};

/** Points to the current API version in use. */
typedef struct sseh_api_v1 sseh_api;

/******************************************************************************/

/**
 * Create an instance of #sseh_api, ready for use.
 *
 * @returns an API
 */

SSEH_API sseh_api SSEH_CCONV
sseh_make_api ();

/** @see #sseh_make_api() */

typedef sseh_api (SSEH_CCONV* sseh_make_api_t) ();

/******************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* SSEH_SSEHOOKS_H */

/* EOF */

