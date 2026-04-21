// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "json/json_utils.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "json/json_types.h"

namespace ungula {
    namespace json {

        // ---- Direct single-key extraction ---------------------------------------
        //
        // These functions deliberately avoid building a parse tree. They scan
        // the source buffer linearly looking for a `"key"` token followed by a
        // `:` and a value, and return as soon as they find a match. The cost is
        // O(buf_len) memory-wise it's a single allocation for the result string.

        string_t jsonExtractStringKey(const char* buf, size_t buf_len, const char* key) {
            size_t keyLen = 0;
            while (key[keyLen] != '\0') {
                ++keyLen;
            }

            for (size_t i = 0; i + keyLen + 2 < buf_len; ++i) {
                if (buf[i] != '"') {
                    continue;
                }
                // Does buf[i+1 .. i+1+keyLen-1] match the key?
                if (std::memcmp(buf + i + 1, key, keyLen) != 0) {
                    continue;
                }
                size_t kEnd = i + 1 + keyLen;
                if (kEnd >= buf_len || buf[kEnd] != '"') {
                    continue;
                }

                // Skip whitespace, expect ':'.
                size_t j = kEnd + 1;
                while (j < buf_len && std::isspace(static_cast<unsigned char>(buf[j])) != 0) {
                    ++j;
                }
                if (j >= buf_len || buf[j] != ':') {
                    continue;
                }
                ++j;

                // Skip whitespace, expect opening quote.
                while (j < buf_len && std::isspace(static_cast<unsigned char>(buf[j])) != 0) {
                    ++j;
                }
                if (j >= buf_len || buf[j] != '"') {
                    continue;
                }
                ++j;

                // Capture until the closing quote.
                size_t start = j;
                while (j < buf_len && buf[j] != '"') {
                    ++j;
                }
                if (j >= buf_len) {
                    return {};  // unterminated string
                }
                return string_t(buf + start, j - start);
            }
            return {};
        }

        string_t jsonExtractAsStr(const string_t& json, const char* key, int expected_len_result) {
            if (json.empty() || key == nullptr || *key == '\0') {
                return "";
            }

            auto p = json.find(key);
            if (p == string_t::npos) {
                return "";
            }
            if (p > 0 && json[p - 1] != '"') {
                return "";  // key must be quoted
            }

            const int json_len = static_cast<int>(json.size());

            p += std::strlen(key) + 1;  // skip the closing quote of the key
            while (static_cast<int>(p) < json_len &&
                   std::isspace(static_cast<unsigned char>(json[p])) != 0) {
                ++p;
            }
            if (static_cast<int>(p) >= json_len) {
                return "";
            }

            // Expect colon after key.
            if (json[p] != ':') {
                return "";
            }
            ++p;
            while (static_cast<int>(p) < json_len &&
                   std::isspace(static_cast<unsigned char>(json[p])) != 0) {
                ++p;
            }
            if (static_cast<int>(p) >= json_len) {
                return "";
            }

            // Quoted or unquoted value?
            bool is_quoted = false;
            if (json[p] == '"') {
                is_quoted = true;
                ++p;
            } else {
                while (static_cast<int>(p) < json_len &&
                       std::isspace(static_cast<unsigned char>(json[p])) != 0) {
                    ++p;
                }
            }
            if (static_cast<int>(p) >= json_len) {
                return "";
            }

            string_t value;
            value.reserve(static_cast<size_t>(expected_len_result));
            while (static_cast<int>(p) < json_len) {
                if (is_quoted) {
                    if (json[p] == '"') {
                        break;  // end of quoted string
                    }
                } else {
                    if (json[p] == ',' || json[p] == '}' ||
                        std::isspace(static_cast<unsigned char>(json[p])) != 0) {
                        break;
                    }
                }
                value += json[p++];
            }
            return value;
        }

        int jsonExtractAsInt(const string_t& json, const char* key, int /*len_number*/) {
            return std::atoi(jsonExtractAsStr(json, key).c_str());
        }

        // ---- Serialization ------------------------------------------------------

        JsonStr serializeJson(const JsonObject& json) {
            JsonStr out;
            out.reserve(256);
            serializeJson(json, out);
            return out;
        }

        void serializeJson(const JsonObject& json, JsonStr& out) {
            out.clear();
            out.push_back('{');

            bool first = true;
            for (const auto& [key, value] : json) {
                if (!first) {
                    out.push_back(',');
                }
                first = false;

                out.push_back('"');
                out += stru::escapeString(key);
                out.push_back('"');
                out.push_back(':');

                out += value.serialize(true);  // quote string values
            }

            out.push_back('}');
        }

    }  // namespace json
}  // namespace ungula
