// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

// Free-function helpers that complement the JsonObject / Json types in
// json_types.h. Two flavours live here:
//
//   1. Quick single-key extractors that scan the raw JSON text WITHOUT
//      building a parse tree. Use them when you have a large JSON document
//      and only need one or two values — they have constant memory cost
//      and avoid the std::vector allocations of the full parser. The trade-
//      off is that they are linear in the document length and only work for
//      first-level (or single-occurrence) keys.
//
//   2. Mutating helpers for building JsonObject documents (`putTojson`,
//      `findInObject`, `updateInObject`). These keep insertion order which
//      matters when the resulting JSON is consumed by something that expects
//      a stable field order (HTTP APIs, MQTT topics, etc.).
//
// All helpers live in `ungula::json::`.

#include <algorithm>
#include <cstring>

#include "json_types.h"

namespace ungula {
    namespace json {

        /// @brief Serialize a JsonObject into a freshly allocated string.
        JsonStr serializeJson(const JsonObject& json);

        /// @brief Serialize a JsonObject into the caller-provided buffer.
        /// @param json The JSON object to serialize.
        /// @param out  The destination string. Cleared before writing.
        void serializeJson(const JsonObject& json, JsonStr& out);

        /// @brief Extract the value of a top-level string key from a raw buffer
        ///        without parsing the entire document.
        ///
        /// Designed for the very common embedded pattern of receiving a JSON
        /// envelope where the only field you actually care about is something
        /// like `"action"` and the rest of the payload is large or irregular.
        ///
        /// Example:
        /// ```cpp
        /// const char* buf = R"({"action":"capture","payload":{ ... lots ... }})";
        /// auto action = ungula::json::jsonExtractStringKey(buf, std::strlen(buf), "action");
        /// // action == "capture"
        /// ```
        ///
        /// @param buf     Raw JSON character buffer (does NOT need to be NUL-terminated).
        /// @param buf_len Length of `buf` in bytes.
        /// @param key     The key to look up. Must be a plain identifier with no dots.
        /// @return The string value associated with the key, or an empty
        ///         string if the key isn't found, the value isn't a quoted
        ///         string, or the document is truncated.
        string_t jsonExtractStringKey(const char* buf, size_t buf_len, const char* key);

        /// @brief Extract a value as string from a JSON document by simple key,
        ///        accepting both quoted and unquoted values.
        ///
        /// Linear scan, returns the first occurrence. Does NOT walk dotted
        /// paths — pass the leaf key only.
        ///
        /// @param json                 The JSON text to scan.
        /// @param key                  The leaf key (no dots).
        /// @param expected_len_result  Hint for the destination string capacity.
        string_t jsonExtractAsStr(const string_t& json, const char* key,
                                  int expected_len_result = 128);

        /// @brief Extract an integer value from a JSON document by simple key.
        /// @return The parsed int, or 0 if the key isn't found / not numeric.
        int jsonExtractAsInt(const string_t& json, const char* key, int len_number = 8);

        /// @brief Find a key in a JsonObject. Returns nullptr when missing.
        inline Json* findInObject(JsonObject& obj, const string_t& key) {
            auto it = std::find_if(obj.begin(), obj.end(),
                                   [&](auto const& kv) { return kv.first == key; });
            return (it == obj.end() ? nullptr : &it->second);
        }

        /// @brief Const overload of `findInObject`.
        inline const Json* findInObject(const JsonObject& obj, const string_t& key) {
            auto it = std::find_if(obj.begin(), obj.end(),
                                   [&](auto const& kv) { return kv.first == key; });
            return (it == obj.end() ? nullptr : &it->second);
        }

        /// @brief Update an existing key in a JsonObject.
        /// @return True if the key existed and was updated, false otherwise.
        inline bool updateInObject(JsonObject& obj, const string_t& key, const Json& newValue) {
            if (auto* p = findInObject(obj, key)) {
                *p = newValue;
                return true;
            }
            return false;
        }

        /// @brief Insert-or-update a key in a JsonObject. Order is preserved.
        inline void putTojson(JsonObject& doc, const string_t& key, const Json& value) {
            if (updateInObject(doc, key, value)) {
                return;
            }
            doc.emplace_back(key, value);
        }

        /// @brief Insert-or-update a key whose value is itself an object.
        inline void putTojson(JsonObject& doc, const string_t& key, const JsonObject& value) {
            if (updateInObject(doc, key, value)) {
                return;
            }
            doc.emplace_back(key, Json(value));
        }

    }  // namespace json
}  // namespace ungula
