# PprzLink C++

The C++ library requires C++23, TinyXML2, Boost 1.83+ (Asio and Bimap) and CMake 3.20+.
Unit conversion uses an internal affine table and bounded SI-prefix resolution.
The default build also includes Ivy **3.18 or newer** with its native C++ wrapper
(`ivy-cpp`) and pkg-config. Set `PPRZLINK_WITH_IVY=OFF` to build without Ivy.
On Ubuntu 26.04, use the native GCC 15 toolchain with Boost 1.90 and TinyXML2 11.
The minimum compiler is GCC 13. Use an Ivy wrapper built for the selected
compiler and runtime.

The serial device is part of both aggregate library builds and the `io`
component. It uses `boost::asio::io_context`.
Boost.System is header-only with the supported Boost versions.

Start with the [public API examples](API_USAGE.md)
for Ivy reception, an aircraft serial peer, or a UDP recorder. The examples can
also be built as an independent project against the installed SDK.
The [architecture guide (in French)](architecture.md) explains transport
abstractions, XML message definitions, and complete-message reception and sending.

## Class roles

Definitions describe what can be sent; values describe one particular message.
For example, `MessageField("altitude", "float")` describes a field, while its
`FieldValue` can hold `123.5f` in one message and `98.0f` in another.

| Class or file | Responsibility |
| --- | --- |
| [`FieldType`](../../lib/v2.0/C++/pprzlink/MessageFieldTypes.h) | XML scalar/array type, such as `float`, `int16[]` or `char[5]`. |
| [`MessageField`](../../lib/v2.0/C++/pprzlink/MessageField.h) | Field name, XML type/unit metadata and prepared explicit SI conversion, without a value. |
| [`UnitAliases.cpp`](../../lib/v2.0/C++/pprzlink/UnitAliases.cpp) | Editable XML-to-SI slopes/offsets, including scoped rules and explicitly unsupported units. |
| [`MessageDefinition`](../../lib/v2.0/C++/pprzlink/MessageDefinition.h) | Message name, class/message IDs and ordered field definitions. |
| [`MessageDictionary`](../../lib/v2.0/C++/pprzlink/MessageDictionary.h) | Load definitions with TinyXML2 and look them up by name or IDs. |
| [`FieldValue`](../../lib/v2.0/C++/pprzlink/FieldValue.h) | Field definition and actual scalar/array value in a `std::variant`. |
| [`BytesBuffer`](../../lib/v2.0/C++/pprzlink/BytesBuffer.h) | Owned byte storage shared by values, codecs and devices, without an Asio dependency. |
| [`MemoryDevice`](../../lib/v2.0/C++/pprzlink/MemoryDevice.h) | Asynchronous in-memory loopback and byte injection on a caller-owned Asio context. |
| [`Message`](../../lib/v2.0/C++/pprzlink/Message.h) | A copy of a definition, populated field values and sender/receiver addressing. |
| [`IvyLink`](../../lib/v2.0/C++/pprzlink/IvyLink.h) | Own an Ivy bus and manage subscriptions, sends and requests. |
| [`PprzTransport`](../../lib/v2.0/C++/pprzlink/PprzTransport.h) | Implement `Transport` with binary PprzLink v2 framing over an owned `Device`. |
| [`XbeeTransport`](../../lib/v2.0/C++/pprzlink/XbeeTransport.h) | Carry PprzLink v2 messages in XBee 802.15.4 API frames (AP=1), with radio addressing, status events and optional transmit validation. |
| [`XbeeModem`](../../lib/v2.0/C++/pprzlink/XbeeModem.h) | Initialize the modem through AT commands, with guard times, checked replies and response deadlines. |
| [`BoostSerialPortDevice`](../../lib/v2.0/C++/pprzlink/BoostSerialPortDevice.h) | Implement the `Device` byte-stream interface with Boost.Asio serial I/O. |
| [`PprzFrameDecoder`](../../lib/v2.0/C++/pprzlink/PprzFrameCodec.h) | Decode PPRZ frames without owning a device or performing I/O. |
| [`UdpTransport`](../../lib/v2.0/C++/pprzlink/UdpTransport.h) | Send PPRZ datagrams and return messages with their source IP/port. |
| [`ReceivedMessage`](../../lib/v2.0/C++/pprzlink/ReceivedMessage.h) | Own a message, its complete frame size and optional XBee/UDP metadata. |
| [`BinaryCodec`](../../lib/v2.0/C++/pprzlink/BinaryCodec.h), [`IvyMessageCodec`](../../lib/v2.0/C++/pprzlink/IvyMessageCodec.h), [`TextCodec`](../../lib/v2.0/C++/pprzlink/TextCodec.h) | Convert messages/fields to and from bytes or text; these functions perform no device or bus I/O. |

The usual path is XML → dictionary → definition → populated message → Ivy link
or binary transport. Reception reconstructs a message using the same definitions.
The dictionary must outlive an Ivy link or transport that borrows it; a Message
owns its own definition and values.

## API documentation

The library headers document schemas, checked values, parameters, return values,
exceptions, ownership and threading. Source files also describe framing recovery,
numeric/text parsing, serial cancellation and modem initialization algorithms.

From the Paparazzi root, generate the HTML and XML reference with Doxygen 1.9.8+:

```sh
cmake -S sw/ext/pprzlink/lib/v2.0/C++ -B var/build/pprzlink-docs-ubuntu26 \
  -DCMAKE_CXX_COMPILER=g++ -DPPRZLINK_BUILD_DOCS=ON \
  -DPPRZLINK_WITH_IVY=OFF \
  -DPPRZLINK_BUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF
cmake --build var/build/pprzlink-docs-ubuntu26 --target pprzlink_docs
```

Open `var/build/pprzlink-docs-ubuntu26/docs/html/index.html`. XML is generated under
`docs/xml` in that build directory. All library `.h`/`.cpp` files are covered,
including internal helpers and the optional Ivy adapter; tests and applications
are outside this API reference. Documentation warnings fail the target and are
recorded in `docs/warnings.log`. The same target can be enabled in an existing
CMake build with `-DPPRZLINK_BUILD_DOCS=ON`.

## Build

The conversion table is compiled directly into the library. No additional unit
package or submodule initialization is required.

Run the following commands from the C++ source directory, `lib/v2.0/C++`:

```sh
# With ivy-c / ivy-c-dev installed, the system pkg-config paths are sufficient.
pkg-config --modversion ivy-cpp ivy-c

cmake -S . -B build-ubuntu26 -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DPPRZLINK_BUILD_EXAMPLES=ON
cmake --build build-ubuntu26 -j
ctest --test-dir build-ubuntu26 --output-on-failure
cmake --install build-ubuntu26 --prefix /path/to/install
```

