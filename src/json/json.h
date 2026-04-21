// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

// JsonWrapper — full parsing front-end for the embedded JSON library.
//
// JsonWrapper takes a JSON document, walks it once with a hand-rolled
// recursive parser, and stores every leaf value in a flat
// `vector<pair<key, Json>>` map keyed by the dotted path
// (`payload.settings.roi`). After construction the wrapper is read-only and
// every lookup is a linear scan over the flat list.
//
// Why a flat dotted-path map instead of a real tree?
//
//   * Constant per-key overhead (no pointer chasing).
//   * Iteration order matches the order in the source document, which is
//     useful when round-tripping data.
//   * Lookups by full path (`payload.settings.roi`) are a single string
//     compare per slot, no traversal.
//
// The wrapper also exposes a per-document parse depth limit so embedded
// callers can refuse to parse pathologically nested input.
//
// For one-off single-key extraction without building the map at all, use
// the free functions in `json_utils.h` (`jsonExtractStringKey`, ...).

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "json_types.h"

namespace ungula {
    namespace json {

        // ---- Free-function single-key extractors --------------------------------
        //
        // Each one parses the input through `JsonWrapper`, looks the key up,
        // and writes into `dest` only if the value exists, has the right type,
        // and differs from the current `dest` value (so callers can detect
        // "changed" by checking the return value).

        /// @brief Extract a boolean key into `dest`.
        bool jsonKeyToBoolVar(const char* json_string, const char* key, bool& dest);
        bool jsonKeyToBoolVar(const JsonStr& json_string, const char* key, bool& dest);
        bool jsonKeyToBoolVar(JsonStrView json_string, const char* key, bool& dest);

        /// @brief Extract an integer key into `dest`. Skips the assignment when
        ///        the parsed value equals `ignoreValue` (sentinel for "leave it").
        bool jsonKeyToIntVar(const char* json_string, const char* key, int& dest,
                             int ignoreValue = std::numeric_limits<int>::min());
        bool jsonKeyToIntVar(const JsonStr& json_string, const char* key, int& dest,
                             int ignoreValue = std::numeric_limits<int>::min());
        bool jsonKeyToIntVar(JsonStrView json_string, const char* key, int& dest,
                             int ignoreValue = std::numeric_limits<int>::min());

        /// @brief Extract a float key. Same `ignoreValue` semantics.
        bool jsonKeyToFloatVar(const char* json_string, const char* key, float& dest,
                               float ignoreValue = std::numeric_limits<float>::min());
        bool jsonKeyToFloatVar(const JsonStr& json_string, const char* key, float& dest,
                               float ignoreValue = std::numeric_limits<float>::min());
        bool jsonKeyToFloatVar(JsonStrView json_string, const char* key, float& dest,
                               float ignoreValue = std::numeric_limits<float>::min());

        /// @brief Extract a string key into `dest`.
        bool jsonKeyToStrVar(const char* json_string, const char* key, string_t& dest);
        bool jsonKeyToStrVar(const JsonStr& json_string, const char* key, string_t& dest);
        bool jsonKeyToStrVar(JsonStrView json_string, const char* key, string_t& dest);

        /// @brief Quick syntactic validity check (does the document parse cleanly?).
        bool isValidJson(const JsonStr& json_string);
        bool isValidJson(JsonStrView json_string);

        // ---- JsonWrapper --------------------------------------------------------

        class JsonWrapper {
            public:
                using json_map_t = std::vector<std::pair<string_t, Json>>;

                static constexpr int MAX_PARSE_DEPTH = 4;

                // ---- Construction -------------------------------------------------
                //
                // The `levels` argument caps how deep the parser will descend into
                // nested objects. Anything deeper is silently skipped — the rest of
                // the document still parses, but the deeply-nested keys won't appear
                // in the map. Default depth is 4, which covers the vast majority of
                // realistic embedded payloads.

                explicit JsonWrapper(const char* jsonStr, uint8_t levels = MAX_PARSE_DEPTH) {
                    parseJson(jsonStr, levels);
                }
                explicit JsonWrapper(const string_t& jsonStr, uint8_t levels = MAX_PARSE_DEPTH) {
                    parseJson(jsonStr.c_str(), levels);
                }
                explicit JsonWrapper(string_view_t jsonStr, uint8_t levels = MAX_PARSE_DEPTH) {
                    parseJson(jsonStr.data(), levels);
                }

