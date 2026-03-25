# Lunara on Ubuntu

## Dependencies

- `build-essential`
- `cmake`
- `ninja-build`
- `python3`

## Quick Install

```bash
chmod +x packaging/linux/install_ubuntu.sh
./packaging/linux/install_ubuntu.sh
```

## Manual Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/lunara ./examples/hello.lunara
```

## Notes

- The runtime already has Windows and POSIX socket branches, so the same web backend stack is intended to run on Ubuntu.
- `lunara_embed` also builds on Linux and can be loaded through the existing C API bridge.
