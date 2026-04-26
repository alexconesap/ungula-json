# UngulaJson

Embedded C++17 JSON for ESP32-class targets. Three surfaces: a full
`JsonWrapper` parser that flattens a document into a dotted-path map, a
`JsonObject` builder with `serializeJson` for outgoing payloads, and free
single-key extractors that scan a raw buffer linearly without building a
parse tree. No Arduino headers, no exceptions, no logging. Depends only
on `UngulaCore` for `string_t` / `string_view_t` and a few string helpers.

---

## Usage

### Use case: extract one key from a large incoming buffer

```cpp
#include <json/json_utils.h>
#include <util/string_types.h>
#include <cstring>

using ungula::string_t;
using namespace ungula::json;

void on_message(const char* buf, size_t n) {
    // Top-level quoted string, fastest path. No parse tree built.
    string_t action = jsonExtractStringKey(buf, n, "action");
    if (action == "capture") {
        // ...
    }
}
```

When to use this: incoming envelope is multi-KB but you only need one or
two top-level fields. Constant memory; one allocation for the result.

### Use case: extract any-typed leaf key from a `string_t` payload

```cpp
#include <json/json_utils.h>
#include <util/string_types.h>

using ungula::string_t;
using namespace ungula::json;

void handle(const string_t& json) {
    // Accepts quoted or unquoted values. First occurrence wins.
    // Does NOT support dotted paths — leaf key only.
    string_t roi = jsonExtractAsStr(json, "roi");
    int      q   = jsonExtractAsInt(json, "q");
}
```

When to use this: producer may emit unquoted tokens, or you want a quick
linear scan and don't care that nested duplicates collapse to the first.

### Use case: full parse with dotted-path lookup

```cpp
#include <json/json.h>
#include <util/string_types.h>

using ungula::string_t;
using namespace ungula::json;

void parse_settings(const char* payload) {
    JsonWrapper doc(payload);
    if (!doc.isValidJson()) {
        return;
    }
    int      q   = doc.getInt  ("payload.settings.quality");
    string_t roi = doc.getStr  ("payload.settings.roi");
    bool     dbg = doc.getBool ("payload.settings.debug");
}
```

When to use this: you need several leaves out of the same document, or
nested access via dotted paths.

### Use case: change-detection against caller-owned variables

```cpp
#include <json/json.h>
#include <util/string_types.h>

using ungula::string_t;
using namespace ungula::json;

struct Settings {
    bool   debug  = false;
    int    intv   = 11;
    float  floatv = 100.0f;
};

bool apply(Settings& s, const char* payload) {
    JsonWrapper doc(payload);
    bool changed = false;
    changed |= doc.keyToBoolVar ("payload.debug",  s.debug);
    changed |= doc.keyToIntVar  ("payload.intv",   s.intv);
    changed |= doc.keyToFloatVar("payload.floatv", s.floatv);
    return changed;
}
```

When to use this: "settings dirty?" patterns. Assignment happens only when
the parsed value differs from the current one. `keyToIntVar` /
`keyToFloatVar` accept an `ignoreValue` sentinel; values matching it are
treated as "leave it" and skipped.

### Use case: slice a sub-document without re-parsing

```cpp
#include <json/json.h>

using namespace ungula::json;

void slice(const JsonWrapper& main_doc) {
    // Keep original key paths.
    JsonWrapper a(main_doc, "payload.settings");
    // a.has("payload.settings.roi") == true

    // Strip the prefix.
    JsonWrapper b(main_doc, "payload.settings", "");
    // b.has("roi") == true

    // Rename the prefix.
    JsonWrapper c(main_doc, "payload.settings", "root");
    // c.has("root.roi") == true
}
```

### Use case: build and serialize an outgoing document

```cpp
#include <json/json_types.h>
#include <json/json_utils.h>
#include <util/string_types.h>

using ungula::string_t;
using namespace ungula::json;

string_t make_reply(const string_t& action) {
    JsonObject settings;
    putTojson(settings, "roi",     "xx");
    putTojson(settings, "quality", 100);
    putTojson(settings, "type",    "jpeg");

    JsonObject reply;
    putTojson(reply, "ack",      true);
    putTojson(reply, "echoed",   action);
    putTojson(reply, "settings", settings);

    return serializeJson(reply);
    // {"ack":true,"echoed":"...","settings":{"roi":"xx","quality":100,"type":"jpeg"}}
}
```

