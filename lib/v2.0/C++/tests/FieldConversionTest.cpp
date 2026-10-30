// SPDX-License-Identifier: LGPL-3.0-or-later
#include "TestSupport.h"
#include <pprzlink/Message.h>
#include <pprzlink/PprzFrameCodec.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>

using namespace pprzlink;

namespace {
  template<class... Arguments>
  concept CanSetFields = requires(Message &message, Arguments&&... arguments) {
    message.setField(std::forward<Arguments>(arguments)...);
  };

  static_assert(CanSetFields<const char*, int>);
  static_assert(CanSetFields<const char*, int, std::string, double>);
  static_assert(CanSetFields<const char*, int, const char*, int, const char*, float>);
  static_assert(std::same_as<decltype(std::declval<Message&>().setField("index", 7)), const Message&>);
  static_assert(std::same_as<decltype(std::declval<Message&>().setField("index", 7, "ac_id", 42)), const Message&>);
  static_assert(!CanSetFields<>);
  static_assert(!CanSetFields<const char*>);
  static_assert(!CanSetFields<const char*, int, const char*>);
  static_assert(!CanSetFields<const char*, int, const char*, int, const char*>);
  static_assert(!CanSetFields<int, int>);
  static_assert(!CanSetFields<const char*, int, int, double>);
  static_assert(!CanSetFields<const char*, int, const char*, int, bool, float>);
  static_assert(!CanSetFields<const char*, int, const char*, int, const char*, float, int, double>);

