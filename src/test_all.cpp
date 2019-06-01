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
#include <nlohmann/json.hpp>

#include <iostream>
#include <fstream>

//--------------------------------------------------------------------------------------------------

using namespace std;
using nlohmann::json;
typedef json::json_pointer json_pointer;

#define TEST(x) \
    if (!x) result = false, \
        std::cout << "Test fail " << __FILE__ << ":" << __LINE__ \
                  << " " << last_error () << std::endl;

//--------------------------------------------------------------------------------------------------

static std::string
last_error ()
{
    size_t n = 0;
    sseh_last_error (&n, nullptr);
    if (n)
    {
        std::string s (n+1, '\0');
        sseh_last_error (&n, &s[0]);
        return s;
    }
    return "";
}

//--------------------------------------------------------------------------------------------------

static const char* generic_json = R"(
{
    "map": {
        "D3D11CreateDeviceAndSwapChain@d3d11.dll": {
            "detours": {
                "0x70a2b9b0": {
                    "original": "0x7ffe81aa0fd6"
                }
            },
            "target": "0x7ffe81ac5950"
        },
        "IDXGISwapChain::Present": {
            "target": "0x7ffe834b5070"
        }
    },
    "profiles": {
        "": 0,
        "SSGUI": 1
    }
}
        )";

//--------------------------------------------------------------------------------------------------

static bool
test_loading ()
{
    bool result = true;
    TEST (sseh_load (generic_json));
    return result;
}

//--------------------------------------------------------------------------------------------------

static bool
test_sseh_version ()
{
    int a, m, i;
    const char* b;
    sseh_version (nullptr, nullptr, nullptr, nullptr);
    sseh_version (&a, nullptr, nullptr, nullptr);
    sseh_version (&a, &m, nullptr, nullptr);
    sseh_version (&a, &a, &a, nullptr);
    sseh_version (nullptr, &m, &m, &b);
    a = m = i = -1; b = nullptr;
    sseh_version (&a, &m, &i, &b);
    return a >= 0 && m >= 0 && i >= 0 && b;
}

//--------------------------------------------------------------------------------------------------

static bool
test_patching ()
{
    bool result = true;

    std::ifstream fi ("test.json");
    if (!fi.is_open ())
        return result;

    std::string content;
    content.assign (std::istreambuf_iterator<char> (fi), std::istreambuf_iterator<char> ());
    TEST (sseh_merge_patch (content.c_str ()));

    return result;
}

//--------------------------------------------------------------------------------------------------

int main ()
{
    int ret = 0;
    ret += test_sseh_version ();
    ret += test_loading ();
    ret += test_patching ();
    return ret;
}

//--------------------------------------------------------------------------------------------------

