#include <pprzlink/IvyLink.h>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;
using pprzlink::IvyLink;
using pprzlink::Message;
using pprzlink::MessageDictionary;

namespace {
  void require(bool condition, const std::string &description)
  {
    if (!condition) throw std::runtime_error(description);
  }

  template<class Predicate>
  void waitFor(Predicate predicate, const std::string &description)
  {
    const auto deadline = std::chrono::steady_clock::now() + 4s;
    while (!predicate()) {
      require(std::chrono::steady_clock::now() < deadline, "Timeout: " + description);
      std::this_thread::sleep_for(5ms);
    }
  }

  template<class T>
  T field(const Message &msg, const std::string &name)
  {
    T value;
    msg.getField(name, value);
    return value;
  }

  void waitBindings(IvyLink &sender, const std::string &peerName, size_t count)
  {
    waitFor([&] {
      auto peer = sender.getBus().find_application(peerName);
      if (!peer || !*peer) return false;
      auto regexps = sender.getBus().application_regexps(**peer);
      return regexps && regexps->size() == count;
    }, "bindings of " + peerName);
  }

  void sendRaw(IvyLink &link, std::string_view text)
  {
    auto result = link.getBus().send(text);
    require(result.has_value(), "Raw Ivy send");
  }

  void testReactive(const MessageDictionary &dict, const std::string &domain)
  {
    std::atomic<int> all{0}, numeric{0}, literal{0};
    IvyLink receiver(dict, "reactive-rx", domain, true);
    IvyLink sender(dict, "reactive-tx", domain, true);
    receiver.bind(pprzlink::ALL, [&](const Message &, const pprzlink::ReceiveInfo &info) {
      require(info.frameSize == 0 && !info.udpPeer && !info.xbee, "Ivy metadata is explicitly unavailable");
      ++all;
    });
    const auto id = receiver.bind("EMPTY", {.senderId = 42}, [&](const Message &) { ++numeric; });
    receiver.bind("EMPTY", {.sender = "plane.1"}, [&](const Message &) { ++literal; });
    bool refused = false;
    try { receiver.bind(pprzlink::ALL, {.receiverId = 42}, [](const Message &) {}); }
    catch (const std::invalid_argument &) { refused = true; }
    require(refused, "Ivy binary routing filters are rejected");
    receiver.start();
    waitBindings(sender, "reactive-rx", 1);
    sendRaw(sender, "42 EMPTY"); sendRaw(sender, "43 EMPTY"); sendRaw(sender, "plane.1 EMPTY");
    waitFor([&] { return all == 3 && numeric == 1 && literal == 1; }, "reactive Ivy source filters");
    receiver.unbind(id);
    sendRaw(sender, "42 EMPTY");
    waitFor([&] { return all == 4; }, "reactive Ivy unbind");
    require(numeric == 1, "Receiver-owned binding removed without affecting ALL");
    require(receiver.getBus().take_callback_error().has_value(), "Reactive Ivy callback error");
  }

