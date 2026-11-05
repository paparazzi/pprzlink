#include "TestSupport.h"
#include <pprzlink/BinaryCodec.h>
#include <pprzlink/TextCodec.h>
#include <pprzlink/Message.h>
#include <pprzlink/PprzTransport.h>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <limits>
#include <locale>
#include <utility>

using namespace pprzlink;

namespace {
  struct CommaPunctuation : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
    char do_thousands_sep() const override { return '_'; }
    std::string do_grouping() const override { return "\3"; }
  };

  void require(bool condition, const char *description)
  {
    if (!condition) throw std::runtime_error(description);
  }

  template<class Exception, class Callback>
  void expectException(Callback callback)
  {
    try { callback(); }
    catch (const Exception&) { return; }
    throw std::runtime_error("Expected exception was not thrown");
  }

  template<class T>
  void checkBytes(const char *type, const T &input, const BytesBuffer &expected)
  {
    const MessageField definition("value", type);
    const FieldValue field(definition, input);
    BytesBuffer buffer{0xaa};
    require(binary::writeField(buffer, field) == expected.size(), "Encoded size");
    require(field.getByteSize() == expected.size(), "Field size");
    require(BytesBuffer(buffer.begin() + 1, buffer.end()) == expected, type);
    size_t offset = 1;
    const auto decoded = binary::readField(definition, buffer, offset);
    require(offset == buffer.size(), "Decode advances offset");
    require(decoded.getValue<T>() == input, "Decoded field value");
    require(decoded.getValue() == field.getValue(), "Stored alternative survives binary round trip");
    BytesBuffer wrapperBuffer;
    require(field.addToBuffer(wrapperBuffer) == expected.size() && wrapperBuffer == expected,
            "Existing FieldValue binary API");

    // Every truncation of a complete field must fail without advancing the cursor.
    for (size_t length = 0; length < expected.size(); ++length) {
      size_t cursor = 0;
      expectException<std::out_of_range>([&] {
        (void)binary::readField(definition, std::span(expected).first(length), cursor);
      });
      require(cursor == 0, "Failed read leaves offset unchanged");
    }
  }

  void testScalars()
  {
    checkBytes("char", 'A', {0x41});
    checkBytes("int8", int8_t{-2}, {0xfe});
    checkBytes("uint8", uint8_t{255}, {0xff});
    checkBytes("int16", int16_t{-2}, {0xfe, 0xff});
    checkBytes("uint16", uint16_t{0xabcd}, {0xcd, 0xab});
    checkBytes("int32", int32_t{-2147483647 - 1}, {0x00, 0x00, 0x00, 0x80});
    checkBytes("uint32", uint32_t{0x89abcdef}, {0xef, 0xcd, 0xab, 0x89});
    checkBytes("float", 1.0f, {0x00, 0x00, 0x80, 0x3f});
    checkBytes("double", -2.5, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xc0});
    checkBytes("string", std::string("a\0b", 3), {3, 'a', 0, 'b'});
    checkBytes("string", std::string{}, {0});

    // Binary floating-point values must preserve their bits, including NaN payloads.
    for (const uint32_t bits : {0x80000000u, 0x7f800000u, 0x7fc01234u}) {
      const FieldValue original(MessageField("f", "float"), std::bit_cast<float>(bits));
      BytesBuffer bytes;
      binary::writeField(bytes, original);
      size_t offset = 0;
      const auto decoded = binary::readField(original.getField(), bytes, offset);
      require(std::bit_cast<uint32_t>(decoded.getValue<float>()) == bits, "Preserve float bits");
    }
  }

  void testArrays()
  {
    checkBytes("char[]", std::vector<char>{'a', 'b'}, {2, 'a', 'b'});
    checkBytes("char[2]", std::vector<char>{'a', 'b'}, {'a', 'b'});
    checkBytes("int8[]", std::vector<int8_t>{-2, 127}, {2, 0xfe, 0x7f});
    checkBytes("uint8[]", std::vector<uint8_t>{0, 255}, {2, 0, 255});
    checkBytes("int16[]", std::vector<int16_t>{0x1234, -2}, {2, 0x34, 0x12, 0xfe, 0xff});
    checkBytes("uint16[2]", std::vector<uint16_t>{0xabcd, 0x9876}, {0xcd, 0xab, 0x76, 0x98});
    checkBytes("int32[]", std::vector<int32_t>{0x12345678, -2},
               {2, 0x78, 0x56, 0x34, 0x12, 0xfe, 0xff, 0xff, 0xff});
    checkBytes("uint32[1]", std::vector<uint32_t>{0x89abcdef}, {0xef, 0xcd, 0xab, 0x89});
    checkBytes("float[]", std::vector<float>{1.f, -2.5f}, {2, 0, 0, 0x80, 0x3f, 0, 0, 0x20, 0xc0});
    checkBytes("double[1]", std::vector<double>{1.0}, {0, 0, 0, 0, 0, 0, 0xf0, 0x3f});
    checkBytes("int32[]", std::vector<int32_t>{}, {0});

    BytesBuffer maxBytes(256, 0x7f);
    maxBytes[0] = 255;
    checkBytes("uint8[]", std::vector<uint8_t>(255, 0x7f), maxBytes);
    const FieldValue tooLong(MessageField("large", "uint8[]"), std::vector<uint8_t>(256));
    BytesBuffer buffer{42};
    expectException<std::length_error>([&] { binary::writeField(buffer, tooLong); });
    require(buffer == BytesBuffer{42}, "Rejected write leaves buffer unchanged");
    const FieldValue longString(MessageField("large", "string"), std::string(256, 'a'));
    expectException<std::length_error>([&] { (void)longString.getByteSize(); });

    const FieldValue strings(MessageField("strings", "string[]"), std::vector<std::string>{"a", "b"});
    require(std::holds_alternative<std::vector<std::string>>(strings.getValue()), "Typed string array");
    expectException<std::logic_error>([&] { binary::writeField(buffer, strings); });
    require(buffer == BytesBuffer{42}, "Unsupported representation leaves buffer unchanged");
    size_t offset = 99;
    expectException<std::out_of_range>([&] {
      (void)binary::readField(MessageField("byte", "uint8"), buffer, offset);
    });
    require(offset == 99, "Out-of-bounds offset remains unchanged");
  }

  void testValueInvariants()
  {
    static_assert(!std::default_initializable<FieldValue>);
    const FieldValue value(MessageField("number", "int16"), 42);
    require(std::holds_alternative<int16_t>(value.getValue()), "XML selects variant alternative");
    require(value.getValue<int16_t>() == 42, "Returning getter");
    expectException<std::bad_variant_access>([&] { (void)value.getValue<double>(); });
    expectException<std::bad_variant_access>([&] { (void)value.getValue<bool>(); });
    expectException<std::logic_error>([] { (void)FieldValue(MessageField("values", "int16[]"), 42); });
    expectException<std::logic_error>([] { (void)FieldValue(MessageField("value", "int16"), std::vector<int>{42}); });
    const FieldValue array(MessageField("values", "int16[]"), std::vector<int>{1, 2, 3});
    expectException<std::length_error>([&] { (void)array.getValue<std::array<int16_t, 2>>(); });
    expectException<std::length_error>([&] { (void)array.getValue<std::array<int16_t, 4>>(); });
  }

  void testText()
  {
    const FieldValue numbers(MessageField("numbers", "int8[]"), std::vector<int>{-1, 65});
    std::ostringstream ivy, debug;
    writeIvyField(ivy, numbers.getValue());
    writeDebugField(debug, numbers);
    require(ivy.str() == "-1,65", "Ivy numeric arrays");
    require(debug.str() == "{-1,65}", "Debug numeric arrays");
    std::ostringstream stream;
    stream.imbue(std::locale(std::locale::classic(), new CommaPunctuation));
    stream << std::scientific << std::setprecision(1);
    const auto flags = stream.flags();
    const auto locale = stream.getloc();
    writeIvyField(stream, FieldValue(MessageField("f", "float"), 123.6f));
    require(stream.str() == "123.6" && stream.flags() == flags && stream.precision() == 1 &&
            stream.getloc() == locale, "Ivy numbers ignore stream formatting and preserve its state");
    std::ostringstream integer;
    integer.imbue(locale);
    integer << std::hex << std::showbase;
    writeIvyField(integer, FieldValue(MessageField("n", "uint32"), 12345));
    require(integer.str() == "12345", "Ivy integers are decimal without locale grouping or stream prefixes");
    std::ostringstream words, empty, chars;
    writeIvyField(words, FieldValue(MessageField("text", "string"), "two words"));
    writeIvyField(empty, FieldValue(MessageField("text", "string"), ""));
    chars << FieldValue(MessageField("chars", "char[]"), "a b");
    require(words.str() == "\"two words\"" && empty.str() == "\"\"" && chars.str() == "\"a b\"",
            "Ivy quoting");
    require(numbers.getValue<std::vector<int8_t>>() == std::vector<int8_t>({-1, 65}), "Formatting preserves value");
  }

  void testMessages()
  {
    tinyxml2::XMLDocument xml;
    xml.Parse(R"(<protocol><msg_class name="test" id="1"><message name="TEST" id="1">
      <field name="number" type="uint32"/><field name="values" type="float[]"/>
      <field name="text" type="string"/></message></msg_class></protocol>)");
    MessageDictionary dictionary(xml.RootElement());
    const auto &def = dictionary.getDefinition("TEST");
    Message source(def);
    expectException<field_has_no_value>([&] { (void)source.getField<uint32_t>("number"); });
    expectException<no_such_field>([&] { (void)source.getField<int>("unknown"); });
    expectException<field_has_no_value>([&] { (void)source.getField("number"); });
    expectException<field_has_no_value>([&] { (void)source.getField(0); });
    expectException<no_such_field>([&] { (void)source.getField("unknown"); });
    expectException<no_such_field>([&] { (void)source.getField(def.getNbFields()); });
    source.addField("number", uint32_t{1});
    source.addField("number", uint32_t{0x89abcdef});
    expectException<std::logic_error>([&] { source.addField("number", std::vector<int>{1}); });
    require(source.getNbValues() == 1 && source.getField<uint32_t>("number") == 0x89abcdef,
            "Replacement never inserts an invalid default value");
    source.addField("values", std::vector<float>{1.0f, -2.5f});
    source.addField("text", "hello");
    require(source.getField<uint32_t>(size_t{0}) == 0x89abcdef, "Indexed returning getter");
    require(source.getField<std::array<float, 2>>("values") == std::array<float, 2>{1.f, -2.5f}, "Array getter");
    BytesBuffer bytes;
    for (size_t i = 0; i < def.getNbFields(); ++i) source.addFieldToBuffer(i, bytes);
    require(source.getByteSize() == bytes.size(), "Message binary size");
    Message decoded(def);
    size_t offset = 0;
    for (size_t i = 0; i < def.getNbFields(); ++i) decoded.addFieldFromBuffer(i, bytes, offset);
    require(offset == bytes.size(), "Decoded whole payload");
    require(decoded.getField<uint32_t>("number") == source.getField<uint32_t>("number"), "Message scalar");
    require(decoded.getField<std::vector<float>>("values") == source.getField<std::vector<float>>("values"), "Message array");
    require(decoded.getField<std::string>("text") == "hello", "Message string");
    // Dynamic access and typed reads must agree for every XML field type in this message.
    for (size_t i = 0; i < def.getNbFields(); ++i) {
      const auto &name = def.getField(i).getName();
      require(decoded.getField(name) == source.getField(i), "Dynamic field round trip by name/index");
      std::visit([&]<class T>(const T &value) {
        require(value == decoded.getField<T>(name), "Dynamic and typed access agree");
      }, decoded.getField(name));
    }
    require(decoded.toString() == "TEST [number=2309737967; values={1.000000,-2.500000}; text=hello]", "Message debug output");
    size_t badOffset = bytes.size() - 1;
    expectException<std::out_of_range>([&] { decoded.addFieldFromBuffer(0, bytes, badOffset); });
    require(badOffset == bytes.size() - 1 && decoded.getField<uint32_t>("number") == 0x89abcdef,
            "Failed field decode preserves the old value and offset");

    // Exercise the existing transport API with fragmented and consecutive packets.
    auto deviceOwner = std::make_unique<MemoryDevice>();
    auto &device = *deviceOwner;
    PprzTransport transport(std::move(deviceOwner), dictionary);
    ReceiptQueue inbox(transport);
    source.setSenderId(uint8_t{42});
    source.setReceiverId(7);
    source.setComponentId(3);
    require(transport.sendMessage(source) == bytes.size() + 8, "Framed message size");
    const auto packet = device.outgoing;
    require(BytesBuffer(packet.begin(), packet.begin() + 6) == BytesBuffer({0x99, 27, 42, 7, 0x31, 1}),
            "PprzLink v2 header");
    require(BytesBuffer(packet.begin() + 6, packet.end() - 2) == bytes, "Transport payload");
    device.incoming.assign(packet.begin(), packet.begin() + 4);
    require(!inbox.hasMessage(), "Wait for the rest of a packet");
    device.incoming.assign(packet.begin() + 4, packet.end());
    device.incoming.insert(device.incoming.end(), packet.begin(), packet.end());
    for (int i = 0; i < 2; ++i) {
      require(inbox.hasMessage(), "Complete packet available");
      const auto received = inbox.getMessage();
      require(received && received->toString() == source.toString(), "Transport round trip");
      require(std::get<uint8_t>(received->getSenderId()) == 42 && received->getReceiverId() == 7 &&
              received->getComponentId() == 3, "Transport addressing");
    }
    require(!inbox.hasMessage(), "Both packets consumed");
  }
}

int main()
{
  try {
    testScalars();
    testArrays();
    testValueInvariants();
    testText();
    testMessages();
    std::cout << "Binary byte fixtures, truncation, variant invariants, text and message APIs passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
