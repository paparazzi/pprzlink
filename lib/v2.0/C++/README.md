# PprzLink C++

The C++23 library loads PPRZLINK v2 message definitions from XML at runtime.
It provides codecs, reactive reception, serial/UDP transports, an optional
Ivy adapter and XBee support.

All receivers share `bind()`, `start()` and `stop()`. On `IvyLink`, `stop()`
pauses reactive bindings while the bus remains active; use `close()` to end
the session and return from `run()`. Ivy callbacks can also run on an
[application-owned Asio context](docs/ivy-asio.md).

## Documentation

The manuals are maintained centrally in [`doc/cpp`](../../../doc/cpp/index.rst):

- [Step-by-step usage guide](../../../doc/cpp/guide_d_utilisation.md), in French.
- [API and build reference](../../../doc/cpp/reference.md).
- [Standalone client examples](../../../doc/cpp/API_USAGE.md).
- [Architecture](../../../doc/cpp/architecture.md), in French.

## Build

For Ubuntu 26.04, use the native GCC 15 toolchain, Boost 1.90 and TinyXML2 11.
The source requirements remain C++23 with GCC 13+, CMake 3.20+, TinyXML2 and
Boost 1.83+. Ivy support also needs Ivy 3.18+ with its native `ivy-cpp` wrapper
and pkg-config. Use the matching system `libstdc++` at runtime: the installed
Ivy wrapper can require symbols absent from an older runtime. Unit conversions
use an internal table and require no additional unit library.

Value and codec headers share `<pprzlink/BytesBuffer.h>` without pulling in
`Device.h` or its Asio APIs. The `core` CMake component also contains reactive
reception and `MemoryDevice`, so it still requires Boost headers and threads.

After an OS or toolchain upgrade, configure a fresh build directory and rebuild
the library and its applications together. Existing CMake caches and binaries
can still refer to the previous compiler and dependency versions.

From this directory:

```sh
cmake -S . -B build-ubuntu26 -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON
cmake --build build-ubuntu26 -j4
ctest --test-dir build-ubuntu26 --output-on-failure
cmake --install build-ubuntu26 --prefix /path/to/sdk
```

Tests and examples are disabled by default. Enable them with
`-DBUILD_TESTING=ON -DPPRZLINK_BUILD_EXAMPLES=ON`.
To build without Ivy, use `-DPPRZLINK_WITH_IVY=OFF`.

The [full reference](../../../doc/cpp/reference.md#build) covers SDK components,
private dependency prefixes and standalone consumers.

## Generated API reference

Header/source comments and [`docs/mainpage.dox`](docs/mainpage.dox) remain with
the implementation. Enable `PPRZLINK_BUILD_DOCS=ON` and build the
`pprzlink_docs` target with Doxygen 1.9.8+ installed. The
[reference manual](../../../doc/cpp/reference.md#api-documentation) lists the
complete commands and generated HTML/XML locations.
