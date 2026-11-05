// SPDX-License-Identifier: LGPL-3.0-or-later
/**
 * @file PosixFileDevice.cpp
 * @brief Asynchronous file/FIFO reception and complete descriptor writes.
 * @ingroup transports
 *
 * Buffered input is delivered before EOF or read errors. Nonblocking writes wait for readiness with a bounded timeout and fail on incomplete delivery.
 */

#include "PosixFileDevice.h"
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/post.hpp>
#include <array>
#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <poll.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace pprzlink {
  /// @brief Descriptor, buffers and readiness observer retained by pending asynchronous reads.
  /// @ingroup internals
  struct PosixFileDevice::State : std::enable_shared_from_this<State> {
    /// @brief Open a nonblocking descriptor on a caller-owned event loop.
    /// @param[in] context Loop that must outlive pending cancellation handlers.
    /// @param[in] path File, FIFO or pseudo-terminal to open.
    explicit State(boost::asio::io_context &context, const std::string &path) : descriptor(context)
    {
      const int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
      if (fd < 0) throw std::system_error(errno, std::generic_category(), "open " + path);
      boost::system::error_code error;
      descriptor.assign(fd, error);
      if (error) { ::close(fd); throw boost::system::system_error(error); }
    }
    boost::asio::posix::stream_descriptor descriptor; ///< Exclusively owned asynchronous descriptor.
    std::recursive_mutex mutex; ///< Serializes descriptor operations and buffer changes.
    std::array<uint8_t, 4096> buffer; ///< Scratch storage retained until a read completes.
    BytesBuffer received; ///< Completed input awaiting transport consumption.
    boost::system::error_code receiveError; ///< Terminal failure deferred until buffered bytes are consumed.
    ReceiveCallback callback; ///< Readiness observer called outside the descriptor lock.
    bool receiving = false; ///< Whether further reads may be armed.
    bool pending = false; ///< Prevents overlapping operations on the scratch buffer.
    /// @brief Arm one eligible read and notify the observer after committing its bytes.
    void read()
    {
      if (!receiving || pending || receiveError) return;
      pending = true;
      descriptor.async_read_some(boost::asio::buffer(buffer),
        [self = shared_from_this()](const boost::system::error_code &error, size_t size) {
          ReceiveCallback observer;
          {
            std::lock_guard lock(self->mutex);
            self->pending = false;
            if (error != boost::asio::error::operation_aborted) {
              self->received.insert(self->received.end(), self->buffer.begin(), self->buffer.begin() + size);
              if (error) { self->receiveError = error; self->receiving = false; }
              observer = self->callback;
            }
            self->read();
          }
          if (observer) {
            observer();
            if (size && error && error != boost::asio::error::operation_aborted) observer();
          }
        });
    }
    /// @brief Rethrow a terminal failure after already completed input is drained.
    void checkError() const
    { if (receiveError && received.empty()) throw boost::system::system_error(receiveError, "Link device reception"); }
  };

  PosixFileDevice::PosixFileDevice(boost::asio::io_context &context, const std::string &path)
    : state(std::make_shared<State>(context, path)) {}
  PosixFileDevice::~PosixFileDevice()
  {
    std::lock_guard lock(state->mutex);
    state->receiving = false;
    state->callback = {};
    boost::system::error_code ignored;
    state->descriptor.close(ignored);
  }
  size_t PosixFileDevice::availableBytes()
  { std::lock_guard lock(state->mutex); state->checkError(); return state->received.size(); }
  BytesBuffer PosixFileDevice::readAll()
  { std::lock_guard lock(state->mutex); state->checkError(); return std::exchange(state->received, {}); }
  void PosixFileDevice::setReceiveCallback(ReceiveCallback callback)
  { std::lock_guard lock(state->mutex); state->callback = std::move(callback); }
  void PosixFileDevice::startReception()
  {
    std::lock_guard lock(state->mutex);
    state->checkError();
    state->receiving = true;
    state->read();
    if (!state->received.empty() && state->callback) boost::asio::post(state->descriptor.get_executor(), [state = state] {
      ReceiveCallback observer;
      { std::lock_guard lock(state->mutex); if (state->receiving) observer = state->callback; }
      if (observer) observer();
    });
  }
  void PosixFileDevice::stopReception()
  { std::lock_guard lock(state->mutex); state->receiving = false; state->descriptor.cancel(); }
  boost::asio::any_io_executor PosixFileDevice::getExecutor() { return state->descriptor.get_executor(); }

  void PosixFileDevice::writeBuffer(const BytesBuffer &bytes)
  {
    std::lock_guard lock(state->mutex);
    const int descriptor = state->descriptor.native_handle();
    size_t offset = 0;
    while (offset < bytes.size()) {
      const auto count = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
      if (count > 0) offset += static_cast<size_t>(count);
      else if (count < 0 && errno == EINTR) continue;
      else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        pollfd waiting{descriptor, POLLOUT, 0};
        int ready;
        do { ready = ::poll(&waiting, 1, 1000); } while (ready < 0 && errno == EINTR);
        if (ready < 0) throw std::system_error(errno, std::generic_category(), "poll link device");
        if (ready == 0 || (waiting.revents & (POLLERR | POLLHUP | POLLNVAL)))
          throw std::runtime_error("Link device cannot complete the write");
      } else throw std::system_error(count == 0 ? EIO : errno, std::generic_category(), "write link device");
    }
  }
}
