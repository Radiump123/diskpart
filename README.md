# diskpart

This repository contains the ReactOS `diskpart` sources plus a Linux compatibility build.

## Build and run on Linux

```bash
cmake -S . -B build
cmake --build build
./build/diskpart
```

> Note: `diskpart` alone will not work until you install it to a directory in your `PATH`.

## Optional: install command globally (user-local)

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

Then ensure `~/.local/bin` is in your `PATH`, and run:

```bash
diskpart
```

The Linux compatibility CLI currently supports:

- `help`
- `list disk`
- `list volume`
- `exit`
- script mode via `-s <script>`
