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

  void testRadioRetries()
  {
    require(MessageDefinition{}.getRadioRetries() == RadioRetries::Enabled,
            "An empty definition preserves the legacy radio retry default");
    const auto dict = dictionary("<protocol><msg_class name='test' id='1'>"
      "<message name='LEGACY' id='1'/>"
      "<message name='ENABLED' id='2' radio_retry='true'/>"
      "<message name='DISABLED' id='3' radio_retry='false'><field name='x' type='uint8'/></message>"
      "<message name='ONE' id='4' radio_retry='1'/>"
      "<message name='ZERO' id='5' radio_retry='0'/>"
      "<message name='UPPER' id='6' RADIO_RETRY='false'/>"
      "<message name='BOTH' id='7' radio_retry='false' RADIO_RETRY='true'/>"
      "</msg_class></protocol>");
    for (const auto *name : {"LEGACY", "ENABLED", "ONE"})
      require(dict.getDefinition(name).getRadioRetries() == RadioRetries::Enabled,
              "Missing/true/1 radio_retry enables the normal modem behavior");
    for (const auto *name : {"DISABLED", "ZERO", "UPPER", "BOTH"})
      require(dict.getDefinition(name).getRadioRetries() == RadioRetries::Disabled,
              "false/0 disables retries; lowercase takes precedence over uppercase");
    Message copied(dict.getDefinition("DISABLED"));
    require(copied.getDefinition().getRadioRetries() == RadioRetries::Disabled &&
            copied.getDefinition().getNbFields() == 1 && copied.getDefinition().getMinimumSize() == 1,
            "Message retains the hint after XML destruction without adding a payload field");
    for (const auto *value : {"", "enabled", "disabled", "yes", "TRUE", "2", "-1", "falsejunk"}) {
      const auto error = expectException<bad_message_file>([&] {
        (void)dictionary(std::string("<protocol><msg_class name='test' id='1'>") +
          "<message name='BAD' id='1' radio_retry='" + value + "'/></msg_class></protocol>");
      });
      for (const auto *context : {"test", "BAD", "radio_retry"})
        require(error.find(context) != std::string::npos,
                "Invalid retry values are rejected with class/message/attribute context");
    }
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
    testRadioRetries();
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
