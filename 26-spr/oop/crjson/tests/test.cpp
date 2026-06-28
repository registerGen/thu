// MIT License
//
// Copyright (c) 2026 registerGen
//
// Tests for crjson library

#include <catch2/catch_all.hpp>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "crjson.h"

using namespace crjson;
using namespace std::string_view_literals;

struct Point {
  double x, y;
};

template <>
struct crjson::jconvert<Point> {
  template <typename VA>
  static constexpr Point from(VA const& a) {
    return {a["x"].as_num(), a["y"].as_num()};
  }

  template <crjson::detail::builder Builder>
  static constexpr void to(Builder& b, Point const& p) {
    b.begin_obj();
    b.key("x");
    jconvert<double>::to(b, p.x);
    b.key("y");
    jconvert<double>::to(b, p.y);
    b.end_obj();
  }
};

struct TaggedNumber {
  std::string tag;
  double value;
};

template <>
struct crjson::jconvert<TaggedNumber> {
  template <typename VA>
  static TaggedNumber from(VA const& a) {
    return {std::string{a["tag"].as_str()}, a["value"].as_num()};
  }

  template <crjson::detail::builder Builder>
  static constexpr void to(Builder& b, TaggedNumber const& t) {
    b.begin_obj();
    b.key("tag");
    jconvert<std::string>::to(b, t.tag);
    b.key("value");
    jconvert<double>::to(b, t.value);
    b.end_obj();
  }
};

namespace {

std::string make_obj(int n) {
  std::string s = "{";
  for (int i = 0; i < n; ++i) {
    if (i > 0) s += ',';
    s += '"';
    s += "key" + std::to_string(i);
    s += "\":";
    s += std::to_string(i);
  }
  s += '}';
  return s;
}

}  // namespace

TEST_CASE("runtime parsing basics") {
  SECTION("null and booleans") {
    auto doc_null = parse{R"(null)"};
    REQUIRE(doc_null.root().type() == jtype::null_t);
    REQUIRE(doc_null.root().as_null() == nullptr);

    auto doc_true = parse{R"(true)"};
    REQUIRE(doc_true.root().type() == jtype::bool_t);
    REQUIRE(doc_true.root().as_bool() == true);

    auto doc_false = parse{R"(false)"};
    REQUIRE(doc_false.root().type() == jtype::bool_t);
    REQUIRE(doc_false.root().as_bool() == false);
  }

  SECTION("numbers basic forms") {
    auto doc_zero = parse{R"(0)"};
    REQUIRE(doc_zero.root().type() == jtype::num_t);
    REQUIRE(doc_zero.root().as_num() == Catch::Approx(0.0));

    auto doc_int = parse{R"(42)"};
    REQUIRE(doc_int.root().as_num() == Catch::Approx(42.0));

    auto doc_neg = parse{R"(-123)"};
    REQUIRE(doc_neg.root().as_num() == Catch::Approx(-123.0));

    auto doc_dec = parse{R"(3.14159)"};
    REQUIRE(doc_dec.root().as_num() == Catch::Approx(3.14159));

    auto doc_neg_dec = parse{R"(-2.718)"};
    REQUIRE(doc_neg_dec.root().as_num() == Catch::Approx(-2.718));

    auto doc_exp_pos = parse{R"(1e10)"};
    REQUIRE(doc_exp_pos.root().as_num() == Catch::Approx(1e10));

    auto doc_exp_neg = parse{R"(1e-5)"};
    REQUIRE(doc_exp_neg.root().as_num() == Catch::Approx(1e-5));

    auto doc_exp_plus = parse{R"(2.5e+3)"};
    REQUIRE(doc_exp_plus.root().as_num() == Catch::Approx(2500.0));

    auto doc_large_exp = parse{R"(1.23E100)"};
    REQUIRE(doc_large_exp.root().as_num() == Catch::Approx(1.23e100));

    auto doc_dec_exp = parse{R"(6.022e23)"};
    REQUIRE(doc_dec_exp.root().as_num() == Catch::Approx(6.022e23));
  }

  SECTION("strings basic") {
    auto doc_empty = parse{R"("")"};
    REQUIRE(doc_empty.root().type() == jtype::str_t);
    REQUIRE(doc_empty.root().as_str() == ""sv);

    auto doc_simple = parse{R"("hello")"};
    REQUIRE(doc_simple.root().as_str() == "hello"sv);

    auto doc_spaces = parse{R"("hello world")"};
    REQUIRE(doc_spaces.root().as_str() == "hello world"sv);
  }

  SECTION("string escapes") {
    auto doc_quote = parse{R"("say \"hi\"")"};
    REQUIRE(doc_quote.root().as_str() == R"(say "hi")"sv);

    auto doc_backslash = parse{R"("path\\to\\file")"};
    REQUIRE(doc_backslash.root().as_str() == R"(path\to\file)"sv);

    auto doc_slash = parse{R"("a\/b")"};
    REQUIRE(doc_slash.root().as_str() == "a/b"sv);

    auto doc_b = parse{R"("bell\b")"};
    REQUIRE(doc_b.root().as_str() == "bell\b"sv);

    auto doc_f = parse{R"("form\ffeed")"};
    REQUIRE(doc_f.root().as_str() == "form\ffeed"sv);

    auto doc_n = parse{R"("new\nline")"};
    REQUIRE(doc_n.root().as_str() == "new\nline"sv);

    auto doc_r = parse{R"("carriage\rreturn")"};
    REQUIRE(doc_r.root().as_str() == "carriage\rreturn"sv);

    auto doc_t = parse{R"("tab\there")"};
    REQUIRE(doc_t.root().as_str() == "tab\there"sv);
  }

  SECTION("arrays basic") {
    auto doc_empty = parse{R"([])"};
    REQUIRE(doc_empty.root().type() == jtype::arr_t);
    REQUIRE(doc_empty.root().size() == 0);

    auto doc_single = parse{R"([42])"};
    REQUIRE(doc_single.root().size() == 1);
    REQUIRE(doc_single.root()[0].as_num() == Catch::Approx(42.0));

    auto doc_multi = parse{R"([1, 2, 3, 4, 5])"};
    REQUIRE(doc_multi.root().size() == 5);
    for (std::size_t i = 0; i < 5; ++i) {
      REQUIRE(doc_multi.root()[i].as_num() == Catch::Approx(static_cast<double>(i + 1)));
    }

    auto doc_mixed = parse{R"([null, true, 42, "hello"])"};
    REQUIRE(doc_mixed.root().size() == 4);
    REQUIRE(doc_mixed.root()[0].type() == jtype::null_t);
    REQUIRE(doc_mixed.root()[1].type() == jtype::bool_t);
    REQUIRE(doc_mixed.root()[2].type() == jtype::num_t);
    REQUIRE(doc_mixed.root()[3].type() == jtype::str_t);
  }

  SECTION("arrays nested") {
    auto doc_nested = parse{R"([[1, 2], [3, 4]])"};
    REQUIRE(doc_nested.root().size() == 2);
    REQUIRE(doc_nested.root()[0].size() == 2);
    REQUIRE(doc_nested.root()[1].size() == 2);
    REQUIRE(doc_nested.root()[0][0].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_nested.root()[0][1].as_num() == Catch::Approx(2.0));
    REQUIRE(doc_nested.root()[1][0].as_num() == Catch::Approx(3.0));
    REQUIRE(doc_nested.root()[1][1].as_num() == Catch::Approx(4.0));

    auto doc_deep = parse{R"([[[[[1]]]]])"};
    REQUIRE(doc_deep.root()[0][0][0][0][0].as_num() == 1.0);
  }

  SECTION("objects basic") {
    auto doc_empty = parse{R"({})"};
    REQUIRE(doc_empty.root().type() == jtype::obj_t);
    REQUIRE(doc_empty.root().size() == 0);

    auto doc_single = parse{R"({"key": "value"})"};
    REQUIRE(doc_single.root().size() == 1);
    REQUIRE(doc_single.root()["key"].as_str() == "value"sv);

    auto doc_multi = parse{R"({"a": 1, "b": 2, "c": 3})"};
    REQUIRE(doc_multi.root().size() == 3);
    REQUIRE(doc_multi.root()["a"].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_multi.root()["b"].as_num() == Catch::Approx(2.0));
    REQUIRE(doc_multi.root()["c"].as_num() == Catch::Approx(3.0));

    auto doc_mixed = parse{R"({"null": null, "bool": true, "num": 42, "str": "text"})"};
    REQUIRE(doc_mixed.root()["null"].type() == jtype::null_t);
    REQUIRE(doc_mixed.root()["bool"].as_bool() == true);
    REQUIRE(doc_mixed.root()["num"].as_num() == Catch::Approx(42.0));
    REQUIRE(doc_mixed.root()["str"].as_str() == "text"sv);

    auto doc_nested = parse{R"({"outer": {"inner": 42}})"};
    REQUIRE(doc_nested.root()["outer"]["inner"].as_num() == Catch::Approx(42.0));

    auto doc_arr = parse{R"({"array": [1, 2, 3]})"};
    REQUIRE(doc_arr.root()["array"].size() == 3);
    REQUIRE(doc_arr.root()["array"][0].as_num() == Catch::Approx(1.0));
  }

  SECTION("whitespace handling") {
    auto doc_before = parse{R"(   42)"};
    REQUIRE(doc_before.root().as_num() == Catch::Approx(42.0));

    auto doc_after = parse{R"(42   )"};
    REQUIRE(doc_after.root().as_num() == Catch::Approx(42.0));

    auto doc_around = parse{R"(   42   )"};
    REQUIRE(doc_around.root().as_num() == Catch::Approx(42.0));

    auto doc_array = parse{R"([ 1 , 2 , 3 ])"};
    REQUIRE(doc_array.root().size() == 3);

    auto doc_obj = parse{R"({ "key" : "value" })"};
    REQUIRE(doc_obj.root()["key"].as_str() == "value"sv);
  }

  SECTION("complex document") {
    auto doc = parse{R"({
      "name": "Ada Lovelace",
      "age": 36,
      "programmer": true,
      "contributions": ["Analytical Engine", "First Algorithm"],
      "address": {
        "city": "London",
        "country": "England"
      }
    })"};
    REQUIRE(doc.root()["name"].as_str() == "Ada Lovelace"sv);
    REQUIRE(doc.root()["age"].as_num() == Catch::Approx(36.0));
    REQUIRE(doc.root()["programmer"].as_bool() == true);
    REQUIRE(doc.root()["contributions"].size() == 2);
    REQUIRE(doc.root()["address"]["city"].as_str() == "London"sv);
  }
}

