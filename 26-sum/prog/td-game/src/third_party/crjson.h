// MIT License
//
// Copyright (c) 2026 registerGen
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/// crjson - a small C++20 (C)ompile-time and (R)untime (JSON) library.
///
/// Usage:
///
/// #include <iostream>
/// #include <string_view>
///
/// #include "crjson.h"
///
/// using namespace crjson;
/// using namespace std::string_view_literals;
///
/// using person_schema = jobj<
///   jkv<"name"_fs, jstr>,  // Use _fs UDL to create fixed_string NTTP for keys in schema.
///   jkv<"age"_fs, jnum>,
///   jkv<"tags"_fs, jarr<jstr>>
/// >;  // The order of key-value pairs in jobj does not matter.
///
/// using coord_schema = jtup<jnum, jnum, jstr>;  // Use jtup for heterogeneous arrays (tuples).
///
/// struct labeled_point {
///   double x, y;
///   std::string_view label;
/// };
///
/// // Enable as<labeled_point>() and serialize() by specializing jconvert.
/// // Implement from() for de-serialization, to() for serialization.
/// template <>
/// struct jconvert<labeled_point> {
///   template <typename VA>
///   static constexpr labeled_point from(VA const& a) {
///     return labeled_point{a[0].as_num(), a[1].as_num(), a[2].as_str()};
///   }
///
///   template <typename Builder>
///   static constexpr void to(Builder& b, labeled_point const& p) {
///     b.begin_arr();
///     b.arr_elem(); b.bld_num(p.x);
///     b.arr_elem(); b.bld_num(p.y);
///     b.arr_elem(); b.bld_str(p.label);
///     b.end_arr();
///   }
/// };
///
/// int main() {
///   // Bind a JSON document to a schema at compile time.
///   // The JSON text is parsed and validated against the schema at compile time.
///   // Errors are reported as compile-time errors.
///   constexpr static_bind<R"({"name":"Ada","age":37,"tags":["math"]})"_fs, person_schema>
///     ct_doc1{};
///   static_assert(ct_doc1["name"].as_str() == "Ada"sv);
///
///   // Parse a JSON document without validating against schema at compile time.
///   constexpr auto ct_doc2 = static_parse<R"({"key1":"Ada","key2":37})"_fs>{};
///   static_assert(ct_doc2["key2"].as_num() == 37.0);
///
///   constexpr auto ct_doc3 =
///     static_parse<R"({"name":"Bob","age":30,"tags":["programming"]})"_fs>{}.root();
///   // ct_doc3 is of type `static_accessor`. The underlying document is stored in a
///   // program-lifetime static, so there is no lifetime concern here.
///   constexpr auto ct_a3 = ct_doc3["tags"];
///   static_assert(ct_a3[0].as_str() == "programming"sv);
///
///   constexpr auto ct_tuple = static_bind<R"([1, 2, "label"])"_fs, coord_schema>{};
///   static_assert(ct_tuple.root().type() == jtype::arr_t);  // Tuples are arrays in JSON.
///   static_assert(ct_tuple[0].as_num() == 1.0);
///   static_assert(ct_tuple[2].as_str() == "label"sv);
///   // Convert JSON to labeled_point using as<labeled_point>().
///   constexpr auto ct_point = ct_tuple.root().as<labeled_point>();
///   static_assert(ct_point.x == 1.0 && ct_point.y == 2.0);
///   static_assert(ct_point.label == "label"sv);
///
///   // Bind a JSON document to a schema at runtime.
///   // The JSON text is parsed and validated against the schema at runtime.
///   // Errors are reported through exceptions.
///   bind<person_schema> doc1{R"({"name":"Ada","age":37,"tags":["math","logic"]})"};
///   std::cout << "name=" << doc1["name"].as_str() << "\n";  // name=Ada
///   std::cout << "age=" << doc1["age"].as_num() << "\n";  // age=37
///   std::cout << "first_tag=" << doc1["tags"][0].as_str() << "\n";  // first_tag=math
///
///   // Parse a JSON document without validating against schema at runtime.
///   auto doc2 = parse{R"({"key1":"Ada","key2":37})"};
///   std::cout << "key2=" << doc2["key2"].as_num() << "\n";  // key2=37
///
///   auto doc3 = parse{R"({"name":"Bob","age":30,"tags":["programming"]})"};
///   // a3 is of type `accessor`, which holds a pointer to the document in doc3.
///   auto a3 = doc3.root();
///   // The two lines above must NOT combine into a single line like
///   // `auto a3 = parse{...}.root();`,
///   // since that would call root() on an rvalue parse object, which is not allowed to prevent
///   // dangling pointer in the accessor.
///   a3 = a3["tags"][0];
///   std::cout << "first_tag=" << a3.as_str() << "\n";  // first_tag=programming
///
///   // Serialize C++ objects to JSON.
///   labeled_point p{1.0, 2.0, "origin"};
///   std::string json = serialize(p);
///   std::cout << "serialized=" << json << "\n";  // serialized=[1,2,"origin"]
///
///   // Serialize standard types.
///   std::vector<int> nums{1, 2, 3};
///   std::cout << "vector: " << serialize(nums) << "\n";  // vector: [1,2,3]
///
///   std::tuple<std::string, int, bool> data{"test", 42, true};
///   std::cout << "tuple: " << serialize(data) << "\n";  // tuple: ["test",42,true]
///
///   // Round-trip conversion (deserialize -> serialize).
///   auto parsed = parse{R"([3, 4, "point"])"};
///   auto round_trip_json = serialize(parsed.root().as<labeled_point>());
///   std::cout << "round-trip: " << round_trip_json << "\n";  // round-trip: [3,4,"point"]
///
///   // Compile-time serialization.
///   constexpr auto ct_data = static_serialize(std::tuple{"test"sv, 42, true});
///   static_assert(ct_data == "[\"test\",42,true]"sv);
///
///   return 0;
/// }

#ifndef CRJSON_H
#define CRJSON_H

#if __cplusplus < 202002L && (!defined(_MSVC_LANG) || _MSVC_LANG < 202002L)
# error "C++20 or later is required to use crjson"
#endif

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

// The outline of the library is as follows:
//
// namespace crjson {
//
// concept schema;
//
// struct jnull;         // Schema for JSON null.
// struct jbool;         // Schema for JSON boolean.
// struct jnum;          // Schema for JSON number.
// struct jstr;          // Schema for JSON string.
// struct jarr<T>;       // Schema for JSON array with element schema T.
// struct jtup<Ts...>;   // Schema for JSON array with heterogeneous element schemas Ts.
// struct jkv<Key, T>;   // Schema for JSON object key-value pair with key and value schema T.
// struct jobj<KVs...>;  // Schema for JSON object with key-value pairs KVs.
//
// struct jconvert<T>;  // Hook for converting between JSON values and user-defined types T.
//                      // Implement from() for deserialization, to() for serialization.
//
// namespace detail {
//
// enum jtype;
//
// struct node;  // Representation of a JSON node.
//
// concept source;         // Concept for source, which can access context at arbitrary indices.
// concept pool;           // Concept for pool, which stores parsed nodes, key-value entries, string
//                         // values, index list, and stack.
// concept accessor;       // Concept for accessor, which can access parsed nodes or string values.
// concept builder;        // Concept for builder, which serializes JSON values to output.
// concept error_handler;  // Concept for error handler, which handles errors.
//
// class parser<Src, Pool, Err>;
//                       // Generic parser that works with any source, pool, and error handler.
//                       // It parses JSON text from the source, constructs nodes and other data in
//                       // the pool, and reports errors through the error handler.
//
// class validator<Accessor, Err>;
//                       // Generic schema validator that works with any accessor and error handler.
//                       // It validates parsed JSON nodes, accessed through the accessor, against
//                       // the schema, and reports errors through the error handler.
//
// class value_accessor<Accessor, Err>;
//                       // Generic value accessor that works with any accessor and error handler.
//                       // It provides convenient APIs to access JSON values, and reports errors
//                       // through the error handler.
//
// namespace ct {
//
// // S = JSON string literal, the source for compile-time parsing.
//
// struct source<S>;
// class pool<S>;
// class document<S>;  // Parsed JSON document, containing the pool and the index of the root node.
// class accessor<S>;  // An accessor has a pointer to the document.
// struct error_handler;
//
// document<S> parse_impl();
//
// class parse<S>;         // Entry point for compile-time parsing.
// class bind<S, Schema>;  // Entry point for compile-time parsing and validating.
//
// class builder;
// builder static_serialize(T const& value);  // Entry point for serialization.
//
// }  // namespace ct
//
// namespace rt {
//
// struct source;  // JSON string literal is passed as std::string_view at runtime.
// class pool;
// class document;
// class accessor;
// struct error_handler;
//
// document parse_impl(std::string_view json);
//
// class parse;         // Entry point for runtime parsing.
// class bind<Schema>;  // Entry point for runtime parsing and validating.
//
// class builder;
// std::string serialize(T const& value);  // Entry point for serialization.
//
// }  // namespace rt
//
// }  // namespace detail
//
// // Public interfaces.
//
// using detail::jtype;
// using static_parse<S> = detail::ct::parse<S>;
// using static_bind<S, Schema> = detail::ct::bind<S, Schema>;
// using static_accessor<S> =
//   detail::value_accessor<detail::ct::accessor<S>, detail::ct::error_handler>;
// using parse = detail::rt::parse;
// using bind<Schema> = detail::rt::bind<Schema>;
// using accessor = detail::value_accessor<detail::rt::accessor, detail::rt::error_handler>;
// using detail::ct::static_serialize;
// using detail::rt::serialize;
//
// // Built-in jconvert specializations for common standard types.
//
// struct jconvert<T>;  // T = std::nullptr_t, bool, arithmetic types, std::string_view,
//                      //     std::string, std::vector, std::pair, std::tuple,
//                      //     std::optional
//
// }  // namespace crjson

namespace crjson {

/// NTTP (non-type template parameter) for string literals.
/// C++20 does not allow raw string literals as NTTPs directly, so we wrap them in this struct.
/// fixed_string<N> can be used as a template parameter, enabling compile-time string keys and
/// JSON source text (e.g. jkv<"name"_fs, jstr> and static_parse<R"(...)"_fs>).
template <std::size_t N>
struct fixed_string {
  static constexpr std::size_t size = N - 1;

  char data[N]{};

  constexpr fixed_string(char const (&s)[N]) {
    for (std::size_t i = 0; i < N; ++i) data[i] = s[i];
  }