After an OS or compiler upgrade, use a fresh build directory, as above, and
rebuild the library and all its consumers. Do not reuse a CMake cache pointing
to the previous compiler, Boost or TinyXML2 installation. On Ubuntu 26.04,
the base development packages are installed with:

```sh
sudo apt install g++ cmake pkg-config libtinyxml2-dev libboost-dev
```

If Ivy is installed in a private prefix, configure its paths before building
and running the examples:

```sh
export IVY_PREFIX=/path/to/ivy-3.18.3
export PKG_CONFIG_PATH="$IVY_PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$IVY_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

The build rejects Ivy versions below 3.18. Ensure pkg-config and the runtime
loader select the same installation. The installed Ivy C++ wrapper may depend
on a recent `libstdc++`; keep the system runtime supplied with Ubuntu 26.04
when building and running these programs. The source minimum GCC 13 does not
guarantee that a prebuilt Ivy wrapper works with an older C++ runtime. Rebuild
the wrapper with the selected toolchain when changing its compiler or runtime.
Instructions for building the Ivy packages
are in [Ivy's Debian README](https://github.com/alex31/libivy-c/blob/b0bf831702c5bfd1313e83ded62eb14d17198534/debian/README).
The [validation reports](validation/index.rst) record tested environments
and limitations.

To check source compatibility with the minimum compiler explicitly, use a fresh
build directory and an Ivy wrapper compatible with that toolchain/runtime:

```sh
cmake -S . -B build-gcc13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-gcc13 -j4
ctest --test-dir build-gcc13 --output-on-failure
```

`make libpprzlink++` and `make install DESTDIR=/path/to/install` are also supported.
Use the same `PKG_CONFIG_PATH`; `CXX`, `CPPFLAGS`, `CXXFLAGS`, `LDFLAGS`, and
`OBJ_DIR` may be supplied to make. Both shared and static libraries are built.

## Borrowed text and array views

For populated fields, `getField<T>()` can borrow the message's existing storage
without copying the text bytes or array elements. Include `<string_view>` for
text views and `<span>` for array views:

```cpp
const auto label = samples.getField<std::string_view>("label");
const auto axes = samples.getField<std::span<const float>>("axes");
```

`std::string_view` accepts XML `string`, `char[]` and `char[N]` fields. It keeps
the complete byte length, including embedded NULs, and needs no terminator.
`std::span<const T>` accepts array fields with the exact stored element type
and returns their stored length. These getters support dynamic-extent spans.
An incompatible field type throws `field_type_mismatch`.
Named/indexed reads and output-parameter overloads support both view types.

Views remain valid until the field is replaced or the message is assigned or
destroyed; do not obtain a view from a temporary message. The span elements
must be `const`: a `const std::span<T>` still allows element mutation, and
mutable spans are not accepted by these getters. Use `std::string`,
`std::vector<T>` or `std::array<T, N>` getters when an independent copy is needed.

## Explicit SI field access

`getField`/`setField` use XML units with type and range checks.
`getFieldAs<T>` changes only the numeric representation. To request unit
conversion explicitly, use `getFieldSI`/`setFieldSI`:

```cpp
pprzlink::Message gps(dictionary.getDefinition("GPS"));
gps.setFieldSI("alt", 123.456); // SI metres -> XML int32 millimetres.
double metres = gps.getFieldSI("alt");
int32_t millimetres = gps.getField<int32_t>("alt"); // 123456
const auto& field = gps.getFieldDefinition("alt");
// field.getUnit(), getAltUnit(), getAltUnitCoef(), getSIUnit(), canConvertSI()
int32_t raw = field.fromSI<int32_t>(1.23456); // 1235 mm, nearest integer.
double si = field.toSI(raw);                 // 1.235 m.
```

SI numbers are doubles: metres, seconds, radians (also for latitude/longitude),
kelvins for absolute temperatures, and dimensionless ratios for percentages.
Integer destinations always round to nearest, with ties away from zero. Range
overflow throws without saturation or replacement. Numeric homogeneous arrays
use `std::span<const double>` inputs. `getFieldArraySI()` returns an owned
`std::vector<double>` with the stored array's length, by field name or XML index.
`getFieldSI()` returns a scalar `double`; its output parameters
also accept `double&` and `std::vector<double>&`.
NaN/non-finite floating measurements retain their numeric meaning; integer
writes reject them. A double cannot preserve all 64-bit integer values.

Original unit strings and explicitly supplied coefficients are preserved.
An explicit `alt_unit_coef` plus a supported `alt_unit` defines raw scaling and
has priority over the alias scale, even when the XML coefficient is rounded.
This also supports fixed-point fields without `unit`. Incomplete metadata,
calibration codes, logarithmic levels and mixed-frame arrays remain readable
through raw access; requesting SI conversion throws `field_unit_error`.
Conversion never changes coordinate frames, altitude references or time epochs.

Edit [UnitAliases.cpp](../../lib/v2.0/C++/pprzlink/UnitAliases.cpp) to add an explicit rule
`{"XML spelling", "canonical SI symbol", multiplier, offset}`. Conversion is
`SI = XML * multiplier + offset`, with inverse `(SI - offset) / multiplier`.
Optional message/field selectors disambiguate legacy spellings. An empty SI
target recognizes an opaque protocol label but does not permit SI access.

Exact rules precede one common SI prefix on registered prefix-eligible bases.
Areas/volumes apply the prefix factor squared/cubed; arbitrary algebraic
expressions and fixed-point encodings are not inferred. Unknown `unit` or
`alt_unit` declarations throw `bad_message_file` at XML loading, including on
string fields and unused alternative units. The error names the message,
field, spelling and table to update. Known opaque labels remain available
through native reads. Incompatible physical dimensions also fail at loading.
The explicit XML coefficient multiplies the alternative rule's slope once;
it does not scale the offset. See [the validation and size report](validation/VALIDATION_AFFINE_UNITS.md).

`link++` forwards XML values; its routing IDs and byte/message counters
are not SI quantities.
See [the usage guide](guide_d_utilisation.md#convertir-explicitement-les-unités)
for executable examples and the [API notes](API_USAGE.md#champs--unités-si-explicites)
for the complete contract.

Installed CMake consumers can use `find_package(pprzlink++ CONFIG REQUIRED)` and
link `pprzlink++` or `pprzlink++_static`; dependencies and C++23 propagate.

New consumers can select `COMPONENTS core`, `io` or `ivy` and link
`pprzlink::core`, `pprzlink::io` or `pprzlink::ivy`. The component libraries are
static/PIC. Selecting `core` or `io` does not look for Ivy, even with a full SDK.
Without `COMPONENTS`, `find_package` imports the aggregate targets.
The Makefile also supports `WITH_IVY=0`; see [API_USAGE.md](API_USAGE.md) for the
complete build and installed-consumer examples.

## Ground agent: link++

`link++` is a separate Unix application. It accepts
all 24 options of Paparazzi's OCaml `link`, plus its `-help`/`--help` switches.
Defaults match OCaml, including **9600 baud**, uplink enabled, UDP ports
4242/4243, aircraft timeout 5000 ms, PING period 5000 ms and report period 1000 ms.

```sh
export PAPARAZZI_HOME=/path/to/paparazzi
./build-ubuntu26/apps/link/link++ -d /dev/ttyUSB0 -s 57600 -transport pprz
./build-ubuntu26/apps/link/link++ -udp -udp_port 4242 -udp_uplink_port 4243
./build-ubuntu26/apps/link/link++ -d /dev/ttyUSB0 -s 57600 -transport xbee -hfc
./build-ubuntu26/apps/link/link++ -help
```

The message file is `$PPRZLINK_DIR/messages.xml`, otherwise
`$PAPARAZZI_HOME/var/messages.xml`, otherwise `/usr/share/pprzlink/messages.xml`.
`IVY_BUS` supplies the default bus; `-b` overrides it.

The agent bridges telemetry to Ivy, routes XML `forwarded`/`broadcasted` commands,
tracks known aircraft, sends PING, receives PONG and publishes LINK_REPORT. It
supports serial PPRZ/XBee, file/FIFO descriptors, UDP with per-aircraft peers,
traffic output, local timestamps and redundant-link telemetry encapsulation.

The agent links the static `pprzlink::io` and `pprzlink::ivy` components. A copied
`link++` executable therefore runs independently of the pprzlink build directory;
it still needs the system Ivy and TinyXML2 shared libraries and the C++ runtime.
From the Paparazzi root, build and copy it with a fresh Ubuntu 26.04 directory:

```sh
make -C sw/ground_segment/tmtc -f link_cpp.mk PAPARAZZI_SRC="$PWD" \
  PPRZLINK_CPP_BUILD="$PWD/var/build/pprzlink-cpp-ubuntu26"