TEST_CASE("runtime unicode and UTF-8") {
  SECTION("unicode escapes ASCII") {
    auto doc_ascii = parse{R"("\u0041")"};
    REQUIRE(doc_ascii.root().as_str() == "A"sv);

    auto doc_digit = parse{R"("\u0030")"};
    REQUIRE(doc_digit.root().as_str() == "0"sv);

    auto doc_max = parse{R"("\u007F")"};
    REQUIRE(doc_max.root().as_str().size() == 1);
    REQUIRE(doc_max.root().as_str()[0] == '\x7F');
  }

  SECTION("unicode escapes lowercase hex and BMP") {
    auto doc_lower = parse{R"("\u00ab")"};
    REQUIRE(doc_lower.root().as_str() == "\xc2\xab"sv);

    auto doc_2byte = parse{R"("\u0080")"};
    REQUIRE(doc_2byte.root().as_str().size() == 2);
    REQUIRE(static_cast<unsigned char>(doc_2byte.root().as_str()[0]) == 0xC2);
    REQUIRE(static_cast<unsigned char>(doc_2byte.root().as_str()[1]) == 0x80);

    auto doc_latin1 = parse{R"("\u00E9")"};
    REQUIRE(doc_latin1.root().as_str() == "\xC3\xA9"sv);

    auto doc_3byte = parse{R"("\u4E2D")"};
    REQUIRE(doc_3byte.root().as_str() == "\xE4\xB8\xAD"sv);

    auto doc_surrogate = parse{R"("\uD83D\uDE00")"};
    REQUIRE(doc_surrogate.root().as_str() == "\xF0\x9F\x98\x80"sv);

    auto doc_pua = parse{R"("\uE001")"};
    REQUIRE(doc_pua.root().as_str() == "\xEE\x80\x81"sv);
  }

  SECTION("raw UTF-8 valid") {
    auto doc_raw2 = parse{"\"\xC3\xA9\""};
    REQUIRE(doc_raw2.root().as_str() == "\xC3\xA9"sv);

    auto doc_raw3 = parse{"\"\xE4\xB8\xAD\""};
    REQUIRE(doc_raw3.root().as_str() == "\xE4\xB8\xAD"sv);

    auto doc_f1 = parse{"\"\xF1\x80\x80\x80\""};
    REQUIRE(doc_f1.root().as_str() == "\xF1\x80\x80\x80"sv);

    auto doc_e0 = parse{"\"\xE0\xA0\x80\""};
    REQUIRE(doc_e0.root().as_str() == "\xE0\xA0\x80");

    auto doc_ed = parse{"\"\xED\x9F\xBF\""};
    REQUIRE(doc_ed.root().as_str() == "\xED\x9F\xBF");

    auto doc_ee = parse{"\"\xEE\x80\x80\""};
    REQUIRE(doc_ee.root().as_str() == "\xEE\x80\x80");

    auto doc_f0 = parse{"\"\xF0\x90\x80\x80\""};
    REQUIRE(doc_f0.root().as_str() == "\xF0\x90\x80\x80");

    auto doc_f4 = parse{"\"\xF4\x8F\xBF\xBF\""};
    REQUIRE(doc_f4.root().as_str() == "\xF4\x8F\xBF\xBF");
  }

  SECTION("unicode escape errors") {
    REQUIRE_THROWS(parse{R"("\uGGGG")"});
    REQUIRE_THROWS(parse{R"("\u00g0")"});
    REQUIRE_THROWS(parse{R"("\u00:0")"});
    REQUIRE_THROWS(parse{"\"\\u\t012\""});
  }

  SECTION("surrogate errors") {
    REQUIRE_THROWS(parse{"\"\\uD83D\""});
    REQUIRE_THROWS(parse{"\"\\uDE00\""});
    REQUIRE_THROWS(parse{"\"\\uD83D\\u0041\""});
    REQUIRE_THROWS(parse{"\"\\uD83D\\n\""});
    REQUIRE_THROWS(parse{"\"\\uD83D\\uE000\""});
  }

  SECTION("raw UTF-8 errors") {
    REQUIRE_THROWS(parse{"\"\xFF\""});
    REQUIRE_THROWS(parse{"\"\x80\""});
    REQUIRE_THROWS(parse{"\"\xC3\""});
    REQUIRE_THROWS(parse{"\"\xC3\x41\""});
    REQUIRE_THROWS(parse{"\"\xE0\x80\xA1\""});
    REQUIRE_THROWS(parse{"\"\xC1\x81\""});
    REQUIRE_THROWS(parse{"\"\xED\xA0\x80\""});
    REQUIRE_THROWS(parse{"\"\xED\xBF\xBF\""});
    REQUIRE_THROWS(parse{"\"\xF0\x80\x80\x80\""});
    REQUIRE_THROWS(parse{"\"\xF4\x90\x80\x80\""});
    REQUIRE_THROWS(parse{"\"\xF5\x80\x80\x80\""});
  }

  SECTION("string control character") { REQUIRE_THROWS(parse{"\"\x01\""}); }
}

TEST_CASE("runtime parse errors") {
  SECTION("empty, invalid, trailing") {
    REQUIRE_THROWS(parse{""});
    REQUIRE_THROWS(parse{"invalid"});
    REQUIRE_THROWS(parse{"42 extra"});
  }

  SECTION("string termination and escape errors") {
    REQUIRE_THROWS(parse{R"("unterminated)"});
    REQUIRE_THROWS(parse{"\"hello\\"});
    REQUIRE_THROWS(parse{"\"\\x\""});
  }

  SECTION("array and object structural errors") {
    REQUIRE_THROWS(parse{"[1, 2, 3"});
    REQUIRE_THROWS(parse{"[1 2 3]"});
    REQUIRE_THROWS(parse{"[1, 2, 3,]"});
    REQUIRE_THROWS(parse{R"({"key": "value")"});
    REQUIRE_THROWS(parse{R"({"key" "value"})"});
    REQUIRE_THROWS(parse{R"({"a": 1 "b": 2})"});
    REQUIRE_THROWS(parse{R"({"key": "value",})"});
    REQUIRE_THROWS(parse{R"({42: "value"})"});
  }

  SECTION("malformed structures") {
    REQUIRE_THROWS(parse{R"([{"a":1}{"b":2}])"});
    REQUIRE_THROWS(parse{R"({"a":[1,2})"});
    REQUIRE_THROWS(parse{R"([{"a":1])"});
    REQUIRE_THROWS(parse{"[[[1, 2]"});
    REQUIRE_THROWS(parse{R"({"a": {"b": 1)"});
    REQUIRE_THROWS(parse{"[1,, 2]"});
    REQUIRE_THROWS(parse{R"({"a": 1,, "b": 2})"});
    REQUIRE_THROWS(parse{"[1: 2]"});
    REQUIRE_THROWS(parse{R"({"key" = "value"})"});
    REQUIRE_THROWS(parse{"'string'"});
    REQUIRE_THROWS(parse{"{key: 'value'}"});
  }

  SECTION("incomplete literals") {
    REQUIRE_THROWS(parse{"nul"});
    REQUIRE_THROWS(parse{"tru"});
    REQUIRE_THROWS(parse{"fals"});
    REQUIRE_THROWS(parse{"-"});
    REQUIRE_THROWS(parse{"."});
    REQUIRE_THROWS(parse{"e"});
  }
}

TEST_CASE("runtime access errors") {
  SECTION("missing key and bounds") {
    auto doc = parse{R"({"key": "value"})"};
    REQUIRE_THROWS(doc.root()["nonexistent"]);

    auto doc_arr = parse{R"([1, 2, 3])"};
    REQUIRE_THROWS(doc_arr.root()[10]);
  }

  SECTION("type mismatch conversions") {
    auto doc_num = parse{R"(42)"};
    REQUIRE_THROWS(doc_num.root().as_bool());
    REQUIRE_THROWS(doc_num.root().as_str());
    REQUIRE_THROWS(doc_num.root().as_null());

    auto doc_str = parse{R"("string")"};
    REQUIRE_THROWS(doc_str.root().as_num());
  }

  SECTION("access on wrong types") {
    auto doc_obj = parse{R"({"key": "value"})"};
    REQUIRE_THROWS(doc_obj.root()[0]);

    auto doc_arr = parse{R"([1, 2, 3])"};
    REQUIRE_THROWS(doc_arr.root()["key"]);

    auto doc_scalar = parse{R"(42)"};
    REQUIRE_THROWS(doc_scalar.root().size());
  }
}

TEST_CASE("runtime schema success") {
  SECTION("scalar schemas") {
    bind<jnull> doc_null{R"(null)"};
    REQUIRE(doc_null.root().as_null() == nullptr);

    bind<jbool> doc_bool{R"(true)"};
    REQUIRE(doc_bool.root().as_bool() == true);

    bind<jnum> doc_num{R"(42.5)"};
    REQUIRE(doc_num.root().as_num() == Catch::Approx(42.5));

    bind<jstr> doc_str{R"("hello")"};
    REQUIRE(doc_str.root().as_str() == "hello"sv);
  }

  SECTION("array schemas") {
    using num_schema = jarr<jnum>;
    bind<num_schema> doc_num{R"([1, 2, 3])"};
    REQUIRE(doc_num.root().size() == 3);
    REQUIRE(doc_num.root()[0].as_num() == Catch::Approx(1.0));

    using str_schema = jarr<jstr>;
    bind<str_schema> doc_str{R"(["a", "b", "c"])"};
    REQUIRE(doc_str.root()[1].as_str() == "b"sv);

    using nested_schema = jarr<jarr<jnum>>;
    bind<nested_schema> doc_nested{R"([[1, 2], [3, 4]])"};
    REQUIRE(doc_nested.root()[0][1].as_num() == Catch::Approx(2.0));
  }

  SECTION("object schemas") {
    using simple_schema = jobj<jkv<"name"_fs, jstr>, jkv<"age"_fs, jnum>>;
    bind<simple_schema> doc_simple{R"({"name": "Alice", "age": 30})"};
    REQUIRE(doc_simple.root()["name"].as_str() == "Alice"sv);
    REQUIRE(doc_simple.root()["age"].as_num() == Catch::Approx(30.0));

    using inner_schema = jobj<jkv<"city"_fs, jstr>>;
    using outer_schema = jobj<jkv<"name"_fs, jstr>, jkv<"address"_fs, inner_schema>>;
    bind<outer_schema> doc_nested{R"({"name": "Bob", "address": {"city": "NYC"}})"};
    REQUIRE(doc_nested.root()["address"]["city"].as_str() == "NYC"sv);

    using arr_schema = jobj<jkv<"name"_fs, jstr>, jkv<"tags"_fs, jarr<jstr>>>;
    bind<arr_schema> doc_arr{R"({"name": "Ada", "tags": ["math", "logic"]})"};
    REQUIRE(doc_arr.root()["tags"][0].as_str() == "math"sv);

    using person_schema = jobj<
      jkv<"name"_fs, jstr>,
      jkv<"age"_fs, jnum>,
      jkv<"active"_fs, jbool>,
      jkv<"tags"_fs, jarr<jstr>>,
      jkv<"metadata"_fs, jobj<jkv<"id"_fs, jnum>>>>;

    bind<person_schema> doc_person{R"({
      "name": "Charles Babbage",
      "age": 79,
      "active": false,
      "tags": ["inventor", "mathematician"],
      "metadata": {"id": 123}
    })"};
    REQUIRE(doc_person.root()["name"].as_str() == "Charles Babbage"sv);
    REQUIRE(doc_person.root()["age"].as_num() == Catch::Approx(79.0));
    REQUIRE(doc_person.root()["active"].as_bool() == false);
    REQUIRE(doc_person.root()["tags"].size() == 2);
    REQUIRE(doc_person.root()["metadata"]["id"].as_num() == Catch::Approx(123.0));

    using ab_schema = jobj<jkv<"a"_fs, jnum>, jkv<"b"_fs, jnum>>;
    bind<ab_schema> doc_ab{R"({"a": 1, "b": 2})"};
    REQUIRE(doc_ab.root()["a"].as_num() == 1.0);
    REQUIRE(doc_ab.root()["b"].as_num() == 2.0);

    using req_schema = jobj<jkv<"required"_fs, jstr>>;
    bind<req_schema> doc_req{R"({"required": "ok"})"};
    REQUIRE(doc_req.root()["required"].as_str() == "ok"sv);
  }

  SECTION("tuple schemas") {
    using schema_basic = jtup<jnum, jstr, jbool>;
    bind<schema_basic> doc_basic{R"([42, "hello", true])"};
    REQUIRE(doc_basic.root().size() == 3);
    REQUIRE(doc_basic.root()[0].as_num() == Catch::Approx(42.0));
    REQUIRE(doc_basic.root()[1].as_str() == "hello"sv);
    REQUIRE(doc_basic.root()[2].as_bool() == true);

    using schema_single = jtup<jnum>;
    bind<schema_single> doc_single{R"([3.14])"};
    REQUIRE(doc_single.root()[0].as_num() == Catch::Approx(3.14));

    using schema_arr = jtup<jnum, jarr<jstr>>;
    bind<schema_arr> doc_arr{R"([1, ["a", "b"]])"};
    REQUIRE(doc_arr.root()[0].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_arr.root()[1][0].as_str() == "a"sv);

    using schema_obj = jtup<jstr, jobj<jkv<"x"_fs, jnum>>>;
    bind<schema_obj> doc_obj{R"(["origin", {"x": 10}])"};
    REQUIRE(doc_obj.root()[0].as_str() == "origin"sv);
    REQUIRE(doc_obj.root()[1]["x"].as_num() == Catch::Approx(10.0));

    using schema_tup = jtup<jnum, jtup<jstr, jbool>>;
    bind<schema_tup> doc_tup{R"([1, ["hi", false]])"};
    REQUIRE(doc_tup.root()[0].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_tup.root()[1][0].as_str() == "hi"sv);
    REQUIRE(doc_tup.root()[1][1].as_bool() == false);

    using schema_in_obj = jobj<jkv<"point"_fs, jtup<jnum, jnum>>>;
    bind<schema_in_obj> doc_in_obj{R"({"point": [3.0, 4.0]})"};
    REQUIRE(doc_in_obj.root()["point"][0].as_num() == Catch::Approx(3.0));
    REQUIRE(doc_in_obj.root()["point"][1].as_num() == Catch::Approx(4.0));

    using schema_in_arr = jarr<jtup<jnum, jstr>>;
    bind<schema_in_arr> doc_in_arr{R"([[1, "one"], [2, "two"], [3, "three"]])"};
    REQUIRE(doc_in_arr.root().size() == 3);
    REQUIRE(doc_in_arr.root()[0][0].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_in_arr.root()[1][1].as_str() == "two"sv);
    REQUIRE(doc_in_arr.root()[2][0].as_num() == Catch::Approx(3.0));

    using inner_tup = jtup<jarr<jnum>, jbool>;
    using schema_deep = jtup<jobj<jkv<"label"_fs, jstr>>, inner_tup>;
    bind<schema_deep> doc_deep{R"([{"label": "pt"}, [[1, 2, 3], true]])"};
    REQUIRE(doc_deep.root()[0]["label"].as_str() == "pt"sv);
    REQUIRE(doc_deep.root()[1][0][0].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_deep.root()[1][0][2].as_num() == Catch::Approx(3.0));
    REQUIRE(doc_deep.root()[1][1].as_bool() == true);

    using row = jtup<jobj<jkv<"name"_fs, jstr>>, jnum>;
    using schema_rows = jobj<jkv<"rows"_fs, jarr<row>>>;
    bind<schema_rows> doc_rows{R"({"rows": [[{"name": "a"}, 1], [{"name": "b"}, 2]]})"};
    REQUIRE(doc_rows.root()["rows"][0][0]["name"].as_str() == "a"sv);
    REQUIRE(doc_rows.root()["rows"][0][1].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_rows.root()["rows"][1][0]["name"].as_str() == "b"sv);
    REQUIRE(doc_rows.root()["rows"][1][1].as_num() == Catch::Approx(2.0));

    using schema_three = jtup<jnum, jnum, jnum>;
    bind<schema_three> doc_three{R"([1, 2, 3])"};
    REQUIRE(doc_three.root()[0].as_num() == 1.0);
    REQUIRE(doc_three.root()[1].as_num() == 2.0);
    REQUIRE(doc_three.root()[2].as_num() == 3.0);
  }
}

