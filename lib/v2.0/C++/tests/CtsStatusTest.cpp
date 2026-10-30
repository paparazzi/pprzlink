// SPDX-License-Identifier: LGPL-3.0-or-later
#include "TestSupport.h"
#include <pprzlink/BoostSerialPortDevice.h>
#include <pprzlink/XbeeModem.h>
#include <cerrno>
#include <cstdarg>
#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

using namespace pprzlink;

namespace {
  // Interpose only the native status query. Production code still opens and
  // configures a real PTY; Linux PTYs cannot supply physical CTS transitions.
  bool interceptStatus = false;
  int statusBits = 0, statusError = 0, statusQueries = 0;

  class Terminal {
  public:
    Terminal()
    {
      descriptor = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
      require(descriptor >= 0, "Open pseudo-terminal");
      require(grantpt(descriptor) == 0 && unlockpt(descriptor) == 0, "Prepare pseudo-terminal");
      name = ptsname(descriptor);
    }
    ~Terminal() { if (descriptor >= 0) close(descriptor); }
    int descriptor = -1;
    std::string name;
  };

  termios attributes(int descriptor)
  {
    termios result{};
    require(tcgetattr(descriptor, &result) == 0, "Read native UART attributes");
    return result;
  }

  void testSignalStates()
  {
    Terminal terminal;
    boost::asio::io_context context;
    BoostSerialPortDevice device(context, terminal.name);
    const SerialDevice &serial = device;
    interceptStatus = true;
    for (const bool flow : {false, true}) {
      device.setHardwareFlowControl(flow);
      const auto before = attributes(terminal.descriptor);
      for (const bool asserted : {false, true, false}) {
        // Other asserted modem signals must not be mistaken for CTS.
        statusBits = TIOCM_RTS | TIOCM_DSR | (asserted ? TIOCM_CTS : 0);
        require(serial.getCTS() == asserted, "Read logical CTS independently of flow-control configuration");
        require(device.getHardwareFlowControl() == flow, "CTS inspection never changes flow-control ownership/settings");
      }
      const auto after = attributes(terminal.descriptor);
      require(before.c_cflag == after.c_cflag && before.c_iflag == after.c_iflag,
              "Status queries do not modify native UART settings");
    }
    require(statusQueries == 6, "Each query reads the driver instead of returning a cached/configuration value");

    XbeeConfiguration configuration;
    configuration.targetBaudrate.reset();
    XbeeModem modem(configuration);
    require(!modem.poll(device), "Claim flow control for XBee before command mode");
    expectException<std::logic_error>([&] { device.setHardwareFlowControl(true); });
    statusBits = TIOCM_CTS;
    require(serial.getCTS(), "CTS remains observable after XBee claims flow-control configuration");
    interceptStatus = false;
  }

  void testNativeErrors()
  {
    Terminal terminal;
    boost::asio::io_context context;
    BoostSerialPortDevice device(context, terminal.name);
    interceptStatus = true;
    for (const auto error : {ENOTTY, EIO, EBADF}) {
      statusError = error;
      bool failed = false;
      try { (void)device.getCTS(); }
      catch (const boost::system::system_error &failure) {
        failed = true;
        require(failure.code().value() == error, "Preserve the native CTS query error");
        require(std::string(failure.what()).find("CTS") != std::string::npos, "Name the failed CTS operation");
      }
      require(failed, "Unsupported/disconnected CTS is an error, never a false readiness value");
    }
    statusError = 0;
    interceptStatus = false;
  }

  void testUnsupportedAdapter()
  {
    // An existing serial adapter need not implement the optional CTS query.
    struct SerialWithoutCTS : SerialDevice {
      MemoryDevice memory;
      void resetBaudrate(unsigned int) override {}
      void setReceiveCallback(ReceiveCallback callback) override { memory.setReceiveCallback(std::move(callback)); }
      void startReception() override { memory.startReception(); }
      void stopReception() override { memory.stopReception(); }
      boost::asio::any_io_executor getExecutor() override { return memory.getExecutor(); }
      size_t availableBytes() override { return memory.availableBytes(); }
      BytesBuffer readAll() override { return memory.readAll(); }
      void writeBuffer(const BytesBuffer &bytes) override { memory.writeBuffer(bytes); }
      void applyHardwareFlowControl(bool) override {}
      bool readHardwareFlowControl() const override { return false; }
    } device;
    bool failed = false;
    try { (void)device.getCTS(); }
    catch (const boost::system::system_error &failure) {
      failed = true;
      require(failure.code() == boost::asio::error::operation_not_supported,
              "Optional CTS capability reports operation_not_supported by default");
    }
    require(failed, "A serial adapter without CTS support never fabricates a signal state");
  }
}

extern "C" int __real_ioctl(int descriptor, unsigned long request, ...);

extern "C" int __wrap_ioctl(int descriptor, unsigned long request, ...)
{
  va_list arguments;
  va_start(arguments, request);
  auto *value = va_arg(arguments, void *);
  va_end(arguments);
  if (interceptStatus && request == TIOCMGET) {
    ++statusQueries;
    if (statusError) { errno = statusError; return -1; }
    *static_cast<int *>(value) = statusBits;
    return 0;
  }
  return __real_ioctl(descriptor, request, value);
}

int main()
{
  try {
    testSignalStates();
    testNativeErrors();
    testUnsupportedAdapter();
    std::cout << "CTS assertion/deassertion, native errors and optional-adapter capability passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
