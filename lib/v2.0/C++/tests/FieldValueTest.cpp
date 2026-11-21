#include <pprzlink/FieldValue.h>
#include <deque>
#include <forward_list>
#include <iostream>
#include <list>
#include <set>
#include <span>
#include <string_view>

using pprzlink::FieldValue;
using pprzlink::MessageField;

namespace {
  template<class T>
  concept CanReadField = requires(const FieldValue &field, T &output) {
    field.getValue(output);
  };

  template<class T>
  concept CanReturnField = requires(const FieldValue &field) {
    { field.getValue<T>() } -> std::same_as<T>;
  };

  struct ValueTypeOnly { using value_type = int; };
  struct OutputBuffer {
    using value_type = int16_t;
    std::vector<int16_t> values;
    void clear() { values.clear(); }
    void push_back(int16_t value) { values.push_back(value); }
  };

  // Check overload selection, including types previously rejected in function bodies.
  static_assert(std::constructible_from<FieldValue, const MessageField&, int>);
  static_assert(std::constructible_from<FieldValue, const MessageField&, double>);
  static_assert(std::constructible_from<FieldValue, const MessageField&, char>);
  static_assert(std::constructible_from<FieldValue, const MessageField&, bool>);
  static_assert(std::constructible_from<FieldValue, const MessageField&, std::string>);
  static_assert(std::constructible_from<FieldValue, const MessageField&, const char*>);
  static_assert(std::constructible_from<FieldValue, const MessageField&, const int*, size_t>);
  static_assert(std::constructible_from<FieldValue, const MessageField&, std::vector<int>>);
  static_assert(std::constructible_from<FieldValue, const MessageField&, std::array<int, 3>>);
  static_assert(std::constructible_from<FieldValue, const MessageField&, std::span<const int>>);
  static_assert(std::constructible_from<FieldValue, const MessageField&, std::set<int>>);
  static_assert(!std::constructible_from<FieldValue, const MessageField&, int*>);
  static_assert(!std::constructible_from<FieldValue, const MessageField&, ValueTypeOnly>);
  static_assert(!std::constructible_from<FieldValue, const MessageField&, std::forward_list<int>>);
  static_assert(CanReadField<int> && CanReadField<double> && CanReadField<char>);
  static_assert(CanReadField<std::string> && CanReadField<std::vector<int>>);
  static_assert(CanReadField<std::array<int, 3>> && CanReadField<std::list<int>>);
  static_assert(CanReadField<std::deque<int>>);
  static_assert(CanReadField<OutputBuffer>);
  static_assert(!CanReadField<ValueTypeOnly> && !CanReadField<std::set<int>>);
  static_assert(!CanReadField<std::span<int>> && CanReadField<std::string_view>);
  static_assert(CanReadField<std::span<const float>> && !CanReadField<std::span<const float, 3>>);
  static_assert(!CanReadField<std::span<const bool>> && !CanReadField<std::span<const volatile float>>);

  static_assert(CanReturnField<int> && CanReturnField<std::string>);
  static_assert(CanReturnField<std::array<int, 3>> && CanReturnField<OutputBuffer>);
  static_assert(!CanReturnField<ValueTypeOnly> && !CanReturnField<std::set<int>>);
  static_assert(!CanReturnField<std::span<int>> && CanReturnField<std::string_view>);
  static_assert(CanReturnField<std::span<const float>> && !CanReturnField<std::span<const float, 3>>);
  static_assert(!CanReturnField<std::span<const float, 0>>);
  static_assert(!CanReturnField<std::span<float, 3>> && !CanReturnField<std::span<const bool>>);

  void require(bool condition, const char *description)
  {
    if (!condition) throw std::runtime_error(description);
  }

  template<class Exception, class Callback>
  void expectException(Callback callback)
  {
    try {
      callback();
    } catch (const Exception&) {
      return;
    }
    throw std::runtime_error("Expected exception was not thrown");
  }

  template<class T>
  T read(const FieldValue &field)
  {
    T result{};
    field.getValue(result);
    return result;
  }

