# SSE Hooks

API for hooking upon SSE's sub-system, Papyrus API and maybe other functions.

## Requirements

* Python 3.x for the build system (2.x may work too)
* C++14 compatible compiler available on the PATH

## General features

* Main purpose: detour functions from the process memory (relies on MinHook for that
  https://github.com/TsudaKageyu/minhook)
* Flexible configuration management for what and how (relies on JSON, see
  https://github.com/nlohmann/json)
* Integrates small default logic for making SSEH a SKSE plugin

## Usage scenarios

The normal flow to use SSEH is to:

1. Initialize the library once, by calling `sseh_init ()`
2. Load a configuration file, read below, `sseh_load ()`
3. Apply all hooks, call `sseh_apply ()`
4. If all okay, do whatever is needed and at the end close the library by calling `sseh_uninit ()`

### Error handling

If there is an error, you can retreive some useful text by callying `sseh_last_error ()`. The errors
are per thread and valid until next API call.

```c++
std::size_t n;
if (sseh_last_error (&n, nullptr) && n)
{
    std::string s (n, '\0');
    sseh_last_error (&n, &s[0]);
    std::cout << s << std::endl;
}
```

### Calling the original

After `sseh_apply ()`, the trampolines to the original functions may be retreived by calling
`sseh_identify ()`. This function implements https://tools.ietf.org/html/rfc6901 and can be used as
way to report whatever part, or the whole, structure of the internal JSON. Also note that as the
library supports overpatching of the same target, there is a need for users to identify their
patches, hence all of them are uniquely named.

```c++
std::size_t n = 10; // 0x12345678
std::string s (n, '\0');
if (sseh_identify ("/hooks/GetWindowText/patches/MyPatch/original", &n, &s[0]) && n)
{
    if (auto trampoline = std::stoull (s, nullptr, 0))
        //...
}
```

It is not big saving, but for cleaner user code there is one wrapper provided:

```c++
std::uintptr_t trampoline;
if (sseh_original ("GetWindowText", "MyPatch", &trampoline))
    //...
```

Note that fetching of this function can be done only after a call to `sseh_apply ()` i.e. the hook
has been applied to the memory and the address can be retrieved.

### New hook at runtime

If the loaded JSON is not enough, and during runtime, a new hook is desired, it can be done through
`sseh_merge_patch()`. This function basically implements the https://tools.ietf.org/html/rfc6902
JSON patch merging. Hence addition, removal, insertion and etc are supported operations. Note that
`sseh_apply()` should be called again. It is advised that such patches are batched to avoid the
overhead.

```c++
std::string newdetour = R"([
    { "op": "add", "path": "/hooks/GetWindowText/patches/MyPatch", "value":
                {
                    "detour": "0x00001234"
                }
    }])";
if (sseh_merge_patch (newdetour.c_str ()))
{
    //... sseh_apply ();
}
```

Note however that the example above assumes that the hook `GetWindowText` already exists. If it does
not this entry will be invalid, hence first it can be checked whether it exists, if not added:

```c++
if (!sseh_identify ("/hooks/GetWindowText", nullptr, nullptr))
{
    std::string newhook = R"([
        { "op": "add", "path": "/hooks/GetWindowText", "value":
                {
                    "module": "kernel32"
                }
        }])";
    if (sseh_merge_patch (newhook.c_str ()))
        // ...
}
//... sseh_merge_patch ()
//... sseh_apply ()
```

In both examples, not full specification of the JSON node is given. During the patching, SSEH will
rebuild the rest of the parameters. Some of them are mandatory: there cannot be a patch entry
without detour target, nor there can be a hook withouth a module or a target address (SSEH will try
to find the address of that function if there is a module specified, not it can be also an empty
string, which means to search the current process).

This can get a bit clumsy to write so, there is a shortcut for that:

```c++
if (sseh_detour ("kernel32", "GetWindowText", "MyPatch", 0x00001234))
    //...
```

Which basically will create a hook if needed and insert the patch.

### Finding an address of function

Currently, there is an way to find a function address by searching for its name. The search is done
either in the process, or in some module loaded by the process.

```c++
std::uintptr_t address;
if (sseh_find_address ("kernel32", "GetOverlappedText", &address))
    //...
if (sseh_find_address (nullptr, "SomeFunction", &address))
    //...
```

## Configuration

SSEH is using JSON formatted configuration structure. This allows flexible management, but working
with it requires certain schema to comply to.

An example of configuration:

```json
{
    "_comment1": "All keys prefixed with underscore are reserved by SSEH."
    "_comment2": "User-specific keys are supported, though ignored. They also should not collide."

    "_" : {
        "version": "1.0.0"
    },

    "hooks" : 
    {
        "GetWindowText" : 
        {
            "_comment": "Ask SSEH to try find the procedure address and set it as 'target'",

            "module": "user32",
            "target": 0
        },
        "Some process function" :
        {
            "_comment1": "Names may be whatever unique string. 'name' and 'target' are mandatory.",
            "_comment2": "'target' is unique too. A hex or a dec string, or just an number.",

            "target": "0x40002000"
        },
        "foo" :
        {
            "target": "0x44002FFF",

            "_comment": "SSEH creates and updates some fields during runtime, available for users",

            "_comment_applied": "Is this hook applied (i.e. function is patched/detoured)?",
            "applied": false,

            "_comment_status": "A human-readable message, it can be an error text too",
            "status": "created"
        },
        "GetOverlappedResult" :
        {
            "module": "kernel32",
            "target": "0x400020",
            "applied": true,
            "status": "detoured",

            "_comment": "'patches' is a list of pairs describing the detours made after apply.",
            "patches": {
                "User #1" :
                {
                    "detour": "0x8000FF00",
                    "original": "0x400020"
                },
                "Another user #2" :
                {
                    "detour": "0xC000000",
                    "original": "0x8000FF00"
                }
            }
        }
    }
}
```

## Woking with configuration

There are few procedures which can be used to fetch some data, ot modify it or just to load at once
a file with JSON configuration.

Loading of configuration file after initialization is common case. It reads a file as JSON and
assigns, or replace if exist, the current configuration database. Refer to `sseh_load()`.

In order to read, or see, what kind of data is currently stored in SSEH, `sseh_identify()` can be
used. By passing a path or the whole root as `/`, a JSON will be returned. This function in fact
follows the convention as described in https://tools.ietf.org/html/rfc6901

In order to modify the configuration, a JSON merge patch is is passed to `sseh_merge_patch()`. It
allows flexible addition, replacing, removing and copying of values. See
https://tools.ietf.org/html/rfc6902



