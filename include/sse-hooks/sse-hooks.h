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
 * @details
 * This file encompass all the functions which are presented to the users of
 * SSE Hooks. The interface targets to be maximum compatible and portable
 * across the expected build and usage scenarios. This file uses generic C API.
 */

#ifndef SSEH_SSEHOOKS_H
#define SSEH_SSEHOOKS_H

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

/******************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* SSEH_SSEHOOKS_H */

/* EOF */

