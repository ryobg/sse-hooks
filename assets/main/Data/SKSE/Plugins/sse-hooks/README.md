# Folder for placing SSE-Hooks JSON patches

All files here will be sorted and automatically loaded when SSE Hooks initializes.

* See https://tools.ietf.org/html/rfc6902 for what is JSON patch.
* You may also use some tools like https://json-patch-builder-online.github.io

## Example of file content:

```json
[
    {
        "op": "add",
        "path": "/_comment",
        "value": "Keys prefixed with underscore are not actually part of the document."
    },
    {
        "op": "add",
        "path": "/map",
        "value": {
            "ConsoleManager": {
                "target": "0x4002800",
                "detours": {
                    "0x120ab000": {
                        "original": "0x4002800"
                    },
                    "0x12012000": {
                        "original": "0x120ab000"
                    }
                }
            },
            "GetWindowText@user32.dll": {
                "_comment": "Module based detours are remembered too",
                "target": "0x12000",
                "detours": {
                    "0x120fff00": {
                        "original": "0x80020"
                    }
                }
            }
        }
    }
]
```
