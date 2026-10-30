#pragma once

#include <pprzlink/Device.h>
#include <pprzlink/Transport.h>
#include <boost/asio/io_context.hpp>
#include <deque>
#include <stdexcept>
#include <string>
#include <utility>

inline void require(bool condition, const std::string &description)
{
  if (!condition) throw std::runtime_error(description);
}

template<class Exception, class Callback>
std::string expectException(Callback callback)
{
  try { callback(); }
  catch (const Exception &error) { return error.what(); }
  throw std::runtime_error("Expected exception was not thrown");
}

class MemoryDevice : public pprzlink::Device {
public:
  boost::asio::io_context context;
  pprzlink::BytesBuffer incoming, outgoing;
  void setReceiveCallback(ReceiveCallback value) override { callback = std::move(value); }
  void startReception() override { receiving = true; notify(); }
  void stopReception() override { receiving = false; }
  boost::asio::any_io_executor getExecutor() override { return context.get_executor(); }
  void notify() { if (receiving && callback && !incoming.empty()) { auto observer = callback; observer(); } }
  size_t availableBytes() override { return incoming.size(); }
  pprzlink::BytesBuffer readAll() override { return std::exchange(incoming, {}); }
  void writeBuffer(const pprzlink::BytesBuffer &bytes) override { outgoing = bytes; }
private:
  ReceiveCallback callback;
  bool receiving = false;
};

// Test fixture: explicitly notify a synthetic input source and retain callback results.
// Real transports have no polling/getMessage API; this queue is only for framing fixtures.
class ReceiptQueue {
public:
  explicit ReceiptQueue(pprzlink::Transport &transport) : transport(transport)
  {
    binding = transport.bind(pprzlink::ALL, [this](const pprzlink::Message &message, const pprzlink::ReceiveInfo &info) {
      messages.push_back(pprzlink::ReceivedMessage{message, info.frameSize, info.xbee, info.udpPeer});
    });
    transport.onError([this](const pprzlink::ReceiveError &error) { errors.push_back(error.exception); });
    transport.start();
  }
  ~ReceiptQueue() { transport.unbind(binding); transport.onError({}); }
  bool hasMessage()
  {
    if (auto device = dynamic_cast<MemoryDevice *>(&transport.getDevice())) {
      device->notify();
      device->context.restart();
      device->context.poll(); // Run synthetic notifications posted by start/restart.
    }
    if (!errors.empty()) { auto error = errors.front(); errors.pop_front(); std::rethrow_exception(error); }
    return !messages.empty();
  }
  std::optional<pprzlink::ReceivedMessage> receive()
  {
    if (!hasMessage()) return std::nullopt;
    auto value = std::move(messages.front()); messages.pop_front(); return value;
  }
  std::unique_ptr<pprzlink::Message> getMessage()
  { auto value = receive(); return value ? std::make_unique<pprzlink::Message>(std::move(value->message)) : nullptr; }
private:
  pprzlink::Transport &transport;
  pprzlink::Receiver::BindingId binding;
  std::deque<pprzlink::ReceivedMessage> messages;
  std::deque<std::exception_ptr> errors;
};