When to use this: outgoing replies, MQTT publishes, HTTP request bodies
where field order must be deterministic. `JsonObject` preserves insertion
order on purpose (it is a `std::vector<std::pair<...>>`, not a `std::map`).

### Use case: in-place edits on a `Json` value tree

```cpp
#include <json/json_types.h>

using namespace ungula::json;

void edit() {
    JsonObject obj = {
        {"payload", JsonObject{
            {"settings", JsonObject{
                {"roi",     "xx"},
                {"quality", 100},
            }},
        }},
    };
    Json doc(obj);

    doc.has("payload.settings.roi");           // true
    doc.update("payload.settings.roi", "yy");  // mutate (must already exist)
    doc.add   ("payload.tag",   "draft");      // create new key (must NOT already exist)
    doc.remove("payload.settings.quality");    // erase existing
}
```

### Use case: depth-limited parse

```cpp
#include <json/json.h>
using namespace ungula::json;

JsonWrapper shallow(R"({"a":{"b":{"c":{"d":{"e":1}}}}})", /*levels=*/3);
// shallow.has("a.b.c")   == true
// shallow.has("a.b.c.d") == false  // past the cap
```

Default cap is `JsonWrapper::MAX_PARSE_DEPTH` (4).

### Use case: cheap syntactic validity check

```cpp
#include <json/json.h>
using namespace ungula::json;

isValidJson(R"({"a":1})");                   // true
isValidJson(R"({"a":1)");                    // false (truncated)
isValidJson(R"({"action": take_one_shot})"); // true (lenient: unquoted token)
```

The lenient parser accepts unquoted tokens after `:` and tolerant
whitespace. Layer strict validation on top if you need it.

---

## Public types

### `ungula::json::Json` — tagged-union value

Defined in `<json/json_types.h>`.

`Json` holds one of: `null` (`std::monostate`), `string_t`, `int`,
`float`, `double`, `bool`, or `JsonObject`. The variant alternatives are
mirrored by `Json::Type`:

```cpp
enum class Json::Type : uint8_t {
    Null = 0, String, Int, Float, Double, Bool, Object,
};
```

Implicit constructors: `nullptr_t`, `const char*` (null pointer becomes
`""`), `const string_t&`, `int`, `float`, `double`, `unsigned long`
(stored as `double`), `bool`, `const JsonObject&`.

Predicates: `isObject`, `isString`, `isInt`, `isFloat`, `isDouble`,
`isBool`, `isNull`, `isEmpty` (null OR empty string).

Typed accessors return `std::optional<T>`, `std::nullopt` on type
mismatch: `asString`, `asInt`, `asFloat`, `asDouble`, `asBool`,
`asObject`, `asNull`.

Dotted-path tree ops (require `isObject()`):

- `Json* find(const string_t& key)` / const overload — `nullptr` if
  missing or non-object.
- `bool has(const string_t& key) const`.
- `bool update(const string_t& key, const Json& newVal)` — replaces only
  if the key already exists; `false` otherwise.
- `bool add(const string_t& key, const Json& newVal)` — fails if the
  key already exists or any prefix is not an object; `false` otherwise.
- `bool remove(const string_t& key)` — `false` if not found.

Serialization:

- `string_t serialize(bool quote_strings = false) const` — recursive.
  When `quote_strings` is `false`, bare string values are emitted
  unquoted (handy for table-printing); when `true`, the output is valid
  JSON.

### `ungula::json::JsonObject` — order-preserving map

```cpp
using JsonObject = std::vector<std::pair<string_t, Json>>;
```

Brace-initializable. Insertion order is preserved across mutations and
serialization. Use `findInObject` / `updateInObject` / `putTojson` to
manipulate.

### `ungula::json::JsonWrapper` — full parser

Defined in `<json/json.h>`. Walks a document once, stores every leaf in
a flat `vector<pair<key, Json>>` keyed by dotted path. Read-only after
construction.