  void testStringViews()
  {
    tinyxml2::XMLDocument xml;
    require(xml.Parse(R"(<message name="GUIDE_SAMPLES" id="1">
      <field name="label" type="char[4]"/>
      <field name="text" type="string"/>
      <field name="empty" type="char[]"/>
      <field name="number" type="uint8"/>
      <field name="unset" type="char[]"/>
    </message>)") == tinyxml2::XML_SUCCESS, "Text-view fixture parses");
    Message samples(MessageDefinition(xml.RootElement(), 2));
    const std::string text("text\0payload", 12);
    samples.setField("label", "TEST", "text", text, "empty", std::vector<char>{}, "number", 42);

    const auto label = samples.getField<std::string_view>("label");
    require(label == "TEST" &&
            label.data() == std::get<std::vector<char>>(samples.getField("label")).data(),
            "Named message view borrows the existing fixed character array");
    require(samples.getField<std::string_view>(size_t{0}).data() == label.data(),
            "Indexed message view borrows the same character array");
    const auto description = samples.getField<std::string_view>("text");
    require(description == text &&
            description.data() == std::get<std::string>(samples.getField("text")).data(),
            "Message string view retains the full stored bytes without copying");
    std::string_view output;
    samples.getField("label", output);
    require(output.data() == label.data() && output.size() == label.size(),
            "Named output-parameter view borrows the same storage");
    samples.getField(size_t{1}, output);
    require(output.data() == description.data() && output.size() == description.size(),
            "Indexed output-parameter view borrows string storage");
    require(samples.getField<std::string_view>("empty").empty(), "Message supports an empty text view");
    samples.setField("number", 43);
    require(label == "TEST" && description == text, "Updating another field preserves existing text views");

    const auto mismatch = expectException<field_type_mismatch>([&] {
      (void)samples.getField<std::string_view>("number");
    });
    require(mismatch.find("number") != std::string::npos && mismatch.find("uint8") != std::string::npos &&
            mismatch.find("string_view") != std::string::npos, "Text view type error identifies the field and types");
    expectException<field_has_no_value>([&] { (void)samples.getField<std::string_view>("unset"); });
    expectException<no_such_field>([&] { (void)samples.getField<std::string_view>("unknown"); });
  }

  void testArrayViews()
  {
    tinyxml2::XMLDocument xml;
    require(xml.Parse(R"(<message name="ARRAYS" id="1">
      <field name="axes" type="float[3]"/>
      <field name="samples" type="uint16[]"/>
      <field name="empty" type="float[]"/>
    </message>)") == tinyxml2::XML_SUCCESS, "Array-view fixture parses");
    Message samples(MessageDefinition(xml.RootElement(), 2));
    samples.setField("axes", std::array<float, 3>{1.f, 2.f, 3.f},
                     "samples", std::vector<uint16_t>{100, 200}, "empty", std::vector<float>{});
    const auto axes = samples.getField<std::span<const float>>("axes");
    require(axes.data() == std::get<std::vector<float>>(samples.getField("axes")).data() &&
            axes.size() == 3 && axes[0] == 1.f && axes[2] == 3.f,
            "Named span borrows the message's existing array storage");
    const auto indexed = samples.getField<std::span<const float>>(size_t{0});
    require(indexed.data() == axes.data() && indexed.size() == axes.size(),
            "Indexed span borrows the same message storage");
    std::span<const float> output;
    samples.getField("axes", output);
    require(output.data() == axes.data() && output.size() == axes.size(), "Named span output");
    samples.getField(size_t{0}, output);
    require(output.data() == axes.data() && output.size() == axes.size(), "Indexed span output");
    const auto values = samples.getField<std::span<const uint16_t>>("samples");
    require(values.size() == 2 && values[1] == 200 &&
            values.data() == std::get<std::vector<uint16_t>>(samples.getField("samples")).data(),
            "Dynamic array view preserves element type and stored length");
    require(samples.getField<std::span<const float>>("empty").empty(), "Empty message span");
    expectException<field_type_mismatch>([&] { (void)samples.getField<std::span<const double>>("axes"); });
    samples.setField("samples", std::vector<uint16_t>{300});
    require(axes[0] == 1.f && indexed[2] == 3.f, "Updating another field preserves the borrowed axes");
    const auto updated = samples.getField<std::span<const uint16_t>>("samples");
    require(updated.size() == 1 && updated[0] == 300, "A new span follows the current stored array length");
  }

  void testMultipleFields()
  {
    tinyxml2::XMLDocument xml;
    require(xml.Parse(R"(<message name="VALUES" id="1">
      <field name="index" type="uint8"/>
      <field name="ac_id" type="uint8"/>
      <field name="value" type="float"/>
      <field name="samples" type="int16[]"/>
      <field name="label" type="string"/>
    </message>)") == tinyxml2::XML_SUCCESS, "Multiple-field fixture parses");
    const MessageDefinition definition(xml.RootElement(), 2);
    const std::vector<int> samples{-1, 2, 300};
    const std::string aircraftField = "ac_id";

    Message individual(definition);
    individual.setField("index", 7);
    individual.setField(aircraftField, 42);
    individual.setField("value", 12.5);
    individual.setField("samples", samples);
    individual.setField("label", "settings");

    Message grouped(definition);
    const auto &configured = grouped.setField("label", "settings", "samples", samples, "value", 12.5,
                                              aircraftField, 42, "index", 7);
    require(&configured == &grouped, "Grouped setter returns the original message by reference");
    require(encodePprzFrame(configured) == encodePprzFrame(individual),
            "Grouped scalar, array and string fields preserve the existing binary encoding and XML order");

    require(&grouped.setField("index", 8) == &grouped,
            "Single-field setter returns the original message by reference");
    grouped.setField("index", 8, "ac_id", 43);
    require(grouped.getField<uint8_t>("index") == 8 && grouped.getField<uint8_t>("ac_id") == 43 &&
            grouped.getField<float>("value") == 12.5f, "Two pairs update only the requested fields");

    expectException<field_conversion_error>([&] {
      grouped.setField("value", 1.5, "index", 300, "ac_id", 44);
    });
    require(grouped.getField<float>("value") == 1.5f && grouped.getField<uint8_t>("index") == 8 &&
            grouped.getField<uint8_t>("ac_id") == 43,
            "A rejected pair preserves its old value, keeps earlier updates and stops later updates");

    expectException<no_such_field>([&] {
      grouped.setField("index", 9, "unknown", 1, "ac_id", 44);
    });
    require(grouped.getField<uint8_t>("index") == 9 && grouped.getField<uint8_t>("ac_id") == 43,
            "Unknown names use the existing XML checks and stop subsequent pairs");

    grouped.setField("index", 10, "index", 11);
    require(grouped.getField<uint8_t>("index") == 11, "Repeated names use the last value");
  }
}

int main()
{
  try {
    testStringViews();
    testArrayViews();
    testMultipleFields();
    const MessageField byte("aircraft", "uint8");
    for (int invalid : {-1, 256, 300}) {
      const auto error = expectException<field_conversion_error>([&] { FieldValue ignored(byte, invalid); });
      require(error.find("aircraft") != std::string::npos && error.find("uint8") != std::string::npos,
              "Conversion error must identify the field and target type");
    }
    require(FieldValue(byte, 255).getValue<uint8_t>() == 255, "Inclusive integer upper bound");
    require(FieldValue(byte, true).getValue<uint8_t>() == 1, "Bool remains valid numeric input");
    expectException<field_conversion_error>([&] { FieldValue ignored(byte, 3.5); });
    expectException<field_conversion_error>([&] { FieldValue ignored(byte, std::numeric_limits<double>::infinity()); });
    expectException<field_conversion_error>([&] { FieldValue ignored(byte, std::numeric_limits<double>::quiet_NaN()); });
    require(FieldValue(byte, 3.0).getValue<uint8_t>() == 3, "Exact floating-point integer conversion");

    const MessageField signedWide("signed", "int64"), unsignedWide("unsigned", "uint64");
    require(FieldValue(signedWide, std::numeric_limits<int64_t>::min()).getValue<int64_t>() ==
            std::numeric_limits<int64_t>::min(), "INT64_MIN is preserved");
    require(FieldValue(unsignedWide, std::numeric_limits<uint64_t>::max()).getValue<uint64_t>() ==
            std::numeric_limits<uint64_t>::max(), "UINT64_MAX is preserved");
    expectException<field_conversion_error>([&] { FieldValue ignored(signedWide, uint64_t{1} << 63); });
    expectException<field_conversion_error>([&] { FieldValue ignored(unsignedWide, std::ldexp(1.0, 64)); });
    expectException<field_conversion_error>([&] { FieldValue ignored(signedWide, std::ldexp(1.0, 63)); });
    require(FieldValue(signedWide, -std::ldexp(1.0, 63)).getValue<int64_t>() ==
            std::numeric_limits<int64_t>::min(), "Floating-point lower bound is representable");
    const auto largestDoubleBelowUint64Limit = std::nextafter(std::ldexp(1.0, 64), 0.0);
    require(FieldValue(unsignedWide, largestDoubleBelowUint64Limit).getValue<uint64_t>() ==
            std::numeric_limits<uint64_t>::max() - 2047, "Safe conversion below 2^64");

    const MessageField real("altitude", "float");
    expectException<field_conversion_error>([&] { FieldValue ignored(real, std::numeric_limits<double>::max()); });
    const FieldValue altitude(real, 12.5f);
    require(altitude.getValueAs<double>() == 12.5, "Explicit widening read");
    const auto mismatch = expectException<std::bad_variant_access>([&] { (void)altitude.getValue<double>(); });
    require(mismatch.find("altitude") != std::string::npos && mismatch.find("float") != std::string::npos &&
            mismatch.find("double") != std::string::npos, "Strict getter supplies context and remains catch-compatible");
    expectException<field_conversion_error>([&] { (void)altitude.getValueAs<int>(); });
    expectException<field_type_mismatch>([&] {
      (void)FieldValue(MessageField("text", "string"), "12").getValueAs<int>();
    });
    require(std::isnan(FieldValue(real, std::numeric_limits<float>::quiet_NaN()).getValue<float>()),
            "Same-type floating-point values retain protocol support for NaN");
    expectException<field_conversion_error>([&] {
      FieldValue ignored(MessageField("array", "uint8[]"), std::vector<int>{1, 300, 2});
    });
    expectException<field_conversion_error>([&] {
      FieldValue ignored(MessageField("array", "int16[]"), std::vector<double>{1, 2.5});
    });

    tinyxml2::XMLDocument xml;
    xml.Parse("<message name='COMMAND' id='1'><field name='ac_id' type='uint8'/><field name='value' type='float'/></message>");
    Message command(MessageDefinition(xml.RootElement(), 2));
    command.setField("ac_id", 42);
    command.setSenderId(42);
    command.setReceiverId(255);
    command.setComponentId(7);
    expectException<field_conversion_error>([&] { command.setSenderId(300); });
    expectException<field_conversion_error>([&] { command.setReceiverId(-1); });
    expectException<field_conversion_error>([&] { command.setComponentId(256); });
    require(std::get<uint8_t>(command.getSenderId()) == 42 && command.getReceiverId() == 255 &&
            command.getComponentId() == 7, "Header ID conversions reject overflow without changing values");
    command.addField("value", 12.5); // Legacy setter obeys the same range checks.
    expectException<field_conversion_error>([&] { command.setField("ac_id", 300); });
    require(command.getField<uint8_t>("ac_id") == 42, "Invalid replacement leaves the previous value intact");
    require(command.getFieldAs<double>("value") == 12.5 && command.getFieldAs<int>(size_t{0}) == 42,
            "Converted message reads by name and index");
    return 0;
  } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