  constexpr char operator[](std::size_t i) const { return data[i]; }
};

/// UDL (user-defined literal) for fixed_string.
template <fixed_string S>
constexpr auto operator""_fs() {
  return S;
}

// JSON schema types and concepts.

template <typename>
struct is_schema : std::false_type { };

template <typename T>
concept schema = is_schema<T>::value;

template <typename>
struct is_kv_schema : std::false_type { };

template <typename T>
concept kv_schema = is_kv_schema<T>::value;

/// Schema for JSON null.
struct jnull { };

template <>
struct is_schema<jnull> : std::true_type { };

/// Schema for JSON boolean.
struct jbool { };

template <>
struct is_schema<jbool> : std::true_type { };

/// Schema for JSON number.
struct jnum { };

template <>
struct is_schema<jnum> : std::true_type { };

/// Schema for JSON string.
struct jstr { };

template <>
struct is_schema<jstr> : std::true_type { };

/// Schema for homogeneous JSON array, where all elements conform to the same schema T.
/// Note that the parser does not enforce the homogeneity of array elements, so only
/// the validator checks that all elements conform to T when validating against the schema.
template <schema T>
struct jarr {
  using value_type = T;
};

template <schema T>
struct is_schema<jarr<T>> : std::true_type { };

/// Schema for a heterogeneous JSON array where each positional element must conform to the
/// corresponding schema in Ts. The element count must exactly match sizeof...(Ts).
template <schema... Ts>
struct jtup {
  using value_types = std::tuple<Ts...>;
};

template <schema... Ts>
struct is_schema<jtup<Ts...>> : std::true_type { };

/// Schema for JSON object key-value pair.
template <fixed_string Key, schema T>
struct jkv {
  static constexpr auto key = Key;
  using value_type = T;
};

template <fixed_string Key, schema T>
struct is_kv_schema<jkv<Key, T>> : std::true_type { };

/// Schema for JSON object.
template <kv_schema... KVs>
struct jobj { };

template <kv_schema... KVs>
struct is_schema<jobj<KVs...>> : std::true_type { };

/// Hook for converting a JSON value to a user-defined type T.
///
/// Users should specialize this template and implement following methods:
/// - template <typename VA> static [constexpr] T from(VA const& a);
/// - template <typename B> static [constexpr] void to(B& b, T const& value);
///   where VA is a value_accessor, B is a builder (see below),
///   constexpr is optional but required for compile-time usage.
/// for their own types to enable conversion between JSON values and their types.
///
/// from(a) is used in value_accessor::as<T>() for de-serialization, while
/// to(b, value) is used in serialize(T const& value) for serialization.
template <typename T>
struct jconvert;

namespace detail {

// Algorithms for parsing JSON, validating against schema, and accessing values.
//
// We implement a generic parsing, validation, and value accessing algorithm that can work with any
// source, pool, accessor, and error handler, as long as they satisfy the required concepts. This
// allows us to unify the logic of compile-time and runtime parsing, and enables users to provide
// their own sources, pools, accessors, and error handlers if they want to.
//
// Data flow (one direction, no back-edges):
//
//
//   source                   schema
//      |                        |
//      v                        v                (thin view over pool)
//   parser  -->  pool  -->  validator (optional)  -->  accessor  -->  value_accessor
//                                                          (user-facing API: operator[], as_*, ...)
//
//
// 1. `source`          abstracts character-level access to the JSON text.
// 2. `parser`          walks the source, builds nodes / kv entries / strings in the `pool`,
//                      and returns the root node index.
// 3. `validator`       walks the node tree (via `accessor`) and checks it against a schema.
//                      Skipped entirely when using parse / static_parse (no schema).
// 4. `accessor`        wraps a const pointer to the pool and provides typed lookup helpers.
// 5. `value_accessor`  wraps an accessor + a node index and exposes the public query API.
//
// ct:: and rt:: each provide concrete implementations of source, pool, accessor, and
// error_handler that satisfy the concepts above. The parser, validator, and value_accessor
// templates are instantiated with these implementations and require no further specialisation.

/// Define CRJSON_64BIT_SIZE_TYPE before including this header to use 64-bit pool indices
/// (std::size_t). This increases node size from 16 to 24 bytes but supports pools larger
/// than 4 GiB. By default 32-bit indices are used.
using size_type =
#ifdef CRJSON_64BIT_SIZE_TYPE
  std::size_t;
#else
  std::uint32_t;
#endif

// JSON node definitions begin.

/// Type tag for JSON nodes. This is the return type of value_accessor::type().
enum jtype : std::uint8_t { null_t, bool_t, num_t, str_t, arr_t, obj_t };

// Tags to disambiguate constructors.
// Tuples are arrays in JSON, and they only differ in the sense of validators, so they share
// the same arr_tag in the node.

struct null_tag { };
struct bool_tag { };
struct num_tag { };
struct str_tag { };
struct arr_tag { };
struct obj_tag { };

// Representation of strings, arrays, and objects.

// A view into the string pool, representing a JSON string value.
// The actual characters of the string are str_pool[begin..begin+len-1] in the pool.
struct str_view {
  size_type begin;
  size_type len;
};

// A view into the index list, representing a JSON array or tuple value.
// Child nodes are NOT stored contiguously in the node pool (which is append-only), so we
// maintain a separate index_list that maps sequential positions to node pool indices.
// The actual child nodes are nodes[index_list[begin..begin+len-1]] in the pool.
struct arr_view {
  size_type begin;
  size_type len;
};

// A view into the index list, representing a JSON object value.
// Same indirection as arr_view: contiguous key-value entry indices live in index_list,
// not directly in the kv pool, for the same reason (see arr_view).
// The actual key-value entries are kvs[index_list[begin..begin+len-1]] in the pool.
struct obj_view {
  size_type begin;
  size_type len;
};

// A key-value entry for JSON object, consisting of a key and the pool index of the value node.
struct kv_entry {
  str_view key;
  size_type value_idx;
};

// A JSON node, consisting of a type tag and a union of possible values. The actual values are
// stored in the pool, and the node only stores indices into the pool.
struct node {
  jtype type;

  // A union of all possible JSON value representations.
  // Explicit constexpr constructors are required for each member because C++ does not
  // synthesize constexpr constructors for unions that contain non-trivial members (e.g.
  // str_view, arr_view, obj_view have user-defined members). Tag types are used to
  // disambiguate which member is being initialized, mirroring the node constructors below.
  union value_t {
    struct {
    } empty;  // for null
    bool b;
    double num;
    str_view str;
    arr_view arr;
    obj_view obj;

    constexpr value_t() : empty{} { }
    constexpr value_t(bool_tag, bool v) : b{v} { }
    constexpr value_t(num_tag, double v) : num{v} { }
    constexpr value_t(str_tag, size_type begin, size_type len) : str{begin, len} { }
    constexpr value_t(arr_tag, size_type begin, size_type len) : arr{begin, len} { }
    constexpr value_t(obj_tag, size_type begin, size_type len) : obj{begin, len} { }
  } value;

  constexpr node() : type{null_t}, value{} { }
  constexpr node(null_tag) : type{null_t}, value{} { }
  constexpr node(bool_tag, bool v) : type{bool_t}, value{bool_tag{}, v} { }
  constexpr node(num_tag, double v) : type{num_t}, value{num_tag{}, v} { }
  constexpr node(str_tag, size_type begin, size_type len)
      : type{str_t}, value{str_tag{}, begin, len} { }
  constexpr node(arr_tag, size_type begin, size_type len)
      : type{arr_t}, value{arr_tag{}, begin, len} { }
  constexpr node(obj_tag, size_type begin, size_type len)
      : type{obj_t}, value{obj_tag{}, begin, len} { }

