# UngulaJson — embedded-first JSON

> **High-performance embedded C++ libraries for ESP32, STM32 and other MCUs** — compact JSON parser, builder and zero-alloc single-key extractor.

A small JSON library for ESP32-class targets. It does three things and tries
to do them well:

- **Parse** a JSON document into a flat dotted-path map you can query in
  one shot (`JsonWrapper`).
- **Build** an order-preserving JSON document from C++ types and serialize
  it to text (`JsonObject`, `Json`, `serializeJson`).
- **Extract one key** out of a raw JSON buffer **without parsing the
  whole thing** (`jsonExtractStringKey`, `jsonExtractAsStr`,
  `jsonExtractAsInt`).

The third one is the reason this library exists. On a constrained device
you often receive a multi-KB JSON envelope where the only field you
actually care about is `"action"`, `"id"` or whatever. Parsing the entire document
just to read one string is wasteful. The single-key extractors do a single
linear scan of the source buffer and return as soon as they find the value
— constant memory, no `std::vector`, no parse tree.

No Arduino headers. No logging. No exceptions. Just C++17 and the standard
library. The only external dependency is `UngulaCore` for the string
type aliases and a handful of string utilities.

## Quick taste

```cpp
#include <json/json.h>
#include <json/json_utils.h>

using namespace ungula::json;
using ungula::string_t;          // one-liner alias for the project-wide `std::string`

const char* payload =
    R"({"action":"capture","payload":{"settings":{"roi":"xx","quality":100}}})";

// 1) Single-key fast path: pull "action" out without parsing the rest.
string_t action = jsonExtractStringKey(payload, std::strlen(payload), "action");
//   action == "capture"

// 2) Full parse when you really need the tree.
JsonWrapper doc(payload);
if (doc.isValidJson()) {
    int q = doc.getInt("payload.settings.quality");      // 100
    string_t roi = doc.getStr("payload.settings.roi");   // "xx"
}

// 3) Build a document from C++ types and serialize it.
JsonObject reply;
putTojson(reply, "ack",     true);
putTojson(reply, "echoed",  action);
putTojson(reply, "quality", 100);

string_t out = serializeJson(reply);
// out == {"ack":true,"echoed":"capture","quality":100}
```

## Headers

| Header                  | What it gives you                                          |
|-------------------------|------------------------------------------------------------|
| `<json/json_types.h>`   | `Json` value type, `JsonObject` container, type predicates |
| `<json/json.h>`         | `JsonWrapper` full parser + free single-key helpers        |
| `<json/json_utils.h>`   | `serializeJson`, raw-buffer extractors, builder helpers    |
| `<ungula_json.h>`   | Umbrella include — pulls all of the above                  |

Everything lives in `ungula::json::`.

## API tour

### `Json` — tagged-union value

`Json` wraps any of: `null`, `string`, `int`, `float`, `double`, `bool` or
a nested `JsonObject`. Implicit constructors mean you can write the natural
literal:

```cpp
Json a = nullptr;
Json b = "hello";
Json c = 42;
Json d = 3.14f;
Json e = true;
Json f = JsonObject{{"x", 1}, {"y", 2}};
```

Type checks and accessors:

```cpp
Json v = 42;

v.isInt();           // true
v.type();            // Json::Type::Int
v.asInt().value();   // 42
v.asString();        // std::nullopt — wrong type
```

Walk into nested values with dotted paths:

```cpp
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
doc.find("payload.settings.quality");      // pointer to Json(100)
doc.update("payload.settings.roi", "yy");  // mutate in place
doc.add("payload.tag",   "draft");         // add a new key
doc.remove("payload.settings.quality");    // remove an existing key
```

### `JsonObject` — order-preserving map

`JsonObject` is a `std::vector<std::pair<string_t, Json>>` rather than a
`std::map`, on purpose. It preserves insertion order, which matters when
the consumer of the resulting JSON expects fields in a particular order
(many HTTP APIs and MQTT topics do).

You build it directly with brace init or with the `putTojson` helper:

```cpp
// Brace init
JsonObject person = {
    {"name",    "Alice"},
    {"age",     30},
    {"premium", true},
};

// putTojson — overwrites existing keys, preserves order for new ones
JsonObject settings;
putTojson(settings, "roi",     "xx");
putTojson(settings, "quality", 100);
putTojson(settings, "focus",   "6.5");
putTojson(settings, "type",    "jpeg");
```

Combine and serialize:

```cpp
JsonObject message;
putTojson(message, "action",   "shot");
putTojson(message, "settings", settings);

string_t out = serializeJson(message);
// {"action":"shot","settings":{"roi":"xx","quality":100,"focus":"6.5","type":"jpeg"}}
```

### `JsonWrapper` — full parser

`JsonWrapper` walks a JSON document once and stores every leaf in a flat
map keyed by the **dotted path**. After construction the wrapper is
read-only and every lookup is one string compare per slot.

```cpp
JsonWrapper doc(R"({"a":1,"payload":{"settings":{"roi":"xx","q":100}}})");

doc.isValidJson();                  // true
doc.has("payload.settings.roi");    // true
doc.getStr("payload.settings.roi"); // "xx"
doc.getInt("payload.settings.q");   // 100
doc.getFloat("payload.settings.q"); // 100.0f (best-effort coercion)
doc.getBool("payload.settings.q");  // true (non-zero)
```

#### Parse depth limit

Pass a depth limit to refuse pathologically nested input:

