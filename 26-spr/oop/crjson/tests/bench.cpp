// MIT License
//
// Copyright (c) 2026 registerGen
//
// Benchmarks for crjson library, with comparisons against
// nlohmann/json, simdjson, RapidJSON, and glaze where applicable.

#include <benchmark/benchmark.h>

#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "crjson.h"

// Silence RapidJSON deprecation warnings for std::iterator.
#define RAPIDJSON_NOMEMBERITERATORCLASS
#include <rapidjson/document.h>
#include <simdjson.h>

#include <glaze/glaze.hpp>
#include <nlohmann/json.hpp>

using namespace crjson;

#ifndef CRJSON_BENCH_MIN_TIME
# define CRJSON_BENCH_MIN_TIME 3.0
#endif
#define CRJSON_BENCHMARK(fn) BENCHMARK(fn)->MinTime(CRJSON_BENCH_MIN_TIME)

static constexpr int kLogSizes[] = {1 << 12, 1 << 14, 1 << 16, 1 << 18, 1 << 20};

static void ApplyLogSizes(benchmark::Benchmark* b) {
  for (int n : kLogSizes) b->Arg(n);
}

// =============================================================================
// Helpers — pre-built large realistic log datasets
// =============================================================================

static std::string make_log_dataset(int n) {
  static const char* levels[] = {"INFO", "WARN", "ERROR", "DEBUG"};
  static const char* services[] = {"auth", "billing", "search", "gateway", "profile"};
  static const char* hosts[] = {"edge-01", "edge-02", "api-01", "api-02"};
  static const char* messages[] = {
    "login ok",
    "token refresh",
    "search query",
    "checkout complete",
    "profile update",
    "permission denied"
  };
  static const char* paths[] = {"/login", "/search", "/checkout", "/profile", "/logout"};
  static const char* plans[] = {"free", "pro", "enterprise"};
  static const char* tags[] = {"prod", "staging", "us-east", "eu-west", "auth", "payments"};
  static const char* metrics[] = {"db", "cache", "net", "cpu"};

  std::string s;
  s.reserve(static_cast<std::size_t>(n) * 220u + 2u);
  s += '[';
  for (int i = 0; i < n; ++i) {
    if (i) s += ',';
    s += '{';
    s += "\"ts\":";
    s += std::to_string(1710500000 + i);
    s += ",\"level\":\"";
    s += levels[i % 4];
    s += "\",\"service\":\"";
    s += services[i % 5];
    s += "\",\"host\":\"";
    s += hosts[i % 4];
    s += "\",\"message\":\"";
    s += messages[i % 6];
    s += "\",\"user\":{\"id\":";
    s += std::to_string(100000 + i);
    s += ",\"tenant\":\"t";
    s += std::to_string(i % 200);
    s += "\",\"plan\":\"";
    s += plans[i % 3];
    s += "\"}";
    s += ",\"ctx\":{\"ip\":\"10.0.";
    s += std::to_string(i % 256);
    s += '.';
    s += std::to_string((i / 256) % 256);
    s += "\",\"req\":\"r";
    s += std::to_string(1000000 + i);
    s += "\",\"path\":\"";
    s += paths[i % 5];
    s += "\"}";
    int latency_int = 5 + (i % 400);
    int latency_frac = static_cast<int>((static_cast<long long>(i) * 1234567) % 1000000000LL);
    s += ",\"latency_ms\":";
    s += std::to_string(latency_int);
    s += '.';
    char frac_buf[10];
    std::snprintf(frac_buf, sizeof(frac_buf), "%09d", latency_frac);
    s += frac_buf;
    s += ",\"ok\":";
    s += (i % 20 == 0) ? "false" : "true";
    s += ",\"tags\":[\"";
    s += tags[i % 6];
    s += "\",\"";
    s += tags[(i + 2) % 6];
    s += "\"]";
    s += ",\"metrics\":[{\"name\":\"";
    s += metrics[i % 4];
    s += "\",\"value\":";
    s += std::to_string(1 + (i % 10));
    s += "},{\"name\":\"";
    s += metrics[(i + 1) % 4];
    s += "\",\"value\":";
    s += std::to_string(1 + ((i + 3) % 10));
    s += "}]";
    s += '}';
  }
  s += ']';
  return s;
}

struct log_dataset {
  std::string json;
  simdjson::padded_string padded;
  std::size_t count;
  explicit log_dataset(int n)
      : json(make_log_dataset(n)), padded(json), count(static_cast<std::size_t>(n)) { }
};

static const log_dataset& log_dataset_for(int n) {
  static std::mutex lock;
  static std::map<int, std::unique_ptr<log_dataset>> cache;
  std::lock_guard<std::mutex> guard(lock);
  auto it = cache.find(n);
  if (it == cache.end()) {
    it = cache.emplace(n, std::make_unique<log_dataset>(n)).first;
  }
  return *it->second;
}

