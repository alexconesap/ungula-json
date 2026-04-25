// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

// Core JSON value types for the embedded JSON library.
//
// Two pieces live here:
//   * `Json` — a tagged-union value that can hold null, string, int, float,
//     double, bool, or a nested object. It supports type-safe accessors,
//     dotted-path lookup, in-place edits and recursive serialization.
//   * `JsonObject` — an ordered list of (key, Json) pairs. Used as the
//     underlying container for JSON objects so insertion order is preserved
//     when serializing (unlike `std::map` which would re-sort the keys).
//
// Design notes:
//   - The library targets ESP32-class devices: it uses `std::variant`,
//     `std::vector` and `std::string` from the C++ standard library, but
//     never `<iostream>`, never any Arduino header, never any logging
//     dependency. Memory grows linearly with the number of keys parsed.
//   - The dotted key syntax (`"a.b.c"`) is the canonical way to address a
//     nested value across the whole API: `find`, `has`, `update`, `add`,
//     `remove` all accept it.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include <util/string_types.h>
#include <util/string_utils.h>

namespace ungula {
    namespace json {

        using JsonStr = string_t;
        using JsonStrView = string_view_t;

        struct Json;  // forward declaration

        using JsonObject = std::vector<std::pair<string_t, Json>>;

        /// @brief Tagged-union JSON value.
        ///
        /// Holds one of: null, string, int, float, double, bool, or a nested
        /// `JsonObject`. The Type enum mirrors the variant index so callers can
        /// switch on it cheaply. Implicit conversions are provided for the
        /// common literal types so you can write idiomatic code:
        ///
        /// ```cpp
        /// using namespace ungula::json;
        /// JsonObject person = {
        ///   {"name",    Json("Alice")},
        ///   {"age",     Json(30)},
        ///   {"premium", Json(true)},
        /// };
        /// ```
        struct Json {
                using JsonValue = std::variant<std::monostate, string_t, int, float, double, bool,
                                               JsonObject>;

                JsonValue value;

                enum class Type : uint8_t {
                    Null = 0,
                    String,
                    Int,
                    Float,
                    Double,
                    Bool,
                    Object,
                };

                Type type() const {
                    // Ordinal must match the JsonValue alternative order above.
                    switch (value.index()) {
                        case 0:
                            return Type::Null;
                        case 1:
                            return Type::String;
                        case 2:
                            return Type::Int;
                        case 3:
                            return Type::Float;
                        case 4:
                            return Type::Double;
                        case 5:
                            return Type::Bool;
                        case 6:
                            return Type::Object;
                        default:
                            return Type::Null;
                    }
                }

                Json() = default;
                Json(const JsonValue& val) : value(val) {}
                Json(JsonValue&& val) : value(std::move(val)) {}

                // Implicit conversions for ergonomic literal construction.
                Json(std::nullptr_t) : value(std::monostate{}) {}
                Json(const char* val) : value(val == nullptr ? "" : string_t(val)) {}
                Json(const string_t& val) : value(val) {}
                Json(int val) : value(val) {}
                Json(float val) : value(val) {}
                Json(double val) : value(val) {}
                Json(unsigned long val) : value(static_cast<double>(val)) {}
                Json(bool val) : value(val) {}
                Json(const JsonObject& obj) : value(obj) {}

                bool isObject() const {
                    return std::holds_alternative<JsonObject>(value);
                }
                bool isString() const {
                    return std::holds_alternative<string_t>(value);
                }
                bool isInt() const {
                    return std::holds_alternative<int>(value);
                }
                bool isFloat() const {
                    return std::holds_alternative<float>(value);
                }
                bool isDouble() const {
                    return std::holds_alternative<double>(value);
                }
                bool isBool() const {
                    return std::holds_alternative<bool>(value);
                }
                bool isNull() const {
                    return std::holds_alternative<std::monostate>(value);
                }

                bool isEmpty() const {
                    return std::holds_alternative<std::monostate>(value) ||
                           (std::holds_alternative<string_t>(value) &&
                            std::get<string_t>(value).empty());
                }

                // ---- Dotted-path lookup -------------------------------------------
                //
                // `find("a.b.c")` walks into nested objects. Both const and non-const
                // overloads share the same implementation through `findImpl`.

                Json* find(const string_t& key) {
                    return findImpl(key);
                }
                const Json* find(const string_t& key) const {
                    return const_cast<Json*>(this)->findImpl(key);
                }

                bool has(const string_t& key) const {
                    return find(key) != nullptr;
                }

                bool update(const string_t& key, const Json& newVal) {
                    if (auto* p = find(key)) {
                        *p = newVal;
                        return true;
                    }
                    return false;
                }