```

The resulting executable is `sw/ground_segment/tmtc/link++`.
The make target uses the system `g++` by default; `PPRZLINK_CXX` overrides it.

`LINK_REPORT` round-trip time uses a monotonic clock and starts at `9999 ms`
until the first PONG associated with a pending PING. Reports retain the last
completed measurement while waiting for subsequent replies; unsolicited PONGs
do not change it. Only a PING sent to a live aircraft starts or replaces a
pending measurement. PING/PONG carry no sequence number, so a delayed PONG can
be associated with a newer PING.

On Linux, `-socat start` creates two connected virtual serial ports using the
`socat` executable on PATH. It prints **Port A** and **Port B**, then returns to
the shell. Both ports are bidirectional and interchangeable: each program opens
one of them. The socat process runs in the background until `-socat stop`:

```sh
./build-ubuntu26/apps/link/link++ -socat start
./build-ubuntu26/apps/link/link++ -d /dev/pts/N -transport pprz -s 57600
./build-ubuntu26/apps/link/link++ -socat stop
```

Replace `/dev/pts/N` with either printed port. These management commands do
not need a messages XML or an Ivy bus. There is one managed pair per user;
repeating `start` prints the existing ports, and repeating `stop` is harmless.
`stop` only terminates the process recorded by this helper and removes its PTYs.
Virtual ports exchange raw bytes in both directions without emulating baud-rate
timing. State is kept under `$XDG_RUNTIME_DIR/linkpp-socat`, or
`/tmp/linkpp-socat-<uid>` when `XDG_RUNTIME_DIR` is unset.

Unlike the diagnostic example, the compatible XBee launch uses a fixed baudrate
and does **not** change or save the modem's baudrate. AT replies and deadlines
are still checked. Host retries preserve the frame ID; `-xbee_retries` sets the
maximum total attempts, including the first send. Pending IDs are not reused;
a missing status expires after five seconds without blindly resending a command.
If all 255 IDs are pending, a new send is skipped with a diagnostic on stderr.
The agent keeps running and the existing expiry/retry timer stays armed, even
when further commands arrive. Skipped sends are not queued; subsequent sends
can succeed once a status or timeout frees an ID. A skipped PING does not
start or replace an RTT measurement, and the periodic PING timer continues.

The agent follows the existing XML `link` attribute. `forwarded` commands
use unicast to the aircraft identified by `ac_id`; `broadcasted` commands use
radio destination `0xffff` and PPRZLINK receiver 255. The application fields
remain unchanged: targeted command handlers in the autopilot filter on `ac_id`.
Broadcasts request no MAC ACK, even with the normal `0x00` transmit options.

In UDP mode, a broadcast command normally sends one datagram to each live
aircraft's peer. With `-udp_broadcast`, it sends a single datagram to
`-udp_broadcast_addr` and `-udp_uplink_port`, provided at least one aircraft is
live. Learning more aircraft does not multiply that network broadcast; with
no live aircraft, no datagram is sent.

Host resends apply only to unicast transmissions and are bounded by
`-xbee_retries` (default 10 total attempts). Broadcast failures release their
pending ID without scheduling a resend, regardless of this CLI limit.
Transmit-status tracking remains enabled for both destinations.
`-xbee_retries 1` suppresses all host resends. Some 802.15.4 firmware can repeat
broadcasts according to its global `RR` setting; the agent does not change it.
See Digi's [RR documentation](https://docs.digi.com/resources/documentation/digidocs/90001500/reference/r_cmd_rr.htm).

`-hfc` enables paired RTS/CTS on the host and XBee modem (`D6=1`, `D7=1`).
Omitting it disables both sides (`D6=0`, `D7=0`); the default is off.
The AT initialization verifies both settings before the ready message. In
PPRZ serial mode, `-hfc` configures the host UART only. The
[French guide](guide_d_utilisation.md#utiliser-link-avec-une-radio-xbee) includes
Paparazzi build/launch commands and the existing XML routing convention.

`-xbee_868` selects AP=1 TX 0x10, RX 0x90 and status 0x8b frames. This framing is
covered by fixtures and a simulated modem; physical 868 modem configuration and
delivery have not been validated. See the comparison report for the corrected
OCaml 868 transmit-type bug and other intentional differences.

Library facilities used by the agent:

- `MessageDefinition::getLinkMode()` and `MessageField::getFormat()` expose XML
  routing/display metadata.
- `serializeLegacyMessage()` honors XML display formats. The standard
  `serializeMessage()` uses compact, round-trippable numbers. `parseLegacyMessageBody()` accepts
  the legacy whitespace/quoted-field syntax while checking numeric ranges.
- Signed/unsigned 64-bit scalar and array fields are supported.
- `PprzFrameDecoder` and `encodePprzFrame()` provide framing without device I/O.
  `discardPendingInput()` keeps incomplete UDP datagrams from contaminating later ones.
- Transports expose `getStatistics()` and `getLastReceivedFrameSize()`;
  `PosixFileDevice` supplies the file/FIFO stream adapter.
- `XbeeTransport::Api::Series868` selects the extended framing;
  `sendMessageWithId()` lets an application track/retry a transmission explicitly.
  RX 0x90 has no RSSI; check `ReceiveInfo::hasRssi` before using that value.

## Complete example

[`IvyRoundTrip.cpp`](../../lib/v2.0/C++/examples/IvyRoundTrip.cpp) loads
[`messages.xml`](../../lib/v2.0/C++/examples/messages.xml), subscribes to `EXAMPLE_ALTITUDE`,
creates a message with altitude `123.5f`, sends it and reads the received value.
It runs two Ivy links in one process, using their owned event-loop threads.

The example is built by default and exercised by CTest. From the C++ source
directory (`lib/v2.0/C++`),
after configuring the dependencies as above:

```sh
cmake --build build-ubuntu26 --target ivy_round_trip
./build-ubuntu26/ivy_round_trip examples/messages.xml
```

Expected output:

```text
Sent: EXAMPLE_ALTITUDE [altitude=123.500000]
Received from 42: altitude = 123.5 m
```

By default it uses a local loopback bus with a process-dependent port. An optional
second argument selects an Ivy domain, for example `127.255.255.255:2010`.
The sender waits for the receiver's subscription before sending; a `std::promise`
transfers the result back to the main thread. Both waits time out on failure,
and the links stop/join their threads when leaving scope. Serial hardware is
not needed. Set `-DPPRZLINK_BUILD_EXAMPLES=OFF` to omit the example target.

## Select the serial transport: PPRZ or XBee API

Transport selection is a runtime application choice, independent of the message
XML. The same definitions and Message objects work with `PprzTransport` (the
usual `0x99` serial frames) or `XbeeTransport` (XBee `0x7e` API frames).
Both implement `Transport` and take exclusive ownership of a `Device` through
`std::unique_ptr<Device>`, including `BoostSerialPortDevice`. Configure the serial
port before transferring ownership:

```cpp
boost::asio::io_context context; // Must outlive the transport and its device.
auto serialDevice = std::make_unique<pprzlink::BoostSerialPortDevice>(context, "/dev/ttyUSB0");
serialDevice->setBaudrate(pprzlink::BoostSerialPortDevice::Baudrate(57600));
// Set any other serial options here, before transferring ownership.
pprzlink::XbeeTransport transport(std::move(serialDevice), dictionary);
pprzlink::XbeeConfiguration configuration;
configuration.hardwareFlowControl = true; // One choice for both the host UART and modem.
transport.startInitialization(configuration); // Autobaud -> 57600, MY=0x100, channel unchanged, AP=1
transport.setSanityChecksEnabled(true);
message.setReceiverId(42);
transport.onReady([&] {
    transport.sendMessage(message); // XBee destination 42, after initialization.
    transport.sendMessageTo(message, pprzlink::XbeeAddress::ieee(0x0013a200405291ab));
});
transport.start();
context.run();
```

Use a named radio destination with the unified `sendMessageTo()` API:

```cpp
using pprzlink::XbeeAddress;
const auto receiver = XbeeAddress::network(0x0100); // Short MY network address.
const auto radio = XbeeAddress::ieee(0x0013a200405291ab); // IEEE SH/SL address.
transport.sendMessageTo(message, receiver);
transport.sendMessageTo(message, radio);
```

The address kind is selected explicitly by the factory, never inferred from
the numeric magnitude. `ieee(0x0100)` still uses IEEE addressing. Network values
must fit in sixteen bits; IEEE values must fit in sixty-four bits. Negative or
oversized values throw `std::out_of_range` before narrowing. Integer literals
need no casts or suffixes; floating-point and bool inputs are rejected.
Network addresses require legacy 802.15.4 mode; Series868 requires `ieee()`.
The standalone header is `<pprzlink/XbeeAddress.h>`; `XbeeTransport.h` includes it.
The older `sendMessageTo16()` and `sendMessageTo64()` remain compatibility wrappers.

To choose both the radio destination and the transmit-status ID, use
`sendMessageToWithId()`:

```cpp
transport.sendMessageToWithId(message, radio, 42);
transport.sendMessageToWithId(message, radio, 0, pprzlink::RadioRetries::Disabled);
```

The first call uses normal radio ACK/retries and requests status ID 42; the
second disables status responses and radio ACK/retries independently. The
PPRZLINK receiver stays unchanged. The caller must allocate IDs without reusing
one still awaiting a status. `sendMessageWithId()` remains available when the
radio destination should be derived from the PPRZLINK receiver.

Choose the radio ACK/retry behavior per transmission with the scoped enum
`XbeeTransport::RadioRetries`, in the last argument position:

```cpp
using RadioRetries = pprzlink::XbeeTransport::RadioRetries;
const auto receiver = pprzlink::XbeeAddress::network(0x0100);
transport.sendMessageTo(setting, receiver); // Default: Enabled.
transport.sendMessageTo(setting, receiver, RadioRetries::Enabled);
transport.sendMessageTo(position, receiver, RadioRetries::Disabled);
```

`Enabled` preserves the existing wire option `0x00`; `Disabled` sets the
per-frame Disable ACK bit (`0x01`). This selects modem behavior, not a host-side
resend queue. On legacy 802.15.4 it controls MAC ACK/retries; it does not rewrite
the modem's MM/RR configuration. The 868 `0x10` frame uses the same Disable ACK
bit. Other firmware retry layers and global modem settings still apply.
See Digi's [TX16 format](https://docs.digi.com/resources/documentation/digidocs/90001500/reference/r_frame_0x01.htm)
and [868 format, page 54](https://ftp1.digi.com/support/documentation/90001020_F.pdf).

`sendMessageWithId()`, `sendMessageTo()`, `sendMessageToWithId()` and the
compatibility wrappers accept the optional last argument with default
`RadioRetries::Enabled`.
`sendMessage(message, radioRetries)` is an overload: the one-argument override
also selects `Enabled`, including calls through `Transport&`.
The shared enum is declared in `<pprzlink/RadioRetries.h>`;
`XbeeTransport::RadioRetries` remains an alias. `MessageDefinition` carries
routing metadata through `getLinkMode()`; applications choose the destination.
A broadcast address does not request a MAC ACK even with `Enabled`.
An explicit `Disabled` choice can also send unicast without a MAC ACK.

Disabling retries on one send does not affect subsequent sends. Invalid enum
values throw before writing any bytes. The frame ID and transmit-status callback
remain independent: ID zero suppresses the status, not the radio retries.
Without radio ACKs, a successful transmit status does not prove remote reception.

The default radio destination is the PPRZLINK receiver ID. Receiver 255 means
broadcast and maps to radio address `0xffff`. The RF data consists of the four
PPRZLINK v2 header bytes followed by the XML fields, **without an additional
PPRZ serial envelope**. This matches the non-868 OCaml XBee transport.
The RF payload limit is 100 bytes including that four-byte header; larger
messages are rejected before writing to the device. Messages are not fragmented.

Default radio format: **XBee 802.15.4 legacy API, AP=1 (without escaping)**,
TX16 `0x01`, TX64 `0x00`, RX16 `0x81`, RX64 `0x80`. A frequency of 2.4 GHz alone
is not sufficient to identify compatible firmware. The separate `Api::Series868`
mode supplies 0x10/0x90/0x8b framing; it does not implement Zigbee/DigiMesh network
management. AP=2 escaped mode is not implemented. On firmware with an `AO`
setting, select legacy RX16/RX64 output as specified by that modem's manual.
The PAN must already match the intended setup. Initialization assumes the default
command escape character `+` and a modem supporting UART command mode. Adjust the
guard time if the modem's `GT` value was customized.

`startInitialization(configuration)` prepares the AT sequence. `start()` activates
reception, and input callbacks plus exact guard/reply deadlines advance it on Asio.
`onReady()` observes successful completion. No periodic receive polling is needed.
**Autobaud is enabled by default, with a target of 57600 baud.** Set
`XbeeConfiguration::targetBaudrate` to another standard rate if needed.
The initializer tries the target first, then 9600, 57600, 115200, 38400, 19200,
4800, 2400 and 1200, without duplicates. Each attempt waits for the pre-escape
guard, sends `+++` without a carriage return, respects the post-escape guard,
and waits for `OK`. A missing entry reply advances to the next rate.
Once the modem answers, `ATBD` must confirm the detected rate.

If a change is necessary, the initializer sets `ATBD`, waits for `ATCN`'s
`OK` at the old rate, then switches the host UART to the target. It re-enters
command mode and queries `ATBD` again to verify the change before issuing
`ATWR` and checking its `OK`. The next startup tries the saved target first.
There is no flash write when the modem already uses the requested rate.
**ATWR stores all current modem parameters**, so it is deliberately issued
before this initializer changes MY, CH, AP, D6 or D7; pre-existing volatile settings
are still included in that save. The initializer then sends `ATMY`, optional
`ATCH`, `ATAP1`, paired D6/D7 settings and the final `ATCN`, checking each `OK`. Those MY/CH/AP/D6/D7
changes remain volatile. The PAN and API output format (`AO`) are not changed.

Baud detection requires a `SerialDevice`, implemented by `BoostSerialPortDevice`.
Its `resetBaudrate()` changes the host UART and discards buffered input and
completions from the previous rate. Stale kernel input is drained during the
guard before `+++`; no flush is performed after AT commands.
Set `targetBaudrate = std::nullopt` for fixed-baud initialization with a
`SerialDevice`: the host and modem baud rates must then already match, and no BD query or flash
write is performed. Automatic targets are limited to the eight standard rates
listed above, common to legacy S1/S2C firmware. Rounded custom BD replies near
a probed rate are accepted for discovery, but arbitrary custom rates are not scanned.

`XbeeConfiguration::hardwareFlowControl` is the single RTS/CTS setting for both
the host UART and the modem. Its default `false` explicitly configures **00**:
no host flow control, `D6=0`, `D7=0`. Set it to `true` for **11**: host RTS/CTS,
`D6=1`, `D7=1`. Independent host/modem choices are not exposed.
`BoostSerialPortDevice` uses Asio's native serial option (`CRTSCTS` on Linux).
Initialization temporarily disables host flow control so an unconfigured CTS
cannot block AT writes. It checks both `ATD6` and `ATD7` readbacks, waits for the
final `ATCN` reply, applies and reads back the requested host setting, and only
then marks the modem ready. No binary message can be sent before those checks.
After initialization first claims the UART, both `setFlowcontrol()` and
`setHardwareFlowControl()` reject independent changes with `std::logic_error`.
To switch either way, call `startInitialization()` again with the shared option.

Two physical endpoints cannot be changed atomically: a failure after a radio
command can leave partially applied hardware settings. Such a failure never
publishes readiness or enables binary sends; correct the cause and reinitialize.
RTS/CTS wiring is required for **11**. Baud rate and flow control are separate:
this setting does not increase UART throughput and synchronous writes can wait
while the modem deasserts CTS. No additional `ATWR` saves the new D6/D7 settings.
A later initialization that changes the baud rate can save the settings left
by an earlier session, because `ATWR` stores all parameters current at that time.
The legacy already-configured constructor mode skips all modem checks; callers
using that mode are responsible for an already consistent hardware configuration.

Read the local UART's instantaneous CTS input through `SerialDevice::getCTS()`:

```cpp
const auto &serial = dynamic_cast<const pprzlink::SerialDevice &>(transport.getDevice());
const bool clearToSend = serial.getCTS(); // true: asserted; false: deasserted.
```

The name follows [`wjwwood/serial`](https://github.com/wjwwood/serial/blob/main/include/serial/serial.h).
`BoostSerialPortDevice` implements it using `ioctl(TIOCMGET)` and `TIOCM_CTS` on
POSIX systems supporting those constants. It reads the logical signal even when
flow control is disabled, without changing settings or waiting for assertion.
Calls are serialized with other port I/O. An unsupported query or native error
throws `boost::system::system_error`, rather than pretending CTS is deasserted;
Linux PTYs normally report `ENOTTY`. Other `SerialDevice` adapters inherit an
optional default that throws `operation_not_supported` until they implement it.
`getHardwareFlowControl()` still reports configuration, and `isReady()` still
reports modem initialization. A CTS sample cannot guarantee the next synchronous
write will not block: the signal can change and host buffers may be full.

The default guard time is two seconds before and after `+++`; the reply timeout
is two seconds per AT command (for entry, after the post-escape guard).
Timings can be set in `XbeeConfiguration`, along with `localAddress` and optional
`channel` (0x0c..0x17). After successful automatic initialization,
`getBaudrateInfo()` reports the detected and configured rates and whether the
change was saved to flash.

While initialization is active or failed, all message sends throw before any
binary frame is written. `ERROR`, unexpected AT replies and response timeouts
include the failed operation in the exception text. Restart explicitly with
`startInitialization()` after correcting the cause. Previously applied settings
are not rolled back. The caller must provide exclusive access to the device
throughout initialization; no other serial writer or transport may interfere.
The response deadlines do not limit a blocking `Device::writeBuffer()` call.

For an already configured modem, constructing `XbeeTransport` without calling
`startInitialization()` keeps the original direct API operation. Initialization
clears pending transport data; start it before exchanging messages. API bytes
following the final `OK` in the same read are preserved for normal reception.

`sendMessage()` returns the number of serial bytes written, not proof of RF
delivery. Frame IDs cycle through 1..255 (`getLastFrameId()`); applications must
limit outstanding transmissions so IDs are not reused while awaiting replies.
`setStatusCallback()` receives a `RadioStatus` variant containing `TransmitStatus`
(`0x89`), `ModemStatus` (`0x8a`) or `AtCommandResponse` (`0x88`). The callback runs
on the receive executor as status frames arrive, including when no message
subscription matches. Run the supplied event loop to receive these statuses. A missing callback
discards these events. Host-side retry scheduling, timeouts and pending-send
tracking belong to the application; the modem's normal acknowledgement/retry
mechanism remains enabled. A broadcast success status is not a delivery guarantee.

`getLastReceiveInfo()` exposes the source radio address, RSSI magnitude and option
bits of the last successfully decoded message. The PPRZLINK sender stays the
sender from the RF payload, which can differ from the radio address. Reception
handles fragmented/concatenated frames, discards bad checksums and impossible
lengths, and skips unhandled API indications. Checksum-valid malformed messages
are consumed before raising an exception, so the next frame remains accessible.

The `serial_messages` example exposes the choice with the same spelling as the
OCaml link: **`-transport pprz`** (default) or **`-transport xbee`**. XBee mode
initializes the modem automatically before sending or receiving messages.
No opt-in flag is needed for autobaud. `-s` sets the target (default **57600**),
and a changed rate is verified and saved to the modem's flash. The example
prints the detected/configured rates after successful initialization.
`-xbee-no-autobaud` disables discovery and baud changes while keeping AT setup;
`-s` must then match the modem. In PPRZ mode, `-s` only sets the host UART.
`-xbee_addr` sets MY (default `0x100`), `-ch` optionally sets the channel,
`-xbee-guard-ms` and `-xbee-timeout-ms` override the timing defaults.
`-xbee-no-init` skips AT initialization for a preconfigured AP=1 modem.
It receives and prints messages and optionally sends one message. It is a serial diagnostic
example; it does not start an Ivy bridge or replace the full OCaml `link` agent.
From the C++ source directory (`lib/v2.0/C++`) after the CMake build:

```sh
./build-ubuntu26/serial_messages -messages /path/to/messages.xml \
  -d /dev/ttyUSB0 -transport xbee -xbeesan