  constexpr bool as_bool() const { return value.b; }
  constexpr double as_num() const { return value.num; }
  constexpr str_view as_str() const { return value.str; }
  // There are no separate views for tuples and arrays,
  // since they are represented the same way in the node.
  constexpr arr_view as_arr() const { return value.arr; }
  constexpr obj_view as_obj() const { return value.obj; }
};

static_assert(sizeof(node) == (sizeof(size_type) == 4 ? 16 : 24));

// Concept for `source`, which can access `context` at arbitrary indices.
//
// Src should implement:
// - Src::ctx                                       Type of the context.
// - Src::at(Src::ctx const&, std::size_t) -> char  Character at the given index in the context.
// - Src::size(Src::ctx const&) -> std::size_t      Size of the context.
// - Src::data(Src::ctx const&) -> char const*      Pointer to the raw string data of the context.
//                                                  (Runtime only)
// - Src::safe_str_run(Src::ctx const&, std::size_t) -> std::size_t
//                                                  Number of characters that can be safely
//                                                  bulk-copied from the given index. (Runtime only)
// - Src::has_fast_parse_num: bool                  Constant indicating whether the source
//                                                  supports fast number parsing.
template <typename Src>
concept source =
  requires { typename Src::ctx; } && requires(typename Src::ctx const& ctx, std::size_t idx) {
    { Src::at(ctx, idx) } -> std::convertible_to<char>;
    { Src::size(ctx) } -> std::convertible_to<std::size_t>;
    { Src::data(ctx) } -> std::convertible_to<char const*>;
    { Src::safe_str_run(ctx, idx) } -> std::convertible_to<std::size_t>;
    { Src::has_fast_parse_num } -> std::convertible_to<bool>;
  };

// Concept for `pool`, which stores parsed JSON nodes, key-value entries, string values, the
// index list which stores the indices of child nodes for arrays and objects, and a stack for
// temporary storage during parsing.
//
// The stack is used during array/object parsing: as child values are parsed they are pushed onto
// the stack. When the closing `]` or `}` is reached, the total child count is known, and all
// accumulated indices are flushed from the stack into the contiguous index_list in one pass.
// This two-phase approach allows arr_view/obj_view to always refer to a contiguous slice of
// index_list even though children are interleaved with other nodes in the node pool.
//
// Pool should implement:
// - pool.push_node(node) -> size_type           Add node to the pool and returns its pool index.
//                                               (The index is not added to the stack or index list)
// - pool.push_kv(kv_entry) -> size_type         Add key-value entry to the pool and
//                                               return its pool index.
//                                               (The index is not added to the stack or index list)
// - pool.push_str(char) -> void                 Add char to the string pool.
// - pool.push_str_bulk(char const*, std::size_t) -> void
//                                               Bulk-copy characters from the given pointer
//                                               into the string pool.
// - pool.push_index(size_type) -> void          Add index to the index list.
// - pool.push_stack(size_type) -> void          Add index to the stack.
// - pool.node_at(size_type) -> node const&      Get node at the given pool index.
// - pool.kv_at(size_type) -> kv_entry const&    Get key-value entry at the given pool index.
// - pool.str_at(size_type) -> char              Get char at the given index in the string pool.
// - pool.str_data() -> char const*              Get raw string pool data.
// - pool.index_at(size_type) -> size_type       Get index at the given position in the index list.
// - pool.stack_at(size_type) -> size_type       Get index at the given position in the stack.
// - pool.str_len() -> size_type                 Number of characters in the string pool.
// - pool.index_len() -> size_type               Number of indices in the index list.
// - pool.stack_len() -> size_type               Number of indices in the stack.
// - pool.reset_stack(size_type) -> void         Truncate the stack to the given length.
// - pool.on_obj_parsed(obj_view) -> void        Callback when an object is fully parsed.
//                                               (Used for runtime hash table speedup)
template <typename Pool>
concept pool =
  requires(Pool& p, node n, kv_entry kv, char c, size_type idx, char const* str, obj_view ov) {
    { p.push_node(n) } -> std::convertible_to<size_type>;
    { p.push_kv(kv) } -> std::convertible_to<size_type>;
    { p.push_str(c) } -> std::same_as<void>;
    { p.push_str_bulk(str, idx) } -> std::same_as<void>;
    { p.push_index(idx) } -> std::same_as<void>;
    { p.push_stack(idx) } -> std::same_as<void>;
    { p.node_at(idx) } -> std::convertible_to<node const&>;
    { p.kv_at(idx) } -> std::convertible_to<kv_entry const&>;
    { p.str_at(idx) } -> std::convertible_to<char>;
    { p.str_data() } -> std::convertible_to<char const*>;
    { p.index_at(idx) } -> std::convertible_to<size_type>;
    { p.stack_at(idx) } -> std::convertible_to<size_type>;
    { p.str_len() } -> std::convertible_to<size_type>;
    { p.index_len() } -> std::convertible_to<size_type>;
    { p.stack_len() } -> std::convertible_to<size_type>;
    { p.reset_stack(idx) } -> std::same_as<void>;
    { p.on_obj_parsed(ov) } -> std::same_as<void>;
  };

// Concept for `accessor`, which can access parsed JSON nodes or chars in the string pool.
//
// Two overloads of obj_at are required:
// - obj_at<Key>(obj_view) is the compile-time overload: the key is a fixed_string NTTP, enabling
//   the validator to use schema-defined keys as template arguments.
// - obj_at(obj_view, string_view) is the runtime overload: the key is a runtime string, used
//   by value_accessor::operator[](string_view) for dynamic key lookups.
//
// Accessor should implement:
// - accessor.node_at(size_type) -> node const&      Get the node at the given pool index.
// - accessor.arr_at(arr_view, size_type) -> size_type
//                                                   Get the node pool index of the child
//                                                   at the given position in the array.
// - accessor.obj_at<key>(obj_view) -> size_type     Get the node pool index of the value
//                                                   of the given key in the object.
// - accessor.obj_at(obj_view, std::string_view) -> size_type
//                                                   Get the node pool index of the value
//                                                   of the given key in the object.
// - accessor.str_at(str_view, size_type) -> char    Get the character at the given position
//                                                   in the string pool.
// - accessor.str_eq(str_view, std::string_view) -> bool
//                                                   Compares the string represented by str_view
//                                                   with the given string literal for equality.
// - accessor.as_std_sv(str_view) -> std::string_view
//                                                   Get the string represented by str_view
//                                                   as std::string_view.
template <typename Accessor>
concept accessor = requires(Accessor const& a, node const& n, size_type idx, std::string_view sv) {
  { a.node_at(idx) } -> std::convertible_to<node const&>;
  { a.arr_at(n.as_arr(), idx) } -> std::convertible_to<size_type>;
  { a.template obj_at<"key"_fs>(n.as_obj()) } -> std::convertible_to<size_type>;
  { a.obj_at(n.as_obj(), sv) } -> std::convertible_to<size_type>;
  { a.str_at(n.as_str(), idx) } -> std::convertible_to<char>;
  { a.str_eq(n.as_str(), sv) } -> std::convertible_to<bool>;
  { a.as_std_sv(n.as_str()) } -> std::convertible_to<std::string_view>;
};

// Concept for `builder`, which serializes JSON values to output.
//
// Builder should implement:
// - bld_null() -> void                 Output JSON null.
// - bld_bool(bool) -> void             Output JSON boolean.
// - bld_numb(double) -> void           Output JSON number.
// - bld_str(std::string_view) -> void  Output JSON string (with escaping).
// - begin_arr() -> void                Begin a JSON array.
// - arr_elem() -> void                 Signal an array element (for comma management).
// - end_arr() -> void                  End a JSON array.
// - begin_obj() -> void                Begin a JSON object.
// - key(std::string_view) -> void      Output a JSON object key.
// - end_obj() -> void                  End a JSON object.
// - result() -> T                      Get the final serialized JSON string from the builder.
template <typename B>
concept builder = requires(B b, std::string_view sv, double d, bool bl) {
  { b.bld_null() } -> std::same_as<void>;
  { b.bld_bool(bl) } -> std::same_as<void>;
  { b.bld_num(d) } -> std::same_as<void>;
  { b.bld_str(sv) } -> std::same_as<void>;
  { b.begin_arr() } -> std::same_as<void>;
  { b.arr_elem() } -> std::same_as<void>;
  { b.end_arr() } -> std::same_as<void>;
  { b.begin_obj() } -> std::same_as<void>;
  { b.key(sv) } -> std::same_as<void>;
  { b.end_obj() } -> std::same_as<void>;
  { b.result() };
};

// Concept for `error_handler`, which handles errors during parsing and validation.
//
// ErrorHandler should implement:
// - ErrorHandler::err(char const *const) -> void  Handle the error with the given message.
template <typename Err>
concept error_handler = requires(char const* const msg) {
  { Err::err(msg) } -> std::same_as<void>;
};

namespace helpers {

// Helper functions for char classification, number parsing, etc.

constexpr bool is_ws(char c) noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
constexpr bool is_nonzero_digit(char c) noexcept { return c >= '1' && c <= '9'; }
constexpr bool is_hex(char c) noexcept {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Returns the numeric value of a hex digit (0-9 -> 0-9, a-f/A-F -> 10-15).
// Callers always validate with is_hex (or is_digit for the number slow-path), so c is
// guaranteed to be a valid hex or decimal digit.
constexpr int as_hex(char c) noexcept { return c <= '9' ? c - '0' : 10 + ((c | 0x20) - 'a'); }

// Computes 10^e for integer e. Implemented manually because std::pow is not constexpr.
constexpr double pow10(int e) noexcept {
  double r = 1.0;
  if (e >= 0) {
    for (int i = 0; i < e; ++i) r *= 10.0;
  } else {
    for (int i = 0; i < -e; ++i) r /= 10.0;
  }
  return r;
}

// Cursor to represent the current position in the source during parsing.
// It is a separate object (not embedded in the parser) so it can be passed by reference
// through all recursive parse_* calls, letting every level share and mutate the same
// position without the parser itself being stateful.
class cursor final {
public:
  constexpr std::size_t pos() const noexcept { return pos_; }
  constexpr void advance(std::size_t n = 1) noexcept { pos_ += n; }
  constexpr void set_pos(std::size_t pos) noexcept { pos_ = pos; }

private:
  std::size_t pos_ = 0;
};

}  // namespace helpers

// Generic parser.
template <source Src, pool Pool, error_handler Err>
class parser final {
  using Ctx = typename Src::ctx;

public:
  // Parse a JSON value, which can be null, boolean, number, string, array, or object.
  static constexpr node parse(Ctx const& ctx, helpers::cursor& cur, Pool& p) {
    skip_ws(ctx, cur);
    char c = peek(ctx, cur);
    if (c == '\0') Err::err("json: unexpected end of input");

    if (c == 'n') return parse_null(ctx, cur);
    if (c == 't') return parse_true(ctx, cur);
    if (c == 'f') return parse_false(ctx, cur);
    if (c == '"') return parse_string(ctx, cur, p);
    if (c == '[') return parse_array(ctx, cur, p);
    if (c == '{') return parse_object(ctx, cur, p);
    if (c == '-' || helpers::is_digit(c)) return parse_number(ctx, cur);

    Err::err("json: invalid value");
    return node{};  // unreachable
  }

  // Return true if only whitespace remains in the input.
  static constexpr bool eof(Ctx const& ctx, helpers::cursor& cur) {
    skip_ws(ctx, cur);
    return peek(ctx, cur) == '\0';
  }

private:
  // Peek the current character without advancing the cursor. Returns '\0' if at the end of source.
  static constexpr char peek(Ctx const& ctx, helpers::cursor const& cur) {
    return cur.pos() < Src::size(ctx) ? Src::at(ctx, cur.pos()) : '\0';
  }

  // Get the current character and advance the cursor. Returns '\0' if at the end of source.
  static constexpr char next(Ctx const& ctx, helpers::cursor& cur) {
    if (cur.pos() < Src::size(ctx)) {
      char c = Src::at(ctx, cur.pos());
      cur.advance();
      return c;
    }
    return '\0';
  }

  // Skip whitespace characters until the next non-whitespace character or the end of source.
  static constexpr void skip_ws(Ctx const& ctx, helpers::cursor& cur) {
    while (helpers::is_ws(peek(ctx, cur))) cur.advance();
  }

  // If the next character matches `c`, consume it and return true. Otherwise, return false.
  static constexpr bool match(Ctx const& ctx, helpers::cursor& cur, char c) {
    if (peek(ctx, cur) == c) {
      cur.advance();
      return true;
    }
    return false;
  }

  // If the next characters match `s`, consume them and return true. Otherwise, return false.
  template <std::size_t N>
  static constexpr bool match(Ctx const& ctx, helpers::cursor& cur, char const (&s)[N]) {
    for (std::size_t k = 0; k + 1 < N; ++k) {
      if (Src::at(ctx, cur.pos() + k) != s[k]) return false;
    }
    cur.advance(N - 1);
    return true;
  }

  // Expect the next character to be `c`. Consume if matches or report an error.
  static constexpr void expect(Ctx const& ctx, helpers::cursor& cur, char c) {
    if (!match(ctx, cur, c)) Err::err("json: expected character");
  }

  // Expect the next characters to match `s`. Consume if matches or report an error.
  template <std::size_t N>
  static constexpr void expect(Ctx const& ctx, helpers::cursor& cur, char const (&s)[N]) {
    if (!match(ctx, cur, s)) Err::err("json: expected token");
  }

  // Parse a JSON null value.
  static constexpr node parse_null(Ctx const& ctx, helpers::cursor& cur) {
    expect(ctx, cur, "null");
    return node{null_tag{}};
  }

