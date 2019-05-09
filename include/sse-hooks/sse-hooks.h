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
 * @note This API is not thread-safe.
 * @note Unless mentioned, all strings are null-terminated and in UTF-8.
 *
 * @details
 * This file encompass all the functions which are presented to the users of
 * SSE Hooks. The interface targets to be maximum compatible and portable
 * across the expected build and usage scenarios. This file uses generic C, but
 * is compatible with C++. As the methods are to be exported in the DLL, this
 * lib interface can be also accessed from other languages too.
 */

#ifndef SSEH_SSEHOOKS_H
#define SSEH_SSEHOOKS_H

#include <stdint.h>
#include <sse-hooks/platform.h>

/// To match a compiled in API against one loaded at run-time.
#define SSEH_API_VERSION (1)

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

/**
 * Run-time version of this API and its implementation details.
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
 * re-release of something and etc.
 *
 * It is advised to check @param api against #SSEH_API_VERSION.
 *
 * @param[out] api (optional) non-portable
 * @param[out] maj (optional) new features and enhancements
 * @param[out] imp (optional) patches
 * @param[out] timestamp (optional) in ISO format
 */

SSEH_API void SSEH_CCONV
sseh_version (int* api, int* maj, int* imp, const char** timestamp);

/** @see #sseh_version() */

typedef void (SSEH_CCONV* sseh_version_t) (int*, int*, int*, const char**);

/******************************************************************************/

/**
 * Report the last message in more human-readable form.
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
 * Uninitialize SSEH.
 */

SSEH_API void SSEH_CCONV
sseh_uninit ();

/** @see #sseh_uninit() */

typedef void (SSEH_CCONV* sseh_uninit_t) ();

/******************************************************************************/

/**
 * Switch to hook profile
 *
 * Hook profile enables multihooking. As there can be only one hook per target,
 * switching to different global state profile enables multihooking. These
 * profiles influence the following functions: #sseh_detour(), #sseh_enable(),
 * #sseh_disable(), #sseh_enable_all(), #sseh_disable_all() and #sseh_apply ().
 * Basically, all functions which have something to do with hooking. An example:
 * detouring a function in profile "ABC" can happen only once, but if another
 * profile is used (e.g. "MYPROF") it can be detoured again. Also enabling, or
 * disabling a hook in one profile does not influence the others.
 *
 * @param profile to switch to
 * @returns non-zero on success, otherwise see #sseh_last_error()
 */

SSEH_API int SSEH_CCONV
sseh_profile (const char* profile);

/** @see #sseh_profile() */

typedef int (SSEH_CCONV* sseh_profile_t) (const char*);

/******************************************************************************/

/**
 * Reports function address in given runtime module, or the current process.
 *
 * @see ::GetModuleHandle* and ::GetProcAddress*
 *
 * @param[in] module name of the library to search, or nullptr for this process.
 * @param[in] name of the function to search for
 * @param[out] address found, or nullptr on error
 * @returns non-zero on success, otherwise see #sseh_last_error()
 */

SSEH_API int SSEH_CCONV
sseh_find_address (const char* module, const char* name, void** address);

/** @see #sseh_find_address() */

typedef int (SSEH_CCONV* sseh_find_address_t)
    (const char*, const char*, void**);

/******************************************************************************/

/**
 * Load from file or string and replace the JSON configuration.
 *
 * @param[in] filepath to read from
 */

SSEH_API int SSEH_CCONV
sseh_load (const char* filepath);

/** @see #sseh_load() */

typedef int (SSEH_CCONV* sseh_load_t) (const char*);

/**
 * Create new mapping to an address.
 *
 * Dublicated names or addresses are not allowed. The sseh_find_* functions can
 * be used for resolution and reuse.
 *
 * @param name to act as a key
 * @param address to act as a value
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_map_name (const char* name, uintptr_t address);

/** @see #sseh_map_name() */

typedef int (SSEH_CCONV* sseh_map_name_t) (const char*, uintptr_t);