# With a dictionary containing PING, send to aircraft/radio 42 and listen for 5 s:
./build-ubuntu26/serial_messages -messages /path/to/messages.xml \
  -d /dev/ttyUSB0 -transport xbee \
  -send "PING" -sender 0 -receiver 42 -duration 5 -xbeesan

./build-ubuntu26/serial_messages --help
```

`-xbee-dest` and `-xbee-dest64` override the radio destination independently of
`-receiver`. Radio addresses accept decimal or `0x` hexadecimal notation.
Serial settings are 8N1 without flow control by default; `-hfc` selects RTS/CTS
on both the UART and modem in XBee mode, and on the UART alone in PPRZ mode.
`-hfc` is rejected with `-xbee-no-init` because that combination cannot configure
the modem half. Ctrl-C stops reception.

`-xbeesan` enables an additional check of each complete transmit frame immediately
before the serial write. It verifies the `0x7e` delimiter, declared versus actual
length, TX16/TX64 header/address size, reserved option bits, the checksum, the
100-byte RF limit and the PPRZLINK payload against the loaded XML dictionary.
Any failure blocks the whole frame, prints the reason on **stderr**, and exits
the example with a nonzero status. AT initialization failures follow the same
error-reporting path and prevent the binary send. Basic checks remain enabled
without this option. `-xbeesan` is rejected with `-transport pprz`, since that
mode has no XBee API envelope to validate.

C++ applications enable this with `setSanityChecksEnabled(true)` and handle the
exception in their own reporting layer. `validateTransmitFrame(bytes)` exposes
the same check for captured frames without performing I/O. These checks validate
the supported legacy API/PPRZLINK format, not RF delivery or every firmware's
configuration-dependent payload limit. In particular they do not query `NP`.

The XBee tests compare exact TX16/TX64 fixtures with the OCaml frame format and
exercise RX metadata, radio statuses, payload limits and recovery after invalid
frames. AT tests exercise fragmented replies, both guard intervals, errors/timeouts
at every step and the send gate. Baud tests cover all eight detected rates,
an explicit target override, verification before flash write, a simulated restart,
and failure at each negotiation/save step. Sanity tests reject malformed frames and a
message whose definition disagrees with the transport's XML dictionary before
any write. The CLI integration test emulates AT replies and uses a pseudo-terminal
for serial I/O, checking host baud settings during the default 9600-to-57600
transition and a 115200 override, successful initialization, failure diagnostics
on stderr and the absence of bytes for refused sends. The simulated modem retains
the saved rate between two process runs; this does not test physical flash or UART
timing. Serial-device tests also cover baud resets with pending Asio completions. It requires Python 3 when building
examples and tests on Unix. No physical XBee is required for these tests.

Protocol references: [OCaml transport](../../lib/common/ocaml/xbee_transport.ml),
[Digi legacy 802.15.4 manual](https://docs.digi.com/resources/documentation/digidocs/pdfs/90000982.pdf),
[Digi transmit status](https://docs.digi.com/resources/documentation/digidocs/90001500/reference/r_frame_0x89.htm),
[Digi BD command](https://docs.digi.com/resources/documentation/digidocs/90001500/reference/r_cmd_bd.htm).
The legacy manual specifies BD application after ATCN's response; its WR command
stores all current settings and requires waiting for the acknowledgement.

## Ivy integration

`IvyLink` uses `<Ivy/ivy.hpp>` and `libivy-cpp` directly.
Binding IDs are opaque and local to each link.

Use `subscribeMessage(definitionOrName, callback)`,
`subscribeSender(sender, callback)` and `subscribeRequestAnswerer(definition, callback)`.
They return Ivy's own move-only `ivy::Subscription`; retain it while subscribed,
then let destruction unsubscribe or call `unbind()` explicitly. The legacy
ID-based entry points delegate to the same implementation. Unbinding does not
wait for a callback already executing; its captured state must remain alive.

Message definitions and field types are loaded at runtime from XML. Reception
uses `bind_raw`, then builds a `pprzlink::Message` from those definitions.
`bind_convert` requires a fixed callback signature and is not used for this
dynamic interface. Numeric conversions check syntax and range; strings and
arrays keep the PprzLink wire format. Request replies preserve the request ID,
and a reply subscription is canceled before invoking its one-shot callback.

Reactive reception through `start()` ignores Ivy traffic whose apparent message
name is absent from the dictionary. A malformed body for a known message still
reports `ReceiveError::Kind::Decode` through `onError()`. Request/reply APIs keep
their own subscriptions and matching rules.

Every `IvyLink` owns an independent `ivy::Bus`. Several links can use the same
bus address or separate addresses in one process:

```cpp
pprzlink::IvyLink first(dictionary, "first", "127.255.255.255:2010", true);
pprzlink::IvyLink second(dictionary, "second", "127.255.255.255:2011", true);
auto aliveSubscription = first.subscribeMessage("ALIVE",
    [](std::string sender, pprzlink::Message message) {
        // Fields are decoded according to the XML definition.
    });