  // Parse a JSON boolean true value.
  static constexpr node parse_true(Ctx const& ctx, helpers::cursor& cur) {
    expect(ctx, cur, "true");
    return node{bool_tag{}, true};
  }

  // Parse a JSON boolean false value.
  static constexpr node parse_false(Ctx const& ctx, helpers::cursor& cur) {
    expect(ctx, cur, "false");
    return node{bool_tag{}, false};
  }

  // Parse a JSON number value.
  static constexpr node parse_number(Ctx const& ctx, helpers::cursor& cur) {
    // Fast path for runtime: try integer accumulation first, fall back to
    // from_chars<double> only when a '.', 'e'/'E', or uint64 overflow is seen.
    // This avoids the full IEEE 754 decimal-to-float conversion for the common
    // case of plain integers.
    if constexpr (Src::has_fast_parse_num) {
      char const* p = Src::data(ctx) + cur.pos();
      char const* const end = Src::data(ctx) + Src::size(ctx);

      // JSON rejects leading zeros (e.g. "01", "00") - from_chars accepts them.
      {
        char const* q = p;
        if (*q == '-') ++q;  // p < end always holds: parse_number is only called with valid input
        if (q < end && *q == '0' && q + 1 < end && helpers::is_digit(*(q + 1)))
          Err::err("json: invalid number");
      }

      // p < end always holds: parse_number is only called with valid input
      bool const neg = (*p == '-');
      if (neg) ++p;

      // Integer accumulation: consume digits into std::uint64_t.
      // Bail out to from_chars on overflow or any non-digit character that
      // signals a fractional/exponent component.
      constexpr std::uint64_t overflow_thres = (UINT64_MAX - 9) / 10;
      std::uint64_t int_value = 0;
      bool is_int = true;
      char const* const int_start = p;

      while (p < end && helpers::is_digit(*p)) {
        std::uint8_t const digit = static_cast<std::uint8_t>(*p - '0');
        if (int_value > overflow_thres) {
          is_int = false;
          break;
        }
        int_value = int_value * 10 + digit;
        ++p;
      }

      // If we stopped at a '.', 'e', or 'E', we have a float — use from_chars.
      if (is_int && p < end && (*p == '.' || *p == 'e' || *p == 'E')) is_int = false;

      if (is_int && p != int_start) {
        cur.set_pos(static_cast<std::size_t>(p - Src::data(ctx)));
        double const value = static_cast<double>(int_value);
        return node{num_tag{}, neg ? -value : value};
      }

      // Float fallback: reset to start and use from_chars<double>.
      char const* const start = Src::data(ctx) + cur.pos();
      double value{};
      auto const [ptr, ec] = std::from_chars(start, end, value);
      if (ec != std::errc{}) Err::err("json: invalid number");

      // JSON rejects trailing dot ("42.") - from_chars may accept it.
      // ptr > start always holds: a successful from_chars parse consumes at least one character.
      if (*(ptr - 1) == '.') Err::err("json: invalid number");

      cur.set_pos(static_cast<std::size_t>(ptr - Src::data(ctx)));
      return node{num_tag{}, value};
    }

    double number = 0.0;
    bool neg = false;
    if (match(ctx, cur, '-')) neg = true;

    // Integer part.
    if (match(ctx, cur, '0')) {
      // Integer part is zero; number is already 0.0.
    } else if (helpers::is_nonzero_digit(peek(ctx, cur))) {
      while (helpers::is_digit(peek(ctx, cur))) {
        number = number * 10.0 + (helpers::as_hex(next(ctx, cur)));
      }
    } else {
      Err::err("json: invalid number");
    }

    // Fractional part.
    if (match(ctx, cur, '.')) {
      if (!helpers::is_digit(peek(ctx, cur))) Err::err("json: invalid number");

      double scale = 0.1;
      while (helpers::is_digit(peek(ctx, cur))) {
        number += helpers::as_hex(next(ctx, cur)) * scale;
        scale *= 0.1;
      }
    }

    // Exponent part.
    if (match(ctx, cur, 'e') || match(ctx, cur, 'E')) {
      bool exp_neg = false;
      if (match(ctx, cur, '-'))
        exp_neg = true;
      else if (match(ctx, cur, '+'))
        exp_neg = false;

      // There must be at least one digit for the exponent.
      if (!helpers::is_digit(peek(ctx, cur))) Err::err("json: invalid number");

      int exp = 0;
      while (helpers::is_digit(peek(ctx, cur))) {
        exp = exp * 10 + helpers::as_hex(next(ctx, cur));
      }

      if (exp_neg) exp = -exp;
      number *= helpers::pow10(exp);
    }

    return node{num_tag{}, neg ? -number : number};
  }

  // Parse a JSON string value.
  static constexpr node parse_string(Ctx const& ctx, helpers::cursor& cur, Pool& p) {
    expect(ctx, cur, '"');
    size_type const begin = p.str_len();

    // Helper: parse exactly 4 hex digits and return the codepoint.
    auto parse_hex4 = [&]() {
      int cp = 0;
      for (int i = 0; i < 4; ++i) {
        char const h = next(ctx, cur);
        if (!helpers::is_hex(h)) Err::err("json: invalid unicode escape");
        cp = cp * 16 + helpers::as_hex(h);
      }
      return cp;
    };

    // Helper: encode a Unicode codepoint as UTF-8 into the pool.
    auto push_utf8 = [&](int const cp) {
      if (cp <= 0x7F) {
        p.push_str(static_cast<char>(cp));
      } else if (cp <= 0x7FF) {
        p.push_str(static_cast<char>(0xC0 | (cp >> 6)));
        p.push_str(static_cast<char>(0x80 | (cp & 0x3F)));
      } else if (cp <= 0xFFFF) {
        p.push_str(static_cast<char>(0xE0 | (cp >> 12)));
        p.push_str(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        p.push_str(static_cast<char>(0x80 | (cp & 0x3F)));
      } else {
        p.push_str(static_cast<char>(0xF0 | (cp >> 18)));
        p.push_str(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        p.push_str(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        p.push_str(static_cast<char>(0x80 | (cp & 0x3F)));
      }
    };

    // Fast path: try to bulk-copy the entire string in one shot when it contains no special
    // chars (no escapes, no control chars, no high bytes). For ct sources, safe_str_run
    // always returns 0 so this block is dead code and the compiler eliminates it entirely.
    {
      std::size_t const prefix = Src::safe_str_run(ctx, cur.pos());
      if (prefix > 0) {
        if (Src::at(ctx, cur.pos() + prefix) == '"') {
          // Whole string is plain ASCII - copy it and return immediately.
          p.push_str_bulk(Src::data(ctx) + cur.pos(), prefix);
          cur.advance(prefix + 1);  // +1 for closing '"'
          return node{str_tag{}, begin, p.str_len() - begin};
        }
        // String starts with a plain prefix but has special chars later.
        // Copy the prefix now and fall through to the char-by-char loop.
        p.push_str_bulk(Src::data(ctx) + cur.pos(), prefix);
        cur.advance(prefix);
      }
    }

    // Char-by-char loop: handles escapes, high bytes, and strings that start with
    // special chars (or the remainder after a plain prefix was bulk-copied above).
    while (true) {
      char c = next(ctx, cur);
      if (c == '\0') Err::err("json: unterminated string");
      if (static_cast<unsigned char>(c) <= 0x1F) Err::err("json: unescaped control character");
      if (c == '"') break;

      // Escape sequence.
      if (c == '\\') {
        c = next(ctx, cur);
        if (c == '\0') Err::err("json: unterminated string");
        switch (c) {
        case '"':
        case '\\':
        case '/':
          p.push_str(c);
          break;

        case 'b':
          p.push_str('\b');
          break;
        case 'f':
          p.push_str('\f');
          break;
        case 'n':
          p.push_str('\n');
          break;
        case 'r':
          p.push_str('\r');
          break;
        case 't':
          p.push_str('\t');
          break;

        case 'u': {
          int codepoint = parse_hex4();
          if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            // High surrogate: must be followed by \uDC00-\uDFFF.
            if (next(ctx, cur) != '\\' || next(ctx, cur) != 'u')
              Err::err("json: missing low surrogate");
            int const low = parse_hex4();
            if (low < 0xDC00 || low > 0xDFFF) Err::err("json: invalid low surrogate");
            codepoint = 0x10000 + (codepoint - 0xD800) * 0x400 + (low - 0xDC00);
          } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
            Err::err("json: lone low surrogate");
          }
          push_utf8(codepoint);
          break;
        }

        default:
          Err::err("json: invalid escape character");
        }
        continue;
      }

      // Raw byte: validate multi-byte UTF-8.
      unsigned char const uc = static_cast<unsigned char>(c);
      if (uc >= 0x80) {
        int extra = 0;
        unsigned char lo2 = 0x80, hi2 = 0xBF;  // allowed range for the first continuation byte
        if ((uc & 0xE0) == 0xC0 && uc >= 0xC2)
          extra = 1;
        else if ((uc & 0xF0) == 0xE0) {
          extra = 2;
          if (uc == 0xE0)
            lo2 = 0xA0;  // reject overlong 3-byte sequences
          else if (uc == 0xED)
            hi2 = 0x9F;  // reject surrogate codepoints (U+D800..U+DFFF)
        } else if ((uc & 0xF8) == 0xF0 && uc <= 0xF4) {
          extra = 3;
          if (uc == 0xF0)
            lo2 = 0x90;  // reject overlong 4-byte sequences
          else if (uc == 0xF4)
            hi2 = 0x8F;  // reject codepoints > U+10FFFF
        } else
          Err::err("json: invalid UTF-8");

        p.push_str(c);
        for (int i = 0; i < extra; ++i) {
          char const cont = next(ctx, cur);
          unsigned char const ucont = static_cast<unsigned char>(cont);
          if ((ucont & 0xC0) != 0x80) Err::err("json: invalid UTF-8 continuation byte");
          if (i == 0 && (ucont < lo2 || ucont > hi2))
            Err::err("json: invalid UTF-8 continuation byte");
          p.push_str(cont);
        }
        continue;
      }

      // Plain ASCII character.
      p.push_str(c);
    }

    size_type const len = p.str_len() - begin;
    return node{str_tag{}, begin, len};
  }

  // Parse a JSON array (or tuple) value.
  // Uses the pool's stack/flush pattern (see the `pool` concept doc): child node indices are
  // accumulated on the stack while the element count is unknown, then flushed to the
  // contiguous index_list once the closing `]` is found.
  static constexpr node parse_array(Ctx const& ctx, helpers::cursor& cur, Pool& p) {
    expect(ctx, cur, '[');
    skip_ws(ctx, cur);

    // Empty array.
    if (match(ctx, cur, ']')) return node{arr_tag{}, p.index_len(), 0};

    size_type const stack_begin = p.stack_len();

    while (true) {
      // Parse a child value and add it to the stack.
      // MSVC's constexpr evaluator does not correctly handle temporary lifetime extension,
      // so we do not use const references here. The same goes for parse_object.
      node const value = parse(ctx, cur, p);
      size_type const idx = p.push_node(value);
      p.push_stack(idx);

      skip_ws(ctx, cur);
      if (match(ctx, cur, ']')) break;
      expect(ctx, cur, ',');
      skip_ws(ctx, cur);
    }

    // Collect the indices of child nodes from the stack and add them to the index list.
    // This ensures contiguous indices for child nodes.
    size_type const begin = p.index_len();
    size_type const len = p.stack_len() - stack_begin;
    for (size_type i = stack_begin; i < p.stack_len(); ++i) {
      p.push_index(p.stack_at(i));
    }
    p.reset_stack(stack_begin);
    return node{arr_tag{}, begin, len};
  }

  // Parse a JSON object value.
  // Same stack/flush pattern as parse_array, but accumulates kv_entry indices instead
  // of plain node indices (see the `pool` concept doc).
  static constexpr node parse_object(Ctx const& ctx, helpers::cursor& cur, Pool& p) {
    expect(ctx, cur, '{');
    skip_ws(ctx, cur);

    // Empty object.
    if (match(ctx, cur, '}')) return node{obj_tag{}, p.index_len(), 0};

    size_type const stack_begin = p.stack_len();

    while (true) {
      // Parse a key string. The parse_string method always returns str_t or throws.
      str_view const key = parse_string(ctx, cur, p).as_str();

      skip_ws(ctx, cur);
      expect(ctx, cur, ':');
      skip_ws(ctx, cur);

      // Parse a value and add the key-value entry to the stack.
      node const value = parse(ctx, cur, p);
      size_type const value_idx = p.push_node(value);
      size_type const kv_idx = p.push_kv({key, value_idx});
      p.push_stack(kv_idx);

      skip_ws(ctx, cur);
      if (match(ctx, cur, '}')) break;
      expect(ctx, cur, ',');
      skip_ws(ctx, cur);
    }

    // Collect the indices of key-value entries from the stack and add them to the index list.
    // This ensures contiguous indices for child nodes.
    size_type const begin = p.index_len();
    size_type const len = p.stack_len() - stack_begin;
    for (size_type i = stack_begin; i < p.stack_len(); ++i) {
      p.push_index(p.stack_at(i));
    }
    p.reset_stack(stack_begin);
    p.on_obj_parsed(obj_view{begin, len});
    return node{obj_tag{}, begin, len};
  }
};

namespace helpers {

// Helper types for schema introspection.

template <typename>
struct is_jarr : std::false_type { };

template <schema T>
struct is_jarr<jarr<T>> : std::true_type { };

template <typename>
struct is_jtup : std::false_type { };

template <schema... Schemas>
struct is_jtup<jtup<Schemas...>> : std::true_type { };

template <typename>
struct is_jobj : std::false_type { };

template <kv_schema... KVs>
struct is_jobj<jobj<KVs...>> : std::true_type { };

template <typename>
struct jobj_size;

template <kv_schema... KVs>
struct jobj_size<jobj<KVs...>> {
  static constexpr std::size_t value = sizeof...(KVs);
};

}  // namespace helpers

// Generic JSON schema validator.
template <accessor Accessor, error_handler Err>
class validator final {
public:
  // Validate the JSON node at the given index against the given schema.
  template <schema Schema>
  static constexpr void validate(Accessor const& a, size_type idx) {
    node const& n = a.node_at(idx);
    if constexpr (std::same_as<Schema, jnull>) {
      if (n.type != null_t) Err::err("json: expected null");
    } else if constexpr (std::same_as<Schema, jbool>) {
      if (n.type != bool_t) Err::err("json: expected boolean");
    } else if constexpr (std::same_as<Schema, jnum>) {
      if (n.type != num_t) Err::err("json: expected number");
    } else if constexpr (std::same_as<Schema, jstr>) {
      if (n.type != str_t) Err::err("json: expected string");
    } else if constexpr (helpers::is_jarr<Schema>::value) {
      if (n.type != arr_t) Err::err("json: expected array");
      for (size_type i = 0; i < n.as_arr().len; ++i) {
        size_type child_idx = a.arr_at(n.as_arr(), i);
        validate<typename Schema::value_type>(a, child_idx);
      }
    } else if constexpr (helpers::is_jtup<Schema>::value) {
      if (n.type != arr_t) Err::err("json: expected tuple");
      if (n.as_arr().len != std::tuple_size_v<typename Schema::value_types>)
        Err::err("json: tuple arity mismatch");
      validate_tuple<Schema>(a, n);
    } else if constexpr (helpers::is_jobj<Schema>::value) {
      if (n.type != obj_t) Err::err("json: expected object");
      if (n.as_obj().len != helpers::jobj_size<Schema>::value)
        Err::err("json: object arity mismatch");
      validate_kvs<Schema>(a, n);
    } else {
      Err::err("json: invalid schema");
    }
  }

private:
  template <schema Schema, std::size_t... Is>
  static constexpr void
  validate_tuple_impl(Accessor const& a, node const& n, std::index_sequence<Is...>) {
    (validate<typename std::tuple_element_t<Is, typename Schema::value_types>>(
       a,
       a.arr_at(n.as_arr(), Is)
     ),
     ...);
  }

