/**
 * @file sse-hooks.cpp
 * @copybrief sse-hooks.h
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
 * Implements the public API.
 */

#include <sse-hooks/sse-hooks.h>
#include <array>

//--------------------------------------------------------------------------------------------------

SSEH_API void SSEH_CCONV
sseh_version (int* api, int* maj, int* imp)
{
    constexpr std::array<int, 3> ver = {
#include "../VERSION"
    };
    if (api) *api = ver[0];
    if (maj) *maj = ver[1];
    if (imp) *imp = ver[2];
}

//--------------------------------------------------------------------------------------------------