```cpp
JsonWrapper shallow(R"({"a":{"b":{"c":{"d":{"e":1}}}}})", /*levels=*/3);
shallow.isValidJson();    // true — the rest of the document still parses
shallow.has("a.b.c");     // true
shallow.has("a.b.c.d");   // false — past the depth cap
```

The default cap is `JsonWrapper::MAX_PARSE_DEPTH` (4).

#### Slicing into a sub-document

Build a sub-wrapper containing only the keys under a given prefix. This is
useful when you receive a big envelope and want to hand a small piece to
another component:

```cpp
JsonWrapper main_doc(R"({"action":"x","payload":{"settings":{"roi":"xx","q":100}}})");

// Keep original key paths
JsonWrapper a(main_doc, "payload.settings");
a.has("payload.settings.roi");     // true

// Strip the prefix
JsonWrapper b(main_doc, "payload.settings", "");
b.has("roi");                      // true

// Rename the prefix
JsonWrapper c(main_doc, "payload.settings", "root");
c.has("root.roi");                 // true
```

#### Inject-into-variable helpers

`keyToXxxVar` reads a key into a caller-owned variable **only if the value
would actually change**. The return value tells the caller whether
something changed — perfect for "settings dirty?" patterns:

```cpp
JsonWrapper doc(R"({"payload":{"intv":42,"floatv":3.14,"debug":true}})");

bool   debug    = false;
int    intv     = 11;
float  floatv   = 100.0f;

bool changed = false;
changed |= doc.keyToBoolVar ("payload.debug",  debug);
changed |= doc.keyToIntVar  ("payload.intv",   intv);
changed |= doc.keyToFloatVar("payload.floatv", floatv);

// changed == true
// debug == true, intv == 42, floatv == 3.14f

// Calling again without new data: nothing changed.
changed = false;
changed |= doc.keyToIntVar("payload.intv", intv);
// changed == false
```

`keyToIntVar` and `keyToFloatVar` accept an `ignoreValue` sentinel — pass
the value the caller treats as "leave it" and the function will skip the
assignment when the parsed value matches it.

#### Reading sub-objects

```cpp
JsonWrapper doc(R"({"payload":{"settings":{"roi":"xx","q":100.2}}})");

// As JSON text fragment
string_t text = doc.getObjectAsStr("payload.settings");
// text == {"roi":"xx","q":100.200000}

// As a JsonObject (re-buildable, mutatable)
JsonObject obj = doc.getObject("payload.settings");
```

### Single-key extractors (the embedded fast path)

Use these when you have a large incoming buffer and you only need one or
two values. They scan the raw text linearly, do not build a parse tree, and
allocate exactly one string for the result.

```cpp
const char* buf = R"({"action":"capture","payload":{ ... lots of data ... }})";
size_t      n   = std::strlen(buf);

// Top-level quoted string — fastest path.
string_t action = jsonExtractStringKey(buf, n, "action");
// action == "capture"
```

If the value can be either quoted or unquoted, or lives anywhere in the
document (first occurrence wins, dotted paths NOT supported here):

```cpp
string_t json = R"({"action":"x","payload":{"settings":{"q":100}}})";

string_t roi = jsonExtractAsStr(json, "roi");      // ""
string_t qs  = jsonExtractAsStr(json, "q");        // "100"
int      qi  = jsonExtractAsInt(json, "q");        // 100
```

For one-shot extraction into a typed variable, the `jsonKeyToXxxVar` free
functions wrap a transient `JsonWrapper`:

```cpp
string_t roi;
int      quality = 0;

jsonKeyToStrVar(json, "payload.settings.roi", roi);     // dotted paths OK
jsonKeyToIntVar(json, "payload.settings.q",   quality);
```

Note these full-tree variants accept dotted paths and parse the entire
document — use them when you need that. The raw-buffer extractors above
do not.

### Validity check

```cpp
isValidJson(R"({"a":1})");                   // true
isValidJson(R"({"a":1)");                    // false (truncated)
isValidJson(R"({"action": take_one_shot})"); // true (lenient — accepts unquoted token)
```

The lenient parser accepts a few things a strict JSON parser would reject
(unquoted tokens after a `:`, weird whitespace) so it can survive
real-world payloads from hand-rolled producers. If you need strict
validation, layer it on top.

## Dependencies

- `UngulaCore` (`lib`) — provides `<util/string_types.h>` (`ungula::string_t`,
  `ungula::string_view_t`) and `<util/string_utils.h>` (`ungula::str::skipWhitespace`,
  `ungula::str::startsWith`, `ungula::str::escapeString`, `ungula::str::num_to_string`,
  `ungula::str::trimWhitespace`, `ungula::str::countChar`, `ungula::str::replaceAll`).
  Code that lives inside `namespace ungula::json { ... }` finds these unqualified
  via parent-namespace lookup, which is why the JSON sources just write
  `string_t` and `str::escapeString(...)`.

That's it. No Arduino headers. No logger. No FreeRTOS.

## Testing

Host build via CMake + GoogleTest:

```shell
cd lib_json/tests
cmake -B build
cmake --build build
ctest --test-dir build

# Or just
cd lib_json/tests
chmod +x *sh
./1_build.sh
./2_run.sh
```

The test suite covers:

- Empty / malformed input handling
- Top-level and nested key lookup
- Depth-limited parsing
- Whitespace tolerance
- Sub-document slicing with all three `new_root` modes
- Round-tripping through `serializeJson`
- The `keyToXxxVar` change-detection contract
- Both flavours of single-key extraction (raw buffer + dotted-path)

## Acknowledgements

Thanks to Claude and ChatGPT for helping on generating this documentation.

## License

MIT License — see [LICENSE](license.txt) file.