  // Validate tuple elements.
  template <schema Schema>
  static constexpr void validate_tuple(Accessor const& a, node const& n) {
    validate_tuple_impl<Schema>(
      a,
      n,
      std::make_index_sequence<std::tuple_size_v<typename Schema::value_types>>{}
    );
  }

  // Validate a single key-value pair.
  template <kv_schema KV>
  static constexpr void validate_kv(Accessor const& a, node const& n) {
    constexpr auto key = KV::key;
    using value_t = typename KV::value_type;
    size_type const idx = a.template obj_at<key>(n.as_obj());
    validate<value_t>(a, idx);
  }

  template <kv_schema... KVs>
  static constexpr void validate_kvs_impl(Accessor const& a, node const& n, jobj<KVs...>*) {
    (validate_kv<KVs>(a, n), ...);
  }

  // Validate key-value pairs of a JSON object.
  template <schema Schema>
  static constexpr void validate_kvs(Accessor const& a, node const& n) {
    validate_kvs_impl(a, n, static_cast<Schema*>(nullptr));  // unpack Schema into KVs
  }
};

// Generic JSON value accessor.
template <accessor Accessor, error_handler Err>
class value_accessor final {
public:
  constexpr value_accessor() = default;
  constexpr value_accessor(Accessor a, size_type idx) : a_(a), idx_(idx) { }

  /// Get the type of the JSON node.
  [[nodiscard]] constexpr jtype type() const { return node_ref().type; }

  /// Get the size of the JSON node if it is an array, tuple, or object.
  [[nodiscard]] constexpr size_type size() const {
    node const& node = node_ref();
    if (node.type == arr_t) return node.as_arr().len;
    if (node.type == obj_t) return node.as_obj().len;
    Err::err("json: size() on non-array and non-object");
    return 0;  // unreachable
  }

  /// Return nullptr if the JSON node is null, or report an error.
  [[nodiscard]] constexpr std::nullptr_t as_null() const {
    if (node_ref().type != null_t) Err::err("json: expected null");
    return nullptr;
  }

  /// Return the boolean value of the JSON node if it is a boolean, or report an error.
  [[nodiscard]] constexpr bool as_bool() const {
    node const& node = node_ref();
    if (node.type != bool_t) Err::err("json: expected boolean");
    return node.as_bool();
  }

  /// Return the number value of the JSON node if it is a number, or report an error.
  [[nodiscard]] constexpr double as_num() const {
    node const& node = node_ref();
    if (node.type != num_t) Err::err("json: expected number");
    return node.as_num();
  }

  /// Return the string value of the JSON node as std::string_view if it is a string,
  /// or report an error.
  [[nodiscard]] constexpr std::string_view as_str() const {
    node const& node = node_ref();
    if (node.type != str_t) Err::err("json: expected string");
    return a_.as_std_sv(node.as_str());
  }

  /// Convert the JSON value to a user-defined type T via jconvert<T>.
  template <typename T>
    requires requires(value_accessor const& a) {
      { jconvert<T>::from(a) } -> std::same_as<T>;
    }
  [[nodiscard]] constexpr T as() const {
    return jconvert<T>::from(*this);
  }

  /// Array or tuple index lookup.
  [[nodiscard]] constexpr value_accessor operator[](std::size_t index) const {
    node const& node = node_ref();
    if (node.type != arr_t) Err::err("json: operator[index] on non-array");
    arr_view const& arr = node.as_arr();
    if (index >= arr.len) Err::err("json: array index out of bounds");
    return {a_, a_.arr_at(arr, static_cast<size_type>(index))};
  }

  /// Object key lookup.
  [[nodiscard]] constexpr value_accessor operator[](std::string_view key) const {
    node const& node = node_ref();
    if (node.type != obj_t) Err::err("json: operator[key] on non-object");
    obj_view const& obj = node.as_obj();
    return {a_, a_.obj_at(obj, key)};
  }

private:
  constexpr node const& node_ref() const { return a_.node_at(idx_); }

  Accessor a_{};
  size_type idx_ = 0;  // Pool index of the JSON node this value_accessor points to.
};

namespace ct {

// Compile-time specializations.

// Compile-time source. The JSON string literal is stored as a NTTP.
template <fixed_string S>
class source final {
public:
  using ctx = struct { };

  static constexpr char at(ctx const&, std::size_t idx) { return idx < S.size ? S[idx] : '\0'; }
  static constexpr std::size_t size(ctx const&) { return S.size; }
  static constexpr char const* data(ctx const&) { return nullptr; }

  // For constexpr sources the fast-bulk path is disabled; parse_string falls back to char-by-char.
  static constexpr std::size_t safe_str_run(ctx const&, std::size_t) { return 0; }

  // Use std::from_chars in constexpr context only when the compiler supports it (C++23 P2291R3).
  static constexpr bool has_fast_parse_num =
#if defined(__cpp_lib_constexpr_charconv) && __cpp_lib_constexpr_charconv >= 202207L
    true;
#else
    false;
#endif
};

// Compile-time pool. The parsed nodes, key-value entries, string characters, index list, and
// stack are stored in static arrays with fixed maximum sizes for constexpr usage.
template <fixed_string S>
class pool final {
public:
  constexpr size_type push_node(node n) {
    nodes_[node_len_] = n;
    return node_len_++;
  }

