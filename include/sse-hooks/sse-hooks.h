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
 * @note Unless mentioned otherwise, all strings are in UTF-8 encoding.
 *
 * @details
 * This file encompass all the functions which are presented to the users of
 * SSE Hooks. The interface targets to be maximum compatible and portable
 * across the expected build and usage scenarios. This file uses generic C, but
 * is compatible with C++. As the methods are to be exported in the DLL, this
 * lib interface can be also accessed from other languages too.
 *
 * An way to access the interface is to use the SKSE inter-plugin messaging
 * system. Basically, by sending a message to SSEH, in return, a pointer to the
 * API structure (see #sseh_api) will be reported. This way, on one pass, an
 * access to the full suite of methods is given. Note, however that layout of
 * the structure maybe different than what is expected, in order to get the
 * correct structure, the 'type' param should hold the requested API version.
 * This way, if the actual API is different, the user code won't break.
 *
 * An example of how to send a SKSE plugin message:
 *
 * @code
 * std::string req = "sseh_api";
 * p->Dispatch (sender, api_ver = 1, &req[0], req.size (), "SSEH");
 * @endcode
 *
 * In this case, 1 is the version of the API requested. If nothing comes back,
 * then this is not supported request. `p` is of type SKSEMessagingInterface,
 * refer to the SKSE plugin API for more information how it can be obtained.
 *
 * So, an example of how to handle the SKSE plugin messages sent by SSEH:
 *
 * @code
 * void got_skse_message (Message* m)
 * {
 *      std::string func, err;
 *      if (m.sender && std::strcmp (m.sender, "SSEH") == 0)
 *      {
 *          assert (m.type == api_ver);
 *          assert (m.dataLen == sizeof (sseh_api));
 *          auto sseh = *reinterpet_cast<sseh_api*> (m.data);
 *      }
 * }
 * @endcode
 *
 * In most SKSE plugin cases, most work that they should do is just to create
 * association between some name of a function and the target which will be
 * overwitten (see #sseh_map_hook()). If other plugin already hooked on that
 * address, with the same or different name - see #sseh_hook_name() or
 * #sseh_hook_address(). If the address is not known, see #sseh_find_address().
 *
 * After the mapping was done, #sseh_detour() should be called with the actual
 * address to jump to from now on. That's all.
 *
 * When the time window for mapping and detouring passes, SSEH will enable all
 * hooks and notify all interested plugins. As there is some chance or situation
 * when certain hooks failed, others not, during that notification plugins
 * should go and check the status of their hook, see #sseh_hook_state().
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
 * @param[out] api (optional) version
 * @param[out] maj (optional) version
 * @param[out] imp (optional) version
 */

SSEH_API void SSEH_CCONV
sseh_version (int* api, int* maj, int* imp);

/** @see #sseh_version() */

typedef void (SSEH_CCONV* sseh_version_t) (int*, int*, int*);

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
 * This function should not be called explicitly in SKSE/SSE environment. The
 * plugin takes care when to initialize itself.
 *
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_init ();

/** @see #sseh_init() */

typedef int (SSEH_CCONV* sseh_init_t) ();

/**
 * Uninitialize (pre-init state) SSEH.
 *
 * This function should not be called explicitly in SKSE/SSE environment. The
 * plugin itself takes care when to initialize itself.
 */

SSEH_API void SSEH_CCONV
sseh_uninit ();

/** @see #sseh_uninit() */

typedef void (SSEH_CCONV* sseh_uninit_t) ();

/******************************************************************************/

/**
 * Associate an unique name with target address.
 *
 * This just writes down in SSEH that this unique name for a function should
 * from now on refer to the given target (unmodified) function address.
 *
 * 1. If the same name, with the same address was mapped before, nothing is
 * changed.
 * 2. If that name, was before associated with another address, then this
 * operation fails. You can see which address is that name referring to, by
 * calling #sseh_hook_address()
 * 3. If that address is already, hooked under different name, then this
 * operation fails. You can see which name it is registered under, by calling
 * #sseh_hook_name()
 *
 * If the function address is not known, it can be searched first by name, see
 * #sseh_find_address(). After the mapping is done, notification from SSEH (
 * basically after it calls #sseh_enable_hooks()) will be sent, status of
 * each hook can be checked with #sseh_hook_status().
 *
 * @param name of the address (e.g. a function name)
 * @param address of the function to be hooked later
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_map_hook (const char* name, uintptr_t address);

/** @see #sseh_map_hook() */

typedef int (SSEH_CCONV* sseh_map_hook_t) (const char*, uintptr_t);

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
 * Report hook name for given target address.
 *
 * @param[in] address of the target to report for
 * @param[in,out] size of the the incoming @param names - in bytes, on exit how
 * many bytes the name is (trailing null byte is not counted). Can be zero to
 * denote that for this address there is no mapping done yet.
 * @param[out] name of the hook, a null-terminated string. It can be a nullptr,
 * so that only the @param size can be fetched to pre-allocate a buffer.
 * @returns non-zero on finding a hook with such address, zero otherwise.
 */

SSEH_API int SSEH_CCONV
sseh_hook_name (uintptr_t address, size_t* size, char* name);

/** @see #sseh_hook_name() */

typedef int (SSEH_CCONV* sseh_hook_name_t) (uintptr_t, size_t*, char*);

/**
 * Report address associated with given name.
 *
 * @param[in] name of the function to report the address for
 * @param[out] address associated, or zero if such name does not exist
 * @returns non-zero on finding a hook with such name, zero otherwise.
 */

SSEH_API int SSEH_CCONV
sseh_hook_address (const char* name, uintptr_t* address);

/** @see #sseh_hook_address() */

typedef int (SSEH_CCONV* sseh_hook_address_t) (const char*, uintptr_t*);

/**
 * Reports whether the given hook is enabled or not.
 *
 * Hooks which does not exist, or were not successfull, or are still pending for
 * a call to #sseh_enable_hooks() will report zero (aka false).
 *
 * @param[in] name of the hook to get status for
 * @param[out] enabled flag, zero is not enabled, non-zero is enabled/active.
 * @returns non-zero on finding a hook with such name, zero otherwise.
 */

SSEH_API int SSEH_CCONV
sseh_hook_status (const char* name, int* enabled);

/** @see #sseh_hook_status() */

typedef int (SSEH_CCONV* sseh_hook_status_t) (const char*, int*);

/******************************************************************************/

/**
 * Detour a function to new one.
 *
 * Basically patch the address so that new function in @param address is called
 * from now on. If not a nullptr, @param original can report the address of the
 * original function as it was moved.
 *
 * Multiple clients will be served in order of arrival.
 *
 * @param[in] name of the hook
 * @param[in] address to the new function to be called
 * @param[out] original (optional) function address from now on.
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_detour (const char* name, uintptr_t address, uintptr_t* original);

/** @see #sseh_detour() */

typedef int (SSEH_CCONV* sseh_detour_t) (const char*, uintptr_t, uintptr_t*);

/******************************************************************************/

/**
 * Apply all detours.
 *
 * This function should not be called explicitly in SKSE/SSE environment. The
 * plugin itself takes care when to enable all hooks and when to disable them.
 *
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_enable_hooks ();

/** @see #sseh_enable_hooks() */

typedef int (SSEH_CCONV* sseh_enable_hooks_t) ();

/**
 * Remove, restore back all hooks previously made.
 *
 * This function should not be called explicitly in SKSE/SSE environment. The
 * plugin itself takes care when to enable all hooks and when to disable them.
 *
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_disable_hooks ();

/** @see #sseh_disable_hooks() */

typedef int (SSEH_CCONV* sseh_disable_hooks_t) ();

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

/** Set of function pointers as found in this file */
struct sseh_api_v1
{
    /** Holds a pointer to compatible extensions of the current API. */
    uintptr_t next;

	/** @see #sseh_version() */
	sseh_version_t version;
	/** @see #sseh_last_error() */
	sseh_last_error_t last_error;
	/** @see #sseh_init() */
	sseh_init_t init;
	/** @see #sseh_uninit() */
	sseh_uninit_t uninit;
	/** @see #sseh_map_hook() */
	sseh_map_hook_t map_hook;
	/** @see #sseh_find_address() */
	sseh_find_address_t find_address;
	/** @see #sseh_hook_name() */
	sseh_hook_name_t hook_name;
	/** @see #sseh_hook_address() */
	sseh_hook_address_t hook_address;
	/** @see #sseh_hook_status() */
	sseh_hook_status_t hook_status;
	/** @see #sseh_detour() */
	sseh_detour_t detour;
	/** @see #sseh_enable_hooks() */
	sseh_enable_hooks_t enable_hooks;
	/** @see #sseh_disable_hooks() */
	sseh_disable_hooks_t disable_hooks;
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