TEST_CASE("runtime schema failures") {
  SECTION("wrong root types") {
    using schema_num = jnum;
    REQUIRE_THROWS(bind<schema_num>{R"("string")"});
    REQUIRE_THROWS(bind<jnull>{R"(42)"});
    REQUIRE_THROWS(bind<jbool>{R"(42)"});
    REQUIRE_THROWS(bind<jarr<jnum>>{R"(42)"});
    REQUIRE_THROWS((bind<jobj<jkv<"x"_fs, jnum>>>{R"(42)"}));
  }

  SECTION("array element type") {
    using schema = jarr<jnum>;
    REQUIRE_THROWS(bind<schema>{R"([1, "string", 3])"});
  }

  SECTION("object arity and missing keys") {
    using schema_ab = jobj<jkv<"a"_fs, jnum>, jkv<"b"_fs, jnum>>;
    REQUIRE_THROWS(bind<schema_ab>{R"({"a": 1})"});

    using schema_req = jobj<jkv<"required"_fs, jstr>>;
    REQUIRE_THROWS(bind<schema_req>{R"({"wrong": "value"})"});

    using schema_name_addr =
      jobj<jkv<"name"_fs, jstr>, jkv<"address"_fs, jobj<jkv<"city"_fs, jstr>>>>;
    REQUIRE_THROWS(bind<schema_name_addr>{R"({"name": "Alice"})"});

    using schema_person = jobj<
      jkv<"name"_fs, jstr>,
      jkv<"age"_fs, jnum>,
      jkv<"active"_fs, jbool>,
      jkv<"tags"_fs, jarr<jstr>>,
      jkv<"metadata"_fs, jobj<jkv<"id"_fs, jnum>>>>;
    REQUIRE_THROWS(bind<schema_person>{R"({"name": "Bob", "age": 30})"});

    using schema_name_tags = jobj<jkv<"name"_fs, jstr>, jkv<"tags"_fs, jarr<jstr>>>;
    REQUIRE_THROWS(bind<schema_name_tags>{R"({"name": "x"})"});

    using schema_name_age = jobj<jkv<"name"_fs, jstr>, jkv<"age"_fs, jnum>>;
    REQUIRE_THROWS(bind<schema_name_age>{R"({"name": "x"})"});
  }

  SECTION("object extra keys and value type errors") {
    using schema_req = jobj<jkv<"required"_fs, jstr>>;
    REQUIRE_THROWS(bind<schema_req>{R"({"required": "ok", "extra": "x"})"});

    using schema_point = jobj<jkv<"point"_fs, jtup<jnum, jnum>>>;
    REQUIRE_THROWS(bind<schema_point>{R"({"point": [1, 2], "extra": 0})"});

    using schema_v = jobj<jkv<"v"_fs, jnum>>;
    REQUIRE_THROWS(bind<schema_v>{R"({"v": 1, "extra": 2})"});

    using schema_id = jobj<jkv<"id"_fs, jnum>>;
    REQUIRE_THROWS(bind<schema_id>{R"({"id": 1, "extra": 2})"});

    using schema_city = jobj<jkv<"city"_fs, jstr>>;
    REQUIRE_THROWS(bind<schema_city>{R"({"city": "x", "extra": "y"})"});

    using schema_label = jobj<jkv<"label"_fs, jstr>>;
    REQUIRE_THROWS(bind<schema_label>{R"({"label": "x", "extra": "y"})"});

    using schema_x = jobj<jkv<"x"_fs, jnum>>;
    REQUIRE_THROWS(bind<schema_x>{R"({"x": 1, "extra": 2})"});

    using schema_name = jobj<jkv<"name"_fs, jstr>>;
    REQUIRE_THROWS(bind<schema_name>{R"({"name": "x", "extra": "y"})"});

    using schema_tags = jobj<jkv<"tags"_fs, jarr<jstr>>>;
    REQUIRE_THROWS(bind<schema_tags>{R"({"tags": [], "extra": 0})"});

    using row_schema = jobj<jkv<"rows"_fs, jarr<jtup<jobj<jkv<"name"_fs, jstr>>, jnum>>>>;
    REQUIRE_THROWS(bind<row_schema>{R"({"rows": [], "extra": 0})"});

    using schema_kv = jobj<jkv<"a"_fs, jnum>, jkv<"b"_fs, jnum>>;
    REQUIRE_THROWS(bind<schema_kv>{R"({"a": 1, "b": "str"})"});
  }

  SECTION("tuple schema errors") {
    using schema_too_few = jtup<jnum, jnum, jnum>;
    REQUIRE_THROWS(bind<schema_too_few>{R"([1, 2])"});

    using schema_too_many = jtup<jnum, jnum>;
    REQUIRE_THROWS(bind<schema_too_many>{R"([1, 2, 3])"});

    using schema_elem = jtup<jnum, jstr>;
    REQUIRE_THROWS(bind<schema_elem>{R"([1, 2])"});

    using schema_not_arr = jtup<jnum>;
    REQUIRE_THROWS(bind<schema_not_arr>{R"({"x": 1})"});

    using schema_single = jtup<jnum>;
    REQUIRE_THROWS(bind<schema_single>{R"([1, 2])"});

    using schema_num_str = jtup<jnum, jstr>;
    REQUIRE_THROWS(bind<schema_num_str>{R"([1, "x", true])"});

    using schema_num_str_bool = jtup<jnum, jstr, jbool>;
    REQUIRE_THROWS(bind<schema_num_str_bool>{R"([1, "x"])"});

    using schema_str_bool = jtup<jstr, jbool>;
    REQUIRE_THROWS(bind<schema_str_bool>{R"(["x", true, 1])"});

    using schema_num_arr = jtup<jnum, jarr<jstr>>;
    REQUIRE_THROWS(bind<schema_num_arr>{R"([1, [], 2])"});

    using schema_arr_bool = jtup<jarr<jnum>, jbool>;
    REQUIRE_THROWS(bind<schema_arr_bool>{R"([[1, 2], true, 3])"});

    using schema_obj_num = jtup<jobj<jkv<"name"_fs, jstr>>, jnum>;
    REQUIRE_THROWS(bind<schema_obj_num>{R"([{"name": "x"}])"});

    using schema_str_obj = jtup<jstr, jobj<jkv<"x"_fs, jnum>>>;
    REQUIRE_THROWS(bind<schema_str_obj>{R"(["s", {"x": 1}, 0])"});

    using schema_num_nested = jtup<jnum, jtup<jstr, jbool>>;
    REQUIRE_THROWS(bind<schema_num_nested>{R"([1, ["s", true], 0])"});

    using schema_obj_label_inner = jtup<jobj<jkv<"label"_fs, jstr>>, jtup<jarr<jnum>, jbool>>;
    REQUIRE_THROWS(bind<schema_obj_label_inner>{R"([{"label": "x"}, [[1], true], 0])"});
  }
}