```

`threadedIvy=true` owns an `ivy::LoopThread`; destruction stops and joins it
before releasing subscriptions and the bus. Without an Asio context, the default
is `false`: call `link.run()` to run its native loop on the calling thread. `link.stop()`
pauses only reception through `Receiver::bind()`, preserving those bindings
for `link.start()`. Native/scoped subscriptions (`subscribeMessage()`, requests
and their legacy counterparts), sends and bus timers remain active while paused;
`stop()` does not return from `run()`.

Call `link.close()` to end the Ivy session and request return from `run()`.
Both `stop()` and `close()` may be called from a callback. Closing is terminal:
create a new `IvyLink` for another session. Destruction joins an owned loop thread.
`getBus()` borrows the native wrapper for its public APIs and context integration;
do not move/destroy the borrowed bus or run an additional loop on it.

Sends and binding changes can be called from multiple threads. Application
callback state shared with other threads needs synchronization. The dictionary
must outlive the link, and all external callers/event loops must finish before destruction.
Never destroy a link from its callback. Unbinding permits an already executing
callback to finish; Ivy keeps its captures alive until then.

Synchronous Ivy errors throw `std::system_error`. Ivy catches exceptions from
callbacks, records an error and stops that bus. `run()` checks this error on
return; in threaded mode inspect `getBus().take_callback_error()` when the loop
has stopped. A destructor does not report errors. For callbacks on an application
event loop, see [Asio integration](../../lib/v2.0/C++/docs/ivy-asio.md).

The integration test uses loopback UDP/TCP on process-specific ports. It checks
XML scalar/array/string conversions, sender routing, empty messages, concurrent
requests and sends, cancellation, independent bus shutdown and error reporting.

## Typed field values and codecs

`FieldValue` stores a `std::variant` of the supported scalar types and typed
`std::vector` arrays. The XML definition selects the stored type; construction
checks numeric ranges and fixed array lengths before storing a value. Named C++
concepts describe scalar, input-container and output-container overloads.

Getters support return values and output parameters:

```cpp
// For XML fields declared as float and int16[].
auto altitude = message.getField<float>("altitude");
auto convertedAltitude = message.getFieldAs<double>("altitude"); // Explicit checked conversion.
auto samples = message.getField<std::vector<int16_t>>("samples");
float altitudeOut;
message.getField("altitude", altitudeOut); // Output-parameter overload.