  void testMessages(const MessageDictionary &dict, const std::string &domain)
  {
    std::atomic<int> decoded{0}, aircraft{0}, empty{0}, singles{0};
    // Callback state is declared before the buses so it outlives their threads.
    IvyLink receiver(dict, "messages-rx", domain, true);
    const auto valuesId = receiver.BindMessage(dict.getDefinition("VALUES"), [&](std::string sender, Message msg) {
      require(sender == "42", "Sender callback argument");
      require(std::get<std::string>(msg.getSenderId()) == "42", "Message sender");
      require(field<char>(msg, "character") == 'Z', "char");
      require(field<int8_t>(msg, "i8") == -128, "int8");
      require(field<int16_t>(msg, "i16") == -32768, "int16");
      require(field<int32_t>(msg, "i32") == -2147483647 - 1, "int32");
      require(field<uint8_t>(msg, "u8") == 255, "uint8");
      require(field<uint16_t>(msg, "u16") == 65535, "uint16");
      require(field<uint32_t>(msg, "u32") == 4294967295u, "uint32");
      require(field<float>(msg, "f") == 125.f, "float exponent");
      require(field<double>(msg, "d") == -0.025, "double exponent");
      require(field<std::string>(msg, "text") == "100% ready", "quoted string");
      require(field<std::vector<char>>(msg, "chars") == std::vector<char>({'a', ' ', 'b'}), "char array");
      require(field<std::vector<int16_t>>(msg, "array") == std::vector<int16_t>({-2, 3}), "numeric array");
      require(field<std::vector<uint8_t>>(msg, "fixed") == std::vector<uint8_t>({0, 255}), "fixed array");
      require(field<std::vector<float>>(msg, "blank").empty(), "empty array");
      ++decoded;
    });
    receiver.BindOnSrcAc("plane.1", [&](std::string sender, Message msg) {
      require(sender == "plane.1", "Literal aircraft ID");
      require(field<std::string>(msg, "text") == "100% ready", "Aircraft quoted fields");
      require(field<std::vector<int16_t>>(msg, "array").size() == 2, "Aircraft array");
      ++aircraft;
    });
    receiver.BindMessage(dict.getDefinition("EMPTY"), [&](std::string, Message msg) {
      require(msg.getNbValues() == 0, "Fieldless message");
      ++empty;
    });
    receiver.BindMessage(dict.getDefinition("SINGLE"), [&](std::string, Message msg) {
      require(field<std::vector<int8_t>>(msg, "values") == std::vector<int8_t>({-1}), "Single element array");
      require(field<std::string>(msg, "text").empty(), "Empty quoted string");
      ++singles;
    });
    IvyLink sender(dict, "messages-tx", domain, true);
    waitBindings(sender, "messages-rx", 4);
    const std::string payload = " VALUES Z -128 -32768 -2147483648 255 65535 4294967295 1.25e2 -2.5e-2 \"100% ready\" \"a b\" -2,3 0,255 ";
    sendRaw(sender, "42" + payload);
    // Temporarily cancel the generic VALUES binding before testing aircraft routing.
    waitFor([&] { return decoded == 1; }, "dynamic field conversions");
    receiver.UnbindMessage(valuesId);
    waitBindings(sender, "messages-rx", 3);
    sendRaw(sender, "planeX1" + payload); // A dot in an aircraft ID is not a regexp wildcard.
    sendRaw(sender, "plane.1" + payload);
    Message noFields(dict.getDefinition("EMPTY"));
    noFields.setSenderId(uint8_t{42});
    sender.sendMessage(noFields);
    sendRaw(sender, "42 EMPTY "); // Older senders append one space.
    Message single(dict.getDefinition("SINGLE"));
    single.setSenderId(uint8_t{42});
    single.addField("values", std::vector<int8_t>{-1});
    single.addField("text", std::string{});
    sender.sendMessage(single);
    waitFor([&] { return aircraft == 1 && empty == 2 && singles == 1; }, "aircraft/empty/array messages");
    require(receiver.getBus().take_callback_error().has_value(), "Message callback error");
    std::cout << "XML field conversions, aircraft routing and round trips passed\n";
  }