TEST_CASE("compile-time parsing") {
  SECTION("scalars and numbers") {
    constexpr auto doc_null = static_parse<R"(null)"_fs>{};
    STATIC_REQUIRE(doc_null.root().type() == jtype::null_t);
    STATIC_REQUIRE(doc_null.root().as_null() == nullptr);

    constexpr auto doc_true = static_parse<R"(true)"_fs>{};
    STATIC_REQUIRE(doc_true.root().as_bool() == true);

    constexpr auto doc_false = static_parse<R"(false)"_fs>{};
    STATIC_REQUIRE(doc_false.root().as_bool() == false);

    constexpr auto doc_zero = static_parse<R"(0)"_fs>{};
    STATIC_REQUIRE(doc_zero.root().as_num() == 0.0);

    constexpr auto doc_int = static_parse<R"(42)"_fs>{};
    STATIC_REQUIRE(doc_int.root().as_num() == 42.0);

    constexpr auto doc_neg = static_parse<R"(-123)"_fs>{};
    STATIC_REQUIRE(doc_neg.root().as_num() == -123.0);

    constexpr auto doc_dec = static_parse<R"(3.14)"_fs>{};
    STATIC_REQUIRE(doc_dec.root().as_num() == 3.14);

    constexpr auto doc_exp = static_parse<R"(1e3)"_fs>{};
    STATIC_REQUIRE(doc_exp.root().as_num() == 1000.0);

    constexpr auto doc_neg_dec = static_parse<R"(-2.718)"_fs>{};
    STATIC_REQUIRE(doc_neg_dec.root().as_num() == -2.718);

    constexpr auto doc_exp_pos = static_parse<R"(1e10)"_fs>{};
    STATIC_REQUIRE(doc_exp_pos.root().as_num() == 1e10);

    constexpr auto doc_exp_neg = static_parse<R"(1e-5)"_fs>{};
    STATIC_REQUIRE(doc_exp_neg.root().as_num() == 1e-5);

    constexpr auto doc_exp_plus = static_parse<R"(2.5e+3)"_fs>{};
    STATIC_REQUIRE(doc_exp_plus.root().as_num() == 2500.0);

    constexpr auto doc_dec_exp = static_parse<R"(6.022e23)"_fs>{};
    REQUIRE(doc_dec_exp.root().as_num() == Catch::Approx(6.022e23));
  }

  SECTION("strings and unicode escapes") {
    constexpr auto doc_empty = static_parse<R"("")"_fs>{};
    STATIC_REQUIRE(doc_empty.root().as_str() == ""sv);

    constexpr auto doc_simple = static_parse<R"("hello")"_fs>{};
    STATIC_REQUIRE(doc_simple.root().as_str() == "hello"sv);

    constexpr auto doc_escape = static_parse<R"("line1\nline2")"_fs>{};
    STATIC_REQUIRE(doc_escape.root().as_str() == "line1\nline2"sv);

    constexpr auto doc_spaces = static_parse<R"("hello world")"_fs>{};
    STATIC_REQUIRE(doc_spaces.root().as_str() == "hello world"sv);

    constexpr auto doc_quote = static_parse<R"("say \"hi\"")"_fs>{};
    STATIC_REQUIRE(doc_quote.root().as_str() == R"(say "hi")"sv);

    constexpr auto doc_backslash = static_parse<R"("path\\to\\file")"_fs>{};
    STATIC_REQUIRE(doc_backslash.root().as_str() == R"(path\to\file)"sv);

    constexpr auto doc_slash = static_parse<R"("a\/b")"_fs>{};
    STATIC_REQUIRE(doc_slash.root().as_str() == "a/b"sv);

    constexpr auto doc_b = static_parse<R"("bell\b")"_fs>{};
    STATIC_REQUIRE(doc_b.root().as_str() == "bell\b"sv);

    constexpr auto doc_f = static_parse<R"("form\ffeed")"_fs>{};
    STATIC_REQUIRE(doc_f.root().as_str() == "form\ffeed"sv);

    constexpr auto doc_r = static_parse<R"("carriage\rreturn")"_fs>{};
    STATIC_REQUIRE(doc_r.root().as_str() == "carriage\rreturn"sv);

    constexpr auto doc_t = static_parse<R"("tab\there")"_fs>{};
    STATIC_REQUIRE(doc_t.root().as_str() == "tab\there"sv);

    constexpr auto doc_ascii = static_parse<R"("\u0041")"_fs>{};
    STATIC_REQUIRE(doc_ascii.root().as_str() == "A"sv);

    constexpr auto doc_digit = static_parse<R"("\u0030")"_fs>{};
    STATIC_REQUIRE(doc_digit.root().as_str() == "0"sv);
  }

  SECTION("arrays and objects") {
    constexpr auto doc_empty_arr = static_parse<R"([])"_fs>{};
    STATIC_REQUIRE(doc_empty_arr.root().size() == 0);

    constexpr auto doc_nums = static_parse<R"([1, 2, 3])"_fs>{};
    STATIC_REQUIRE(doc_nums.root().size() == 3);
    STATIC_REQUIRE(doc_nums.root()[0].as_num() == 1.0);
    STATIC_REQUIRE(doc_nums.root()[1].as_num() == 2.0);
    STATIC_REQUIRE(doc_nums.root()[2].as_num() == 3.0);

    constexpr auto doc_nested = static_parse<R"([[1, 2], [3, 4]])"_fs>{};
    STATIC_REQUIRE(doc_nested.root()[0][0].as_num() == 1.0);
    STATIC_REQUIRE(doc_nested.root()[1][1].as_num() == 4.0);
    STATIC_REQUIRE(doc_nested.root()[0][1].as_num() == 2.0);

    constexpr auto doc_single = static_parse<R"([42])"_fs>{};
    STATIC_REQUIRE(doc_single.root().size() == 1);
    STATIC_REQUIRE(doc_single.root()[0].as_num() == 42.0);

    constexpr auto doc_multi = static_parse<R"([1, 2, 3, 4, 5])"_fs>{};
    STATIC_REQUIRE(doc_multi.root().size() == 5);
    STATIC_REQUIRE(doc_multi.root()[4].as_num() == 5.0);
    STATIC_REQUIRE(doc_multi.root()[2].as_num() == 3.0);

    constexpr auto doc_mixed = static_parse<R"([null, true, 42, "hello"])"_fs>{};
    STATIC_REQUIRE(doc_mixed.root().size() == 4);
    STATIC_REQUIRE(doc_mixed.root()[0].type() == jtype::null_t);
    STATIC_REQUIRE(doc_mixed.root()[1].type() == jtype::bool_t);
    STATIC_REQUIRE(doc_mixed.root()[2].type() == jtype::num_t);
    STATIC_REQUIRE(doc_mixed.root()[3].type() == jtype::str_t);
    STATIC_REQUIRE(doc_mixed.root()[0].as_null() == nullptr);
    STATIC_REQUIRE(doc_mixed.root()[1].as_bool() == true);
    STATIC_REQUIRE(doc_mixed.root()[2].as_num() == 42.0);
    STATIC_REQUIRE(doc_mixed.root()[3].as_str() == "hello"sv);

    constexpr auto doc_deep = static_parse<R"([[[[[1]]]]])"_fs>{};
    STATIC_REQUIRE(doc_deep.root()[0][0][0][0][0].as_num() == 1.0);

    constexpr auto doc_obj_empty = static_parse<R"({})"_fs>{};
    STATIC_REQUIRE(doc_obj_empty.root().size() == 0);

    constexpr auto doc_obj_simple = static_parse<R"({"key": "value"})"_fs>{};
    STATIC_REQUIRE(doc_obj_simple.root()["key"].as_str() == "value"sv);

    constexpr auto doc_obj_multi = static_parse<R"({"a": 1, "b": 2})"_fs>{};
    STATIC_REQUIRE(doc_obj_multi.root()["a"].as_num() == 1.0);
    STATIC_REQUIRE(doc_obj_multi.root()["b"].as_num() == 2.0);

    constexpr auto doc_obj_mixed =
      static_parse<R"({"null": null, "bool": true, "num": 42, "str": "text"})"_fs>{};
    STATIC_REQUIRE(doc_obj_mixed.root()["null"].type() == jtype::null_t);
    STATIC_REQUIRE(doc_obj_mixed.root()["bool"].as_bool() == true);
    STATIC_REQUIRE(doc_obj_mixed.root()["num"].as_num() == 42.0);
    STATIC_REQUIRE(doc_obj_mixed.root()["str"].as_str() == "text"sv);

    constexpr auto doc_obj_arr = static_parse<R"({"array": [1, 2, 3]})"_fs>{};
    STATIC_REQUIRE(doc_obj_arr.root()["array"].size() == 3);
    STATIC_REQUIRE(doc_obj_arr.root()["array"][0].as_num() == 1.0);
    STATIC_REQUIRE(doc_obj_arr.root()["array"][2].as_num() == 3.0);
  }

  SECTION("whitespace and complex") {
    constexpr auto doc_ws_arr = static_parse<R"([ 1 , 2 , 3 ])"_fs>{};
    STATIC_REQUIRE(doc_ws_arr.root().size() == 3);

    constexpr auto doc_ws_obj = static_parse<R"({ "key" : "value" })"_fs>{};
    STATIC_REQUIRE(doc_ws_obj.root()["key"].as_str() == "value"sv);

    constexpr auto doc_complex = static_parse<R"({
      "name": "Test",
      "value": 42,
      "items": [1, 2, 3]
    })"_fs>{};
    STATIC_REQUIRE(doc_complex.root()["name"].as_str() == "Test"sv);
    STATIC_REQUIRE(doc_complex.root()["value"].as_num() == 42.0);
    STATIC_REQUIRE(doc_complex.root()["items"][2].as_num() == 3.0);

    constexpr auto doc_complex2 = static_parse<R"({
      "name": "Ada Lovelace",
      "age": 36,
      "programmer": true,
      "contributions": ["Analytical Engine", "First Algorithm"],
      "address": {"city": "London", "country": "England"}
    })"_fs>{};
    STATIC_REQUIRE(doc_complex2.root()["name"].as_str() == "Ada Lovelace"sv);
    STATIC_REQUIRE(doc_complex2.root()["age"].as_num() == 36.0);
    STATIC_REQUIRE(doc_complex2.root()["programmer"].as_bool() == true);
    STATIC_REQUIRE(doc_complex2.root()["contributions"].size() == 2);
    STATIC_REQUIRE(doc_complex2.root()["address"]["city"].as_str() == "London"sv);
    STATIC_REQUIRE(doc_complex2.root()["address"]["country"].as_str() == "England"sv);
  }
}

TEST_CASE("compile-time schema success") {
  SECTION("scalar schemas") {
    constexpr static_bind<R"(null)"_fs, jnull> doc_null{};
    STATIC_REQUIRE(doc_null.root().type() == jtype::null_t);

    constexpr static_bind<R"(true)"_fs, jbool> doc_bool{};
    STATIC_REQUIRE(doc_bool.root().as_bool() == true);

    constexpr static_bind<R"(42)"_fs, jnum> doc_num{};
    STATIC_REQUIRE(doc_num.root().as_num() == 42.0);

    constexpr static_bind<R"("hello")"_fs, jstr> doc_str{};
    STATIC_REQUIRE(doc_str.root().as_str() == "hello"sv);
  }

  SECTION("array and object schemas") {
    using num_schema = jarr<jnum>;
    constexpr static_bind<R"([1, 2, 3])"_fs, num_schema> doc_num{};
    STATIC_REQUIRE(doc_num.root()[0].as_num() == 1.0);
    STATIC_REQUIRE(doc_num.root().size() == 3);

    using obj_schema = jobj<jkv<"name"_fs, jstr>, jkv<"age"_fs, jnum>>;
    constexpr static_bind<R"({"name": "Alice", "age": 30})"_fs, obj_schema> doc_obj{};
    STATIC_REQUIRE(doc_obj.root()["name"].as_str() == "Alice"sv);
    STATIC_REQUIRE(doc_obj.root()["age"].as_num() == 30.0);

    using person_schema =
      jobj<jkv<"name"_fs, jstr>, jkv<"age"_fs, jnum>, jkv<"tags"_fs, jarr<jstr>>>;
    constexpr static_bind<
      R"({
      "name": "Ada",
      "age": 36,
      "tags": ["math", "programming"]
    })"_fs,
      person_schema>
      doc_person{};
    STATIC_REQUIRE(doc_person.root()["name"].as_str() == "Ada"sv);
    STATIC_REQUIRE(doc_person.root()["age"].as_num() == 36.0);
    STATIC_REQUIRE(doc_person.root()["tags"][0].as_str() == "math"sv);
    STATIC_REQUIRE(doc_person.root()["tags"][1].as_str() == "programming"sv);

    using str_schema = jarr<jstr>;
    constexpr static_bind<R"(["a", "b", "c"])"_fs, str_schema> doc_str{};
    STATIC_REQUIRE(doc_str.root()[0].as_str() == "a"sv);
    STATIC_REQUIRE(doc_str.root()[1].as_str() == "b"sv);
    STATIC_REQUIRE(doc_str.root()[2].as_str() == "c"sv);

    using nested_schema = jarr<jarr<jnum>>;
    constexpr static_bind<R"([[1, 2], [3, 4]])"_fs, nested_schema> doc_nested{};
    STATIC_REQUIRE(doc_nested.root()[0][0].as_num() == 1.0);
    STATIC_REQUIRE(doc_nested.root()[1][1].as_num() == 4.0);
    STATIC_REQUIRE(doc_nested.root()[0][1].as_num() == 2.0);

    using inner_schema = jobj<jkv<"city"_fs, jstr>>;
    using outer_schema = jobj<jkv<"name"_fs, jstr>, jkv<"address"_fs, inner_schema>>;
    constexpr static_bind<R"({"name": "Bob", "address": {"city": "NYC"}})"_fs, outer_schema>
      doc_outer{};
    STATIC_REQUIRE(doc_outer.root()["name"].as_str() == "Bob"sv);
    STATIC_REQUIRE(doc_outer.root()["address"]["city"].as_str() == "NYC"sv);

    using obj_arr_schema = jobj<jkv<"name"_fs, jstr>, jkv<"tags"_fs, jarr<jstr>>>;
    constexpr static_bind<R"({"name": "Ada", "tags": ["math", "logic"]})"_fs, obj_arr_schema>
      doc_obj_arr{};
    STATIC_REQUIRE(doc_obj_arr.root()["name"].as_str() == "Ada"sv);
    STATIC_REQUIRE(doc_obj_arr.root()["tags"][0].as_str() == "math"sv);
    STATIC_REQUIRE(doc_obj_arr.root()["tags"][1].as_str() == "logic"sv);
  }

  SECTION("tuple schemas") {
    using schema_basic = jtup<jnum, jstr, jbool>;
    constexpr static_bind<R"([42, "hello", true])"_fs, schema_basic> doc_basic{};
    STATIC_REQUIRE(doc_basic.root().size() == 3);
    STATIC_REQUIRE(doc_basic.root()[0].as_num() == 42.0);
    STATIC_REQUIRE(doc_basic.root()[1].as_str() == "hello"sv);
    STATIC_REQUIRE(doc_basic.root()[2].as_bool() == true);

    constexpr static_bind<R"([99])"_fs, jtup<jnum>> doc_single{};
    STATIC_REQUIRE(doc_single.root()[0].as_num() == 99.0);

    using schema_arr = jtup<jnum, jarr<jstr>>;
    constexpr static_bind<R"([1, ["a", "b"]])"_fs, schema_arr> doc_arr{};
    STATIC_REQUIRE(doc_arr.root()[0].as_num() == 1.0);
    STATIC_REQUIRE(doc_arr.root()[1][0].as_str() == "a"sv);
    STATIC_REQUIRE(doc_arr.root()[1][1].as_str() == "b"sv);

    using schema_obj = jtup<jstr, jobj<jkv<"x"_fs, jnum>>>;
    constexpr static_bind<R"(["origin", {"x": 10}])"_fs, schema_obj> doc_obj{};
    STATIC_REQUIRE(doc_obj.root()[0].as_str() == "origin"sv);
    STATIC_REQUIRE(doc_obj.root()[1]["x"].as_num() == 10.0);

    using schema_tup = jtup<jnum, jtup<jstr, jbool>>;
    constexpr static_bind<R"([1, ["hi", false]])"_fs, schema_tup> doc_tup{};
    STATIC_REQUIRE(doc_tup.root()[1][0].as_str() == "hi"sv);
    STATIC_REQUIRE(doc_tup.root()[1][1].as_bool() == false);
    STATIC_REQUIRE(doc_tup.root()[0].as_num() == 1.0);

    using schema_in_obj = jobj<jkv<"point"_fs, jtup<jnum, jnum>>>;
    constexpr static_bind<R"({"point": [3, 4]})"_fs, schema_in_obj> doc_in_obj{};
    STATIC_REQUIRE(doc_in_obj.root()["point"][0].as_num() == 3.0);
    STATIC_REQUIRE(doc_in_obj.root()["point"][1].as_num() == 4.0);

    using schema_in_arr = jarr<jtup<jnum, jstr>>;
    constexpr static_bind<R"([[1, "one"], [2, "two"]])"_fs, schema_in_arr> doc_in_arr{};
    STATIC_REQUIRE(doc_in_arr.root()[0][0].as_num() == 1.0);
    STATIC_REQUIRE(doc_in_arr.root()[1][1].as_str() == "two"sv);
    STATIC_REQUIRE(doc_in_arr.root().size() == 2);

    using inner_tup = jtup<jarr<jnum>, jbool>;
    using schema_deep = jtup<jobj<jkv<"label"_fs, jstr>>, inner_tup>;
    constexpr static_bind<R"([{"label": "pt"}, [[1, 2, 3], true]])"_fs, schema_deep> doc_deep{};
    STATIC_REQUIRE(doc_deep.root()[0]["label"].as_str() == "pt"sv);
    STATIC_REQUIRE(doc_deep.root()[1][0][0].as_num() == 1.0);
    STATIC_REQUIRE(doc_deep.root()[1][1].as_bool() == true);
    STATIC_REQUIRE(doc_deep.root()[1][0][2].as_num() == 3.0);

    using row = jtup<jobj<jkv<"name"_fs, jstr>>, jnum>;
    using schema_rows = jobj<jkv<"rows"_fs, jarr<row>>>;
    constexpr static_bind<R"({"rows": [[{"name": "a"}, 1], [{"name": "b"}, 2]]})"_fs, schema_rows>
      doc_rows{};
    STATIC_REQUIRE(doc_rows.root()["rows"][0][0]["name"].as_str() == "a"sv);
    STATIC_REQUIRE(doc_rows.root()["rows"][1][1].as_num() == 2.0);
    STATIC_REQUIRE(doc_rows.root()["rows"][0][1].as_num() == 1.0);
  }
}

