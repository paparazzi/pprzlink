# PprzLink C++

The C++23 library loads PPRZLINK v2 message definitions from XML at runtime.
It provides codecs, reactive reception, serial/UDP transports, an optional
Ivy adapter and the `link++` ground agent.

## Documentation

The manuals are maintained centrally in [`doc/cpp`](../../../doc/cpp/index.rst):

- [Step-by-step usage guide](../../../doc/cpp/guide_d_utilisation.md), in French.
- [API and build reference](../../../doc/cpp/reference.md).
- [Standalone client examples](../../../doc/cpp/API_USAGE.md).
- [Architecture](../../../doc/cpp/architecture.md), in French.
- [Validation reports](../../../doc/cpp/validation/index.rst).
- [Historical plans and notes](../../../doc/cpp/historique/index.rst).

## Build

The minimum supported platform is Ubuntu 24.04 with GCC 13. Dependencies are
CMake 3.20+, TinyXML2 and Boost 1.83+. Ivy support also needs Ivy 3.18+ with
its native `ivy-cpp` wrapper and pkg-config. Unit conversions use an internal
table and require no additional unit library.

From this directory:

```sh
cmake -S . -B build -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/sdk
```

To build without Ivy and the ground agent, configure with
`-DPPRZLINK_WITH_IVY=OFF -DPPRZLINK_BUILD_LINK=OFF`.

The [full reference](../../../doc/cpp/reference.md#build) covers SDK components,
private dependency prefixes and standalone consumers. The
[XBee guide](../../../doc/cpp/guide_d_utilisation.md#utiliser-link-avec-une-radio-xbee)
explains paired RTS/CTS and per-message XML retry policies in `link++`.

## Generated API reference

Header/source comments and [`docs/mainpage.dox`](docs/mainpage.dox) remain with
the implementation. Enable `PPRZLINK_BUILD_DOCS=ON` and build the
`pprzlink_docs` target with Doxygen 1.9.8+ installed. The
[reference manual](../../../doc/cpp/reference.md#api-documentation) lists the
complete commands and generated HTML/XML locations.