static void set_counters(benchmark::State& state, const log_dataset& data) {
  state.SetBytesProcessed(
    static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(data.json.size())
  );
  state.SetItemsProcessed(
    static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(data.count)
  );
}

// =============================================================================
// Glaze schema (for bind benchmark)
// =============================================================================

struct metric {
  std::string name;
  std::uint64_t value;
};

struct user {
  std::uint64_t id;
  std::string tenant;
  std::string plan;
};

struct ctx {
  std::string ip;
  std::string req;
  std::string path;
};

struct event {
  std::uint64_t ts;
  std::string level;
  std::string service;
  std::string host;
  std::string message;
  user usr;
  ctx context;
  double latency_ms;
  bool ok;
  std::vector<std::string> tags;
  std::vector<metric> metrics;
};

namespace glz {
template <>
struct meta<metric> {
  using T = metric;
  static constexpr auto value = object("name", &T::name, "value", &T::value);
};
template <>
struct meta<user> {
  using T = user;
  static constexpr auto value = object("id", &T::id, "tenant", &T::tenant, "plan", &T::plan);
};
template <>
struct meta<ctx> {
  using T = ctx;
  static constexpr auto value = object("ip", &T::ip, "req", &T::req, "path", &T::path);
};
template <>
struct meta<event> {
  using T = event;
  static constexpr auto value = object(
    "ts",
    &T::ts,
    "level",
    &T::level,
    "service",
    &T::service,
    "host",
    &T::host,
    "message",
    &T::message,
    "user",
    &T::usr,
    "ctx",
    &T::context,
    "latency_ms",
    &T::latency_ms,
    "ok",
    &T::ok,
    "tags",
    &T::tags,
    "metrics",
    &T::metrics
  );
};
}  // namespace glz

// =============================================================================
// Parse benchmarks — realistic log datasets
// =============================================================================

static void Parse_Log_simdjson(benchmark::State& state) {
  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  simdjson::dom::parser parser;
  for (auto _ : state) {
    auto doc = parser.parse(data.padded);
    benchmark::DoNotOptimize(doc);
  }
  set_counters(state, data);
}

static void Parse_Log_crjson(benchmark::State& state) {
  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    auto doc = parse{data.json};
    benchmark::DoNotOptimize(doc);
  }
  set_counters(state, data);
}

static void Parse_Log_rapidjson(benchmark::State& state) {
  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    rapidjson::Document d;
    d.Parse(data.json.data(), data.json.size());
    benchmark::DoNotOptimize(d);
  }
  set_counters(state, data);
}

static void Parse_Log_nlohmann(benchmark::State& state) {
  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    auto doc = nlohmann::json::parse(data.json);
    benchmark::DoNotOptimize(doc);
  }
  set_counters(state, data);
}

static void Parse_Log_glaze(benchmark::State& state) {
  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    glz::generic doc;
    auto ec = glz::read_json(doc, std::string_view{data.json});
    benchmark::DoNotOptimize(doc);
    benchmark::DoNotOptimize(ec);
  }
  set_counters(state, data);
}

// =============================================================================
// Parse + access benchmarks — realistic log datasets
// =============================================================================

static void ParseAccess_Log_simdjson(benchmark::State& state) {
  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  simdjson::dom::parser parser;
  for (auto _ : state) {
    double latency_sum = 0.0;
    std::size_t ok_count = 0;
    for (auto obj : parser.parse(data.padded)) {
      latency_sum += obj["latency_ms"].get_double().value();
      ok_count += obj["ok"].get_bool().value();
      latency_sum += static_cast<double>(obj["user"]["id"].get_int64().value());
    }
    benchmark::DoNotOptimize(latency_sum);
    benchmark::DoNotOptimize(ok_count);
  }
  set_counters(state, data);
}

static void ParseAccess_Log_crjson(benchmark::State& state) {
  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    parse doc{data.json};
    auto root = doc.root();
    double latency_sum = 0.0;
    std::size_t ok_count = 0;
    for (std::size_t i = 0; i < root.size(); ++i) {
      auto obj = root[i];
      latency_sum += obj["latency_ms"].as_num();
      ok_count += obj["ok"].as_bool();
      latency_sum += obj["user"]["id"].as_num();
    }
    benchmark::DoNotOptimize(latency_sum);
    benchmark::DoNotOptimize(ok_count);
  }
  set_counters(state, data);
}

static void ParseAccess_Log_rapidjson(benchmark::State& state) {
  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    rapidjson::Document d;
    d.Parse(data.json.data(), data.json.size());
    double latency_sum = 0.0;
    std::size_t ok_count = 0;
    for (auto& obj : d.GetArray()) {
      latency_sum += obj["latency_ms"].GetDouble();
      ok_count += obj["ok"].GetBool();
      latency_sum += static_cast<double>(obj["user"]["id"].GetInt64());
    }
    benchmark::DoNotOptimize(latency_sum);
    benchmark::DoNotOptimize(ok_count);
  }
  set_counters(state, data);
}