```cpp
class JsonWrapper {
public:
    using json_map_t = std::vector<std::pair<string_t, Json>>;
    static constexpr int MAX_PARSE_DEPTH = 4;

    explicit JsonWrapper(const char*       jsonStr, uint8_t levels = MAX_PARSE_DEPTH);
    explicit JsonWrapper(const string_t&   jsonStr, uint8_t levels = MAX_PARSE_DEPTH);
    explicit JsonWrapper(string_view_t     jsonStr, uint8_t levels = MAX_PARSE_DEPTH);
    explicit JsonWrapper(const JsonWrapper& src, const char* key,
                         const char* new_root = nullptr);
    explicit JsonWrapper(const json_map_t& json_map);

    bool     isValidJson() const;
    bool     isEmpty()     const;
    bool     has         (const char* key) const;

    int      getInt      (const char* key) const;
    float    getFloat    (const char* key) const;
    bool     getBool     (const char* key) const;
    string_t getStr      (const char* key, bool quote_strings = false) const;

    string_t getObjectAsStr(const char* key) const;  // re-emits {"x":..,"y":..}
    JsonObject getObject  (const char* key) const;   // materialize as JsonObject

    bool keyToStrVar  (const char* key, string_t& dest) const;
    bool keyToBoolVar (const char* key, bool&     dest) const;
    bool keyToIntVar  (const char* key, int&      dest,
                       int   ignoreValue = std::numeric_limits<int>::min())   const;
    bool keyToFloatVar(const char* key, float&    dest,
                       float ignoreValue = std::numeric_limits<float>::min()) const;

    void printAll() const;  // debug-only, writes to stdout
};
```

`new_root` semantics for the slicing constructor (input
`{"a":1,"payload":{"settings":{"world":1}}}`, `key = "payload.settings"`):

| `new_root`  | resulting key paths           |
|-------------|-------------------------------|
| `nullptr`   | `payload.settings.world`      |
| `""`        | `world`                       |
| `"what"`    | `what.world`                  |

`getInt` / `getFloat` / `getBool` perform best-effort coercion across
numeric / bool slots and return `0` / `0.0f` / `false` on missing keys.
`getStr` returns `""` on missing.

---

## Public free functions

All in `ungula::json::`.

### Serialization

```cpp
JsonStr serializeJson(const JsonObject& json);
void    serializeJson(const JsonObject& json, JsonStr& out);  // out is cleared first
```

Always emits valid JSON (strings quoted, escapes applied via
`ungula::str::escapeString`). Numbers go through
`ungula::str::num_to_string`.

### Single-key extractors — raw buffer (no parse tree)

```cpp
string_t jsonExtractStringKey(const char* buf, size_t buf_len, const char* key);
string_t jsonExtractAsStr    (const string_t& json, const char* key,
                              int expected_len_result = 128);
int      jsonExtractAsInt    (const string_t& json, const char* key,
                              int len_number = 8);
```

- `jsonExtractStringKey`: top-level quoted string only. Returns `""` if
  the key is missing, the value is not a quoted string, or the document
  is truncated. `buf` does not need to be NUL-terminated.
- `jsonExtractAsStr`: leaf key only (no dotted path). Accepts quoted or
  unquoted values. First occurrence wins.
- `jsonExtractAsInt`: leaf key only. Returns `0` if missing or
  non-numeric. `len_number` is a capacity hint.

### Single-key extractors — full parse via `JsonWrapper`

These accept dotted paths and parse the entire document. Each is
overloaded for `const char*`, `const JsonStr&`, and `JsonStrView`. Each
returns `true` only when `dest` actually changed.

```cpp
bool jsonKeyToBoolVar (/* json */, const char* key, bool&     dest);
bool jsonKeyToIntVar  (/* json */, const char* key, int&      dest,
                       int   ignoreValue = std::numeric_limits<int>::min());
bool jsonKeyToFloatVar(/* json */, const char* key, float&    dest,
                       float ignoreValue = std::numeric_limits<float>::min());
bool jsonKeyToStrVar  (/* json */, const char* key, string_t& dest);
```

### Validity

```cpp
bool isValidJson(const JsonStr&  json_string);
bool isValidJson(JsonStrView     json_string);
```

Lenient: tolerates unquoted tokens after `:` and free-form whitespace.

### `JsonObject` helpers

```cpp
Json*       findInObject  (JsonObject&       obj, const string_t& key);  // nullptr if missing
const Json* findInObject  (const JsonObject& obj, const string_t& key);
bool        updateInObject(JsonObject&       obj, const string_t& key, const Json& v);
void        putTojson     (JsonObject&       doc, const string_t& key, const Json&       v);
void        putTojson     (JsonObject&       doc, const string_t& key, const JsonObject& v);
```

