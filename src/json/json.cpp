// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "json/json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

#include <util/string_utils.h>

namespace ungula {
    namespace json {

        // -----------------------------------------------------------------------
        // JsonWrapper — slicing constructor (sub-document by key)
        // -----------------------------------------------------------------------

        JsonWrapper::JsonWrapper(const JsonWrapper& json, const char* key, const char* new_root) {
            if (!json.isValidJson() || !json.has(key)) {
                return;  // invalid source or missing key — leave wrapper empty
            }

            const auto& source = json.keyValueMap_;

            if (new_root != nullptr) {
                // Build the source-prefix and the rename-prefix, both with a
                // trailing dot so prefix substitution works for the leaf keys.
                string_t key_dotted(key);
                key_dotted.push_back('.');

                string_t new_root_dotted(new_root);
                if (*new_root != '\0') {
                    new_root_dotted.push_back('.');
                }

                for (const auto& [full_key, value] : source) {
                    if (full_key != key && str::startsWith(full_key, key)) {
                        string_t current_key = full_key;
                        str::replaceAll(current_key, key_dotted, new_root_dotted);
                        keyValueMap_.emplace_back(std::move(current_key), value);
                    }
                }
            } else {
                // No renaming — keep the original full keys.
                for (const auto& [full_key, value] : source) {
                    if (full_key != key && str::startsWith(full_key, key)) {
                        keyValueMap_.emplace_back(full_key, value);
                    }
                }
            }

            validJson_ = !keyValueMap_.empty();
            isEmpty_ = keyValueMap_.empty();
        }

        JsonWrapper::JsonWrapper(const json_map_t& json_map) {
            keyValueMap_ = json_map;
            validJson_ = !keyValueMap_.empty();
            isEmpty_ = keyValueMap_.empty();
        }

        // -----------------------------------------------------------------------
        // Parser entry point
        // -----------------------------------------------------------------------

        void JsonWrapper::parseJson(const char* jsonStr, uint8_t levels) {
            keyValueMap_.clear();
            max_parseLevels_ = levels;
            validJson_ = false;
            isEmpty_ = true;

            if (jsonStr == nullptr || *jsonStr == '\0') {
                return;
            }

            const char* p = str::skipWhitespace(jsonStr);
            if (*p == '\0') {
                return;
            }

            // Pre-reserve based on a colon count — overshoots a bit for nested
            // objects but avoids most of the per-key reallocations.
            keyValueMap_.reserve(str::countChar(string_t(jsonStr), ':'));

            char keyBuffer[512];  // upper bound on dotted-key length
            p = parseObject(p, keyBuffer, 0, 0);
            if (p != nullptr) {
                p = str::skipWhitespace(p);
                validJson_ = (*p == '\0');
                isEmpty_ = keyValueMap_.empty();
            }
        }

        // -----------------------------------------------------------------------
        // Recursive object parser
        //
        // Walks {...} blocks. When the parse depth has hit `max_parseLevels_`,
        // the body is skipped (brace-counted) so the rest of the document still
        // parses. The keyBuffer carries the dotted prefix the caller already
        // built so leaf keys can be stored as full paths.
        // -----------------------------------------------------------------------

