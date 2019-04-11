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
 * @note Unless mentioned otherwise, all strings are in UTF-8 encoding.
 * @note This API is thread-safe.
 *
 * @details
 * This file encompass all the functions which are presented to the users of
 * SSE Hooks. The interface targets to be maximum compatible and portable
 * across the expected build and usage scenarios. This file uses generic C API.
 *
 * An example of how to send a SKSE plugin message to call #sseh_map_hook()
 *
 * @code
 * std::vector<char> buff;
 *
 * func = "sse_map_hook";
 * buff.insert (buff.end (), func.begin (), func.end ());
 * buff.push_back ('\0');
 *
 * name = "LoadLibraryA";
 * buff.insert (buff.end (), name.begin (), name.end ());
 * buff.push_back ('\0');
 *
 * buff.insert (buff.end (), (char*) &address, (char*) address + sizeof address)
 *
 * Dispatch (sender, sseh_api_version, buff.data (), buff.size (), "SSEH");
 * @endcode
 *
 * Basically, the null-terminated function name and then all of its parameters
 * are layed out flat memory on the memory buffer. And in return SSEH, will just
 * layout back the function name, any output parameters and at the end an error
 * message if any.
 *
 * An example of how to handle SKSE plugin messages sent by SSEH:
 *
 * @code
 * void got_skse_message (Message* m)
 * {
 *      std::string func, err;
 *      if (m.sender && std::strcmp (m.sender, "SSEH") == 0)
 *      {
 *          assert (m.type == sseh_api_version);
 *          func = reinterpet_cast<char const*> (m.data);
 *          assert (func == "sse_map_hook");
 *          if (m.dataLen > func.size () + 1) // If error has occured.
 *              err = reinterpet_cast<char const*> (m.data) + func.size () + 1;
 *          assert (err.size () + 1 + func.size () + 1 == m.dataLen);
 *      }
 * }
 * @endcode
 *
 * The SKSE Message struct type param is always set to the current api number.
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
 * SKSE dispatch memory: "sseh_version"
 * SKSE receiver memory: "sseh_version", api, maj, imp
 *
 * @param[out] api (optional) version
 * @param[out] maj (optional) version
 * @param[out] imp (optional) version
 */

SSEH_API void SSEH_CCONV
sseh_version (int* api, int* maj, int* imp);

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

/******************************************************************************/

/**
 * Initialize SSEH.
 *
 * This function must be called first for the process. Excluded are:
 * #sseh_version() and #sseh_last_error().
 *
 * This function should not be called explicitly in SKSE/SSE environment. The
 * plugin itself takes care when to initialize itself.
 *
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_init ();

/**
 * Uninitialize (pre-init state) SSEH.
 *
 * This function should not be called explicitly in SKSE/SSE environment. The
 * plugin itself takes care when to initialize itself.
 */

SSEH_API void SSEH_CCONV
sseh_uninit ();

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
 * SKSE dispatch memory: "sseh_map_hook", "name", address
 * SKSE receiver memory: "sseh_map_hook", [error]
 *
 * @param name of the address (e.g. a function name)
 * @param address of the function to be hooked later
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_map_hook (const char* name, uintptr_t address);

/**
 * Report hook names for given target address.
 *
 * SKSE dispatch memory: "sseh_hook_name", address
 * SKSE receiver memory: "sseh_hook_name", "name"
 *
 * @param[in] address of the target to report for
 * @param[in,out] size of the the incoming @param names - in bytes, on exit how
 * many bytes the name is. Trailing null byte is not counted.
 * @param[out] name of the hook, a null-terminated string. It can be a nullptr,
 * so that only the @param size can be fetched to pre-allocate a
 * buffer. Though in multi-threaded environment it may not be valid for long.
 */

SSEH_API void SSEH_CCONV
sseh_hook_name (uintptr_t address, size_t* size, char* name);

/**
 * Report address associated with given name.
 *
 * SKSE dispatch memory: "sseh_hook_address", "name"
 * SKSE receiver memory: "sseh_hook_address", address
 *
 * @see #sseh_hook_name()
 */

SSEH_API void SSEH_CCONV
sseh_hook_address (const char* name, uintptr_t* address);

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
 * SKSE dispatch memory: "sseh_detour", "name", address
 * SKSE receiver memory: "sseh_detour", original, [error]
 *
 * @param[in] name of the hook
 * @param[in] address to the new function to be called
 * @param[out] original (optional) function address from now on.
 * @returns non-zero on success, otherwise see #sseh_last_error ()
 */

SSEH_API int SSEH_CCONV
sseh_detour (const char* name, uintptr_t address, uintptr_t* original);

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

/******************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* SSEH_SSEHOOKS_H */

/* EOF */