  void testReactiveSharedTraffic(const MessageDictionary &dict, const std::string &domain)
  {
    std::atomic<int> all{0}, selected{0}, scoped{0}, decodeErrors{0}, answered{0}, replies{0};
    std::atomic<bool> wrongError{false};
    IvyLink receiver(dict, "shared-rx", domain);
    receiver.bind(pprzlink::ALL, [&](const Message &) { ++all; });
    receiver.bind("BYTE", {.sender = "42"}, [&](const Message &) { ++selected; });
    auto ordinary = receiver.subscribeMessage("EMPTY", [&](std::string, Message) { ++scoped; });
    receiver.onError([&](const pprzlink::ReceiveError &error) {
      if (error.kind != pprzlink::ReceiveError::Kind::Decode ||
          error.message.find("BYTE") == std::string::npos) wrongError = true;
      ++decodeErrors;
    });
    auto deadline = receiver.getBus().bind_event(
      [&](std::chrono::milliseconds) { receiver.close(); }, ivy::after(4s));
    require(deadline.has_value(), "Shared traffic deadline registration");
    std::exception_ptr loopFailure;
    std::jthread runner([&] {
      try { receiver.run(); }
      catch (...) { loopFailure = std::current_exception(); }
    });
    waitFor([&] { return receiver.isRunning(); }, "run implicitly starts reactive reception");

    // These real peers announce '<application> ready' after run() has started.
    IvyLink server(dict, "shared-server", domain, true);
    auto answerer = server.subscribeRequestAnswerer(dict.getDefinition("ANSWER_REQ"),
      [&](std::string, Message request) {
        Message answer(dict.getDefinition("ANSWER"));
        answer.setSenderId(std::string("response-source"));
        answer.addField("value", field<uint32_t>(request, "value") + 1);
        ++answered;
        return answer;
      });
    IvyLink sender(dict, "shared-tx", domain, true);
    waitBindings(sender, "shared-rx", 2);
    waitBindings(server, "shared-rx", 2);
    waitBindings(sender, "shared-server", 1);

    Message request(dict.getDefinition("ANSWER_REQ"));
    request.setSenderId(std::string("request-source"));
    request.addField("value", uint32_t{41});
    sender.sendRequest(request, [&](std::string source, Message answer) {
      require(source == "response-source" && field<uint32_t>(answer, "value") == 42,
              "Shared bus request/reply remains correlated");
      ++replies;
    });
    sendRaw(sender, "foreign UNKNOWN payload");
    sendRaw(sender, "42 BYTE not-a-number");
    sendRaw(sender, "42 EMPTY");
    sendRaw(sender, "42 BYTE 7");
    sendRaw(sender, "43 BYTE 8");
    waitFor([&] { return all == 3 && selected == 1 && scoped == 1 && answered == 1 && replies == 1; },
            "ordinary, filtered, scoped and request callbacks on a shared bus");

    // Sent after the requester received its reply: on the server's connection
    // this marker follows the correlated reply, so all traffic has been seen.
    sendRaw(server, "barrier EMPTY");
    waitFor([&] { return all == 4 && scoped == 2; }, "reply processed before final ordinary message");
    receiver.close();
    runner.join();
    if (loopFailure) std::rethrow_exception(loopFailure);
    require(decodeErrors == 1 && !wrongError,
            "Only malformed known fields report Decode; ready, unknown and request traffic are ignored");
    require(sender.getBus().take_callback_error().has_value(), "Shared sender callbacks remain valid");
    require(server.getBus().take_callback_error().has_value(), "Shared answerer callbacks remain valid");
  }

  void testCompactNumbers(const MessageDictionary &dict, const std::string &domain)
  {
    std::atomic<bool> received{false};
    const float altitude = 123.6f;
    const double precise = 1.2345678901234567;
    const std::vector<float> values{-0.f, std::numeric_limits<float>::denorm_min(),
                                   std::numeric_limits<float>::max()};
    const auto &definition = dict.getDefinition("COMPACT");
    IvyLink receiver(dict, "compact-rx", domain, true);
    receiver.BindMessage(definition, [&](std::string sender, Message msg) {
      require(sender == "42", "Compact message sender");
      require(std::bit_cast<uint32_t>(msg.getField<float>("f")) == std::bit_cast<uint32_t>(altitude),
              "Float precision survives the real Ivy bus");
      require(std::bit_cast<uint64_t>(msg.getField<double>("d")) == std::bit_cast<uint64_t>(precise),
              "Double precision survives the real Ivy bus");
      const auto decoded = msg.getField<std::span<const float>>("values");
      require(decoded.size() == values.size(), "Compact array length on Ivy");
      for (size_t i = 0; i < values.size(); ++i)
        require(std::bit_cast<uint32_t>(decoded[i]) == std::bit_cast<uint32_t>(values[i]),
                "Signed zero and extreme array values survive the real Ivy bus");
      received = true;
    });
    IvyLink sender(dict, "compact-tx", domain, true);
    waitBindings(sender, "compact-rx", 1);
    Message outgoing(definition);
    outgoing.setSenderId(42);
    outgoing.setField("f", altitude, "d", precise, "values", values);
    sender.sendMessage(outgoing);
    waitFor([&] { return received.load(); }, "compact floating-point message round trip");
  }