TEST_CASE("direct access API") {
  SECTION("runtime parse access") {
    auto doc_key = parse{R"({"name":"Ada","age":36})"};
    REQUIRE(doc_key["name"].as_str() == "Ada"sv);
    REQUIRE(doc_key["age"].as_num() == Catch::Approx(36.0));

    auto doc_idx = parse{R"([10,20,30])"};
    REQUIRE(doc_idx[0].as_num() == Catch::Approx(10.0));
    REQUIRE(doc_idx[2].as_num() == Catch::Approx(30.0));

    auto doc_key_idx = parse{R"({"items":[1,2,3]})"};
    REQUIRE(doc_key_idx["items"][1].as_num() == Catch::Approx(2.0));

    auto doc_idx_key = parse{R"([{"a":1},{"a":2}])"};
    REQUIRE(doc_idx_key[0]["a"].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_idx_key[1]["a"].as_num() == Catch::Approx(2.0));

    auto doc_chain = parse{R"({"a":{"b":[0,[1,2]]}})"};
    REQUIRE(doc_chain["a"]["b"][1][0].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_chain["a"]["b"][1][1].as_num() == Catch::Approx(2.0));
  }

  SECTION("runtime bind access") {
    using schema = jobj<jkv<"tags"_fs, jarr<jstr>>>;
    bind<schema> doc{R"({"tags":["x","y","z"]})"};
    REQUIRE(doc["tags"][0].as_str() == "x"sv);
    REQUIRE(doc["tags"][2].as_str() == "z"sv);

    using arr_schema = jarr<jobj<jkv<"v"_fs, jnum>>>;
    bind<arr_schema> doc_arr{R"([{"v":1},{"v":2}])"};
    REQUIRE(doc_arr[0]["v"].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_arr[1]["v"].as_num() == Catch::Approx(2.0));
  }

  SECTION("compile-time parse and bind access") {
    constexpr auto doc_ct = static_parse<R"({"k":[10,20,30]})"_fs>{};
    STATIC_REQUIRE(doc_ct["k"][0].as_num() == 10.0);
    STATIC_REQUIRE(doc_ct["k"][2].as_num() == 30.0);
    STATIC_REQUIRE(doc_ct["k"][1].as_num() == 20.0);

    constexpr auto doc_ct2 = static_parse<R"([{"x":1},{"x":2}])"_fs>{};
    STATIC_REQUIRE(doc_ct2[0]["x"].as_num() == 1.0);
    STATIC_REQUIRE(doc_ct2[1]["x"].as_num() == 2.0);

    using schema = jobj<jkv<"a"_fs, jarr<jobj<jkv<"v"_fs, jnum>>>>>;
    constexpr static_bind<R"({"a":[{"v":7},{"v":8}]})"_fs, schema> doc_bind{};
    STATIC_REQUIRE(doc_bind["a"][0]["v"].as_num() == 7.0);
    STATIC_REQUIRE(doc_bind["a"][1]["v"].as_num() == 8.0);
  }
}

TEST_CASE("edge and stress structures") {
  SECTION("array structures") {
    auto doc_objects = parse{R"([{"a":1},{"a":2},{"a":3}])"};
    REQUIRE(doc_objects.root().size() == 3);
    REQUIRE(doc_objects.root()[0]["a"].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_objects.root()[2]["a"].as_num() == Catch::Approx(3.0));

    auto doc_empty_obj = parse{R"([{},{},{}])"};
    REQUIRE(doc_empty_obj.root().size() == 3);
    REQUIRE(doc_empty_obj.root()[0].type() == jtype::obj_t);
    REQUIRE(doc_empty_obj.root()[0].size() == 0);

    auto doc_empty_arr = parse{R"([[],[],[]])"};
    REQUIRE(doc_empty_arr.root().size() == 3);
    REQUIRE(doc_empty_arr.root()[1].size() == 0);

    auto doc_mixed = parse{R"([[1,2],{"k":"v"},[],{}])"};
    REQUIRE(doc_mixed.root().size() == 4);
    REQUIRE(doc_mixed.root()[0].type() == jtype::arr_t);
    REQUIRE(doc_mixed.root()[1].type() == jtype::obj_t);
    REQUIRE(doc_mixed.root()[2].size() == 0);
    REQUIRE(doc_mixed.root()[3].size() == 0);

    auto doc_nulls = parse{R"([null,null,null])"};
    REQUIRE(doc_nulls.root().size() == 3);
    for (std::size_t i = 0; i < 3; ++i) REQUIRE(doc_nulls.root()[i].type() == jtype::null_t);

    auto doc_alt = parse{R"([1,"a",2,"b",3,"c"])"};
    REQUIRE(doc_alt.root().size() == 6);
    REQUIRE(doc_alt.root()[0].type() == jtype::num_t);
    REQUIRE(doc_alt.root()[1].type() == jtype::str_t);
    REQUIRE(doc_alt.root()[4].as_num() == Catch::Approx(3.0));
    REQUIRE(doc_alt.root()[5].as_str() == "c"sv);

    auto doc_nested = parse{R"([[1,[2,[3]]]])"};
    REQUIRE(doc_nested.root()[0][0].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_nested.root()[0][1][0].as_num() == Catch::Approx(2.0));
    REQUIRE(doc_nested.root()[0][1][1][0].as_num() == Catch::Approx(3.0));

    auto doc_single_null = parse{R"([null])"};
    REQUIRE(doc_single_null.root().size() == 1);
    REQUIRE(doc_single_null.root()[0].as_null() == nullptr);
  }

  SECTION("object structures") {
    auto doc_empty_key = parse{R"({"": 42})"};
    REQUIRE(doc_empty_key.root().size() == 1);
    REQUIRE(doc_empty_key.root()[""].as_num() == Catch::Approx(42.0));

    auto doc_num_keys = parse{R"({"0": "zero", "1": "one"})"};
    REQUIRE(doc_num_keys.root()["0"].as_str() == "zero"sv);
    REQUIRE(doc_num_keys.root()["1"].as_str() == "one"sv);

    auto doc_space_key = parse{R"({"hello world": true})"};
    REQUIRE(doc_space_key.root()["hello world"].as_bool() == true);

    auto doc_escape_key = parse{R"({"a\"b": 1})"};
    REQUIRE(doc_escape_key.root()["a\"b"].as_num() == Catch::Approx(1.0));

    auto doc_obj_arr = parse{R"({"items":[{"id":1,"v":"a"},{"id":2,"v":"b"}]})"};
    REQUIRE(doc_obj_arr.root()["items"].size() == 2);
    REQUIRE(doc_obj_arr.root()["items"][0]["id"].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_obj_arr.root()["items"][1]["v"].as_str() == "b"sv);

    auto doc_empty_arr = parse{R"({"tags":[]})"};
    REQUIRE(doc_empty_arr.root()["tags"].type() == jtype::arr_t);
    REQUIRE(doc_empty_arr.root()["tags"].size() == 0);

    auto doc_all_types = parse{R"({"n":null,"b":false,"i":0,"s":"","a":[],"o":{}})"};
    REQUIRE(doc_all_types.root().size() == 6);
    REQUIRE(doc_all_types.root()["n"].type() == jtype::null_t);
    REQUIRE(doc_all_types.root()["b"].as_bool() == false);
    REQUIRE(doc_all_types.root()["i"].as_num() == 0.0);
    REQUIRE(doc_all_types.root()["s"].as_str() == ""sv);
    REQUIRE(doc_all_types.root()["a"].type() == jtype::arr_t);
    REQUIRE(doc_all_types.root()["o"].type() == jtype::obj_t);

    auto doc_obj_in_arr = parse{R"({"matrix":[[1,2],[3,4]],"meta":{"rows":2}})"};
    REQUIRE(doc_obj_in_arr.root()["matrix"][0][1].as_num() == Catch::Approx(2.0));
    REQUIRE(doc_obj_in_arr.root()["matrix"][1][0].as_num() == Catch::Approx(3.0));
    REQUIRE(doc_obj_in_arr.root()["meta"]["rows"].as_num() == Catch::Approx(2.0));

    auto doc_arr_in_obj = parse{R"([{"vals":[10,20]},{"vals":[30,40]}])"};
    REQUIRE(doc_arr_in_obj.root()[0]["vals"][1].as_num() == Catch::Approx(20.0));
    REQUIRE(doc_arr_in_obj.root()[1]["vals"][0].as_num() == Catch::Approx(30.0));

    auto doc_siblings = parse{R"({"a":[1,2],"b":[3,4],"c":[5,6]})"};
    REQUIRE(doc_siblings.root()["a"].size() == 2);
    REQUIRE(doc_siblings.root()["b"][1].as_num() == Catch::Approx(4.0));
    REQUIRE(doc_siblings.root()["c"][0].as_num() == Catch::Approx(5.0));

    auto doc_deep = parse{R"({"a":{"b":{"c":{"d":[1,[2,[3]]]}}}})"};
    REQUIRE(doc_deep.root()["a"]["b"]["c"]["d"][0].as_num() == Catch::Approx(1.0));
    REQUIRE(doc_deep.root()["a"]["b"]["c"]["d"][1][0].as_num() == Catch::Approx(2.0));
    REQUIRE(doc_deep.root()["a"]["b"]["c"]["d"][1][1][0].as_num() == Catch::Approx(3.0));
  }

  SECTION("deep nesting and large structures") {
    std::string json_arr = "[";
    for (int i = 0; i < 50; ++i) json_arr += "[";
    json_arr += "42";
    for (int i = 0; i < 50; ++i) json_arr += "]";
    json_arr += "]";

    auto doc_arr = parse{json_arr};
    auto node = doc_arr.root();
    for (int i = 0; i < 50; ++i) node = node[0];
    REQUIRE(node[0].as_num() == Catch::Approx(42.0));

    std::string json_obj = R"({"a":)";
    for (int i = 0; i < 10; ++i) json_obj += R"({"a":)";
    json_obj += "42";
    for (int i = 0; i < 10; ++i) json_obj += "}";
    json_obj += "}";

    auto doc_obj = parse{json_obj};
    auto node_obj = doc_obj.root();
    for (int i = 0; i < 10; ++i) node_obj = node_obj["a"];
    REQUIRE(node_obj["a"].as_num() == Catch::Approx(42.0));

    std::string json_large_arr = "[";
    for (int i = 0; i < 1000; ++i) {
      if (i > 0) json_large_arr += ",";
      json_large_arr += std::to_string(i);
    }
    json_large_arr += "]";

    auto doc_large_arr = parse{json_large_arr};
    REQUIRE(doc_large_arr.root().size() == 1000);
    REQUIRE(doc_large_arr.root()[0].as_num() == Catch::Approx(0.0));
    REQUIRE(doc_large_arr.root()[999].as_num() == Catch::Approx(999.0));

    std::string json_large_obj = "{";
    for (int i = 0; i < 100; ++i) {
      if (i > 0) json_large_obj += ",";
      json_large_obj += "\"key" + std::to_string(i) + "\":" + std::to_string(i);
    }
    json_large_obj += "}";

    auto doc_large_obj = parse{json_large_obj};
    REQUIRE(doc_large_obj.root().size() == 100);
    REQUIRE(doc_large_obj.root()["key0"].as_num() == Catch::Approx(0.0));
    REQUIRE(doc_large_obj.root()["key99"].as_num() == Catch::Approx(99.0));
  }

  SECTION("long strings and escapes") {
    std::string long_str(10000, 'a');
    std::string json = "\"" + long_str + "\"";
    auto doc = parse{json};
    REQUIRE(doc.root().as_str().size() == 10000);
    REQUIRE(doc.root().as_str()[0] == 'a');
    REQUIRE(doc.root().as_str()[9999] == 'a');

    std::string json_esc = "\"";
    for (int i = 0; i < 1000; ++i) json_esc += R"(\n)";
    json_esc += "\"";

    auto doc_esc = parse{json_esc};
    REQUIRE(doc_esc.root().as_str().size() == 1000);
    for (std::size_t i = 0; i < 1000; ++i) {
      REQUIRE(doc_esc.root().as_str()[i] == '\n');
    }

    auto doc_all_esc = parse{R"("\"\\/\b\f\n\r\t")"};
    REQUIRE(doc_all_esc.root().as_str() == "\"\\/\b\f\n\r\t"sv);
  }

  SECTION("whitespace heavy cases") {
    auto doc_ws = parse{"  \t\n\r  42  \t\n\r  "};
    REQUIRE(doc_ws.root().as_num() == Catch::Approx(42.0));

    auto doc_arr_ws = parse{"  [  42  ]  "};
    REQUIRE(doc_arr_ws.root().size() == 1);
    REQUIRE(doc_arr_ws.root()[0].as_num() == Catch::Approx(42.0));

    auto doc_obj_ws = parse{R"(  {  "key"  :  "value"  }  )"};
    REQUIRE(doc_obj_ws.root()["key"].as_str() == "value"sv);

    auto doc_empty_arr_ws = parse{R"([     ])"};
    REQUIRE(doc_empty_arr_ws.root().size() == 0);

    auto doc_empty_obj_ws = parse{R"({     })"};
    REQUIRE(doc_empty_obj_ws.root().size() == 0);
  }
}