// Dynamic access: the XML determines which variant alternative is active.
const auto &storage = message.getField("altitude"); // const FieldValue::Storage&
const auto &firstField = message.getField(size_t{0}); // Also accessible by index.
auto sameValue = std::get<float>(storage);

// Metadata remains available through getRawValue().
const auto &field = message.getRawValue("altitude");
const auto &type = field.getType();
```

Without a template argument, `getField(name)` and `getField(index)` return a
read-only reference to the stored variant, suitable for `std::visit` or
`std::get`. The reference belongs to the message; use plain `auto` to obtain an
independent copy. A field missing from the definition throws `no_such_field`,
and a defined field without a value throws `field_has_no_value`, as with the
typed getters.

Typed getters require the exact stored scalar/element type. A mismatch throws
`field_type_mismatch` (derived from `std::bad_variant_access`) with the field and
type names; a `std::array` output must have exactly the stored
number of elements, otherwise it throws `std::length_error`.

`setField()` sets or replaces a field; `addField()` remains a compatible alias.
Both reject out-of-range numeric inputs, including array elements, with
`field_conversion_error` (derived from `std::out_of_range`). Fractional or
non-finite floating-point values cannot become integers. An invalid replacement
leaves the previous value unchanged. `getFieldAs<T>()` applies the same checks
for explicit numeric scalar reads. Floating-point destinations may round;
exact-type floating-point values, including NaNs, retain their binary semantics.
Numeric sender/receiver/component setters also check the byte range, so ordinary
integer literals work without silently wrapping; binary encoding still enforces
the four-bit component limit. String Ivy sender names remain supported.

`<pprzlink/TextCodec.h>` provides `writeIvyField(stream, field)` for the Ivy wire
format and `writeDebugField(stream, field)` for diagnostics. Formatting visits
stored values directly, without copying arrays or changing the field. Both use
numeric int8/uint8 output and preserve the caller's stream flags. `operator<<`
uses diagnostic formatting, including braces around non-character arrays;
`Message::toString()` formats diagnostic output.

Ivy numeric fields use `std::to_chars` on the stored type. Finite `float` and
`double` values use their shortest round-trippable representation, so an Ivy
altitude is sent as `123.6` instead of `123.600000`, and `125` instead of
`125.000000`. Scientific notation is used when shorter; subnormal values and
negative zero retain their original bits when parsed back into the same type.
Formatting is independent of locale and stream precision. Numbers are written
from a stack buffer directly to the message stream, without a temporary stream
or string per field. Parsing borrows numeric tokens unless underscore
normalization is needed. `serializeLegacyMessage()` retains the separate
OCaml/XML formatting contract, including explicitly requested decimal precision.

Floating scalars and array elements also accept `nan`, `inf`, their uppercase
forms `NAN`, `INF`, and an optional `+` or `-` sign. Both the standard and legacy
Ivy codecs preserve the meaning of NaN and the sign of infinity. Text does not
preserve a NaN's binary payload. Numeric overflow/underflow and malformed tokens
still throw `wrong_message_format`; non-finite values are not accepted for
integer fields.

`<pprzlink/BytesBuffer.h>` declares `BytesBuffer` as `std::vector<uint8_t>`
using only standard headers. Value and codec headers use it without importing
`Device.h` or Asio. `Device.h` also includes it.
The CMake `core` target depends on Boost for its receiver and
transport interfaces.

`<pprzlink/BinaryCodec.h>` contains little-endian encoding/decoding using
`std::bit_cast` and bounded `std::span` input. Floating-point bit patterns and
full-width integer arrays are preserved. The `addToBuffer`,
`addFieldToBuffer` and `addFieldFromBuffer` entry points delegate to this codec.
Truncated field reads throw `std::out_of_range` without advancing their offset.
Dynamic array/string counts must fit in one byte; arrays of strings remain
unsupported by the binary format.

Access raw values with typed getters, `std::get` or `std::visit`. Construct
fields with their definition and value. Use `writeIvyField` for Ivy field text.

The serialization tests check exact wire bytes for every scalar and array base
type, floating-point bits, truncated input, length limits, typed getters,
formatting and binary transport round trips. The field-value tests cover
concepts, supported containers and numeric conversions.

## Transport, XML and serial-device contracts

Reception uses receiver-owned `bind()` subscriptions on UDP, serial PPRZ,
XBee and Ivy. `start()` enables reception; binary channels execute callbacks
on the supplied Asio context. Ivy uses its native loop or an optional
[application Asio context](../../lib/v2.0/C++/docs/ivy-asio.md).
The standalone `PprzFrameDecoder` still exposes incremental reads without I/O.

```cpp
receiver.bind("GUIDE_ALTITUDE", [](const pprzlink::Message &message) {
    std::cout << message.getFieldSI("altitude") << " m\n";
});
receiver.bind(pprzlink::ALL, {.senderId = 42},
    [](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
        // Optional metadata: info.udpPeer, info.xbee and info.frameSize.
        (void)message; (void)info;
    });
