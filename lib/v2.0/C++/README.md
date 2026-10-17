# PprzLink C++

The C++23 library loads PPRZLINK v2 message definitions from XML at runtime.
It provides codecs, reactive reception, serial/UDP transports, an optional
Ivy adapter and the `link++` ground agent.

All receivers share `bind()`, `start()` and `stop()`. On `IvyLink`, `stop()`
pauses reactive bindings while the bus remains active; use `close()` to end
the session and return from `run()`. Both Ivy codecs preserve explicit
floating-point NaN/infinity values, including unavailable camera temperatures.
XBee callers can combine an explicit radio destination and frame ID through
`sendMessageToWithId()`; application retries remain the caller's responsibility.

## Documentation

The manuals are maintained centrally in [`doc/cpp`](../../../doc/cpp/index.rst):

- [Step-by-step usage guide](../../../doc/cpp/guide_d_utilisation.md), in French.
- [API and build reference](../../../doc/cpp/reference.md).
- [Standalone client examples](../../../doc/cpp/API_USAGE.md).
- [Architecture](../../../doc/cpp/architecture.md), in French.
- [Validation reports](../../../doc/cpp/validation/index.rst).
- [Historical plans and notes](../../../doc/cpp/historique/index.rst).

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
cmake -S . -B build-ubuntu26 -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ubuntu26 -j4
ctest --test-dir build-ubuntu26 --output-on-failure
cmake --install build-ubuntu26 --prefix /path/to/sdk
```

To build without Ivy and the ground agent, configure with
`-DPPRZLINK_WITH_IVY=OFF -DPPRZLINK_BUILD_LINK=OFF`.

The [full reference](../../../doc/cpp/reference.md#build) covers SDK components,
private dependency prefixes and standalone consumers. The
[XBee guide](../../../doc/cpp/guide_d_utilisation.md#utiliser-link-avec-une-radio-xbee)
explains paired RTS/CTS and the existing XML unicast/broadcast routing in `link++`.

## Generated API reference

Header/source comments and [`docs/mainpage.dox`](docs/mainpage.dox) remain with
the implementation. Enable `PPRZLINK_BUILD_DOCS=ON` and build the
`pprzlink_docs` target with Doxygen 1.9.8+ installed. The
[reference manual](../../../doc/cpp/reference.md#api-documentation) lists the
complete commands and generated HTML/XML locations.