  void testRequests(const MessageDictionary &dict, const std::string &domain)
  {
    std::atomic<int> replies{0}, answered{0}, emptyReplies{0};
    IvyLink server(dict, "request-server", domain, true);
    const auto answerId = server.registerRequestAnswerer(dict.getDefinition("ANSWER_REQ"),
      [&](std::string sender, Message request) {
        require(sender == "client", "Request sender");
        require(std::get<std::string>(request.getSenderId()) == sender, "Request message sender");
        Message answer(dict.getDefinition("ANSWER"));
        answer.setSenderId(std::string("server"));
        answer.addField("value", field<uint32_t>(request, "value") + 1);
        ++answered;
        return answer;
      });
    require(answerId > 0, "Answerer returns a usable subscription ID");
    server.registerRequestAnswerer(dict.getDefinition("EMPTY_REQ"), [&](std::string, Message) {
      Message answer(dict.getDefinition("EMPTY"));
      answer.setSenderId(std::string("server"));
      return answer;
    });
    IvyLink first(dict, "request-first", domain, true);
    IvyLink second(dict, "request-second", domain, true);
    waitBindings(first, "request-server", 2);
    waitBindings(second, "request-server", 2);
    auto issueRequests = [&](IvyLink &client, uint32_t base) {
      for (uint32_t i = 0; i < 10; ++i) {
        Message request(dict.getDefinition("ANSWER_REQ"));
        request.setSenderId(std::string("client"));
        request.addField("value", base + i);
        client.sendRequest(request, [&, expected = base + i + 1](std::string sender, Message answer) {
          require(sender == "server", "Reply sender");
          require(field<uint32_t>(answer, "value") == expected, "Reply matched to correct request");
          ++replies;
        });
      }
    };
    std::jthread one([&] { issueRequests(first, 100); });
    std::jthread two([&] { issueRequests(second, 200); });
    one.join();
    two.join();
    waitFor([&] { return replies == 20; }, "concurrent requests on two links");
    require(answered == 20, "Exactly one answer per request");
    waitBindings(server, "request-first", 0);
    waitBindings(server, "request-second", 0);
    Message emptyRequest(dict.getDefinition("EMPTY_REQ"));
    emptyRequest.setSenderId(std::string("client"));
    first.sendRequest(emptyRequest, [&](std::string, Message) { ++emptyReplies; });
    waitFor([&] { return emptyReplies == 1; }, "fieldless request and answer");
    server.UnbindMessage(answerId);
    waitBindings(first, "request-server", 1);
    Message request(dict.getDefinition("ANSWER_REQ"));
    request.setSenderId(std::string("client"));
    request.addField("value", uint32_t{1});
    const auto canceled = first.sendRequest(request, [](std::string, Message) {
      throw std::runtime_error("Canceled request delivered");
    });
    first.UnbindMessage(canceled);
    first.UnbindMessage(canceled);
    waitBindings(server, "request-first", 0);
    require(first.getBus().take_callback_error().has_value(), "First client callback error");
    require(second.getBus().take_callback_error().has_value(), "Second client callback error");
    require(server.getBus().take_callback_error().has_value(), "Answerer callback error");
    std::cout << "Concurrent requests, correlation and cancellation passed\n";
  }

