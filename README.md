# SSE Hooks

API for labeling and rerouting of functions, or data fields, in a process. Main purpose being used
for uniform patch framework of Skyrim SE implemented as SKSE plugin.

General features:

* Detour functions from the process memory (relies on MinHook for that
  https://github.com/TsudaKageyu/minhook and https://github.com/m417z/minhook)
* Flexible configuration management for what and where (relies on JSON, see
  https://github.com/nlohmann/json)
* Integrates small default logic for making SSEH a SKSE plugin
* Can be used for labeling of any address (e.g. variables, maybe phone numbers).

# Usage

## API sample

```c++
extern decltype (GetWindowText) original;
if (sseh_detour ("GetWindowText@user32", my_window_text, &original))
    sseh_apply ();
```

and somewhere else in the code:

```c++
int my_window_text (HWND h, LPSTR s, int c)
{
    std::cout << "Hello from here!" << std::endl;
    return original (h, s, c);
}
```

## SKSE plugins

Need only the headers from `include` and the DLL itself installed as SKSE plugin. Then just take the
pointer to the API during mod load and use.

```c++
#include <sse-hooks/sse-hooks.h>

extern "C" SSEH_API bool SSEH_CCONV
SKSEPlugin_Load (SKSEInterface const* skse)
{
    auto p = (SKSEMessagingInterface*) skse->QueryInterface (kInterface_Messaging);
    p->RegisterListener (plugin, "SSEH", assign_sseh_interface);
}

static sseh_api sseh;

void assign_sseh_interface (SKSEMessagingInterface::Message* m)
{
    assert (m->type == SSEH_API_VERSION);
    assert (m->dataLen >= sizeof (sseh_api)); // can be bigger if compatible
    sseh = *reinterpret_cast<sseh_api*> (m->data);
}
```

## End users

Not much. For example, as SSE/SKSE is updated, you need just to have the correct address in a
JSON file. This can be community driven so that everybody can benefit. In the case of module based
functions, nothing needs to even change. In the example below, only `/map/ConsoleManager/target` is
subject to change:

```json
{
    "_" : {
        "version": "1.0.0"
    },
    "map" : 
    {
        "ConsoleManager" :
        {
            "target" : "0x4002800"
        }
    }
}
```

Note that while mappings to addresses are done mainly for functions, the same mechanism can be used
for any data field or function which are not going to be detoured later on.

## General flow

1. Initialize the library once, by calling `sseh_init ()`
2. Load pre-defined set of named targets by:
    1. Loading a database file using `sseh_load ()`
    2. And/or call `sseh_map_name ()`
3. Create detours of function(s) with `sseh_detour ()`.
4. Apply one or more hooks through `sseh_enable ()` and at then end - `sseh_apply ()`
5. If all okay, do whatever is needed and at the end close the library by calling `sseh_uninit ()`

If the operating environment for SSEH is as SKSE plugin, then most of these operations are handled
by the default implementation of the bundled plugin. SKSE plugin developers need only to get the
interface, detour functions and wait for them to be called. The plugin takes care to initialize the
library, load a default JSON file, wait for any detours and apply all of them on the go.

# API details

## Error handling

If there is an error, you can retreive some useful text by callying `sseh_last_error ()`:

```c++
std::size_t n;
if (sseh_last_error (&n, nullptr) && n)
{
    std::string s (n, '\0');
    sseh_last_error (&n, &s[0]);
    std::cout << s << std::endl;
}
```

## Loading a configuration

Naming an address of function or variable helps later. The name will stay, but the address
behind it may change, depending on the implementation of the hooked application. When a call to
`sseh_load ()` is made, all internal mapping are replaced with the content in that file. Custom keys
are kept (in fact, ignored) and content is validated. Mappings which are not valid will be deleted.

```c++
if (sseh_load ("sseh.json"))
    //...
```

All names and addresses should be unique. Subsequent dublicates are deleted.

## New mappings at runtime

If the loaded JSON is not enough, a new mapping may be added through `sseh_map_name ()`.

```c++
if (sseh_map_name ("ConsoleManager", 0x400200))
    //...
```

If the name already exists with the same address, the function will succeed. But if either that
address has another name, or that name has another address - it will fail. This helps keep the map
tight and clean it also matches the behaviour for functions based in dynamically loaded libraries.

## Collisions and simple registry queries

As the names of addresses and the addresses themeselves have to be unique across the registry, it is
useful to find the colliding elements. 

```c++
std::uintptr_t target;
if (sseh_find_target ("ConsoleManager", &target))
    //...
if (sseh_find_target ("GetWindowText@user32", &target))
    //...
std::size_t n;
if (sseh_find_name (0x400200, &n, nullptr))
    //...
```

## Detours

After there are unique names for each address, they can be used to create the actual detours and 
queue them for an actual patching (i.e. enable & apply them). For that purpose, `sseh_detour ()` is
used. It accepts the mapped name, the address of the detour and optionally, the address to the new 
function which will call the original. If the passed in name is recognized as module name (e.g.
`GetWindowText@user32`) its target address is searched for.

```c++
void* original;
if (sseh_detour ("GetWindowText@user32", my_window_text, &original))
    //...
```

After a detour is created, it must be enabled. Note again, that detour is needed only for functions
which are hooked, detouring is not needed when the function or the variables are just to be
referenced. Also, the original can be called only after `sseh_apply()`.

## Enabling target detours

As mentioned, after having a detour, it must be queued for enabling, or disabling if already
created and after that, apply the queue at once. This is a bit cumbersome for single enable/disable
operations, but it helps to structure the code so that performance can be improved - applying is
costly operation. The parameter name is the name of the target.

```c++
sseh_enable (const char* name);
sseh_disable (const char* name);
sseh_enable_all ();
sseh_disable_all ();
sseh_apply ();
```

## Multihooks

In order to support many clients hooking the same target, profiles were introduced. Each profile
is represents an unique state for functions which detour, enable, disable or apply hooks. There is
one global profile by default (the empty "" string), and this may suffice for applications where one
client is hooking the same function, but if there are many clients who wants the same function - it
is advised that each one of them has its unique profile.

```c++
sseh_profile ("MyPlugin");
sseh_detour ("GetWindowText@user32", my_window_text, &original);
sseh_apply ();
sseh_profile ("AnotherPlugin");
sseh_detour ("GetWindowText@user32", other_window_text, &other_original);
sseh_apply ();
```

## Accessing the registry

SSEH allows direct access to its JSON registry. This allows as advanced manipulations, workarounds,
custom extensions and so on. To allow these functionalities there are two workhorses for use:
`sseh_merge_patch ()` and `sseh_identify ()`. One is to modify the registry, the other is to
retreive information from it.

`sseh_merge_patch ()` implements the https://tools.ietf.org/html/rfc6902 JSON patch merging.

```c++
std::string foo = R"([{ "op":"add", "path":"/map/ConsoleManager/target", "value":"0x400200" }])";

if (sseh_merge_patch (foo.c_str ()))
{
    //... sseh_apply ();
}
```

And `sseh_identify ()` implements the https://tools.ietf.org/html/rfc6901 JSON pointer semantic.

```c++
std::size_t n;
if (sseh_identify ("/map/ConsoleManager/target", &n, nullptr))
{
    std::string s (n);
    sseh_identify ("/map/ConsoleManager/target", &n, &s[0]));
    auto target = std::stoull (s, nullptr, 0);
    //...
}
```

## JSON structure

The internal registry is updated at runtime, whether it was loaded at first from a file or not. Some
fields like made detours, the kept originals, statuses and etc, are added and updated during work.
Below is an example of such document:

```json
{
    "_comment": "Keys prefixed with underscore are not actually part of the document.",

    "SSEH" : {
        "version": "1.0.0"
    },

    "map" : 
    {
        "ConsoleManager" :
        {
            "target" : "0x4002800",
            "detours":
            {
                "0x120ab000":
                {
                    "original": "0x4002800",
                    "enabled": true,
                }
                "0x12012000":
                {
                    "_comment": "'queue' shows whether this detour is to be enabled or disabled",

                    "original": "0x120ab000",
                    "enabled": false,
                    "queue": true,
                }
            }
        },

        "GetWindowText@user32":
        {
            "_comment": "Module based detours are remembered too",

            "target" : "0x12000",
            "detours":
            {
                "0x120fff00":
                {

                    "original": "0x80020",
                    "enabled": true,
                }
            }
        }
    }
}
```

# Development

* All incoming or outgoing strings are UTF-8 compatible. Internally, SSEH converts these to the
  Windows Unicode when needed.
* The API is not thread safe as in SKSE environment where hooks are done at load time, it is
  unlikely to be needed.

## Required tools

* Python 3.x for the build system (2.x may work too)
* C++14 compatible compiler available on the PATH

## License

LGPLv3, see the LICENSE.md file. 

The idea is to promote this library as open source project, but still keep it as shared resource.
LGPL allows closed source projects to use SSEH, but only when it is used as DLL. If a project wants
to bundle inside SSEH, it should open its source to the public. If many people start bundling this
project, most likely the main idea, to have a central shared file where to update the target
addresses will vanish, as everybody will have its own file and mappings.

