#include "TestSupport.h"
#include <pprzlink/IvyMessageCodec.h>
#include <pprzlink/MessageDictionary.h>
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <locale>

using namespace pprzlink;

namespace {
  template<class T, class Bits>
  void testFloatingRoundTrips(const MessageDefinition &definition)
  {
    Message message(definition);
    message.setSenderId(42);
    const auto check = [&](T value) {
      message.setField("value", value);
      const auto wire = ivy_codec::serializeMessage(message);
      const auto token = std::string_view(wire).substr(wire.find_last_of(' ') + 1);
      const std::array captures{token};
      const auto decoded = ivy_codec::parseFields(definition, "42", captures);
      require(std::bit_cast<Bits>(decoded.getField<T>("value")) == std::bit_cast<Bits>(value),
              "Ivy text must preserve every finite floating-point bit, including signed zero");
    };
    for (const T value : {T{0}, -T{0}, T{1}, T{-1}, T{123.6},
                         std::nextafter(T{1}, T{2}), std::numeric_limits<T>::epsilon(),
                         std::numeric_limits<T>::min(), std::numeric_limits<T>::max(),
                         std::numeric_limits<T>::lowest(), std::numeric_limits<T>::denorm_min(),
                         -std::numeric_limits<T>::denorm_min()}) {
      check(value);
    }
    uint64_t state = 0x123456789abcdef0ULL;
    for (size_t i = 0; i < 4096; ++i) {
      state = state * 6364136223846793005ULL + 1442695040888963407ULL;
      const auto value = std::bit_cast<T>(static_cast<Bits>(state));
      if (std::isfinite(value)) check(value);
    }
  }