                bool remove(const string_t& key) {
                    if (!isObject()) {
                        return false;
                    }
                    auto& obj = std::get<JsonObject>(value);

                    auto dot = key.find('.');
                    if (dot == string_t::npos) {
                        // top-level remove
                        auto it = std::remove_if(obj.begin(), obj.end(),
                                                 [&](auto& kv) { return kv.first == key; });
                        if (it == obj.end()) {
                            return false;
                        }
                        obj.erase(it, obj.end());
                        return true;
                    }
                    // nested remove: descend into the prefix
                    string_t head = key.substr(0, dot);
                    string_t tail = key.substr(dot + 1);
                    for (auto& kv : obj) {
                        if (kv.first == head) {
                            return kv.second.remove(tail);
                        }
                    }
                    return false;
                }

                bool add(const string_t& key, const Json& newVal) {
                    if (!isObject()) {
                        return false;
                    }
                    auto& obj = std::get<JsonObject>(value);

                    auto dot = key.find('.');
                    if (dot == string_t::npos) {
                        // top-level add (no overwrite)
                        if (has(key)) {
                            return false;
                        }
                        obj.emplace_back(key, newVal);
                        return true;
                    }
                    // nested add: descend into the prefix
                    string_t head = key.substr(0, dot);
                    string_t tail = key.substr(dot + 1);
                    for (auto& kv : obj) {
                        if (kv.first == head) {
                            return kv.second.add(tail, newVal);
                        }
                    }
                    return false;
                }

                // ---- Typed accessors ----------------------------------------------
                // Each returns std::nullopt if the value is not the requested type.

                std::optional<string_t> asString() const {
                    if (auto* p = std::get_if<string_t>(&value)) {
                        return *p;
                    }
                    return std::nullopt;
                }
                std::optional<int> asInt() const {
                    if (auto* p = std::get_if<int>(&value)) {
                        return *p;
                    }
                    return std::nullopt;
                }
                std::optional<float> asFloat() const {
                    if (auto* p = std::get_if<float>(&value)) {
                        return *p;
                    }
                    return std::nullopt;
                }
                std::optional<double> asDouble() const {
                    if (auto* p = std::get_if<double>(&value)) {
                        return *p;
                    }
                    return std::nullopt;
                }
                std::optional<bool> asBool() const {
                    if (auto* p = std::get_if<bool>(&value)) {
                        return *p;
                    }
                    return std::nullopt;
                }
                std::optional<JsonObject> asObject() const {
                    if (auto* p = std::get_if<JsonObject>(&value)) {
                        return *p;
                    }
                    return std::nullopt;
                }
                std::optional<std::monostate> asNull() const {
                    if (std::holds_alternative<std::monostate>(value)) {
                        return std::monostate{};
                    }
                    return std::nullopt;
                }

                /// @brief Recursively serialize this value to JSON text.
                /// @param quote_strings If true, string values are wrapped in
                ///        double quotes. Set this to false when you want the bare
                ///        value (e.g. for embedding into a printed table).
                string_t serialize(bool quote_strings = false) const {
                    struct Visitor {
                            bool quote;
                            explicit Visitor(bool quote_in) : quote(quote_in) {}

                            string_t operator()(std::monostate) const {
                                return "null";
                            }
                            string_t operator()(const string_t& str) const {
                                if (quote) {
                                    string_t out;
                                    out.push_back('"');
                                    out += str::escapeString(str);
                                    out.push_back('"');
                                    return out;
                                }
                                return str::escapeString(str);
                            }
                            string_t operator()(int val) const {
                                return str::num_to_string(val);
                            }
                            string_t operator()(float val) const {
                                return str::num_to_string(val);
                            }
                            string_t operator()(double val) const {
                                return str::num_to_string(val);
                            }
                            string_t operator()(bool val) const {
                                return val ? "true" : "false";
                            }
                            string_t operator()(const JsonObject& obj) const {
                                string_t out = "{";
                                bool first = true;
                                for (const auto& pair : obj) {
                                    if (!first) {
                                        out.push_back(',');
                                    }
                                    first = false;
                                    out.push_back('"');
                                    out += str::escapeString(pair.first);
                                    out.push_back('"');
                                    out.push_back(':');
                                    out += pair.second.serialize(quote);
                                }
                                out.push_back('}');
                                return out;
                            }
                    };
                    return std::visit(Visitor{quote_strings}, value);
                }

            private:
                // Single implementation backing both const and non-const find().
                Json* findImpl(const string_t& key) {
                    if (!isObject()) {
                        return nullptr;
                    }
                    auto& obj = std::get<JsonObject>(value);

                    auto dot = key.find('.');
                    if (dot == string_t::npos) {
                        for (auto& kv : obj) {
                            if (kv.first == key) {
                                return &kv.second;
                            }
                        }
                        return nullptr;
                    }
                    // nested lookup
                    string_t head = key.substr(0, dot);
                    string_t tail = key.substr(dot + 1);
                    for (auto& kv : obj) {
                        if (kv.first == head) {
                            return kv.second.find(tail);
                        }
                    }
                    return nullptr;
                }
        };

    }  // namespace json
}  // namespace ungula