        const char* JsonWrapper::parseObject(const char* p, char* keyBuffer, size_t bufferOffset,
                                             int depth) {
            if (depth >= maxParseLevels()) {
                // Past the depth cap — fast-forward to the matching closing brace.
                ++p;
                int braces = 1;
                while (*p != '\0' && braces > 0) {
                    if (*p == '{') {
                        ++braces;
                    } else if (*p == '}') {
                        --braces;
                    }
                    ++p;
                }
                return p;
            }

            p = str::skipWhitespace(p);
            if (*p != '{') {
                return nullptr;
            }
            ++p;  // consume '{'

            // Record an entry for the parent intermediate key (so callers can
            // probe `has("payload.settings")` even though it has no leaf value).
            if (bufferOffset > 0) {
                keyBuffer[bufferOffset] = '\0';
                keyValueMap_.emplace_back(keyBuffer, nullptr);
            }

            p = str::skipWhitespace(p);
            if (*p == '}') {
                return p + 1;  // empty object
            }

            char* end = nullptr;
            float fval = 0.0f;
            double dval = 0.0;
            while (*p != '\0' && *p != '}') {
                p = str::skipWhitespace(p);
                if (*p == '}') {
                    break;
                }

                // Parse the key directly into keyBuffer (after a dot if we're nested).
                size_t keyStart = bufferOffset;
                if (bufferOffset > 0) {
                    keyBuffer[bufferOffset++] = '.';
                }

                size_t keyLen = 0;
                p = parseStringDirect(p, keyBuffer + bufferOffset, &keyLen);
                if (p == nullptr) {
                    return nullptr;
                }
                str::trimWhitespace(keyBuffer + bufferOffset, keyLen);
                bufferOffset += keyLen;

                // Expect colon.
                p = str::skipWhitespace(p);
                if (*p != ':') {
                    return nullptr;
                }
                ++p;
                p = str::skipWhitespace(p);

                if (*p == '{') {
                    // Nested object — recurse.
                    p = parseObject(p, keyBuffer, bufferOffset, depth + 1);
                    if (p == nullptr) {
                        return nullptr;
                    }
                } else {
                    // Parse value.
                    char valueBuffer[256];
                    size_t valueLen = 0;
                    Json::Type type = Json::Type::Null;
                    p = parseValueDirect(p, valueBuffer, &valueLen, type);
                    if (p == nullptr) {
                        return nullptr;
                    }

                    keyBuffer[bufferOffset] = '\0';
                    valueBuffer[valueLen] = '\0';

                    switch (type) {
                        case Json::Type::String:
                            keyValueMap_.emplace_back(keyBuffer,
                                                      Json(string_t(valueBuffer, valueLen)));
                            break;
                        case Json::Type::Int:
                            keyValueMap_.emplace_back(keyBuffer, Json(std::atoi(valueBuffer)));
                            break;
                        case Json::Type::Float:
                            end = nullptr;
                            fval = std::strtof(valueBuffer, &end);
                            if (end == valueBuffer) {
                                fval = std::numeric_limits<float>::quiet_NaN();
                            }
                            keyValueMap_.emplace_back(keyBuffer, Json(fval));
                            break;
                        case Json::Type::Double:
                            end = nullptr;
                            dval = std::strtod(valueBuffer, &end);
                            if (end == valueBuffer) {
                                dval = std::numeric_limits<double>::quiet_NaN();
                            }
                            keyValueMap_.emplace_back(keyBuffer, Json(dval));
                            break;
                        case Json::Type::Bool:
                            keyValueMap_.emplace_back(keyBuffer, Json(valueBuffer[0] == 't' ||
                                                                      valueBuffer[0] == 'T'));
                            break;
                        case Json::Type::Null:
                            keyValueMap_.emplace_back(keyBuffer, Json(nullptr));
                            break;
                        default:
                            return nullptr;  // unsupported type
                    }
                }

                // Reset the buffer offset back to the parent level.
                bufferOffset = keyStart;

                p = str::skipWhitespace(p);
                if (*p == ',') {
                    ++p;
                } else if (*p != '}') {
                    return nullptr;
                }
            }

            if (*p != '}') {
                return nullptr;
            }
            return p + 1;  // consume '}'
        }

        // -----------------------------------------------------------------------
        // Quoted-string parser. Handles \n \t \r \\ \" — anything else after a
        // backslash is passed through verbatim.
        // -----------------------------------------------------------------------

