# Simple Patch Loader

A simple patch loader for Geode.

## Usage

Click "Open Patches Folder" and put your patch files in there.

### Patch Format

too lazy to write this rn so heres just an example

```json

{
    "name": "Test",
    "patches": {
        "test-patch": {
            "name": "Test Patch",
            "description": "old accurate percentage",
            "address": {
                "0x208114": "C7 02 25 66 25 25 8B 87 C0 03 00 00 8B B0 04 01 00 00 F3 0F 5A C0 83 EC 08 F2 0F 11 04 24 83 EC 04 89 14 24 90",
                "0x20813F": "83 C4 0C"
            }
        },
        "test-2": {
            "name": "Test 2",
            "description": "new accurate percentage",
            "address": {
                "[33 C9 F3 0F 10 00 F3 0F 5E 87 B4 03 00 ?? ?? ?? ?? 05]": "F3 0F 10 00 F3 0F 5E 87 B4 03 00 00 BA ref:0x2E65C0 F3 0F 59 02 0F 2F 02 76 04 F3 0F 10 02 8B 87 C0 03 00 00 8B B0 04 01 00 00 F3 0F 5A C0 83 EC 08 F2 0F 11 04 24 68 ref:0x2881B0",
                "0x20813F": "83 C4 0C",
                "0x2881B0": "25 2E 32 66 25 25 00"
            }
        },
        "test-3": {
            "name": "uwu :3",
            "description": "no paritcles",
            "address": {
                "libcocos2d.dll 0xB76C5": "31 F6",
                "libcocos2d.dll 0xB8ED6": "00"
            }
        }
    }
}
```