TEST_CASE("numeric edge cases") {
  SECTION("overflow and extremes") {
    auto doc_overflow = parse{"18446744073709551616"};
    REQUIRE(doc_overflow.root().as_num() == Catch::Approx(1.8446744073709552e19));

    auto doc_large = parse{R"(1.7976931348623157e308)"};
    REQUIRE(doc_large.root().as_num() > 1e308);

    auto doc_small = parse{R"(2.2250738585072014e-308)"};
    REQUIRE(doc_small.root().as_num() > 0.0);
    REQUIRE(doc_small.root().as_num() < 1e-307);

    auto doc_neg_zero = parse{R"(-0)"};
    REQUIRE(doc_neg_zero.root().as_num() == 0.0);
  }

  SECTION("precision and exponent formats") {
    auto doc_max_dec = parse{R"(0.123456789012345)"};
    REQUIRE(doc_max_dec.root().as_num() == Catch::Approx(0.123456789012345).epsilon(1e-15));

    auto doc_large_exp = parse{R"(1e308)"};
    REQUIRE(doc_large_exp.root().as_num() == Catch::Approx(1e308).epsilon(1e-6));

    auto doc_small_exp = parse{R"(1e-308)"};
    REQUIRE(doc_small_exp.root().as_num() >= 0.0);
    REQUIRE(doc_small_exp.root().as_num() <= 1e-307);

    auto doc_single = parse{R"(5)"};
    REQUIRE(doc_single.root().as_num() == Catch::Approx(5.0));

    auto doc_frac = parse{R"(0.5)"};
    REQUIRE(doc_frac.root().as_num() == Catch::Approx(0.5));

    auto doc_zero_exp = parse{R"(0e0)"};
    REQUIRE(doc_zero_exp.root().as_num() == Catch::Approx(0.0));

    auto doc_zero_exp_cap = parse{R"(0E0)"};
    REQUIRE(doc_zero_exp_cap.root().as_num() == Catch::Approx(0.0));

    auto doc_neg_zero_dec = parse{R"(-0.0)"};
    REQUIRE(doc_neg_zero_dec.root().as_num() == 0.0);

    auto doc_neg_zero_exp = parse{R"(-0e5)"};
    REQUIRE(doc_neg_zero_exp.root().as_num() == 0.0);

    auto doc_cap_e = parse{R"(2E3)"};
    REQUIRE(doc_cap_e.root().as_num() == Catch::Approx(2000.0));

    auto doc_unsigned_exp = parse{R"(1e5)"};
    REQUIRE(doc_unsigned_exp.root().as_num() == Catch::Approx(100000.0));

    auto doc_neg_exp = parse{R"(-1.5e2)"};
    REQUIRE(doc_neg_exp.root().as_num() == Catch::Approx(-150.0));

    auto doc_large_int = parse{R"(999999999999)"};
    REQUIRE(doc_large_int.root().as_num() == Catch::Approx(999999999999.0));

    auto doc_boundary = parse{R"([1])"};
    REQUIRE(doc_boundary.root()[0].as_num() == Catch::Approx(1.0));
  }

  SECTION("invalid numeric formats") {
    REQUIRE_THROWS(parse{"01"});
    REQUIRE_THROWS(parse{"-01"});
    REQUIRE_THROWS(parse{"00"});
    REQUIRE_THROWS(parse{"42."});
    REQUIRE_THROWS(parse{".42"});
    REQUIRE_THROWS(parse{"4..2"});
    REQUIRE_THROWS(parse{"1e+ "});
    REQUIRE_THROWS(parse{"01.5"});
    REQUIRE_THROWS(parse{"1e-"});
    REQUIRE_THROWS(parse{"--1"});
    REQUIRE_THROWS(parse{"1e++2"});
    REQUIRE_THROWS(parse{"1e--2"});
    REQUIRE_THROWS(parse{"-42."});
    REQUIRE_THROWS(parse{"42abc"});
    REQUIRE_THROWS(parse{"+1"});
    REQUIRE_THROWS(parse{"0x42"});
    REQUIRE_THROWS(parse{"Infinity"});
    REQUIRE_THROWS(parse{"NaN"});
  }
}