static void ParseAccess_Log_nlohmann(benchmark::State& state) {
  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    auto doc = nlohmann::json::parse(data.json);
    double latency_sum = 0.0;
    std::size_t ok_count = 0;
    for (auto& obj : doc) {
      latency_sum += obj["latency_ms"].get<double>();
      ok_count += obj["ok"].get<bool>();
      latency_sum += obj["user"]["id"].get<double>();
    }
    benchmark::DoNotOptimize(latency_sum);
    benchmark::DoNotOptimize(ok_count);
  }
  set_counters(state, data);
}

static void ParseAccess_Log_glaze(benchmark::State& state) {
  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    glz::generic doc;
    auto ec = glz::read_json(doc, std::string_view{data.json});
    if (ec) {
      benchmark::DoNotOptimize(ec);
      continue;
    }
    double latency_sum = 0.0;
    std::size_t ok_count = 0;
    const auto* events = doc.get_if<glz::generic::array_t>();
    if (!events) {
      benchmark::DoNotOptimize(doc);
      continue;
    }
    for (const auto& ev : *events) {
      const auto* obj = ev.get_if<glz::generic::object_t>();
      if (!obj) continue;
      if (auto it = obj->find("latency_ms"); it != obj->end()) {
        if (const auto* lat = it->second.get_if<double>()) latency_sum += *lat;
      }
      if (auto it = obj->find("ok"); it != obj->end()) {
        if (const auto* ok = it->second.get_if<bool>()) ok_count += *ok;
      }
      if (auto it = obj->find("user"); it != obj->end()) {
        if (const auto* user_obj = it->second.get_if<glz::generic::object_t>()) {
          if (auto id_it = user_obj->find("id"); id_it != user_obj->end()) {
            if (const auto* id = id_it->second.get_if<double>()) latency_sum += *id;
          }
        }
      }
    }
    benchmark::DoNotOptimize(latency_sum);
    benchmark::DoNotOptimize(ok_count);
    benchmark::DoNotOptimize(ec);
  }
  set_counters(state, data);
}

// =============================================================================
// Schema binding benchmarks (crjson + glaze)
// =============================================================================

static void Bind_Log_glaze(benchmark::State& state) {
  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    std::vector<event> events;
    auto ec = glz::read_json(events, std::string_view{data.json});
    benchmark::DoNotOptimize(events);
    benchmark::DoNotOptimize(ec);
  }
  set_counters(state, data);
}

static void Bind_Log_crjson(benchmark::State& state) {
  using metric = jobj<jkv<"name"_fs, jstr>, jkv<"value"_fs, jnum>>;
  using user = jobj<jkv<"id"_fs, jnum>, jkv<"tenant"_fs, jstr>, jkv<"plan"_fs, jstr>>;
  using ctx = jobj<jkv<"ip"_fs, jstr>, jkv<"req"_fs, jstr>, jkv<"path"_fs, jstr>>;
  using event = jobj<
    jkv<"ts"_fs, jnum>,
    jkv<"level"_fs, jstr>,
    jkv<"service"_fs, jstr>,
    jkv<"host"_fs, jstr>,
    jkv<"message"_fs, jstr>,
    jkv<"user"_fs, user>,
    jkv<"ctx"_fs, ctx>,
    jkv<"latency_ms"_fs, jnum>,
    jkv<"ok"_fs, jbool>,
    jkv<"tags"_fs, jarr<jstr>>,
    jkv<"metrics"_fs, jarr<metric>>>;

  const auto& data = log_dataset_for(static_cast<int>(state.range(0)));
  for (auto _ : state) {
    auto doc = bind<jarr<event>>{data.json};
    benchmark::DoNotOptimize(doc);
  }
  set_counters(state, data);
}

CRJSON_BENCHMARK(Parse_Log_simdjson)->Apply(ApplyLogSizes);
CRJSON_BENCHMARK(ParseAccess_Log_simdjson)->Apply(ApplyLogSizes);
CRJSON_BENCHMARK(Parse_Log_nlohmann)->Apply(ApplyLogSizes);
CRJSON_BENCHMARK(ParseAccess_Log_nlohmann)->Apply(ApplyLogSizes);
CRJSON_BENCHMARK(Parse_Log_crjson)->Apply(ApplyLogSizes);
CRJSON_BENCHMARK(ParseAccess_Log_crjson)->Apply(ApplyLogSizes);
CRJSON_BENCHMARK(Parse_Log_glaze)->Apply(ApplyLogSizes);
CRJSON_BENCHMARK(ParseAccess_Log_glaze)->Apply(ApplyLogSizes);
CRJSON_BENCHMARK(Parse_Log_rapidjson)->Apply(ApplyLogSizes);
CRJSON_BENCHMARK(ParseAccess_Log_rapidjson)->Apply(ApplyLogSizes);
CRJSON_BENCHMARK(Bind_Log_crjson)->Apply(ApplyLogSizes);
CRJSON_BENCHMARK(Bind_Log_glaze)->Apply(ApplyLogSizes);

BENCHMARK_MAIN();
