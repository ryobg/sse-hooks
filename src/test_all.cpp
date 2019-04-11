/**
 * @file test_all.cpp
 * @brief Implementing the main test driver
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
 * n / a
 */

#include <sse-hooks/sse-hooks.h>

//--------------------------------------------------------------------------------------------------

/// Test whether we will crash with nullptr arguments
bool test_sseh_version ()
{
    int a, m, i;
    sseh_version (nullptr, nullptr, nullptr);
    sseh_version (&a, nullptr, nullptr);
    sseh_version (&a, &m, nullptr);
    sseh_version (&a, &a, &a);
    sseh_version (nullptr, &m, &m);
    a = m = i = -1;
    sseh_version (&a, &m, &i);
    return a >= 0 && m >= 0 && i >= 0;
}

//--------------------------------------------------------------------------------------------------

int main ()
{
    int ret = 0;
    ret += test_sseh_version ();
    return ret;
}

//--------------------------------------------------------------------------------------------------