TEST_CASE("conversion and as<T>") {
  SECTION("custom types") {
    auto doc_rt = parse{R"({"x":3.0,"y":4.0})"};
    auto p = doc_rt.root().as<Point>();
    REQUIRE(p.x == Catch::Approx(3.0));
    REQUIRE(p.y == Catch::Approx(4.0));

    constexpr auto doc_ct = static_parse<R"({"x":1.0,"y":2.0})"_fs>{};
    STATIC_REQUIRE(doc_ct.root().as<Point>().x == 1.0);
    STATIC_REQUIRE(doc_ct.root().as<Point>().y == 2.0);
  }

  SECTION("nullptr and bool") {
    auto doc_null = parse{"null"};
    REQUIRE(doc_null.root().as<std::nullptr_t>() == nullptr);

    constexpr auto doc_null_ct = static_parse<R"(null)"_fs>{};
    STATIC_REQUIRE(doc_null_ct.root().as<std::nullptr_t>() == nullptr);

    auto doc_true = parse{"true"};
    REQUIRE(doc_true.root().as<bool>() == true);

    auto doc_false = parse{"false"};
    REQUIRE(doc_false.root().as<bool>() == false);

    constexpr auto doc_bool_ct = static_parse<R"(true)"_fs>{};
    STATIC_REQUIRE(doc_bool_ct.root().as<bool>() == true);
  }

  SECTION("arithmetic types") {
    auto doc_double = parse{"3.14"};
    REQUIRE(doc_double.root().as<double>() == Catch::Approx(3.14));

    constexpr auto doc_double_ct = static_parse<R"(2.5)"_fs>{};
    STATIC_REQUIRE(doc_double_ct.root().as<double>() == 2.5);

    auto doc_float = parse{"1.5"};
    REQUIRE(doc_float.root().as<float>() == Catch::Approx(1.5f));

    auto doc_int = parse{"42"};
    REQUIRE(doc_int.root().as<int>() == 42);

    constexpr auto doc_int_ct = static_parse<R"(7)"_fs>{};
    STATIC_REQUIRE(doc_int_ct.root().as<int>() == 7);

    auto doc_long = parse{"100"};
    REQUIRE(doc_long.root().as<long>() == 100L);

    auto doc_unsigned = parse{"255"};
    REQUIRE(doc_unsigned.root().as<unsigned>() == 255u);
  }

  SECTION("string and string_view") {
    auto doc_sv = parse{R"("hello")"};
    REQUIRE(doc_sv.root().as<std::string_view>() == "hello"sv);

    constexpr auto doc_sv_ct = static_parse<R"("world")"_fs>{};
    STATIC_REQUIRE(doc_sv_ct.root().as<std::string_view>() == "world"sv);

    auto doc_str = parse{R"("crjson")"};
    REQUIRE(doc_str.root().as<std::string>() == "crjson");

    auto doc_empty = parse{R"("")"};
    REQUIRE(doc_empty.root().as<std::string>().empty());
  }

  SECTION("vector conversions") {
    auto doc_vec_d = parse{"[1.0, 2.0, 3.0]"};
    auto v_d = doc_vec_d.root().as<std::vector<double>>();
    REQUIRE(v_d.size() == 3);
    REQUIRE(v_d[0] == Catch::Approx(1.0));
    REQUIRE(v_d[1] == Catch::Approx(2.0));
    REQUIRE(v_d[2] == Catch::Approx(3.0));

    auto doc_vec_i = parse{"[10, 20, 30]"};
    auto v_i = doc_vec_i.root().as<std::vector<int>>();
    REQUIRE(v_i == std::vector<int>{10, 20, 30});

    auto doc_vec_s = parse{R"(["foo","bar","baz"])"};
    auto v_s = doc_vec_s.root().as<std::vector<std::string>>();
    REQUIRE(v_s.size() == 3);
    REQUIRE(v_s[0] == "foo");
    REQUIRE(v_s[1] == "bar");
    REQUIRE(v_s[2] == "baz");

    auto doc_vec_empty = parse{"[]"};
    auto v_empty = doc_vec_empty.root().as<std::vector<int>>();
    REQUIRE(v_empty.empty());

    auto doc_vec_nested = parse{"[[1,2],[3,4]]"};
    auto v_nested = doc_vec_nested.root().as<std::vector<std::vector<int>>>();
    REQUIRE(v_nested.size() == 2);
    REQUIRE(v_nested[0] == std::vector<int>{1, 2});
    REQUIRE(v_nested[1] == std::vector<int>{3, 4});
  }

  SECTION("pair and tuple conversions") {
    auto doc_pair = parse{R"([42,"hello"])"};
    auto p = doc_pair.root().as<std::pair<int, std::string>>();
    REQUIRE(p.first == 42);
    REQUIRE(p.second == "hello");

    constexpr auto doc_pair_ct = static_parse<R"([7,"world"])"_fs>{};
    constexpr auto p_ct = doc_pair_ct.root().as<std::pair<int, std::string_view>>();
    STATIC_REQUIRE(p_ct.first == 7);
    STATIC_REQUIRE(p_ct.second == "world"sv);

    auto doc_pair_mismatch = parse{R"([1,2,3])"};
    REQUIRE_THROWS(doc_pair_mismatch.root().as<std::pair<int, int>>());

    auto doc_pair_int = parse{R"([1, 2])"};
    auto p_int = doc_pair_int.root().as<std::pair<int, int>>();
    REQUIRE(p_int.first == 1);
    REQUIRE(p_int.second == 2);

    auto doc_tuple = parse{R"([42,"hello",true])"};
    auto t = doc_tuple.root().as<std::tuple<int, std::string, bool>>();
    REQUIRE(std::get<0>(t) == 42);
    REQUIRE(std::get<1>(t) == "hello");
    REQUIRE(std::get<2>(t) == true);

    constexpr auto doc_tuple_ct = static_parse<R"([7,"world",false])"_fs>{};
    constexpr auto t_ct = doc_tuple_ct.root().as<std::tuple<int, std::string_view, bool>>();
    STATIC_REQUIRE(std::get<0>(t_ct) == 7);
    STATIC_REQUIRE(std::get<1>(t_ct) == "world"sv);
    STATIC_REQUIRE(std::get<2>(t_ct) == false);

    auto doc_tuple_empty = parse{"[]"};
    auto t_empty = doc_tuple_empty.root().as<std::tuple<>>();
    REQUIRE(std::tuple_size_v<decltype(t_empty)> == 0);

    constexpr auto doc_tuple_empty_ct = static_parse<R"([])"_fs>{};
    constexpr auto t_empty_ct = doc_tuple_empty_ct.root().as<std::tuple<>>();
    STATIC_REQUIRE(std::tuple_size_v<decltype(t_empty_ct)> == 0);

    auto doc_tuple_nested = parse{R"([1,[2,"three"],[4]])"};
    auto t_nested =
      doc_tuple_nested.root().as<std::tuple<int, std::tuple<int, std::string>, std::tuple<int>>>();
    REQUIRE(std::get<0>(t_nested) == 1);
    REQUIRE(std::get<0>(std::get<1>(t_nested)) == 2);
    REQUIRE(std::get<1>(std::get<1>(t_nested)) == "three");
    REQUIRE(std::get<0>(std::get<2>(t_nested)) == 4);

    constexpr auto doc_tuple_nested_ct = static_parse<R"([1,[2,"three"],[4]])"_fs>{};
    constexpr auto t_nested_ct =
      doc_tuple_nested_ct.root()
        .as<std::tuple<int, std::tuple<int, std::string_view>, std::tuple<int>>>();
    STATIC_REQUIRE(std::get<0>(t_nested_ct) == 1);
    STATIC_REQUIRE(std::get<0>(std::get<1>(t_nested_ct)) == 2);
    STATIC_REQUIRE(std::get<1>(std::get<1>(t_nested_ct)) == "three"sv);
    STATIC_REQUIRE(std::get<0>(std::get<2>(t_nested_ct)) == 4);

    auto doc_tuple_vec = parse{R"([1,[2,3],[4,5]])"};
    auto t_vec = doc_tuple_vec.root().as<std::tuple<int, std::vector<int>, std::vector<int>>>();
    REQUIRE(std::get<0>(t_vec) == 1);
    REQUIRE(std::get<1>(t_vec)[0] == 2);
    REQUIRE(std::get<1>(t_vec)[1] == 3);
    REQUIRE(std::get<2>(t_vec)[0] == 4);
    REQUIRE(std::get<2>(t_vec)[1] == 5);

    auto doc_tuple_mismatch = parse{R"([1,2])"};
    REQUIRE_THROWS(doc_tuple_mismatch.root().as<std::tuple<int>>());
    auto doc_tuple_empty_mismatch = parse{R"([1])"};
    REQUIRE_THROWS(doc_tuple_empty_mismatch.root().as<std::tuple<>>());
    auto doc_tuple_int_str = parse{R"([1])"};
    REQUIRE_THROWS(doc_tuple_int_str.root().as<std::tuple<int, std::string>>());
    auto doc_tuple_int_str_bool = parse{R"([1, "x"])"};
    REQUIRE_THROWS(doc_tuple_int_str_bool.root().as<std::tuple<int, std::string, bool>>());
    auto doc_tuple_int_vec = parse{R"([1, [2, 3]])"};
    REQUIRE_THROWS(
      doc_tuple_int_vec.root().as<std::tuple<int, std::vector<int>, std::vector<int>>>()
    );
    auto doc_tuple_int_tup = parse{R"([1, [2, "three"]])"};
    REQUIRE_THROWS(
      doc_tuple_int_tup.root().as<std::tuple<int, std::tuple<int, std::string>, std::tuple<int>>>()
    );
  }

  SECTION("optional conversions") {
    auto doc_val = parse{"42"};
    auto v = doc_val.root().as<std::optional<int>>();
    REQUIRE(v.has_value());
    REQUIRE(*v == 42);

    auto doc_null = parse{"null"};
    auto v_null = doc_null.root().as<std::optional<int>>();
    REQUIRE(!v_null.has_value());

    auto doc_str = parse{R"("hi")"};
    auto v_str = doc_str.root().as<std::optional<std::string>>();
    REQUIRE(v_str.has_value());
    REQUIRE(*v_str == "hi");

    auto doc_null_str = parse{"null"};
    auto v_null_str = doc_null_str.root().as<std::optional<std::string>>();
    REQUIRE(!v_null_str.has_value());

    constexpr auto doc_val_ct = static_parse<R"(5)"_fs>{};
    constexpr auto doc_null_ct = static_parse<R"(null)"_fs>{};
    STATIC_REQUIRE(doc_val_ct.root().as<std::optional<int>>().has_value());
    STATIC_REQUIRE(*doc_val_ct.root().as<std::optional<int>>() == 5);
    STATIC_REQUIRE(!doc_null_ct.root().as<std::optional<int>>().has_value());
  }

  SECTION("composite custom conversion") {
    auto doc_comp = parse{R"({"tag":"pi","value":3.14159})"};
    auto t = doc_comp.root().as<TaggedNumber>();
    REQUIRE(t.tag == "pi");
    REQUIRE(t.value == Catch::Approx(3.14159));

    auto doc_vec = parse{R"([{"tag":"a","value":1},{"tag":"b","value":2}])"};
    auto v = doc_vec.root().as<std::vector<TaggedNumber>>();
    REQUIRE(v.size() == 2);
    REQUIRE(v[0].tag == "a");
    REQUIRE(v[1].value == Catch::Approx(2.0));
  }
}

TEST_CASE("runtime object hash table") {
  SECTION("all keys correct") {
    constexpr int N = 100;
    auto doc = parse{make_obj(N)};
    auto root = doc.root();
    for (int i = 0; i < N; ++i) {
      std::string key = "key" + std::to_string(i);
      REQUIRE(root[key].as_num() == Catch::Approx(static_cast<double>(i)));
    }
  }

  SECTION("matches linear scan") {
    std::string small_json = R"({"a":1,"b":2,"c":3,"d":4,"e":5})";
    std::string large_json = "{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5";
    for (int i = 5; i < 50; ++i)
      large_json += ",\"x" + std::to_string(i) + "\":" + std::to_string(i);
    large_json += '}';

    auto small_doc = parse{small_json};
    auto large_doc = parse{large_json};

    for (auto const& key : {"a", "b", "c", "d", "e"}) {
      REQUIRE(small_doc.root()[key].as_num() == large_doc.root()[key].as_num());
    }
  }

  SECTION("missing key throws") {
    auto doc = parse{make_obj(50)};
    REQUIRE_THROWS(doc.root()["notakey"]);
  }

  SECTION("boundary 17 keys") {
    auto doc = parse{make_obj(17)};
    for (int i = 0; i < 17; ++i) {
      std::string key = "key" + std::to_string(i);
      REQUIRE(doc.root()[key].as_num() == Catch::Approx(static_cast<double>(i)));
    }
  }

  SECTION("last key in large object") {
    constexpr int N = 200;
    auto doc = parse{make_obj(N)};
    REQUIRE(doc.root()["key199"].as_num() == Catch::Approx(199.0));
  }

  SECTION("multiple large objects") {
    std::string json = "[" + make_obj(30) + "," + make_obj(30) + "]";
    auto doc = parse{json};
    auto arr = doc.root();
    REQUIRE(arr[0]["key0"].as_num() == Catch::Approx(0.0));
    REQUIRE(arr[0]["key29"].as_num() == Catch::Approx(29.0));
    REQUIRE(arr[1]["key0"].as_num() == Catch::Approx(0.0));
    REQUIRE(arr[1]["key29"].as_num() == Catch::Approx(29.0));
  }

  SECTION("nested large objects") {
    std::string inner = make_obj(20);
    std::string outer = "{\"inner\":" + inner;
    for (int i = 0; i < 19; ++i) outer += ",\"k" + std::to_string(i) + "\":" + std::to_string(i);
    outer += '}';

    auto doc = parse{outer};
    auto root = doc.root();
    REQUIRE(root["k0"].as_num() == Catch::Approx(0.0));
    REQUIRE(root["k18"].as_num() == Catch::Approx(18.0));
    REQUIRE(root["inner"]["key0"].as_num() == Catch::Approx(0.0));
    REQUIRE(root["inner"]["key19"].as_num() == Catch::Approx(19.0));
  }

  SECTION("string values in large object") {
    std::string json = "{";
    for (int i = 0; i < 30; ++i) {
      if (i > 0) json += ',';
      json += "\"k" + std::to_string(i) + "\":\"v" + std::to_string(i) + "\"";
    }
    json += '}';

    auto doc = parse{json};
    for (int i = 0; i < 30; ++i) {
      std::string key = "k" + std::to_string(i);
      std::string expected = "v" + std::to_string(i);
      REQUIRE(doc.root()[key].as_str() == std::string_view{expected});
    }
  }
}