  void testStringViews()
  {
    const std::string words = "A string long enough to use allocated storage";
    const FieldValue text(MessageField("text", "string"), words);
    const auto view = text.getValue<std::string_view>();
    require(view == words && view.data() == std::get<std::string>(text.getValue()).data(),
            "String view borrows the stored string without a copy");
    std::string_view output;
    text.getValue<std::string_view>(output);
    require(output.data() == view.data() && output.size() == view.size(),
            "Explicit string view output borrows the same storage");

    const FieldValue chars(MessageField("label", "char[4]"), std::vector<char>{'T', '\0', 'S', 'T'});
    const auto charView = chars.getValue<std::string_view>();
    require(charView == std::string_view("T\0ST", 4) &&
            charView.data() == std::get<std::vector<char>>(chars.getValue()).data(),
            "Fixed character view preserves all bytes, including embedded NULs");
    chars.getValue(output);
    require(output.data() == charView.data() && output.size() == charView.size(),
            "Deduced string view output borrows character-array storage");

    const FieldValue dynamic(MessageField("chars", "char[]"), "abc");
    const auto dynamicView = dynamic.getValue<std::string_view>();
    require(dynamicView == "abc" &&
            dynamicView.data() == std::get<std::vector<char>>(dynamic.getValue()).data(),
            "Dynamic character array view uses its stored length without a terminator");
    const FieldValue embedded(MessageField("text", "string"), std::string("a\0b", 3));
    require(embedded.getValue<std::string_view>() == std::string_view("a\0b", 3),
            "String view preserves embedded NULs");
    const FieldValue emptyString(MessageField("text", "string"), std::string{});
    const FieldValue emptyChars(MessageField("chars", "char[]"), std::vector<char>{});
    require(emptyString.getValue<std::string_view>().empty() &&
            emptyChars.getValue<std::string_view>().empty(), "Empty string and character-array views");

    const FieldValue number(MessageField("number", "int16"), 123);
    expectException<pprzlink::field_type_mismatch>([&] { number.getValue(output); });
    require(output.data() == charView.data() && output.size() == charView.size(),
            "Rejected view read leaves the previous output unchanged");
    const FieldValue character(MessageField("character", "char"), 'x');
    const FieldValue bytes(MessageField("bytes", "uint8[]"), std::vector<uint8_t>{1, 2});
    const FieldValue strings(MessageField("strings", "string[]"), std::vector<std::string>{"abc"});
    expectException<pprzlink::field_type_mismatch>([&] { (void)character.getValue<std::string_view>(); });
    expectException<pprzlink::field_type_mismatch>([&] { (void)bytes.getValue<std::string_view>(); });
    expectException<pprzlink::field_type_mismatch>([&] { (void)strings.getValue<std::string_view>(); });
  }

  void testArrayViews()
  {
    const FieldValue axes(MessageField("axes", "float[3]"), std::array<float, 3>{1.f, 2.f, 3.f});
    const auto dynamic = axes.getValue<std::span<const float>>();
    require(dynamic.data() == std::get<std::vector<float>>(axes.getValue()).data() &&
            dynamic.size() == 3 && dynamic[0] == 1.f && dynamic[2] == 3.f,
            "Dynamic span borrows the complete stored array without a copy");
    std::span<const float> output;
    axes.getValue(output);
    require(output.data() == dynamic.data() && output.size() == dynamic.size(),
            "Dynamic span output borrows the same storage");

    const FieldValue numbers(MessageField("numbers", "uint16[]"), std::vector<uint16_t>{100, 200});
    const auto integers = numbers.getValue<std::span<const uint16_t>>();
    require(integers.data() == std::get<std::vector<uint16_t>>(numbers.getValue()).data() &&
            integers.size() == 2 && integers[1] == 200, "Span supports other exact array element types");
    const FieldValue empty(MessageField("empty", "float[]"), std::vector<float>{});
    require(empty.getValue<std::span<const float>>().empty(), "Empty array span");
    expectException<pprzlink::field_type_mismatch>([&] { (void)axes.getValue<std::span<const double>>(); });
    const FieldValue scalar(MessageField("scalar", "float"), 1.f);
    expectException<pprzlink::field_type_mismatch>([&] { scalar.getValue(output); });
    require(output.data() == dynamic.data() && output.size() == 3,
            "Type mismatch leaves the previous span output unchanged");
    const FieldValue text(MessageField("text", "string"), "abc");
    expectException<pprzlink::field_type_mismatch>([&] { (void)text.getValue<std::span<const char>>(); });
  }
}

