# diskpart

This repository contains the ReactOS `diskpart` sources plus a Linux compatibility build.

## Build on Linux

```bash
cmake -S . -B build
cmake --build build
```

The Linux build compiles a compatibility CLI that supports:

- `help`
- `list disk`
- `list volume`
- `exit`
- script mode via `-s <script>`