  void testIsolation(const MessageDictionary &dict, const std::string &domainA, const std::string &domainB)
  {
    std::atomic<int> firstCount{0}, secondCount{0}, selfCount{0};
    auto first = std::make_unique<IvyLink>(dict, "isolation-a", domainA, true);
    IvyLink second(dict, "isolation-b", domainB, true);
    first->BindMessage(dict.getDefinition("EMPTY"), [&](std::string, Message) { ++firstCount; });
    second.BindMessage(dict.getDefinition("EMPTY"), [&](std::string, Message) { ++secondCount; });
    IvyLink txA(dict, "isolation-tx-a", domainA, true);
    IvyLink txB(dict, "isolation-tx-b", domainB, true);
    waitBindings(txA, "isolation-a", 1);
    waitBindings(txB, "isolation-b", 1);
    sendRaw(txA, "42 EMPTY");
    waitFor([&] { return firstCount == 1; }, "first bus");
    require(secondCount == 0, "Different bus domains are isolated");
    first.reset(); // Must stop/join only this context.
    sendRaw(txB, "42 EMPTY");
    waitFor([&] { return secondCount == 1; }, "surviving bus after another link's destruction");
    std::atomic<long> selfId{0};
    selfId = second.BindMessage(dict.getDefinition("SINGLE"), [&](std::string, Message) {
      second.UnbindMessage(selfId);
      ++selfCount;
    });
    waitBindings(txB, "isolation-b", 2);
    sendRaw(txB, "42 SINGLE -1 \"\"");
    waitFor([&] { return selfCount == 1; }, "self-unbinding callback");
    waitBindings(txB, "isolation-b", 1);
    std::vector<std::jthread> workers;
    for (int t = 0; t < 4; ++t) {
      workers.emplace_back([&] {
        Message message(dict.getDefinition("EMPTY"));
        message.setSenderId(uint8_t{42});
        for (int i = 0; i < 25; ++i) {
          const auto id = second.BindMessage(dict.getDefinition("SINGLE"), [](std::string, Message) {});
          txB.sendMessage(message);
          second.UnbindMessage(id);
        }
      });
    }
    workers.clear(); // Join before checking delivery.
    waitFor([&] { return secondCount == 101; }, "100 concurrent sends and subscription changes");
    require(second.getBus().take_callback_error().has_value(), "Isolation callback error");
    std::cout << "Independent buses, shutdown, self-unbind and concurrent sends passed\n";
  }

  void testLifecycleAndErrors(const MessageDictionary &dict, const std::string &domain)
  {
    IvyLink manual(dict, "manual-loop", domain);
    auto timer = manual.getBus().bind_event([&](std::chrono::milliseconds) { manual.close(); }, ivy::after(10ms));
    require(timer.has_value(), "Manual loop timer registration");
    manual.run();
    bool sendFailed = false;
    try {
      Message msg(dict.getDefinition("EMPTY"));
      msg.setSenderId(uint8_t{42});
      manual.sendMessage(msg);
    } catch (const std::system_error&) {
      sendFailed = true;
    }
    require(sendFailed, "Stopped-bus send surfaces Ivy error");

    std::atomic<int> invalidDelivered{0};
    IvyLink receiver(dict, "range-rx", domain, true);
    receiver.BindMessage(dict.getDefinition("BYTE"), [&](std::string, Message) { ++invalidDelivered; });
    IvyLink sender(dict, "range-tx", domain, true);
    waitBindings(sender, "range-rx", 1);
    sendRaw(sender, "42 BYTE 256");
    waitFor([&] { return receiver.getBus().state() == IVY_CTX_STOPPED; }, "conversion failure stops bus");
    require(invalidDelivered == 0, "Out-of-range uint8 must not wrap");
    require(!receiver.getBus().take_callback_error(), "Callback failure is observable");
    std::cout << "Manual loop, stopped-bus errors and range validation passed\n";
  }