  constexpr size_type push_kv(kv_entry kv) {
    kvs_[kv_len_] = kv;
    return kv_len_++;
  }

  constexpr void push_str(char c) {
    str_pool_[str_len_] = c;
    ++str_len_;
  }

  constexpr void push_str_bulk(char const* src, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) push_str(src[i]);
  }

  constexpr void push_index(size_type idx) {
    index_list_[index_len_] = idx;
    ++index_len_;
  }

  constexpr void push_stack(size_type idx) {
    stack_[stack_len_] = idx;
    ++stack_len_;
  }

  constexpr node const& node_at(size_type i) const { return nodes_[i]; }
  constexpr kv_entry const& kv_at(size_type i) const { return kvs_[i]; }
  constexpr char str_at(size_type i) const { return str_pool_[i]; }
  constexpr char const* str_data() const { return str_pool_; }
  constexpr size_type index_at(size_type i) const { return index_list_[i]; }
  constexpr size_type stack_at(size_type i) const { return stack_[i]; }
  constexpr size_type str_len() const { return str_len_; }
  constexpr size_type index_len() const { return index_len_; }
  constexpr size_type stack_len() const { return stack_len_; }
  constexpr void reset_stack(size_type len) { stack_len_ = len; }
  constexpr void on_obj_parsed(obj_view) const { }  // No-op: ct uses linear scan.

private:
  // S.size is a safe upper bound for every pool: a JSON source of N characters can produce
  // at most N nodes, N kv entries, N string characters, and N index/stack entries.
  // +1 for the root node that is pushed after parse_impl finishes (see parse_impl).
  static constexpr std::size_t max = S.size + 1;
  node nodes_[max]{};
  size_type node_len_ = 0;
  kv_entry kvs_[max]{};
  size_type kv_len_ = 0;
  char str_pool_[max]{};
  size_type str_len_ = 0;
  size_type index_list_[max]{};
  size_type index_len_ = 0;
  size_type stack_[max]{};
  size_type stack_len_ = 0;
};

// Parsed result. We will validate and access compile-time parsed JSON on this class.
template <fixed_string S>
class document final {
public:
  constexpr pool<S>& pool_ref() { return p_; }
  constexpr pool<S> const& pool_ref() const { return p_; }
  constexpr size_type root_index() const { return root_idx_; }
  constexpr void set_root_index(size_type idx) { root_idx_ = idx; }

private:
  pool<S> p_{};
  size_type root_idx_ = 0;
};

// Compile-time accessor to access parsed JSON nodes holding a pointer to the parsed document.
template <fixed_string S>
class accessor final {
public:
  constexpr accessor() = default;
  constexpr explicit accessor(document<S> const* result) : result_(result) { }

  constexpr node const& node_at(size_type idx) const { return result_->pool_ref().node_at(idx); }

  template <fixed_string Key>
  constexpr size_type obj_at(obj_view const& obj) const {
    return obj_at(obj, std::string_view{Key.data, Key.size});
  }

  constexpr size_type obj_at(obj_view const& obj, std::string_view key) const {
    for (size_type i = 0; i < obj.len; ++i) {
      size_type const kv_idx = result_->pool_ref().index_at(obj.begin + i);
      kv_entry const& kv = result_->pool_ref().kv_at(kv_idx);
      if (str_eq(kv.key, key)) return kv.value_idx;
    }
    throw "json: missing object key";
  }

  constexpr size_type arr_at(arr_view const& arr, size_type idx) const {
    if (idx >= arr.len) throw "json: array index out of bounds";
    return result_->pool_ref().index_at(arr.begin + idx);
  }

  constexpr char str_at(str_view const& str, size_type idx) const {
    if (idx >= str.len) throw "json: string index out of bounds";
    return result_->pool_ref().str_at(str.begin + idx);
  }

  constexpr bool str_eq(str_view const& str, std::string_view sv) const {
    if (str.len != sv.size()) return false;
    for (size_type i = 0; i < str.len; ++i) {
      if (result_->pool_ref().str_at(str.begin + i) != sv[i]) return false;
    }
    return true;
  }

  constexpr std::string_view as_std_sv(str_view const& str) const {
    return std::string_view{result_->pool_ref().str_data() + str.begin, str.len};
  }

private:
  document<S> const* result_ = nullptr;
};

// Compile-time error handler.
// Throwing inside a `constexpr` context that is evaluated at compile time is ill-formed,
// so the compiler emits a hard error and includes the thrown string literal in the diagnostic,
// giving a readable error message without any extra machinery.
#if defined(_MSC_VER) && !defined(__clang__)
// The `if (msg == nullptr) return;` branch provides a valid constexpr path so MSVC does not reject
// the function definition with C3615. In practice err() is always called with a non-null string
// literal, so it always reaches `throw msg`.
struct error_handler final {
  static constexpr void err(char const* const msg) {
    if (msg == nullptr) return;  // valid constexpr path - satisfies MSVC C3615 check
    throw msg;                   // ill-formed in constexpr context -> compile error
  }
};
#else
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Winvalid-constexpr"
struct error_handler final {
  [[noreturn]] static constexpr void err(char const* const msg) { throw msg; }
};
# pragma GCC diagnostic pop
#endif

// Parses the JSON source S into a document. This is a free function so it can be shared
// between ct::parse (which only parses) and ct::bind (which parses then validates).
template <fixed_string S>
constexpr document<S> parse_impl() {
  document<S> result{};
  typename source<S>::ctx ctx{};
  helpers::cursor cur{};

  using Parser = parser<source<S>, pool<S>, error_handler>;

  node const& root = Parser::parse(ctx, cur, result.pool_ref());
  // The root node is pushed into the pool only after it has been fully parsed.
  // Pushing it earlier would give it index 0 before any of its children exist, making
  // its arr_view/obj_view point to the wrong (not-yet-populated) region of the index list.
  result.set_root_index(result.pool_ref().push_node(root));

  if (!Parser::eof(ctx, cur)) throw "json: trailing characters";

  return result;
}

// Compile-time JSON parsing.
template <fixed_string S>
class parse final {
public:
  constexpr parse() { }

  constexpr value_accessor<accessor<S>, error_handler> root() const {
    return {accessor<S>{&result}, result.root_index()};
  }

  constexpr value_accessor<accessor<S>, error_handler> operator[](std::size_t index) const {
    return root()[index];
  }

  constexpr value_accessor<accessor<S>, error_handler> operator[](std::string_view key) const {
    return root()[key];
  }

private:
  // inline static constexpr ensures parse_impl<S>() is evaluated exactly once at compile time
  // and the result is shared across all instances (it is a program-wide constant).
  inline static constexpr auto result = parse_impl<S>();
};

// Bind a JSON value to a schema, which validates the value against it.
template <fixed_string S, schema Schema>
class bind final {
public:
  constexpr bind() {
    validator<accessor<S>, error_handler>::template validate<Schema>(
      accessor<S>{&result},
      result.root_index()
    );
  }

  constexpr value_accessor<accessor<S>, error_handler> root() const {
    return {accessor<S>{&result}, result.root_index()};
  }

  constexpr value_accessor<accessor<S>, error_handler> operator[](std::size_t index) const {
    return root()[index];
  }

  constexpr value_accessor<accessor<S>, error_handler> operator[](std::string_view key) const {
    return root()[key];
  }

private:
  // Same as ct::parse: evaluated exactly once at compile time, shared across instances.
  inline static constexpr auto result = parse_impl<S>();
};

// Compile-time builder: constructs JSON text by appending to a static buffer,
// which is returned as a string_view when done.
class builder final {
public:
  constexpr builder() = default;

  constexpr void bld_null() { append_n("null", 4); }

  constexpr void bld_bool(bool value) {
    if (value)
      append_n("true", 4);
    else
      append_n("false", 5);
  }

  constexpr void bld_num(double value) {
#if defined(__cpp_lib_constexpr_charconv) && __cpp_lib_constexpr_charconv >= 202207L
    char buf[32]{};
    auto const [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    // No error handling for to_chars since it should never fail for double with a 32-byte buffer.
    append_n(buf, static_cast<std::size_t>(ptr - buf));
#else
    if (value == 0.0) {
      append_char('0');
    } else if (value < 0) {
      append_char('-');
      bld_num(-value);
    } else {
      auto const int_value = static_cast<long long>(value);
      if (static_cast<double>(int_value) == value) {
        format_int(int_value);
      } else {
        double frac_value = value - static_cast<double>(int_value);
        format_int(int_value);

        if (frac_value > 0.0) {
          append_char('.');
          for (int i = 0; i < 6 && frac_value != 0.0; ++i) {
            frac_value *= 10.0;
            int const digit = static_cast<int>(frac_value);
            append_char(static_cast<char>('0' + digit));
            frac_value -= static_cast<double>(digit);
          }
        }
      }
    }
#endif
  }

  constexpr void bld_str(std::string_view value) {
    append_char('"');

    for (char const c : value) {
      switch (c) {
      case '"':
        append_n("\\\"", 2);
        break;
      case '\\':
        append_n("\\\\", 2);
        break;
      case '\b':
        append_n("\\b", 2);
        break;
      case '\f':
        append_n("\\f", 2);
        break;
      case '\n':
        append_n("\\n", 2);
        break;
      case '\r':
        append_n("\\r", 2);
        break;
      case '\t':
        append_n("\\t", 2);
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          append_n("\\u00", 4);
          append_hex_byte(static_cast<unsigned char>(c));
        } else {
          append_char(c);
        }
      }
    }

    append_char('"');
  }

  constexpr void begin_arr() {
    append_char('[');
    push_comma(false);
  }

  constexpr void arr_elem() {
    if (need_comma()) append_char(',');
    set_comma(true);
  }

  constexpr void end_arr() {
    append_char(']');
    pop_comma();
  }

  constexpr void begin_obj() {
    append_char('{');
    push_comma(false);
  }

  constexpr void key(std::string_view k) {
    if (need_comma()) append_char(',');
    set_comma(true);
    bld_str(k);
    append_char(':');
  }

  constexpr void end_obj() {
    append_char('}');
    pop_comma();
  }

  constexpr std::string_view result() const { return std::string_view(buffer_, len_); }

  // Implicit conversion to string_view for convenient usage.
  constexpr operator std::string_view() const { return result(); }

  constexpr std::size_t size() const { return len_; }
  constexpr char operator[](std::size_t i) const { return buffer_[i]; }

private:
  constexpr void append_char(char c) {
    if (len_ >= max_size) throw "json: builder buffer overflow";
    buffer_[len_++] = c;
  }