        const char* JsonWrapper::parseStringDirect(const char* p, char* buffer,
                                                   size_t* outLen) const {
            *outLen = 0;
            if (*p != '"') {
                return nullptr;
            }
            ++p;  // skip opening quote

            while (*p != '\0' && *p != '"') {
                if (*p == '\\' && *(p + 1) != '\0') {
                    ++p;
                    switch (*p) {
                        case 'n':
                            buffer[(*outLen)++] = '\n';
                            break;
                        case 't':
                            buffer[(*outLen)++] = '\t';
                            break;
                        case 'r':
                            buffer[(*outLen)++] = '\r';
                            break;
                        case '\\':
                            buffer[(*outLen)++] = '\\';
                            break;
                        case '"':
                            buffer[(*outLen)++] = '"';
                            break;
                        default:
                            buffer[(*outLen)++] = *p;
                            break;
                    }
                } else {
                    buffer[(*outLen)++] = *p;
                }
                ++p;
            }

            if (*p != '"') {
                return nullptr;  // unterminated string
            }
            return p + 1;  // consume closing quote
        }

        // -----------------------------------------------------------------------
        // Generic value parser — strings, numbers, true / false, null. Floats
        // are intentionally promoted to Double because float round-trips lose
        // precision on common decimal literals like 1.1 -> 1.0999... .
        // -----------------------------------------------------------------------

        const char* JsonWrapper::parseValueDirect(const char* p, char* buffer, size_t* outLen,
                                                  Json::Type& type) const {
            *outLen = 0;
            p = str::skipWhitespace(p);

            if (*p == '"') {
                type = Json::Type::String;
                return parseStringDirect(p, buffer, outLen);
            }

            // Number / bool / null — read until the next separator.
            bool dotFound = false;
            bool nonNumberFound = false;
            while (*p != '\0' && *p != ',' && *p != '}' && *p != ']' &&
                   std::isspace(static_cast<unsigned char>(*p)) == 0) {
                if (*p == '.') {
                    dotFound = true;
                } else if (std::isdigit(static_cast<unsigned char>(*p)) == 0 && *p != '-') {
                    nonNumberFound = true;
                }
                buffer[(*outLen)++] = *p++;
            }

            if (*outLen == 0) {
                type = Json::Type::Null;
                return p;
            }

            if (!nonNumberFound) {
                // Use Double for floating-point — see comment above.
                type = dotFound ? Json::Type::Double : Json::Type::Int;
            } else if (std::strncmp(buffer, "true", 4) == 0 ||
                       std::strncmp(buffer, "false", 5) == 0) {
                type = Json::Type::Bool;
            } else {
                type = Json::Type::Null;
            }
            return p;
        }

        // -----------------------------------------------------------------------
        // getObjectAsStr — render the sub-object under `key` back to text.
        //
        // The flat map stores nested objects with dotted keys. To produce the
        // textual sub-object we walk the map looking for any key that starts
        // with `key.` and emit it inside a fresh `{...}` block, stripping the
        // prefix from each entry's name.
        // -----------------------------------------------------------------------

        string_t JsonWrapper::getObjectAsStr(const char* key) const {
            string_t prefix{key};
            prefix.push_back('.');
            const size_t prefixLen = prefix.size();

            string_t out;
            out.reserve(256);
            out.push_back('{');

            bool first = true;
            for (const auto& [full_key, value] : keyValueMap_) {
                if (!str::startsWith(full_key, prefix)) {
                    continue;
                }
                string_t subkey = full_key.substr(prefixLen);

                if (!first) {
                    out.push_back(',');
                }
                first = false;

                out.push_back('"');
                out += subkey;
                out.push_back('"');
                out.push_back(':');

                out += value.serialize(true);  // quote string values
            }
            out.push_back('}');
            return out;
        }

        JsonObject JsonWrapper::getObject(const char* key) const {
            const auto* it = find(key);
            if (it == nullptr) {
                return {};
            }
            if (it->isObject()) {
                return it->asObject().value();
            }

            string_t prefix{key};
            prefix.push_back('.');
            const size_t prefixLen = prefix.size();

            JsonObject out;
            for (const auto& [k, value] : keyValueMap_) {
                if (!str::startsWith(k, prefix)) {
                    continue;
                }
                out.emplace_back(k.substr(prefixLen), value);
            }
            return out;
        }