TEST_CASE("runtime serialization") {
  SECTION("null") {
    std::string json = serialize(nullptr);
    REQUIRE(json == "null");
  }

  SECTION("booleans") {
    REQUIRE(serialize(true) == "true");
    REQUIRE(serialize(false) == "false");
  }

  SECTION("integers") {
    REQUIRE(serialize(0) == "0");
    REQUIRE(serialize(42) == "42");
    REQUIRE(serialize(-100) == "-100");
  }

  SECTION("floating-point") {
    REQUIRE(serialize(3.14) == "3.14");
    REQUIRE(serialize(0.0) == "0");
    REQUIRE(serialize(-2.5) == "-2.5");
  }

  SECTION("strings") {
    REQUIRE(serialize(std::string("hello")) == "\"hello\"");
    REQUIRE(serialize(std::string("")) == "\"\"");
    REQUIRE(serialize(std::string_view("world")) == "\"world\"");
  }

  SECTION("string escaping") {
    REQUIRE(serialize(std::string("hello \"world\"")) == "\"hello \\\"world\\\"\"");
    REQUIRE(serialize(std::string("path\\to\\file")) == "\"path\\\\to\\\\file\"");
    REQUIRE(serialize(std::string("line1\nline2")) == "\"line1\\nline2\"");
    REQUIRE(serialize(std::string("tab\there")) == "\"tab\\there\"");
    REQUIRE(serialize(std::string("backspace:\b")) == "\"backspace:\\b\"");
    REQUIRE(serialize(std::string("form feed:\f")) == "\"form feed:\\f\"");
    REQUIRE(serialize(std::string("carriage return:\r")) == "\"carriage return:\\r\"");
  }

  SECTION("control characters") {
    std::string control_char{'\x01'};
    std::string json = serialize(control_char);
    REQUIRE(json == "\"\\u0001\"");
  }

  SECTION("vector of integers") {
    std::vector<int> nums{1, 2, 3, 4, 5};
    REQUIRE(serialize(nums) == "[1,2,3,4,5]");
  }

  SECTION("vector of strings") {
    std::vector<std::string> strs{"hello", "world"};
    REQUIRE(serialize(strs) == "[\"hello\",\"world\"]");
  }

  SECTION("empty vector") {
    std::vector<int> empty;
    REQUIRE(serialize(empty) == "[]");
  }

  SECTION("pair") {
    std::pair<int, std::string> p{42, "answer"};
    REQUIRE(serialize(p) == "[42,\"answer\"]");
  }

  SECTION("tuple with mixed types") {
    std::tuple<int, std::string, bool> t{1, "test", true};
    REQUIRE(serialize(t) == "[1,\"test\",true]");
  }

  SECTION("empty tuple") {
    std::tuple<> empty_tuple;
    REQUIRE(serialize(empty_tuple) == "[]");
  }

  SECTION("optional with value") {
    std::optional<int> has_value{42};
    REQUIRE(serialize(has_value) == "42");
  }

  SECTION("optional null") {
    std::optional<int> no_value;
    REQUIRE(serialize(no_value) == "null");
  }

  SECTION("nested vectors") {
    std::vector<std::vector<int>> matrix{{1, 2}, {3, 4}, {5, 6}};
    REQUIRE(serialize(matrix) == "[[1,2],[3,4],[5,6]]");
  }

  SECTION("vector of pairs") {
    std::vector<std::pair<std::string, int>> items{{"x", 10}, {"y", 20}};
    REQUIRE(serialize(items) == "[[\"x\",10],[\"y\",20]]");
  }

  SECTION("vector of tuples") {
    std::vector<std::tuple<int, bool>> data{{1, true}, {2, false}};
    REQUIRE(serialize(data) == "[[1,true],[2,false]]");
  }

  SECTION("tuple of vectors") {
    std::tuple<std::vector<int>, std::vector<std::string>> t{{1, 2, 3}, {"a", "b"}};
    REQUIRE(serialize(t) == "[[1,2,3],[\"a\",\"b\"]]");
  }

  SECTION("optional of vector") {
    std::optional<std::vector<int>> opt{{1, 2, 3}};
    REQUIRE(serialize(opt) == "[1,2,3]");
  }

  SECTION("vector<int>") {
    std::vector<int> original{1, 2, 3};
    std::string json = serialize(original);
    auto doc = parse{json};
    std::vector<int> restored = doc.root().as<std::vector<int>>();
    REQUIRE(original == restored);
  }

  SECTION("tuple") {
    std::tuple<int, std::string, bool> original{42, "test", true};
    std::string json = serialize(original);
    auto doc = parse{json};
    auto restored = doc.root().as<std::tuple<int, std::string, bool>>();
    REQUIRE(original == restored);
  }

  SECTION("optional with value") {
    std::optional<int> original{123};
    std::string json = serialize(original);
    auto doc = parse{json};
    auto restored = doc.root().as<std::optional<int>>();
    REQUIRE(original == restored);
  }

  SECTION("optional null") {
    std::optional<int> original;
    std::string json = serialize(original);
    auto doc = parse{json};
    auto restored = doc.root().as<std::optional<int>>();
    REQUIRE(!restored.has_value());
  }

  SECTION("nested structures") {
    std::vector<std::vector<int>> original{{1, 2}, {3, 4}};
    std::string json = serialize(original);
    auto doc = parse{json};
    auto restored = doc.root().as<std::vector<std::vector<int>>>();
    REQUIRE(original == restored);
  }

  SECTION("Point struct") {
    Point p{3.14, 2.71};
    std::string json = serialize(p);
    REQUIRE(json == "{\"x\":3.14,\"y\":2.71}");
  }

  SECTION("TaggedNumber struct") {
    TaggedNumber tn{"answer", 42.0};
    std::string json = serialize(tn);
    REQUIRE(json == "{\"tag\":\"answer\",\"value\":42}");
  }

  SECTION("round-trip custom type") {
    Point original{1.5, 2.5};
    std::string json = serialize(original);
    auto doc = parse{json};
    Point restored = doc.root().as<Point>();
    REQUIRE(restored.x == Catch::Approx(original.x));
    REQUIRE(restored.y == Catch::Approx(original.y));
  }

  SECTION("vector of custom types") {
    std::vector<Point> points{{1.0, 2.0}, {3.0, 4.0}};
    std::string json = serialize(points);
    REQUIRE(json == "[{\"x\":1,\"y\":2},{\"x\":3,\"y\":4}]");
  }
}

// MSVC's constexpr support is not good enough.
#if !defined(_MSC_VER) || defined(__clang__)
TEST_CASE("compile-time serialization") {
  using namespace std::string_view_literals;

  SECTION("primitive types") {
    // Needs static storage to null_sv holds a constant address of serialized string_view.
    static constexpr auto null_json = static_serialize(nullptr);
    STATIC_REQUIRE(null_json.size() == 4);
    STATIC_REQUIRE(null_json[0] == 'n');
    STATIC_REQUIRE(null_json[1] == 'u');
    STATIC_REQUIRE(null_json[2] == 'l');
    STATIC_REQUIRE(null_json[3] == 'l');
    // With implicit conversion to string_view:
    constexpr std::string_view null_sv = null_json;
    STATIC_REQUIRE(null_sv == "null"sv);

    static constexpr auto true_json = static_serialize(true);
    STATIC_REQUIRE(true_json.size() == 4);
    constexpr std::string_view true_sv = true_json;
    STATIC_REQUIRE(true_sv == "true"sv);

    static constexpr auto false_json = static_serialize(false);
    STATIC_REQUIRE(false_json.size() == 5);
    constexpr std::string_view false_sv = false_json;
    STATIC_REQUIRE(false_sv == "false"sv);

    static constexpr auto num_json = static_serialize(42);
    STATIC_REQUIRE(num_json.size() == 2);
    STATIC_REQUIRE(num_json[0] == '4');
    STATIC_REQUIRE(num_json[1] == '2');

    static constexpr auto zero_json = static_serialize(0);
    STATIC_REQUIRE(zero_json.size() == 1);
    STATIC_REQUIRE(zero_json[0] == '0');
    constexpr std::string_view zero_sv = zero_json;
    STATIC_REQUIRE(zero_sv == "0"sv);

    static constexpr auto str_json = static_serialize("hello"sv);
    STATIC_REQUIRE(str_json.size() == 7);
    STATIC_REQUIRE(str_json[0] == '"');
    STATIC_REQUIRE(str_json[6] == '"');
    constexpr std::string_view str_sv = str_json;
    STATIC_REQUIRE(str_sv == "\"hello\""sv);
  }

  SECTION("negative numbers") {
    constexpr auto neg_json = static_serialize(-42);
    STATIC_REQUIRE(neg_json.size() == 3);
    STATIC_REQUIRE(neg_json[0] == '-');
    STATIC_REQUIRE(neg_json[1] == '4');
    STATIC_REQUIRE(neg_json[2] == '2');
    STATIC_REQUIRE(neg_json == "-42"sv);
  }

  SECTION("floating-point numbers") {
    constexpr auto float_json = static_serialize(3.5);
    STATIC_REQUIRE(float_json.size() == 3);
    STATIC_REQUIRE(float_json[0] == '3');
    STATIC_REQUIRE(float_json[1] == '.');
    STATIC_REQUIRE(float_json[2] == '5');
    STATIC_REQUIRE(float_json == "3.5"sv);
  }

  SECTION("string escaping at compile time") {
    constexpr auto escaped_json = static_serialize("line1\nline2"sv);
    STATIC_REQUIRE(escaped_json.size() == 14);  // "line1\nline2"
    STATIC_REQUIRE(escaped_json[6] == '\\');
    STATIC_REQUIRE(escaped_json[7] == 'n');
    STATIC_REQUIRE(escaped_json == "\"line1\\nline2\""sv);

    constexpr auto quote_json = static_serialize("say \"hi\""sv);
    STATIC_REQUIRE(quote_json.size() == 12);
    STATIC_REQUIRE(quote_json[5] == '\\');
    STATIC_REQUIRE(quote_json[6] == '"');

    constexpr auto tab_json = static_serialize("a\tb"sv);
    STATIC_REQUIRE(tab_json == "\"a\\tb\""sv);

    constexpr auto backslash_json = static_serialize("path\\file"sv);
    STATIC_REQUIRE(backslash_json == "\"path\\\\file\""sv);
  }

  SECTION("tuple at compile time") {
    constexpr std::tuple<int, bool, int> tup{1, true, 2};
    constexpr auto tup_json = static_serialize(tup);
    STATIC_REQUIRE(tup_json.size() == 10);  // [1,true,2]
    STATIC_REQUIRE(tup_json[0] == '[');
    STATIC_REQUIRE(tup_json[9] == ']');
    STATIC_REQUIRE(tup_json[1] == '1');
    STATIC_REQUIRE(tup_json[2] == ',');
    STATIC_REQUIRE(tup_json == "[1,true,2]"sv);

    constexpr std::tuple<int> single{99};
    constexpr auto single_json = static_serialize(single);
    STATIC_REQUIRE(single_json == "[99]"sv);

    constexpr std::tuple<> empty{};
    constexpr auto empty_json = static_serialize(empty);
    STATIC_REQUIRE(empty_json == "[]"sv);
  }

  SECTION("pair at compile time") {
    constexpr std::pair<int, bool> p{42, false};
    constexpr auto pair_json = static_serialize(p);
    STATIC_REQUIRE(pair_json.size() == 10);  // [42,false]
    STATIC_REQUIRE(pair_json[0] == '[');
    STATIC_REQUIRE(pair_json[9] == ']');
    STATIC_REQUIRE(pair_json == "[42,false]"sv);

    constexpr std::pair<bool, bool> bools{true, true};
    constexpr auto bools_json = static_serialize(bools);
    STATIC_REQUIRE(bools_json == "[true,true]"sv);
  }

  SECTION("optional at compile time") {
    constexpr std::optional<int> some{123};
    constexpr auto some_json = static_serialize(some);
    STATIC_REQUIRE(some_json.size() == 3);
    STATIC_REQUIRE(some_json[0] == '1');
    STATIC_REQUIRE(some_json[1] == '2');
    STATIC_REQUIRE(some_json[2] == '3');

    constexpr std::optional<int> none = std::nullopt;
    constexpr auto none_json = static_serialize(none);
    STATIC_REQUIRE(none_json.size() == 4);
    STATIC_REQUIRE(none_json == "null"sv);

    constexpr std::optional<bool> some_bool{true};
    constexpr auto some_bool_json = static_serialize(some_bool);
    STATIC_REQUIRE(some_bool_json == "true"sv);
  }

  SECTION("nested structures at compile time") {
    constexpr std::tuple<int, std::pair<int, int>> nested{1, {2, 3}};
    constexpr auto nested_json = static_serialize(nested);
    STATIC_REQUIRE(nested_json == "[1,[2,3]]"sv);

    constexpr std::pair<std::optional<int>, std::optional<int>> opt_pair{42, std::nullopt};
    constexpr auto opt_json = static_serialize(opt_pair);
    STATIC_REQUIRE(opt_json == "[42,null]"sv);
  }

  SECTION("custom type at compile time") {
    constexpr Point p{3, 2};
    constexpr auto point_json = static_serialize(p);
    STATIC_REQUIRE(point_json == "{\"x\":3,\"y\":2}"sv);

    // "answer", as a string literal, should have static storage duration.
    static constexpr TaggedNumber tn{"answer", 42};
    constexpr auto tn_json = static_serialize(tn);
    STATIC_REQUIRE(tn_json == "{\"tag\":\"answer\",\"value\":42}"sv);
  }

  SECTION("compile-time to runtime round-trip") {
    // Serialize at compile time
    constexpr auto ct_json = static_serialize(std::tuple{1, 2, 3});

    // Parse at runtime
    auto doc = parse{std::string(ct_json)};
    auto restored = doc.root().as<std::tuple<int, int, int>>();

    REQUIRE(std::get<0>(restored) == 1);
    REQUIRE(std::get<1>(restored) == 2);
    REQUIRE(std::get<2>(restored) == 3);
  }
}
#endif