  constexpr void append_n(char const* s, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) append_char(s[i]);
  }

  constexpr void format_int(long long value) {
    if (value == 0) {
      append_char('0');
      return;
    }
    char temp[20]{};
    int pos = 0;
    while (value > 0) {
      temp[pos++] = '0' + (value % 10);
      value /= 10;
    }
    for (int i = pos - 1; i >= 0; --i) {
      append_char(temp[i]);
    }
  }

  constexpr void append_hex_byte(unsigned char byte) {
    auto hex = [](unsigned char v) -> char { return v < 10 ? ('0' + v) : ('a' + v - 10); };
    append_char(hex(byte >> 4));
    append_char(hex(byte & 0xF));
  }

  constexpr void push_comma(bool value) {
    if (comma_depth_ >= max_depth) throw "json: max nesting depth exceeded";
    comma_stack_[comma_depth_++] = value;
  }

  constexpr void pop_comma() {
    if (comma_depth_ == 0) throw "json: comma stack underflow";
    --comma_depth_;
  }

  constexpr bool need_comma() const {
    return comma_depth_ > 0 ? comma_stack_[comma_depth_ - 1] : false;
  }

  constexpr void set_comma(bool value) {
    if (comma_depth_ > 0) comma_stack_[comma_depth_ - 1] = value;
  }

  static constexpr std::size_t max_size = 1048576;
  char buffer_[max_size]{};
  std::size_t len_ = 0;
  static constexpr std::size_t max_depth = 1024;
  bool comma_stack_[max_depth]{};
  std::size_t comma_depth_ = 0;
};

/// Serialize a C++ value to JSON at compile time.
/// Returns a builder containing the JSON. Implicit conversion to std::string_view is provided.
template <typename T>
constexpr builder static_serialize(T const& value) {
  builder b;
  crjson::jconvert<T>::to(b, value);
  return b;
}

}  // namespace ct

namespace rt {

// Runtime specializations.

// Runtime source. The JSON string is stored in a std::string_view.
class source final {
public:
  using ctx = std::string_view;

  static char at(ctx const& ctx, std::size_t idx) { return idx < ctx.size() ? ctx[idx] : '\0'; }
  static std::size_t size(ctx const& ctx) { return ctx.size(); }
  static char const* data(ctx const& c) { return c.data(); }

  // Returns the length of the longest run of plain ASCII string chars starting at pos.
  // Plain means: not '"', not '\', not a control char (< 0x20), not a high byte (>= 0x80).
  static std::size_t safe_str_run(ctx const& c, std::size_t pos) {
    char const* p = c.data() + pos;
    char const* const end = c.data() + c.size();
    while (p < end) {
      unsigned char const uc = static_cast<unsigned char>(*p);
      if (uc < 0x20 || uc == '"' || uc == '\\' || uc >= 0x80) break;
      ++p;
    }
    return static_cast<std::size_t>(p - (c.data() + pos));
  }

  // std::from_chars is introduced in C++17, so we can always use it in runtime context.
  static constexpr bool has_fast_parse_num = true;
};

// Runtime pool. The parsed nodes, key-value entries, string characters, index list, and
// stack are stored in std::vectors for dynamic storage.
class pool final {
public:
  pool() = default;
  // Reserve vectors upfront based on JSON input size to avoid geometric reallocations.
  explicit pool(std::size_t json_size) {
    nodes_.reserve(json_size / 4 + 1);
    kvs_.reserve(json_size / 8 + 1);
    str_pool_.reserve(json_size / 2 + 1);
    index_list_.reserve(json_size / 8 + 1);
    stack_.reserve(json_size / 8 + 1);
    ht_start_.reserve(json_size / 8 + 1);
  }

  // Explicitly defaulted move constructor and move assignment operator for pool.
  pool(pool&&) = default;
  pool& operator=(pool&&) = default;

  size_type push_node(node n) {
    nodes_.push_back(n);
    return static_cast<size_type>(nodes_.size() - 1);
  }

  size_type push_kv(kv_entry kv) {
    kvs_.push_back(kv);
    return static_cast<size_type>(kvs_.size() - 1);
  }

  void push_str(char c) { str_pool_.push_back(c); }

  void push_str_bulk(char const* src, std::size_t len) {
    str_pool_.insert(str_pool_.end(), src, src + len);
  }

  void push_index(size_type idx) { index_list_.push_back(idx); }

  void push_stack(size_type idx) {
    if (stack_len_ < stack_.size())
      stack_[stack_len_] = idx;
    else
      stack_.push_back(idx);
    ++stack_len_;
  }

  node const& node_at(size_type i) const { return nodes_[i]; }
  kv_entry const& kv_at(size_type i) const { return kvs_[i]; }
  char str_at(size_type i) const { return str_pool_[i]; }
  char const* str_data() const { return str_pool_.data(); }
  size_type index_at(size_type i) const { return index_list_[i]; }
  size_type stack_at(size_type i) const { return stack_[i]; }
  size_type str_len() const { return static_cast<size_type>(str_pool_.size()); }
  size_type index_len() const { return static_cast<size_type>(index_list_.size()); }
  size_type stack_len() const { return stack_len_; }
  void reset_stack(size_type len) { stack_len_ = len; }

  // Called by parse_object immediately after the closing '}' is processed.
  // Builds a flat open-addressing hash table for large objects; small objects use linear scan.
  // The HT start position is stored in kv_ht_start_[obj.begin] for O(1) retrieval at lookup.
  void on_obj_parsed(obj_view obj) {
    if (obj.len <= SMALL_OBJ_THRESHOLD) return;

    std::size_t const cap = next_pow2(obj.len * 2);
    std::size_t const ht_start = ht_data_.size();
    ht_data_.resize(ht_start + cap, HT_EMPTY);

    for (size_type j = 0; j < obj.len; ++j) {
      size_type const kv_idx = index_list_[obj.begin + j];
      kv_entry const& kv = kvs_[kv_idx];
      std::string_view const key{str_pool_.data() + kv.key.begin, kv.key.len};

      std::size_t slot = fnv1a(key) & (cap - 1);
      while (ht_data_[ht_start + slot] != HT_EMPTY) slot = (slot + 1) & (cap - 1);
      ht_data_[ht_start + slot] = kv_idx;
    }

    // Record the HT start indexed by obj.begin (unique per object, monotonically increasing).
    // obj.begin strictly increases with each parsed object, so the vector always needs growing.
    ht_start_.resize(obj.begin + 1, 0);
    ht_start_[obj.begin] = static_cast<size_type>(ht_start);
  }

  size_type obj_at(obj_view const& obj, std::string_view key) const {
    std::size_t const key_hash = fnv1a(key);

    // Large objects use the hash table for O(1) lookup.
    if (obj.len > SMALL_OBJ_THRESHOLD) {
      std::size_t const ht_start = ht_start_[obj.begin];
      std::size_t const ht_cap = next_pow2(obj.len * 2);
      std::size_t slot = key_hash & (ht_cap - 1);

      while (true) {
        size_type const kv_idx = ht_data_[ht_start + slot];
        if (kv_idx == HT_EMPTY) break;

        kv_entry const& kv = kvs_[kv_idx];
        if (std::string_view{str_pool_.data() + kv.key.begin, kv.key.len} == key)
          return kv.value_idx;
        slot = (slot + 1) & (ht_cap - 1);
      }

      throw std::runtime_error("json: missing object key");
    }

    // ...while small objects use linear scan to save memory.
    for (size_type i = 0; i < obj.len; ++i) {
      kv_entry const& kv = kvs_[index_list_[obj.begin + i]];
      if (std::string_view{str_pool_.data() + kv.key.begin, kv.key.len} == key) return kv.value_idx;
    }

    throw std::runtime_error("json: missing object key");
  }

private:
  static constexpr std::size_t SMALL_OBJ_THRESHOLD = 16;
  static constexpr size_type HT_EMPTY = ~size_type{0};  // Sentinel value.

  // The FNV-1a hash function for hashing object keys.
  // See https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#FNV-1a_hash.
  static std::size_t fnv1a(std::string_view s) noexcept {
    std::size_t h = 14695981039346656037ULL;
    for (char const c : s) {
      h ^= static_cast<unsigned char>(c);
      h *= 1099511628211ULL;
    }
    return h;
  }

  static std::size_t next_pow2(std::size_t n) noexcept {
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
  }

  std::vector<node> nodes_;
  std::vector<kv_entry> kvs_;
  std::vector<char> str_pool_;
  std::vector<size_type> index_list_;
  std::vector<size_type> stack_;
  size_type stack_len_ = 0;
  std::vector<size_type> ht_data_;
  // HT start positions for large objects, indexed by obj.begin (index_list position).
  // Only entries for objects with len > SMALL_OBJ_THRESHOLD are set.
  std::vector<size_type> ht_start_;
};

class document final {
public:
  document() = default;
  explicit document(std::size_t json_size) : p_(json_size) { }

  // Explicitly defaulted move constructor and move assignment operator for document.
  document(document&&) = default;
  document& operator=(document&&) = default;

  pool& pool_ref() { return p_; }
  pool const& pool_ref() const { return p_; }
  size_type root_index() const { return root_idx_; }
  void set_root_index(size_type idx) { root_idx_ = idx; }

private:
  pool p_;
  size_type root_idx_ = 0;
};

// Runtime accessors holding a pointer to the parsed document.
// Almost the same as compile-time accessor, but non-constexpr.
class accessor final {
public:
  accessor() = default;
  explicit accessor(document const* result) : result_(result) { }

  node const& node_at(size_type idx) const { return result_->pool_ref().node_at(idx); }

  template <fixed_string Key>
  size_type obj_at(obj_view const& obj) const {
    return obj_at(obj, std::string_view{Key.data, Key.size});
  }

  // Delegate to pool::obj_at.
  size_type obj_at(obj_view const& obj, std::string_view key) const {
    return result_->pool_ref().obj_at(obj, key);
  }

  size_type arr_at(arr_view const& arr, size_type idx) const {
    // Bounds are validated by value_accessor::operator[] and the schema validator before this.
    return result_->pool_ref().index_at(arr.begin + idx);
  }

  char str_at(str_view const& str, size_type idx) const {
    if (idx >= str.len) throw std::runtime_error("json: string index out of bounds");
    return result_->pool_ref().str_at(str.begin + idx);
  }

  bool str_eq(str_view const& str, std::string_view sv) const {
    if (str.len != sv.size()) return false;
    for (size_type i = 0; i < str.len; ++i) {
      if (result_->pool_ref().str_at(str.begin + i) != sv[i]) return false;
    }
    return true;
  }