        // -----------------------------------------------------------------------
        // Inject-into-variable helpers
        // -----------------------------------------------------------------------

        bool JsonWrapper::keyToStrVar(const char* key, string_t& dest) const {
            const auto* it = find(key);
            if (it == nullptr) {
                return false;
            }
            auto new_value = it->asString();
            if (!new_value) {
                return false;
            }
            if (dest == *new_value) {
                return false;
            }
            dest = *new_value;
            return true;
        }

        bool JsonWrapper::keyToIntVar(const char* key, int& dest, int ignoreValue) const {
            const auto* it = find(key);
            if (it == nullptr) {
                return false;
            }
            if (auto vi = it->asInt()) {
                const int nv = *vi;
                if (nv == ignoreValue || nv == dest) {
                    return false;
                }
                dest = nv;
                return true;
            }
            return false;
        }

        bool JsonWrapper::keyToFloatVar(const char* key, float& dest, float ignoreValue) const {
            const auto* it = find(key);
            if (it == nullptr) {
                return false;
            }
            if (auto vf = it->asFloat()) {
                const float nv = *vf;
                if (nv == ignoreValue || nv == dest) {
                    return false;
                }
                dest = nv;
                return true;
            }
            return false;
        }

        bool JsonWrapper::keyToBoolVar(const char* key, bool& dest) const {
            const auto* it = find(key);
            if (it == nullptr) {
                return false;
            }
            if (auto vb = it->asBool()) {
                const bool nv = *vb;
                if (nv == dest) {
                    return false;
                }
                dest = nv;
                return true;
            }
            return false;
        }

        // -----------------------------------------------------------------------
        // Debug printer (uses stdio so it stays usable in unit tests).
        // -----------------------------------------------------------------------

        void JsonWrapper::printAll() const {
            for (const auto& [key, value] : keyValueMap_) {
                std::printf("\"%s\" => ", key.c_str());
                switch (value.type()) {
                    case Json::Type::Null:
                        std::printf("null\n");
                        break;
                    case Json::Type::String:
                        std::printf("\"%s\"\n", value.asString()->c_str());
                        break;
                    case Json::Type::Int:
                        std::printf("%d\n", *value.asInt());
                        break;
                    case Json::Type::Float:
                        std::printf("%f\n", static_cast<double>(*value.asFloat()));
                        break;
                    case Json::Type::Double:
                        std::printf("%lf\n", *value.asDouble());
                        break;
                    case Json::Type::Bool:
                        std::printf("%s\n", *value.asBool() ? "true" : "false");
                        break;
                    case Json::Type::Object:
                        std::printf("{...object...}\n");
                        break;
                }
            }
        }

        // -----------------------------------------------------------------------
        // Typed getters with defaults — best-effort type coercion.
        // -----------------------------------------------------------------------

        int JsonWrapper::getInt(const char* key) const {
            return lookupOr(key, 0, [](const Json& j) -> int {
                switch (j.type()) {
                    case Json::Type::Int:
                        return *j.asInt();
                    case Json::Type::Float:
                        return static_cast<int>(*j.asFloat());
                    case Json::Type::Double:
                        return static_cast<int>(*j.asDouble());
                    case Json::Type::Bool:
                        return *j.asBool() ? 1 : 0;
                    case Json::Type::String:
                        return std::atoi(j.asString()->c_str());
                    default:
                        return 0;
                }
            });
        }

        float JsonWrapper::getFloat(const char* key) const {
            return lookupOr(key, 0.0f, [](const Json& j) -> float {
                switch (j.type()) {
                    case Json::Type::Float:
                        return *j.asFloat();
                    case Json::Type::Double:
                        return static_cast<float>(*j.asDouble());
                    case Json::Type::Int:
                        return static_cast<float>(*j.asInt());
                    case Json::Type::Bool:
                        return *j.asBool() ? 1.0f : 0.0f;
                    case Json::Type::String:
                        return std::strtof(j.asString()->c_str(), nullptr);
                    default:
                        return 0.0f;
                }
            });
        }