`putTojson` is insert-or-update: existing keys are overwritten in place;
new keys are appended (preserves order).

---

## Headers

| Header                  | Symbols                                                          |
|-------------------------|------------------------------------------------------------------|
| `<json/json_types.h>`   | `Json`, `JsonObject`, `JsonStr`, `JsonStrView`                   |
| `<json/json.h>`         | `JsonWrapper`, `jsonKeyToXxxVar`, `isValidJson`                  |
| `<json/json_utils.h>`   | `serializeJson`, `jsonExtract*`, `findInObject`, `updateInObject`, `putTojson` |
| `<ungula_json.h>`       | Umbrella include — pulls all of the above                        |

Code inside `namespace ungula::json` reaches `string_t`,
`str::escapeString`, etc. via parent-namespace lookup. Code outside the
namespace must qualify (`ungula::string_t`, `ungula::str::...`).

---

## Lifecycle

`JsonWrapper`: construct with the source document → query via `has`,
`getInt`, `getStr`, `getObject*`, `keyToXxxVar` → discard. The wrapper
copies what it needs from the source; the source buffer can be released
after construction.

`Json` / `JsonObject`: build via brace init or `putTojson` → mutate via
`update` / `add` / `remove` / `putTojson` → emit via `serialize` or
`serializeJson` → discard.

Single-key extractors are stateless: each call allocates exactly its
result (or zero, for the integer variant).

---

## Error handling

No exceptions. No error codes outside the documented `bool` returns.

| Condition                                  | Behavior                                       |
|--------------------------------------------|------------------------------------------------|
| `JsonWrapper` parse failure                | `isValidJson() == false`, map is empty         |
| Past `levels` cap during parse             | Deeper keys silently skipped, rest still parses|
| `getInt` / `getFloat` / `getBool` miss     | Returns `0` / `0.0f` / `false`                 |
| `getStr` miss                              | Returns `""`                                   |
| `Json::asXxx` on wrong type                | `std::nullopt`                                 |
| `Json::update` / `add` on wrong shape      | Returns `false`, no mutation                   |
| `jsonExtractStringKey` not found / wrong shape | Returns `""`                               |
| `jsonExtractAsInt` not found / non-numeric | Returns `0`                                    |
| `keyToXxxVar` value equals current `dest` or `ignoreValue` | Returns `false`, no assignment |

Distinguish "missing key" from "value happens to be `0`/`""`/`false`"
via `JsonWrapper::has` or `Json::find` before the typed getter.

---

## Threading / timing / hardware notes

Pure C++17. No globals, no statics with mutable state, no platform
APIs, no allocations after construction (other than what `string_t` and
`std::vector` do internally). All operations are synchronous and run on
the calling thread. Heap usage scales linearly with the number of leaves
(`JsonWrapper`) or the document size (builders). The single-key raw-
buffer extractors allocate at most one `string_t` for the result; they
do not materialize a parse tree.

`JsonWrapper::printAll` writes to stdout — host/test only; do not call
from firmware.

---

## Internals not part of the public API

- `JsonWrapper::parseJson`, `parseObject`, `parseStringDirect`,
  `parseValueDirect` — private parser scaffolding.
- `JsonWrapper::find(const char*)` (private) — the public lookup is
  through `has`, `getXxx`, `keyToXxxVar`.
- `Json::findImpl` — backing both `find` overloads.
- `JsonStr` / `JsonStrView` — internal aliases for `string_t` /
  `string_view_t`. Use `ungula::string_t` directly in caller code.
- `Json::Type::Null` ordinal layout — assume the enum exists, do not
  rely on numeric values.

---

## LLM usage rules

- Use only the documented public API.
- Prefer `jsonExtractStringKey` / `jsonExtractAsStr` / `jsonExtractAsInt`
  over building a `JsonWrapper` when you only need one or two leaves
  from a large buffer.
- Use `JsonWrapper` when you need several leaves, dotted paths, or
  sub-document slicing.
- Build outgoing documents with `JsonObject` + `putTojson` +
  `serializeJson`, never by hand-concatenating strings.
- Do not call `printAll` from firmware.
- Do not depend on the parse-tree or any private parser entry point.
- Do not assume strict JSON validation — `isValidJson` is lenient.
- Preserve the terminology used here: `JsonWrapper`, `JsonObject`,
  `Json`, `putTojson`, `serializeJson`, `jsonExtract*`, `keyToXxxVar`.