  void testCompactNumbers()
  {
    tinyxml2::XMLDocument xml;
    require(xml.Parse(R"(<protocol><msg_class name="test" id="1">
      <message name="FLOAT" id="1"><field name="value" type="float"/></message>
      <message name="DOUBLE" id="2"><field name="value" type="double"/></message>
      <message name="ARRAYS" id="3"><field name="floats" type="float[]"/>
        <field name="doubles" type="double[]"/></message>
    </msg_class></protocol>)") == tinyxml2::XML_SUCCESS, "Floating-point fixtures parse");
    const MessageDictionary dictionary(xml.RootElement());
    const auto &floats = dictionary.getDefinition("FLOAT");
    const auto &doubles = dictionary.getDefinition("DOUBLE");
    testFloatingRoundTrips<float, uint32_t>(floats);
    testFloatingRoundTrips<double, uint64_t>(doubles);

    Message scalar(floats);
    scalar.setSenderId(42);
    scalar.setField("value", 123.6f);
    require(ivy_codec::serializeMessage(scalar) == "42 FLOAT 123.6", "No redundant decimal padding");
    scalar.setField("value", -0.f);
    require(ivy_codec::serializeMessage(scalar) == "42 FLOAT -0", "Compact signed zero");

    const std::vector<float> floatValues{123.6f, -0.f, std::numeric_limits<float>::denorm_min(),
                                         std::numeric_limits<float>::max()};
    const std::vector<double> doubleValues{1.2345678901234567, -0.,
                                           std::numeric_limits<double>::denorm_min(),
                                           std::numeric_limits<double>::max()};
    const auto &arrays = dictionary.getDefinition("ARRAYS");
    Message message(arrays);
    message.setSenderId(42);
    message.setField("floats", floatValues, "doubles", doubleValues);
    const auto wire = ivy_codec::serializeMessage(message);
    const auto decoded = ivy_codec::parseMessageBody(arrays, "42", std::string_view(wire).substr(3));
    const auto decodedFloats = decoded.getField<std::span<const float>>("floats");
    const auto decodedDoubles = decoded.getField<std::span<const double>>("doubles");
    for (size_t i = 0; i < floatValues.size(); ++i)
      require(std::bit_cast<uint32_t>(decodedFloats[i]) == std::bit_cast<uint32_t>(floatValues[i]),
              "Floating-point array values survive Ivy regex parsing exactly");
    for (size_t i = 0; i < doubleValues.size(); ++i)
      require(std::bit_cast<uint64_t>(decodedDoubles[i]) == std::bit_cast<uint64_t>(doubleValues[i]),
              "Double array values survive Ivy regex parsing exactly");
  }

  template<class T>
  void requireSameMeasurement(T actual, T expected)
  {
    require(std::isnan(expected) ? std::isnan(actual) :
              actual == expected && std::signbit(actual) == std::signbit(expected),
            "Ivy text preserves NaN meaning and the sign of infinity and zero");
  }

  template<class T>
  void testSpecialScalars(const MessageDefinition &definition)
  {
    Message message(definition);
    message.setSenderId(42);
    for (const T value : {std::numeric_limits<T>::quiet_NaN(), -std::numeric_limits<T>::quiet_NaN(),
                         std::numeric_limits<T>::infinity(), -std::numeric_limits<T>::infinity()}) {
      message.setField("value", value);
      for (const auto serialize : {ivy_codec::serializeMessage, ivy_codec::serializeLegacyMessage}) {
        const auto wire = serialize(message);
        for (const auto parse : {ivy_codec::parseMessageBody, ivy_codec::parseLegacyMessageBody}) {
          const Message decoded = parse(definition, "42", std::string_view(wire).substr(3));
          requireSameMeasurement(decoded.getField<T>("value"), value);
          expectException<field_conversion_error>([&] { (void)decoded.getFieldAs<int>("value"); });
        }
      }
    }
  }

  void testSpecialNumbers()
  {
    tinyxml2::XMLDocument xml;
    require(xml.Parse(R"(<protocol><msg_class name="test" id="1">
      <message name="FLOAT" id="1"><field name="value" type="float" format="%+F"/></message>
      <message name="DOUBLE" id="2"><field name="value" type="double" format="%+E"/></message>
      <message name="ARRAYS" id="3"><field name="floats" type="float[]"/>
        <field name="doubles" type="double[5]" format="%+F"/></message>
      <message name="SIGNED" id="4"><field name="value" type="int32"/></message>
      <message name="UNSIGNED" id="5"><field name="value" type="uint32"/></message>
      <message name="INTEGERS" id="6"><field name="value" type="int16[]"/></message>
    </msg_class></protocol>)") == tinyxml2::XML_SUCCESS, "Non-finite measurement fixtures parse");
    const MessageDictionary dictionary(xml.RootElement());
    testSpecialScalars<float>(dictionary.getDefinition("FLOAT"));
    testSpecialScalars<double>(dictionary.getDefinition("DOUBLE"));

    const std::vector<float> floats{1.25f, std::numeric_limits<float>::quiet_NaN(),
                                  std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity(), -0.f};
    const std::vector<double> doubles{1.25, std::numeric_limits<double>::quiet_NaN(),
                                    std::numeric_limits<double>::infinity(),
                                    -std::numeric_limits<double>::infinity(), -0.};
    const auto &arrays = dictionary.getDefinition("ARRAYS");
    Message message(arrays);
    message.setSenderId(42);
    message.setField("floats", floats, "doubles", doubles);
    for (const auto serialize : {ivy_codec::serializeMessage, ivy_codec::serializeLegacyMessage}) {
      const auto wire = serialize(message);
      for (const auto parse : {ivy_codec::parseMessageBody, ivy_codec::parseLegacyMessageBody}) {
        const auto decoded = parse(arrays, "42", std::string_view(wire).substr(3));
        const auto decodedFloats = decoded.getField<std::span<const float>>("floats");
        const auto decodedDoubles = decoded.getField<std::span<const double>>("doubles");
        require(decodedFloats.size() == floats.size() && decodedDoubles.size() == doubles.size(),
                "Special floating-point values preserve fixed and dynamic array extents");
        for (size_t i = 0; i < floats.size(); ++i) requireSameMeasurement(decodedFloats[i], floats[i]);
        for (size_t i = 0; i < doubles.size(); ++i) requireSameMeasurement(decodedDoubles[i], doubles[i]);
      }
    }

    for (const auto *name : {"FLOAT", "DOUBLE"}) {
      const auto &definition = dictionary.getDefinition(name);
      for (const auto *token : {"1e1000", "-1e1000", "1e-1000", "nan-tail", "inf-tail", "+nanx"}) {
        const auto body = definition.getName() + " " + token;
        for (const auto parse : {ivy_codec::parseMessageBody, ivy_codec::parseLegacyMessageBody})
          expectException<wrong_message_format>([&] { (void)parse(definition, "42", body); });
      }
    }
    for (const auto *name : {"SIGNED", "UNSIGNED", "INTEGERS"}) {
      const auto &definition = dictionary.getDefinition(name);
      for (const auto *token : {"nan", "-nan", "+NAN", "inf", "-inf", "+INF"}) {
        const auto body = definition.getName() + " " + token;
        for (const auto parse : {ivy_codec::parseMessageBody, ivy_codec::parseLegacyMessageBody})
          expectException<wrong_message_format>([&] { (void)parse(definition, "42", body); });
      }
    }
  }

  void testCameraSnapshot(const std::string &dictionaryPath)
  {
    const MessageDictionary dictionary(dictionaryPath);
    const auto &definition = dictionary.getDefinition("CAMERA_SNAPSHOT_DL");
    require(definition.getLinkMode() == MessageDefinition::LinkMode::Forwarded,
            "The production camera snapshot message is routed by link++");
    Message message(definition);
    message.setSenderId("payload");
    message.setField("camera_id", 7, "snapshot_image_number", 12, "camera_state", 1,
                     "snapshot_valid", 1, "array_temp", std::numeric_limits<float>::quiet_NaN(),
                     "lens_temp", std::numeric_limits<float>::quiet_NaN(), "ac_id", 42);
    for (const auto serialize : {ivy_codec::serializeMessage, ivy_codec::serializeLegacyMessage}) {
      const auto wire = serialize(message);
      require(wire == "payload CAMERA_SNAPSHOT_DL 7 12 1 1 nan nan 42",
              "Production camera messages encode unavailable temperatures as NaN");
      for (const auto parse : {ivy_codec::parseMessageBody, ivy_codec::parseLegacyMessageBody}) {
        const auto decoded = parse(definition, "payload", std::string_view(wire).substr(8));
        require(std::isnan(decoded.getField<float>("array_temp")) &&
                std::isnan(decoded.getField<float>("lens_temp")) &&
                decoded.getField<uint8_t>("ac_id") == 42 && decoded.getField<uint16_t>("camera_id") == 7,
                "Production camera snapshots retain missing measurements and their destination");
      }
    }
  }

  struct UnusualPunctuation : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
    char do_thousands_sep() const override { return '_'; }
    std::string do_grouping() const override { return "\1"; }
  };

  struct ScopedLocale {
    std::locale previous;
    explicit ScopedLocale(const std::locale &locale) : previous(std::locale::global(locale)) {}
    ~ScopedLocale() { std::locale::global(previous); }
  };
}

int main(int argc, char **argv)
{
  try {
    require(argc == 2, "Pass the production messages.xml path");
    testCompactNumbers();
    testSpecialNumbers();
    testCameraSnapshot(argv[1]);
    tinyxml2::XMLDocument xml;
    xml.Parse(R"(<protocol><msg_class name="test" id="1">
      <message name="M.1" id="1"><field name="n" type="uint8"/>
        <field name="values" type="int16[2]"/><field name="text" type="string"/>
        <field name="f" type="float"/></message>
    </msg_class></protocol>)");
    MessageDictionary dictionary(xml.RootElement());
    const auto &definition = dictionary.getDefinition("M.1");
    const auto msg = ivy_codec::parseMessageBody(definition, "42", "M.1 +255 -1,2 \"hello world\" 1.25e2");
    require(msg.getField<uint8_t>("n") == 255 && msg.getField<float>("f") == 125.f, "Runtime numeric conversions");
    require(msg.getField<std::string>("text") == "hello world", "Quoted text");
    require(msg.getField<std::vector<int16_t>>("values") == std::vector<int16_t>({-1, 2}), "Fixed array");
    const auto wire = ivy_codec::serializeMessage(msg);
    require(wire == "42 M.1 255 -1,2 \"hello world\" 125", "Compact Ivy numeric wire format");
    const auto decoded = ivy_codec::parseMessageBody(definition, "42", std::string_view(wire).substr(3));
    require(decoded.toString() == msg.toString(), "Standalone codec round trip");
    {
      const ScopedLocale locale(std::locale(std::locale::classic(), new UnusualPunctuation));
      require(ivy_codec::serializeMessage(msg) == wire, "Ivy header and numbers ignore the global locale");
    }
    for (const auto *body : {"MX1 255 -1,2 text 1", "M.1 256 -1,2 text 1",
                             "M.1 1 -1 text 1", "M.1 1 -1,2, text 1", "M.1 1 -1,2 text nan-tail"}) {
      expectException<wrong_message_format>([&] { (void)ivy_codec::parseMessageBody(definition, "42", body); });
    }
    const std::array<std::string_view, 4> captures{"255", "-1,2", "\"\"", "1e1000"};
    expectException<wrong_message_format>([&] { (void)ivy_codec::parseFields(definition, "42", captures); });
    expectException<wrong_message_format>([&] { (void)ivy_codec::parseFields(definition, "42", {}); });
    std::cout << "Standalone Ivy codec, wire format and invalid input passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