        bool JsonWrapper::getBool(const char* key) const {
            return lookupOr(key, false, [](const Json& j) -> bool {
                switch (j.type()) {
                    case Json::Type::Bool:
                        return *j.asBool();
                    case Json::Type::Int:
                        return *j.asInt() != 0;
                    case Json::Type::Float:
                        return *j.asFloat() != 0.0f;
                    case Json::Type::Double:
                        return *j.asDouble() != 0.0;
                    case Json::Type::String:
                        return *j.asString() == "true" || *j.asString() == "1";
                    default:
                        return false;
                }
            });
        }

        string_t JsonWrapper::getStr(const char* key, bool quote_strings) const {
            return lookupOr(key, string_t(), [quote_strings](const Json& j) -> string_t {
                return j.serialize(quote_strings);
            });
        }

        // -----------------------------------------------------------------------
        // Free-function single-key extractors. Each one builds a transient
        // JsonWrapper, performs the lookup, and returns. They are convenient
        // when you need to read a single value out of a one-shot payload.
        // -----------------------------------------------------------------------

        bool jsonKeyToBoolVar(const char* json_string, const char* key, bool& dest) {
            JsonWrapper parser(json_string);
            return parser.keyToBoolVar(key, dest);
        }
        bool jsonKeyToBoolVar(const JsonStr& json_string, const char* key, bool& dest) {
            return jsonKeyToBoolVar(json_string.c_str(), key, dest);
        }
        bool jsonKeyToBoolVar(JsonStrView json_string, const char* key, bool& dest) {
            return jsonKeyToBoolVar(json_string.data(), key, dest);
        }

        bool jsonKeyToIntVar(const char* json_string, const char* key, int& dest, int ignoreValue) {
            JsonWrapper parser(json_string);
            return parser.keyToIntVar(key, dest, ignoreValue);
        }
        bool jsonKeyToIntVar(const JsonStr& json_string, const char* key, int& dest,
                             int ignoreValue) {
            return jsonKeyToIntVar(json_string.c_str(), key, dest, ignoreValue);
        }
        bool jsonKeyToIntVar(JsonStrView json_string, const char* key, int& dest, int ignoreValue) {
            return jsonKeyToIntVar(json_string.data(), key, dest, ignoreValue);
        }

        bool jsonKeyToFloatVar(const char* json_string, const char* key, float& dest,
                               float ignoreValue) {
            JsonWrapper parser(json_string);
            return parser.keyToFloatVar(key, dest, ignoreValue);
        }
        bool jsonKeyToFloatVar(const JsonStr& json_string, const char* key, float& dest,
                               float ignoreValue) {
            return jsonKeyToFloatVar(json_string.c_str(), key, dest, ignoreValue);
        }
        bool jsonKeyToFloatVar(JsonStrView json_string, const char* key, float& dest,
                               float ignoreValue) {
            return jsonKeyToFloatVar(json_string.data(), key, dest, ignoreValue);
        }

        bool jsonKeyToStrVar(const char* json_string, const char* key, string_t& dest) {
            JsonWrapper parser(json_string);
            return parser.keyToStrVar(key, dest);
        }
        bool jsonKeyToStrVar(const JsonStr& json_string, const char* key, string_t& dest) {
            return jsonKeyToStrVar(json_string.c_str(), key, dest);
        }
        bool jsonKeyToStrVar(JsonStrView json_string, const char* key, string_t& dest) {
            return jsonKeyToStrVar(json_string.data(), key, dest);
        }

        bool isValidJson(const JsonStr& json_string) {
            return JsonWrapper(json_string).isValidJson();
        }

        bool isValidJson(JsonStrView json_string) {
            return JsonWrapper(json_string.data()).isValidJson();
        }

    }  // namespace json
}  // namespace ungula