receiver.start();
context.run();
```

Filters combine with AND and cover message names, sender/destination/class/component,
UDP source address/port, XBee source/RSSI, and application predicates. Unsupported
filters fail at bind time. Bindings remain active without retaining a return value;
retain the returned ID only for `unbind(id)`. Callbacks are serialized per receiver,
borrow their arguments for the invocation, and can stop reception or send replies.
Copy messages and metadata before retaining them or transferring them to another
thread. `stop()` pauses this receiver and preserves its subscriptions for
`start()`. It cancels binary channel reads; on Ivy it removes only the reactive
bus subscription and leaves the native event loop running. Use `IvyLink::close()`
when that loop must terminate.
Application callback/predicate exceptions remain visible through the loop.
`onError()` observes decoding, I/O and initialization failures; unhandled decoding
failures are skipped and counted, while terminal failures stop reception and throw.
See [the API contract](API_USAGE.md#réception-réactive--abonnements-et-filtres).

`UdpTransport` takes an external Asio context and a `UdpOptions` value with a
local endpoint and broadcast setting. Its dictionary and context must outlive
it; serialize its calls. Sends require an explicit `UdpEndpoint`. Received
datagrams keep their source through all contained frames, including recovery
after a malformed frame. Incomplete frames are discarded at datagram boundaries.

The binary transport searches iteratively for valid frames, keeps incomplete
frames, and discards noise, invalid lengths and bad checksums. A checksum-valid
frame with an unknown definition or malformed payload is consumed before the
decoding error is reported; reception then processes the following frame. Fields
are decoded only inside that frame's payload, and a message becomes visible
only when every field is decoded with no trailing bytes. Frames over 255 bytes
and class/component IDs outside four bits are rejected before writing. String
sender IDs must be decimal integers in [0, 255].

XML loading checks file/parse errors, required attributes, exact field types,
ID ranges and duplicate class/message/field names or IDs. Uppercase NAME/ID/TYPE
and lowercase attributes remain supported, as does a protocol nested inside a
configuration element. Errors retain filename, class, message and field context
when available. A missing file throws `messages_file_not_found`; invalid XML or
definitions throw `bad_message_file`. Array syntax is `type[]` or `type[N]`
with a positive N; `[0]` is rejected. `FieldType::getArraySize()` throws
`std::logic_error` for scalar fields.

Every indexed field accessor checks bounds through `MessageDefinition`.
`getRawValue()` follows the same `no_such_field` / `field_has_no_value`
contract as `getField()`. Indices use `size_t`, including in `MessageDefinition`
and `getRawValue`. Borrowed field references must not outlive message
destruction/assignment or replacement of that field.

`IvyMessageCodec.h` groups runtime regexp generation, textual parsing and
serialization independently of the bus. `IvyLink` uses this codec for ordinary
messages, sender subscriptions and requests. `TextCodec.h` still handles field
formatting and diagnostics.

`Device::writeBuffer()` writes the entire buffer or throws; on failure some
bytes may already have been sent. `Device` and `Transport` have virtual
destructors. A transport takes a non-null `std::unique_ptr<Device>` and destroys the
device when it is destroyed; passing an empty pointer throws. It borrows its
dictionary, which must outlive it. Calls on one transport must be serialized
by the application.

`PosixFileDevice` reports a failed POSIX `open()` with `std::system_error`, and
a failed Asio descriptor assignment with `boost::system::system_error`.
Receive failures, including EOF, are deferred until buffered input is drained
and then throw `boost::system::system_error` from `availableBytes()`, `readAll()`
or `startReception()`. Its synchronous write/poll failures use
`std::system_error`; a readiness timeout or terminal condition reported by
`poll()` uses `std::runtime_error`.

`getDevice()` returns a borrowed reference (const for a const transport), valid until
transport destruction. Do not delete the device or give it to another owner.
Transports cannot be copied or moved; a `unique_ptr<Transport>` can still be moved.
To change devices, recreate the transport. Pass the device with
`std::make_unique` or transfer ownership with `std::move`.

`BoostSerialPortDevice` retains Boost.Asio. Reception is asynchronous; writes
are synchronous and complete. Run the supplied `io_context`, which must outlive
the device. `startReception()` is idempotent; `stopReception()` cancels reception
without closing the port. `readAll()` drains the received queue and also starts
or resumes low-level reception for modem dialogues; transports use readiness notifications. Completion handlers
use their own scratch buffer and retain state independently of the device.
Destruction closes the port safely even when a callback is still pending.

Port I/O and option calls are serialized internally. Finish external calls
before destroying the device. Receive errors are reported by `availableBytes()`,
`readAll()` or `startReception()`; transports forward them through `onError()`;
buffered bytes can be drained before the error is reported. Recreate the device
after a terminal receive error. Option getters return values read from the port.

Additional tests cover malformed binary frames and recovery, XML/file errors,
field-access contracts and the standalone Ivy codec. On Unix, pseudo-terminal
tests exercise serial data larger than the scratch buffer, complete writes,
cancellation/restart, peer closure and destruction with pending callbacks.
They do not require serial hardware.