  std::string_view as_std_sv(str_view const& str) const {
    return std::string_view{result_->pool_ref().str_data() + str.begin, str.len};
  }

private:
  document const* result_ = nullptr;
};

// Runtime error handler. We throw a std::runtime_error with the error message.
struct error_handler final {
  [[noreturn]] static void err(char const* const msg) { throw std::runtime_error(msg); }
};

// Parses the JSON string into a document. Free function shared by rt::parse and rt::bind.
inline document parse_impl(std::string_view json) {
  document result{json.size()};
  typename source::ctx ctx = json;
  helpers::cursor cur{};

  using Parser = parser<source, pool, error_handler>;

  node const& root = Parser::parse(ctx, cur, result.pool_ref());
  result.set_root_index(result.pool_ref().push_node(root));

  if (!Parser::eof(ctx, cur)) throw std::runtime_error("json: trailing characters");

  return result;
}

// Runtime JSON parsing.
class parse final {
public:
  explicit parse(std::string_view json) : result_(parse_impl(json)) { }

  /// Returns a value_accessor, the view of the JSON value.
  /// It is user's responsibility to ensure the parse object outlives all accessors derived from it.
  value_accessor<accessor, error_handler> root() const& {
    return {accessor{&result_}, result_.root_index()};
  }

  /// Users should not call root() on an rvalue parse object,
  /// since it would return an accessor with a dangling pointer to the document.
  /// The same goes for operator[]'s.
  value_accessor<accessor, error_handler> root() const&& = delete;

  value_accessor<accessor, error_handler> operator[](std::size_t index) const& {
    return root()[index];
  }
  value_accessor<accessor, error_handler> operator[](std::size_t index) const&& = delete;

  value_accessor<accessor, error_handler> operator[](std::string_view key) const& {
    return root()[key];
  }
  value_accessor<accessor, error_handler> operator[](std::string_view key) const&& = delete;

private:
  document result_{};
};

// Runtime JSON schema validation.
template <schema Schema>
class bind final {
public:
  explicit bind(std::string_view json) : result_(parse_impl(json)) {
    validator<accessor, error_handler>::template validate<Schema>(
      accessor{&result_},
      result_.root_index()
    );
  }

  /// Returns a value_accessor, the view of the JSON value.
  /// It is user's responsibility to ensure the bind object outlives all accessors derived from it.
  value_accessor<accessor, error_handler> root() const& {
    return {accessor{&result_}, result_.root_index()};
  }

  /// Users should not call root() on an rvalue parse object,
  /// since it would return an accessor with a dangling pointer to the document.
  /// The same goes for operator[]'s.
  value_accessor<accessor, error_handler> root() const&& = delete;

  value_accessor<accessor, error_handler> operator[](std::size_t index) const& {
    return root()[index];
  }
  value_accessor<accessor, error_handler> operator[](std::size_t index) const&& = delete;

  value_accessor<accessor, error_handler> operator[](std::string_view key) const& {
    return root()[key];
  }
  value_accessor<accessor, error_handler> operator[](std::string_view key) const&& = delete;

private:
  document result_{};
};

// Runtime builder: constructs JSON text by appending to a std::string.
class builder final {
public:
  builder() = default;

  void bld_null() { output_ += "null"; }

  void bld_bool(bool value) { output_ += value ? "true" : "false"; }

  void bld_num(double value) {
    char buf[32]{};
    auto const [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    // No error handling for to_chars since it should never fail for double with a 32-byte buffer.
    output_.append(buf, static_cast<size_t>(ptr - buf));
  }

  void bld_str(std::string_view value) {
    output_ += '"';

    for (char c : value) {
      switch (c) {
      case '"':
        output_ += "\\\"";
        break;
      case '\\':
        output_ += "\\\\";
        break;
      case '\b':
        output_ += "\\b";
        break;
      case '\f':
        output_ += "\\f";
        break;
      case '\n':
        output_ += "\\n";
        break;
      case '\r':
        output_ += "\\r";
        break;
      case '\t':
        output_ += "\\t";
        break;

      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char esc[7]{};
          std::snprintf(esc, sizeof(esc), "\\u%04x", static_cast<unsigned char>(c));
          output_ += esc;
        } else {
          output_ += c;
        }
      }
    }

    output_ += '"';
  }

  void begin_arr() {
    output_ += '[';
    need_comma_.push_back(false);
  }

  void arr_elem() {
    if (need_comma_.back()) output_ += ',';
    need_comma_.back() = true;
  }

  void end_arr() {
    output_ += ']';
    need_comma_.pop_back();
  }

  void begin_obj() {
    output_ += '{';
    need_comma_.push_back(false);
  }

  void key(std::string_view k) {
    if (need_comma_.back()) output_ += ',';
    need_comma_.back() = true;
    bld_str(k);
    output_ += ':';
  }

  void end_obj() {
    output_ += '}';
    need_comma_.pop_back();
  }

  std::string const& result() const { return output_; }

private:
  std::string output_;
  std::vector<bool> need_comma_;
};

/// Serialize a C++ value to JSON string at runtime.
template <typename T>
std::string serialize(T const& value) {
  builder b;
  crjson::jconvert<T>::to(b, value);
  return b.result();
}

}  // namespace rt

}  // namespace detail

// Public interfaces.

using detail::jtype;  // For value_accessor::type().

/// Parse a JSON string literal at compile time.
template <fixed_string S>
using static_parse = detail::ct::parse<S>;

/// Bind a JSON string literal to a schema at compile time, validating the JSON against the schema.
template <fixed_string S, schema Schema>
using static_bind = detail::ct::bind<S, Schema>;

/// Type alias for the compile-time value_accessor, which provides operator[] and as_* methods.
template <fixed_string S>
using static_accessor = detail::value_accessor<detail::ct::accessor<S>, detail::ct::error_handler>;

/// Parse a JSON string at runtime.
using parse = detail::rt::parse;

/// Bind a JSON string to a schema at runtime, validating the JSON against the schema.
template <schema Schema>
using bind = detail::rt::bind<Schema>;

/// Type alias for the runtime value_accessor, which provides operator[] and as_* methods.
using accessor = detail::value_accessor<detail::rt::accessor, detail::rt::error_handler>;

/// Serialize a C++ value to JSON at compile time.
using detail::ct::static_serialize;

/// Serialize a C++ value to JSON string at runtime.
using detail::rt::serialize;

/// Built-in jconvert specializations for common standard types.

template <>
struct jconvert<std::nullptr_t> {
  template <typename VA>
  static constexpr std::nullptr_t from(VA const& a) {
    return a.as_null();
  }

  template <detail::builder Builder>
  static constexpr void to(Builder& b, std::nullptr_t) {
    b.bld_null();
  }
};

template <>
struct jconvert<bool> {
  template <typename VA>
  static constexpr bool from(VA const& a) {
    return a.as_bool();
  }

  template <detail::builder Builder>
  static constexpr void to(Builder& b, bool value) {
    b.bld_bool(value);
  }
};

template <typename T>
  requires(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
struct jconvert<T> {
  template <typename VA>
  static constexpr T from(VA const& a) {
    return static_cast<T>(a.as_num());
  }

  template <detail::builder Builder>
  static constexpr void to(Builder& b, T value) {
    b.bld_num(static_cast<double>(value));
  }
};

template <>
struct jconvert<std::string_view> {
  template <typename VA>
  static constexpr std::string_view from(VA const& a) {
    return a.as_str();
  }

  template <detail::builder Builder>
  static constexpr void to(Builder& b, std::string_view value) {
    b.bld_str(value);
  }
};

template <>
struct jconvert<std::string> {
  template <typename VA>
  static std::string from(VA const& a) {
    return std::string{a.as_str()};
  }

  template <detail::builder Builder>
  static constexpr void to(Builder& b, std::string const& value) {
    b.bld_str(value);
  }
};

template <typename T>
struct jconvert<std::vector<T>> {
  template <typename VA>
  static std::vector<T> from(VA const& a) {
    std::vector<T> result;
    result.reserve(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) result.push_back(jconvert<T>::from(a[i]));
    return result;
  }

  template <detail::builder Builder>
  static constexpr void to(Builder& b, std::vector<T> const& value) {
    b.begin_arr();
    for (auto const& elem : value) {
      b.arr_elem();
      jconvert<T>::to(b, elem);
    }
    b.end_arr();
  }
};

template <typename T, typename U>
struct jconvert<std::pair<T, U>> {
  template <typename VA>
  static constexpr std::pair<T, U> from(VA const& a) {
    if (a.size() != 2) throw std::runtime_error("json: pair arity mismatch");
    return {jconvert<T>::from(a[0]), jconvert<U>::from(a[1])};
  }

  template <detail::builder Builder>
  static constexpr void to(Builder& b, std::pair<T, U> const& value) {
    b.begin_arr();
    b.arr_elem();
    jconvert<T>::to(b, value.first);
    b.arr_elem();
    jconvert<U>::to(b, value.second);
    b.end_arr();
  }
};

template <typename... Ts>
struct jconvert<std::tuple<Ts...>> {
  template <typename VA>
  static constexpr std::tuple<Ts...> from(VA const& a) {
    if (a.size() != sizeof...(Ts)) throw std::runtime_error("json: tuple arity mismatch");
    return from_impl(a, std::index_sequence_for<Ts...>{});
  }

  template <detail::builder Builder>
  static constexpr void to(Builder& b, std::tuple<Ts...> const& value) {
    b.begin_arr();
    to_impl(b, value, std::index_sequence_for<Ts...>{});
    b.end_arr();
  }

private:
  template <typename VA, std::size_t... Is>
  static constexpr std::tuple<Ts...> from_impl(VA const& a, std::index_sequence<Is...>) {
    return std::tuple<Ts...>{jconvert<Ts>::from(a[Is])...};
  }

  template <detail::builder Builder, std::size_t... Is>
  static constexpr void
  to_impl(Builder& b, std::tuple<Ts...> const& value, std::index_sequence<Is...>) {
    ((b.arr_elem(), jconvert<Ts>::to(b, std::get<Is>(value))), ...);
  }
};

template <typename T>
struct jconvert<std::optional<T>> {
  template <typename VA>
  static constexpr std::optional<T> from(VA const& a) {
    if (a.type() == jtype::null_t) return std::nullopt;
    return jconvert<T>::from(a);
  }

  template <detail::builder Builder>
  static constexpr void to(Builder& b, std::optional<T> const& value) {
    if (!value.has_value())
      b.bld_null();
    else
      jconvert<T>::to(b, *value);
  }
};

}  // namespace crjson

#endif