  void testPauseResume(const MessageDictionary &dict, const std::string &domain, bool threaded)
  {
    std::atomic<int> selected{0}, all{0}, scoped{0}, ticks{0};
    std::exception_ptr loopFailure;
    const std::string name = threaded ? "pause-threaded" : "pause-manual";
    IvyLink receiver(dict, name, domain, threaded);
    pprzlink::Receiver &generic = receiver;
    generic.bind("BYTE", {.senderId = 42}, [&](const Message &) {
      if (++selected == 1) generic.stop(); // Callback-side pause suppresses later bindings.
    });
    generic.bind(pprzlink::ALL, [&](const Message &) { ++all; });
    auto subscription = receiver.subscribeMessage("EMPTY", [&](std::string, Message) { ++scoped; });
    auto timer = receiver.getBus().bind_event([&](std::chrono::milliseconds) { ++ticks; }, ivy::every(5ms));
    auto deadline = receiver.getBus().bind_event(
      [&](std::chrono::milliseconds) { receiver.close(); }, ivy::after(4s));
    require(timer.has_value() && deadline.has_value(), "Pause/resume timer registration");
    std::optional<std::jthread> runner;
    if (threaded) generic.start();
    else runner.emplace([&] {
      try { receiver.run(); }
      catch (...) { loopFailure = std::current_exception(); }
    });
    IvyLink sender(dict, name + "-tx", domain, true);
    waitBindings(sender, name, 2);
    sendRaw(sender, "42 BYTE 7");
    waitFor([&] { return selected == 1 && !generic.isRunning(); }, "callback-side Ivy pause");
    waitBindings(sender, name, 1);
    generic.stop(); // Repeated pause is harmless.
    const int before = ticks.load();
    sendRaw(sender, "42 BYTE 8");
    sendRaw(sender, "42 EMPTY");
    waitFor([&] { return scoped == 1 && ticks > before; }, "native subscriptions and timers survive pause");
    require(selected == 1 && all == 0, "A paused receiver never dispatches its bindings");

    generic.start();
    generic.start(); // Repeated start must not install a second subscription.
    waitBindings(sender, name, 2);
    sendRaw(sender, "43 BYTE 9");
    sendRaw(sender, "42 BYTE 10");
    waitFor([&] { return selected == 2 && all == 2; }, "restart preserves bindings and sender filters");
    generic.stop();
    waitBindings(sender, name, 1);
    generic.start();
    waitBindings(sender, name, 2);
    sendRaw(sender, "42 BYTE 11");
    waitFor([&] { return selected == 3 && all == 3; }, "multiple pause/resume cycles remain usable");

    receiver.close();
    receiver.close(); // A terminal close is idempotent and ends manual run().
    if (runner) runner->join();
    if (loopFailure) std::rethrow_exception(loopFailure);
    waitFor([&] { return receiver.getBus().state() == IVY_CTX_STOPPED; }, "explicit close stops Ivy loop");
    bool restartFailed = false;
    try { receiver.start(); }
    catch (const std::system_error &) { restartFailed = true; }
    require(restartFailed && !generic.isRunning(), "A permanently closed link cannot be restarted");
    require(receiver.getBus().take_callback_error().has_value(), "Pause/resume callbacks remain valid");
  }