/**
 * Find to which target address is mapped already the given name.
 *
 * Names are case-sensitive. Module based names can be used too, but they
 * make sense only if there is entry already for them after a detour. If
 * an address of such a method is needed, see #sseh_find_address().
 *
 * @param[in] name to search the target address for
 * @param[out] target to receive the found value
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_find_target (const char* name, uintptr_t* target);

/** @see #sseh_find_target() */

typedef int (SSEH_CCONV* sseh_find_target_t) (const char*, uintptr_t*);

/**
 * Find the name mapped to given target address.
 *
 * @param[in] target address to search for, zero is invalid
 * @param[in,out] size (optional) in bytes of @param name, on exit how many
 * bytes were actually written (excluding the terminating null) or how many
 * bytes are needed in order to get the full name.
 * @param[out] name (optional) to hold the value, can be nullptr if @param size
 * is needed to pre-allocate a buffer.
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_find_name (uintptr_t target, size_t* size, char* name);

/** @see #sseh_find_name() */

typedef int (SSEH_CCONV* sseh_find_name_t) (uintptr_t, size_t*, char*);

/******************************************************************************/

/**
 * Create a new detour and queue it for enabling.
 *
 * @param[in] name of the mapped ones, or function@module to find and use
 * @param[in] detour function to replace the target one
 * @param[out] original to use when the target function has to be called
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_detour (const char* name, void* detour, void** original);

/** @see #sseh_detour() */

typedef int (SSEH_CCONV* sseh_detour_t) (const char*, void*, void**);

/******************************************************************************/

/**
 * Queue a pre-created detour for enabling.
 *
 * @param name of the hook to queue
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_enable (const char* name);

/** @see #sseh_enable() */

typedef int (SSEH_CCONV* sseh_enable_t) (const char*);

/**
 * Queue a pre-created detour for disabling.
 *
 * @param name of the hook to queue
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_disable (const char* name);

/** @see #sseh_enable() */

typedef int (SSEH_CCONV* sseh_disable_t) (const char*);

/**
 * Queue all disabled detours for enabling.
 *
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_enable_all ();

/** @see #sseh_enable_all() */

typedef int (SSEH_CCONV* sseh_enable_all_t) ();

/**
 * Queue all enabled detours for disabling.
 *
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_disable_all ();

/** @see #sseh_disable_all() */

typedef int (SSEH_CCONV* sseh_disable_all_t) ();

/**
 * Applies the queued operations.
 *
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_apply ();

/** @see #sseh_apply() */

typedef int (SSEH_CCONV* sseh_apply_t) ();

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
 * @param[in,out] size (optional) number of bytes of the incoming @param json
 * array. On exit reports how many bytes were actually used (w/o the
 * terminating null) or how many bytes are needed to store the whole result
 * (again, w/o the terminating null).
 * @param[out] json (optional) to store the reported result
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
	/** @see #sseh_profile() */
	sseh_profile_t profile;
	/** @see #sseh_find_address() */
	sseh_find_address_t find_address;
	/** @see #sseh_load() */
	sseh_load_t load;
	/** @see #sseh_map_name() */
	sseh_map_name_t map_name;
	/** @see #sseh_find_target() */
	sseh_find_target_t find_target;
	/** @see #sseh_find_name() */
	sseh_find_name_t find_name;
	/** @see #sseh_detour() */
	sseh_detour_t detour;
	/** @see #sseh_enable() */
	sseh_enable_t enable;
	/** @see #sseh_disable() */
	sseh_disable_t disable;
	/** @see #sseh_enable_all() */
	sseh_enable_all_t enable_all;
	/** @see #sseh_disable_all() */
	sseh_disable_all_t disable_all;
	/** @see #sseh_apply() */
	sseh_apply_t apply;
	/** @see #sseh_identify() */
	sseh_identify_t identify;
    /** @see #sseh_merge_patch() */
    sseh_merge_patch_t merge_patch;
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