                /// @brief Build a sub-wrapper from a parent that contains only the
                ///        keys living under `key`. Useful for slicing a parsed
                ///        document into smaller scoped views without re-parsing.
                ///
                /// `new_root` controls how the sliced keys are renamed:
                ///   * `nullptr` — keys keep their original full paths.
                ///   * `""`      — the prefix is stripped entirely.
                ///   * `"abc"`   — the prefix is replaced with `"abc"`.
                ///
                /// Example for input `{"a":1,"payload":{"settings":{"world":1}}}`
                /// with `key = "payload.settings"`:
                ///   * `new_root = nullptr` → `{ "payload.settings.world": 1 }`
                ///   * `new_root = ""`      → `{ "world": 1 }`
                ///   * `new_root = "what"`  → `{ "what.world": 1 }`
                explicit JsonWrapper(const JsonWrapper& json, const char* key,
                                     const char* new_root = nullptr);

                /// @brief Build a wrapper directly from an existing flat map. Useful
                ///        for tests and for hand-built fixtures.
                explicit JsonWrapper(const json_map_t& json_map);

                // ---- Predicates ---------------------------------------------------

                bool isValidJson() const {
                    return validJson_;
                }
                bool isEmpty() const {
                    return isEmpty_;
                }

                /// @brief True when the wrapper contains a key matching `key`.
                bool has(const char* key) const {
                    return find(key) != nullptr;
                }

                // ---- Typed getters with default ------------------------------------
                // These return a sensible default (0, 0.0f, false, "") if the key
                // is missing or cannot be converted to the requested type.

                int getInt(const char* key) const;
                float getFloat(const char* key) const;
                bool getBool(const char* key) const;
                string_t getStr(const char* key, bool quote_strings = false) const;

                /// @brief Render the JSON object stored under `key` back to a JSON
                ///        text fragment. Walks one level deep — only the immediate
                ///        children are included.
                ///
                /// For input `{"a":1,"payload":{"settings":{"x":1,"y":42}}}` and
                /// `key = "payload.settings"` this returns `{"x":1,"y":42}`.
                string_t getObjectAsStr(const char* key) const;

                /// @brief Materialize the sub-object under `key` as a fresh
                ///        `JsonObject` (the same container the constructors accept).
                JsonObject getObject(const char* key) const;

                // ---- Inject-into-variable helpers ----------------------------------
                //
                // These read a key out of the JSON and assign it to a caller-owned
                // variable IF AND ONLY IF the value would actually change. The
                // return value tells the caller whether anything changed — handy
                // for "settings dirty?" patterns.

                bool keyToStrVar(const char* key, string_t& dest) const;
                bool keyToBoolVar(const char* key, bool& dest) const;
                bool keyToIntVar(const char* key, int& dest,
                                 int ignoreValue = std::numeric_limits<int>::min()) const;
                bool keyToFloatVar(const char* key, float& dest,
                                   float ignoreValue = std::numeric_limits<float>::min()) const;

                /// @brief Print every (key, value) pair to stdout. Debug helper —
                ///        does NOT use the logging library on purpose so this code
                ///        stays usable in unit tests and on the host.
                void printAll() const;

            private:
                bool validJson_ = false;
                bool isEmpty_ = true;

                json_map_t keyValueMap_;
                int max_parseLevels_ = 0;

                uint8_t maxParseLevels() const {
                    return static_cast<uint8_t>(max_parseLevels_);
                }

                const Json* find(const char* key) const {
                    auto it = std::find_if(keyValueMap_.begin(), keyValueMap_.end(),
                                           [&](auto const& kv) { return kv.first == key; });
                    if (it == keyValueMap_.end()) {
                        return nullptr;
                    }
                    return &it->second;
                }

                template <class T, class F>
                inline T lookupOr(const char* key, T fallback, F conv) const {
                    const Json* it = find(key);
                    if (it != nullptr) {
                        return conv(*it);
                    }
                    return fallback;
                }

                void parseJson(const char* jsonStr, uint8_t levels);

                // Recursive object parser. Returns the pointer just after the
                // closing brace `}`, or nullptr on parse failure.
                const char* parseObject(const char* p, char* keyBuffer, size_t bufferOffset,
                                        int depth);

                // String value parser — handles escape sequences (\n \t \r \\ \").
                const char* parseStringDirect(const char* p, char* buffer, size_t* outLen) const;

                // Generic non-string value parser — numbers, bool, null.
                const char* parseValueDirect(const char* p, char* buffer, size_t* outLen,
                                             Json::Type& type) const;
        };

    }  // namespace json
}  // namespace ungula