  void testOwnedSubscriptions(const MessageDictionary &dict, const std::string &domain)
  {
    std::atomic<int> messages{0}, fromSender{0}, answers{0};
    IvyLink receiver(dict, "owned-rx", domain, true);
    IvyLink sender(dict, "owned-tx", domain, true);
    {
      auto subscription = receiver.subscribeMessage("EMPTY", [&](std::string, Message) { ++messages; });
      auto moved = std::move(subscription);
      require(!subscription.is_bound() && moved.is_bound(), "Moving transfers native Ivy ownership");
      waitBindings(sender, "owned-rx", 1);
      sendRaw(sender, "42 EMPTY");
      waitFor([&] { return messages == 1; }, "owned message callback");
    }
    waitBindings(sender, "owned-rx", 0); // Scope exit really removed the remote subscription.
    {
      auto subscription = receiver.subscribeSender("plane.1", [&](std::string source, Message) {
        require(source == "plane.1", "Literal sender selector");
        ++fromSender;
      });
      waitBindings(sender, "owned-rx", 1);
      sendRaw(sender, "plane.1 EMPTY");
      waitFor([&] { return fromSender == 1; }, "owned sender callback");
      require(subscription.unbind().has_value(), "Explicit native cancellation");
    }
    waitBindings(sender, "owned-rx", 0);
    {
      auto answerer = receiver.subscribeRequestAnswerer(dict.getDefinition("EMPTY_REQ"), [&](std::string, Message) {
        Message answer(dict.getDefinition("EMPTY"));
        answer.setSenderId(std::string("server"));
        return answer;
      });
      waitBindings(sender, "owned-rx", 1);
      Message request(dict.getDefinition("EMPTY_REQ"));
      request.setSenderId(std::string("client"));
      sender.sendRequest(request, [&](std::string, Message) { ++answers; });
      waitFor([&] { return answers == 1; }, "owned answerer with legacy request API");
    }
    waitBindings(sender, "owned-rx", 0);

    ivy::Subscription surviving;
    {
      IvyLink temporary(dict, "owned-temporary", domain, true);
      surviving = temporary.subscribeMessage("EMPTY", [](std::string, Message) {});
      require(surviving.is_bound(), "Subscription starts active");
    }
    require(!surviving.is_bound() && surviving.unbind().has_value(), "Token safely outlives its link");
    require(receiver.getBus().take_callback_error().has_value(), "Owned callbacks remain valid");
  }
}

int main()
{
  try {
    tinyxml2::XMLDocument xml;
    require(xml.Parse(R"(<protocol><msg_class name="test" id="1">
      <message NAME="VALUES" ID="1">
        <field name="character" type="char"/><field name="i8" type="int8"/>
        <field name="i16" type="int16"/><field name="i32" type="int32"/>
        <field name="u8" type="uint8"/><field name="u16" type="uint16"/>
        <field name="u32" type="uint32"/><field name="f" type="float"/>
        <field name="d" type="double"/><field name="text" type="string"/>
        <field name="chars" type="char[]"/><field name="array" type="int16[]"/>
        <field name="fixed" type="uint8[2]"/><field name="blank" type="float[]"/>
      </message>
      <message name="EMPTY" id="2"/><message name="EMPTY_REQ" id="3"/>
      <message name="ANSWER" id="4"><field name="value" type="uint32"/></message>
      <message name="ANSWER_REQ" id="5"><field name="value" type="uint32"/></message>
      <message name="SINGLE" id="6"><field name="values" type="int8[]"/><field name="text" type="string"/></message>
      <message name="BYTE" id="7"><field name="value" type="uint8"/></message>
      <message name="COMPACT" id="8"><field name="f" type="float"/>
        <field name="d" type="double"/><field name="values" type="float[]"/></message>
    </msg_class></protocol>)") == tinyxml2::XML_SUCCESS, "Test dictionary XML");
    MessageDictionary dict(xml.RootElement());
    require(dict.getDefinition("VALUES").getName() == "VALUES", "Uppercase XML message name");
    require(dict.getDefinition("EMPTY").getId() == 2, "Lowercase XML message ID");
    const int port = 25000 + (getpid() % 10000) * 2;
    const auto domainA = "127.255.255.255:" + std::to_string(port);
    const auto domainB = "127.255.255.255:" + std::to_string(port + 1);
    testReactive(dict, domainA);
    testReactiveSharedTraffic(dict, domainA);
    testMessages(dict, domainA);
    testCompactNumbers(dict, domainA);
    testRequests(dict, domainA);
    testIsolation(dict, domainA, domainB);
    testLifecycleAndErrors(dict, domainA);
    testPauseResume(dict, domainA, false);
    testPauseResume(dict, domainA, true);
    testOwnedSubscriptions(dict, domainA);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
