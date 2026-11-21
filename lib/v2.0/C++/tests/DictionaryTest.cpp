#include "TestSupport.h"
#include <pprzlink/MessageDictionary.h>
#include <pprzlink/Message.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <unistd.h>

using namespace pprzlink;

namespace {
  MessageDictionary dictionary(const std::string &text)
  {
    tinyxml2::XMLDocument xml;
    require(xml.Parse(text.c_str()) == tinyxml2::XML_SUCCESS, "Well-formed XML fixture");
    return MessageDictionary(xml.RootElement());
  }

  void testInvalidDefinitions()
  {
    expectException<bad_message_file>([] { MessageDictionary invalid(nullptr); });
    for (const auto *xml : {
      "<wrong/>", "<configuration/>",
      "<protocol><msg_class id='1'/></protocol>",
      "<protocol><msg_class name='x'/></protocol>",
      "<protocol><msg_class name='x' id='16'/></protocol>",
      "<protocol><msg_class name='x' id='-1'/></protocol>",
      "<protocol><msg_class name='x' id='1garbage'/></protocol>",
      "<protocol><msg_class name='x' id='1'/><msg_class name='y' id='1'/></protocol>",
      "<protocol><msg_class name='x' id='1'/><msg_class name='x' id='2'/></protocol>"}) {
      expectException<bad_message_file>([&] { (void)dictionary(xml); });
    }
    for (const auto *messages : {
      "<message name='M'/>", "<message id='1'/>",
      "<message name='M' id='256'/>", "<message name='M' id='1x'/>",
      "<message name='M' id='1'/><message name='M' id='2'/>",
      "<message name='M' id='1'/><message name='N' id='1'/>",
      "<message name='M' id='1'><field name='x'/></message>",
      "<message name='M' id='1'><field type='float'/></message>",
      "<message name='M' id='1'><field name='x' type='float'/><field name='x' type='float'/></message>"}) {
      expectException<bad_message_file>([&] {
        (void)dictionary(std::string("<protocol><msg_class name='test' id='1'>") + messages +
                         "</msg_class></protocol>");
      });
    }
    const auto error = expectException<bad_message_file>([] {
      (void)dictionary("<protocol><msg_class name='test' id='1'><message name='M' id='1'>"
                       "<field name='altitude' type='floatjunk'/></message></msg_class></protocol>");
    });
    for (const auto *context : {"test", "M", "altitude", "floatjunk"}) {
      require(error.find(context) != std::string::npos, "XML error preserves class/message/field/type");
    }
    for (const auto *type : {"", "floatjunk", "uint8foo[2]", "float[", "float]", "float[0]",
                             "float[-1]", "float[2x]", "float[2]junk", "float[2][3]",
                             "double[18446744073709551615]", "float[999999999999999999999]"}) {
      expectException<bad_message_file>([&] { FieldType invalid(type); });
    }
    for (const auto *type : {"float", "char[]", "uint8[2]", "string", "double[100]"}) {
      require(FieldType(type).toString() == type, "Exact field type parsing and formatting");
    }
    expectException<std::logic_error>([] { (void)FieldType("float").getArraySize(); });
  }

  void testLookups()
  {
    const auto dict = dictionary("<configuration><protocol><msg_class NAME='test' ID='15'>"
      "<message NAME='M' ID='255'><field NAME='x' TYPE='uint8'/></message>"
      "<message name='M_REQ' id='0'/></msg_class></protocol></configuration>");
    const auto &def = dict.getDefinition(15, 255);
    require(def.getName() == "M" && def.getField(0).getName() == "x", "Uppercase attributes");
    require(dict.getMessageId("M") == std::pair(15, 255) && dict.getMessageName(15, 255) == "M",
            "Bidirectional message lookup");
    require(dict.getClassId("test") == 15 && dict.getClassName(15) == "test", "Bidirectional class lookup");
    require(dict.getMsgsForClass("test").size() == 2 && dict.getDefinition("M_REQ").isRequest(),
            "Class listing and request detection");
    expectException<no_such_message>([&] { (void)dict.getDefinition(15, 23); });
    expectException<no_such_class>([&] { (void)dict.getClassName(0); });
    Message message(def);
    expectException<field_has_no_value>([&] { (void)message.getRawValue("x"); });
    expectException<field_has_no_value>([&] { (void)message.getRawValue(size_t{0}); });
    expectException<no_such_field>([&] { (void)message.getRawValue("missing"); });
    for (const size_t index : {size_t{1}, std::numeric_limits<size_t>::max(), size_t{1} << 32}) {
      uint8_t output = 0;
      expectException<no_such_field>([&] { (void)def.getField(index); });
      expectException<no_such_field>([&] { (void)message.getField(index); });
      expectException<no_such_field>([&] { (void)message.getField<uint8_t>(index); });
      expectException<no_such_field>([&] { message.getField(index, output); });
      expectException<no_such_field>([&] { (void)message.getRawValue(index); });
    }
  }

  void testLinkModes()
  {
    using Mode = MessageDefinition::LinkMode;
    require(MessageDefinition{}.getLinkMode() == Mode::None, "An empty schema has no routing hint");
    const auto dict = dictionary("<protocol><msg_class name='test' id='1'>"
      "<message name='DEFAULT' id='1'/>"
      "<message name='TARGET' id='2' link='forwarded'/>"
      "<message name='BROADCAST' id='3' link='broadcasted'><field name='ac_id' type='uint8'/></message>"
      "<message name='UPPER' id='4' LINK='broadcasted'/>"
      "</msg_class></protocol>");
    require(dict.getDefinition("DEFAULT").getLinkMode() == Mode::None &&
            dict.getDefinition("TARGET").getLinkMode() == Mode::Forwarded &&
            dict.getDefinition("BROADCAST").getLinkMode() == Mode::Broadcasted &&
            dict.getDefinition("UPPER").getLinkMode() == Mode::Broadcasted,
            "Existing XML link attributes describe routing without a separate retry policy");
    Message message(dict.getDefinition("BROADCAST"));
    message.setReceiverId(255);
    message.setField("ac_id", 42);
    require(message.getDefinition().getLinkMode() == Mode::Broadcasted &&
            message.getDefinition().getNbFields() == 1 && message.getField<uint8_t>("ac_id") == 42,
            "Broadcast addressing preserves the application aircraft identifier after XML destruction");
  }

  void testFiles()
  {
    char path[] = "/tmp/pprzlink-xml-XXXXXX";
    const auto fd = mkstemp(path);
    require(fd >= 0, "Create XML test file");
    close(fd);
    struct Cleanup { const char *path; ~Cleanup() { unlink(path); } } cleanup{path};
    expectException<bad_message_file>([&] { MessageDictionary invalid{std::string(path)}; });
    { std::ofstream file(path); file << "<protocol><unclosed>"; }
    const auto error = expectException<bad_message_file>([&] { MessageDictionary invalid{std::string(path)}; });
    require(error.find(path) != std::string::npos, "Parse error includes filename");
    { std::ofstream file(path); file << "<configuration><protocol/></configuration>"; }
    MessageDictionary valid{std::string(path)};
    unlink(path);
    expectException<messages_file_not_found>([&] { MessageDictionary missing{std::string(path)}; });
  }
}

int main(int argc, char **argv)
{
  try {
    testInvalidDefinitions();
    testLookups();
    testLinkModes();
    testFiles();
    if (argc > 1) {
      MessageDictionary real(argv[1]);
      require(!real.getMsgsForClass("telemetry").empty(), "Repository XML definitions");
    }
    std::cout << "XML errors, strict types, dictionary lookup and field contracts passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