int main()
{
  try {
    testStringViews();
    testArrayViews();
    const MessageField integer("number", "int16");
    const MessageField real("real", "double");
    const MessageField text("text", "string");
    const MessageField chars("chars", "char[]");
    const MessageField numbers("numbers", "int16[]");
    const MessageField fixed("fixed", "int16[3]");

    require(read<int16_t>(FieldValue(integer, -123)) == -123, "Integer input");
    require(read<double>(FieldValue(real, 1.25f)) == 1.25, "Floating-point input");
    require(read<uint8_t>(FieldValue(MessageField("flag", "uint8"), true)) == 1, "Bool input");
    require(read<char>(FieldValue(MessageField("char", "char"), 'x')) == 'x', "Char input");
    const FieldValue number(integer, 123);
    expectException<std::bad_variant_access>([&] { (void)read<double>(number); });

    const std::string words = "two words";
    require(read<std::string>(FieldValue(text, words)) == words, "std::string input");
    require(read<std::string>(FieldValue(text, "literal")) == "literal", "String literal input");
    char mutableText[] = "buffer";
    require(read<std::string>(FieldValue(text, mutableText)) == "buffer", "Mutable C string input");
    const FieldValue empty(text, std::string{});
    std::string explicitResult;
    empty.getValue<std::string>(explicitResult);
    require(explicitResult.empty(), "Explicit string getter template");
    require(read<std::vector<char>>(FieldValue(chars, "abc")) == std::vector<char>({'a', 'b', 'c'}),
            "C string to char array");
    require(read<std::vector<char>>(FieldValue(chars, std::string_view("abc"))) ==
            std::vector<char>({'a', 'b', 'c'}), "Character view keeps array semantics");
    expectException<std::logic_error>([&] { (void)FieldValue(integer, words); });

    const int raw[] = {1, -2, 3};
    const std::vector<int16_t> expected = {1, -2, 3};
    require(read<std::vector<int16_t>>(FieldValue(numbers, raw, 3)) == expected, "Pointer and length input");
    require(read<std::vector<int16_t>>(FieldValue(numbers, std::span(raw))) == expected, "Span input");
    require(read<std::vector<int16_t>>(FieldValue(numbers, std::list<int>{1, -2, 3})) == expected, "List input");
    const FieldValue sequence(numbers, std::vector<int>{1, -2, 3});
    std::vector<int16_t> replaced{99};
    sequence.getValue(replaced);
    require(replaced == expected, "Vector output replaces its previous contents");
    require(sequence.getValue<std::vector<int16_t>>() == expected, "Return vector value");
    require(sequence.getValue<std::array<int16_t, 3>>() == std::array<int16_t, 3>{1, -2, 3},
            "Return fixed array value");
    require(number.getValue<int16_t>() == 123, "Return scalar value");
    require(read<OutputBuffer>(sequence).values == expected, "Output requires no iteration or size");
    require(read<std::list<int16_t>>(sequence) == std::list<int16_t>({1, -2, 3}), "List output");
    require(read<std::deque<int16_t>>(sequence) == std::deque<int16_t>({1, -2, 3}), "Deque output");
    require(read<std::array<int16_t, 3>>(sequence) == std::array<int16_t, 3>{1, -2, 3}, "Fixed array output");
    require(read<std::vector<int16_t>>(FieldValue(fixed, std::array<int, 3>{1, -2, 3})) == expected,
            "Fixed array input");
    require(read<std::vector<int16_t>>(FieldValue(numbers, std::set<int>{1, 2})) == std::vector<int16_t>({1, 2}),
            "Read-only container input");
    require(read<std::vector<int16_t>>(FieldValue(numbers, std::vector<int>{})).empty(), "Empty array");
    require(read<std::vector<uint8_t>>(FieldValue(MessageField("flags", "uint8[]"), std::vector<bool>{true, false})) ==
            std::vector<uint8_t>({1, 0}), "Proxy container input");
    expectException<std::logic_error>([&] { (void)FieldValue(fixed, std::vector<int>{1}); });
    expectException<std::logic_error>([&] { (void)FieldValue(integer, raw, 3); });
    std::cout << "FieldValue overloads, conversions and error behavior passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
