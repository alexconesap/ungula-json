// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include "json/json.h"
#include "json/json_types.h"
#include "json/json_utils.h"
#include "test_helpers.h"

// Bring everything into the unqualified namespace so the test bodies stay
// readable. Test code is allowed to do this; library code never should.
using namespace ungula::json;

TEST(JsonTest, EmptyJson) {
    string_t original_payload = "";
    JsonWrapper json = JsonWrapper(original_payload);
    EXPECT_TRUE(json.isEmpty());
    EXPECT_FALSE(json.isValidJson());
    EXPECT_FALSE(isValidJson(original_payload));

    json = JsonWrapper("");
    EXPECT_TRUE(json.isEmpty());
    EXPECT_FALSE(json.isValidJson());
    EXPECT_FALSE(isValidJson(original_payload));

    json = JsonWrapper("     ");
    EXPECT_TRUE(json.isEmpty());
    EXPECT_FALSE(json.isValidJson());
    EXPECT_FALSE(isValidJson(original_payload));

    string_t original_payload2 = "{}";
    JsonWrapper json2 = JsonWrapper(original_payload2);
    EXPECT_TRUE(json2.isEmpty());
    EXPECT_TRUE(json2.isValidJson());
    EXPECT_TRUE(isValidJson(original_payload2));

    json2 = JsonWrapper("{}");
    EXPECT_TRUE(json2.isEmpty());
    EXPECT_TRUE(json2.isValidJson());
    EXPECT_TRUE(isValidJson(original_payload2));
}

TEST(JsonTest, IndividualKeys) {
    const string_t original_payload = R"({"hello":"world","value":42})";
    JsonWrapper json = JsonWrapper(original_payload);
    EXPECT_TRUE(json.isValidJson());
    // ulog("------------------------------------------------------------------------------------------");
    // json.printAll();
    // ulog("------------------------------------------------------------------------------------------");

    EXPECT_EQ(json.getStr("hello"), "world");
    EXPECT_EQ(json.getStr("hello", true), "\"world\"");
    EXPECT_EQ(json.getInt("value"), 42);

    // Unexisting keys should return default values
    EXPECT_EQ(json.getStr("whatever"), "");
    EXPECT_EQ(json.getStr("whatever.and.ever"), "");
    EXPECT_EQ(json.getInt("whatever.and.ever"), 0);
    EXPECT_EQ(json.getFloat("whatever.and.ever"), 0.0f);
    EXPECT_EQ(json.getBool("whatever.and.ever"), false);
}

TEST(JsonTest, JsonWithNormalPayload) {
    const string_t original_payload = R"({"payload":{"hello":"world","value":42}})";
    JsonWrapper json = JsonWrapper(original_payload);
    EXPECT_TRUE(json.isValidJson());

    EXPECT_TRUE(json.has("payload"));

    EXPECT_EQ(json.getStr("payload.hello"), "world");
    EXPECT_EQ(json.getInt("payload.value"), 42);
}

TEST(JsonTest, HasChecking) {
    const string_t original_payload = R"({"payload":{"hello":"world","value":42}})";
    JsonWrapper json = JsonWrapper(original_payload);
    EXPECT_TRUE(json.isValidJson());

    EXPECT_TRUE(json.has("payload"));

    EXPECT_TRUE(json.has("payload"));
    EXPECT_TRUE(json.has("payload.hello"));
    EXPECT_TRUE(json.has("payload.value"));
    EXPECT_FALSE(json.has("payload.XX"));
}

TEST(JsonTest, SetLevelsDeep) {
    const string_t original_payload =
            R"({"action":"take_one_shot","payload":{"settings":{"roi":"xx","quality":100}}})";

    JsonWrapper parser1 = JsonWrapper(original_payload);
    EXPECT_TRUE(parser1.isValidJson());
    EXPECT_TRUE(parser1.has("payload"));
    EXPECT_TRUE(parser1.has("payload"));
    EXPECT_TRUE(parser1.has("payload.settings"));
    EXPECT_TRUE(parser1.has("action"));
    EXPECT_EQ(parser1.getStr("action"), "take_one_shot");
    EXPECT_TRUE(parser1.has("payload.settings.roi"));
    EXPECT_EQ(parser1.getStr("payload.settings.roi"), "xx");

    JsonWrapper parser2 = JsonWrapper(original_payload, 1);
    EXPECT_TRUE(parser2.isValidJson());
    EXPECT_FALSE(parser2.has("payload"));
    EXPECT_FALSE(parser2.has("payload.settings"));
    EXPECT_TRUE(parser2.has("action"));
    EXPECT_EQ(parser2.getStr("action"), "take_one_shot");
    EXPECT_FALSE(parser2.has("payload.settings.roi"));
    EXPECT_EQ(parser2.getStr("payload.settings.roi"), "");

    JsonWrapper parser3 = JsonWrapper(original_payload, 2);
    EXPECT_TRUE(parser3.isValidJson());
    EXPECT_TRUE(parser3.has("payload"));
    EXPECT_FALSE(parser3.has("payload.settings"));
    EXPECT_TRUE(parser3.has("action"));
    EXPECT_EQ(parser3.getStr("action"), "take_one_shot");
    EXPECT_FALSE(parser3.has("payload.settings.roi"));
    EXPECT_EQ(parser3.getStr("payload.settings.roi"), "");

    JsonWrapper parser4 = JsonWrapper(original_payload, 3);
    EXPECT_TRUE(parser4.isValidJson());
    EXPECT_TRUE(parser4.has("payload"));
    EXPECT_TRUE(parser4.has("payload.settings"));
    EXPECT_TRUE(parser4.has("action"));
    EXPECT_EQ(parser4.getStr("action"), "take_one_shot");
    EXPECT_TRUE(parser4.has("payload.settings.roi"));
    EXPECT_EQ(parser4.getStr("payload.settings.roi"), "xx");
}

TEST(JsonTest, MalformedJsonPrettyDeep) {
    JsonWrapper parser1 = JsonWrapper(
            R"({action":"take_one_shot","payload":{"settings":{"roi":"xx","quality":100}}})");
    EXPECT_FALSE(parser1.isValidJson());
    EXPECT_FALSE(parser1.has("payload"));

    EXPECT_FALSE(parser1.has("payload"));
    EXPECT_FALSE(parser1.has("payload.settings"));
    EXPECT_FALSE(parser1.has("payload.settings.roi"));

    JsonWrapper parser3 = JsonWrapper(R"(})");
    EXPECT_FALSE(parser3.isValidJson());
    EXPECT_FALSE(parser3.has("payload"));

    JsonWrapper parser4 = JsonWrapper(R"()");
    EXPECT_FALSE(parser4.isValidJson());
    EXPECT_FALSE(parser4.has("payload"));
}

TEST(JsonTest, WhiteSpacedJsons) {
    JsonWrapper parser1 = JsonWrapper(
            R"({   "  action"  : "take_{one}_shot",   "payload"  :  {    "settings": { "roi": "xx","quality":100}  }  } )");
    EXPECT_TRUE(parser1.isValidJson());
    EXPECT_TRUE(parser1.has("payload"));

    EXPECT_TRUE(parser1.has("payload"));
    EXPECT_TRUE(parser1.has("payload.settings"));
    EXPECT_TRUE(parser1.has("payload.settings.roi"));

    EXPECT_EQ(parser1.getStr("payload.settings.roi"), "xx");
    EXPECT_EQ(parser1.getInt("payload.settings.quality"), 100);
    EXPECT_EQ(parser1.getStr("action"), "take_{one}_shot");
}

TEST(JsonTest, WeirdJsonThatMustBeParsed) {
    JsonWrapper parser1 = JsonWrapper(
            R"({"action":take_one_shot,"payload":{"settings":{"roi":"xx","quality":100}}})");
    EXPECT_TRUE(parser1.isValidJson());
    EXPECT_TRUE(parser1.has("payload"));

    JsonWrapper parser2 =
            JsonWrapper(R"({"action":@(0120),"payload":{"settings":{"roi":"xx","quality":100}}})");
    EXPECT_TRUE(parser2.isValidJson());

    JsonWrapper parser3 =
            JsonWrapper(R"({"action":,"payload":{"settings":{"roi":"xx","quality":100}}})");
    EXPECT_TRUE(parser3.isValidJson());
}

TEST(JsonTest, AvoidTooManyDepthLevels) {
    JsonWrapper parser1 = JsonWrapper(R"( {"a":"1", "b":{"c": {"d":{"e": {"f":1}   } } } })");
    EXPECT_TRUE(parser1.isValidJson());
    EXPECT_FALSE(parser1.has("payload"));

    EXPECT_FALSE(parser1.has("b.c.d.e.f"));  // This should not be parsed due to depth limit
    // parser1.printAll();
}

TEST(JsonTest, QuickValueExtractionByKeyChar) {
    const char* buffer =
            R"({  "action" : "take_one_shot"   ,  "payload" : {  "settings"   :{"roi":"xx","quality":100}}})";
    unsigned long len = strlen(buffer);

    string_t v = jsonExtractStringKey(buffer, len, "action");
    EXPECT_EQ(v, "take_one_shot");

    v = jsonExtractStringKey(buffer, len, "whatever");
    EXPECT_EQ(v, "");

    v = jsonExtractStringKey(buffer, len, "payload");
    EXPECT_EQ(v, "");

    v = jsonExtractStringKey(R"("action" : "take_one_shot")", len, "action");
    EXPECT_EQ(v, "take_one_shot");

    v = jsonExtractStringKey(R"(action:take_one_shot)", len, "action");
    EXPECT_EQ(v, "");
}

TEST(JsonTest, SerializeJsonTest) {
    JsonObject settings;
    putTojson(settings, "roi", "xx");
    putTojson(settings, "quality", 100);
    putTojson(settings, "focus", "6.5");
    putTojson(settings, "type", "jpeg");

    JsonObject message;
    putTojson(message, "action", "shot");
    putTojson(message, "settings", settings);

    JsonStr message_to_str;
    serializeJson(message, message_to_str);
    EXPECT_EQ(
            message_to_str,
            R"({"action":"shot","settings":{"roi":"xx","quality":100,"focus":"6.5","type":"jpeg"}})");
}

TEST(JsonTest, ExtractObject) {
    string_t original_json =
            R"({"action":"take_one_shot","payload":{"settings":{"roi":"xx","quality":100.2,"a":1}, "other":{"a":1,"b":2}}})";

    JsonWrapper main_doc = JsonWrapper(original_json);
    EXPECT_TRUE(main_doc.isValidJson());
    // ulog("---------------------------------------------------------------------");
    // main_doc.printAll();
    // ulog("---------------------------------------------------------------------");

    JsonWrapper subdoc = JsonWrapper(main_doc, "payload.settings", "");
    // ulog("---------------------------------------------------------------------");
    // subdoc.printAll();
    // ulog("---------------------------------------------------------------------");
    EXPECT_TRUE(subdoc.isValidJson());
    EXPECT_TRUE(subdoc.has("roi"));
    EXPECT_EQ(subdoc.getStr("roi"), "xx");
    EXPECT_TRUE(subdoc.has("quality"));
    EXPECT_FLOAT_EQ(subdoc.getFloat("quality"), 100.2f);

    // JsonStr message_to_str;
    // serializeJson(subdoc, message_to_str);
    // EXPECT_EQ(message_to_str, R"({"roi":"xx","quality":100})");

    JsonWrapper subdoc2 = JsonWrapper(main_doc, "payload.settings", "root");
    // ulog("---------------------------------------------------------------------");
    // subdoc2.printAll();
    // ulog("---------------------------------------------------------------------");
    EXPECT_TRUE(subdoc2.isValidJson());
    EXPECT_TRUE(subdoc2.has("root.roi"));
    EXPECT_EQ(subdoc2.getStr("root.roi"), "xx");
    EXPECT_TRUE(subdoc2.has("root.quality"));
    EXPECT_FLOAT_EQ(subdoc2.getFloat("root.quality"), 100.2f);

    JsonWrapper subdoc3 = JsonWrapper(main_doc, "payload.settings");
    // ulog("---------------------------------------------------------------------");
    // subdoc3.printAll();
    // ulog("---------------------------------------------------------------------");
    EXPECT_TRUE(subdoc3.isValidJson());
    EXPECT_TRUE(subdoc3.has("payload.settings.roi"));
    EXPECT_EQ(subdoc3.getStr("payload.settings.roi"), "xx");
    EXPECT_TRUE(subdoc3.has("payload.settings.quality"));
    EXPECT_FLOAT_EQ(subdoc3.getFloat("payload.settings.quality"), 100.2f);
}

TEST(JsonTest, ReadObjectAsString) {
    string_t original_json =
            R"({"action":"take_one_shot","payload":{"settings":{"roi":"xx","quality":100}}})";

    JsonWrapper parser1 = JsonWrapper(original_json);

    string_t settings = parser1.getObjectAsStr("payload.settings");

    // ulog("---------------------------------------------------------------------");
    // ulog(settings.c_str());
    // ulog("---------------------------------------------------------------------");
    EXPECT_EQ(settings, R"({"roi":"xx","quality":100})");

    int i1 = jsonExtractAsInt(original_json, "roi");
    EXPECT_EQ(i1, 0);

    int i2 = jsonExtractAsInt(original_json, "quality");
    EXPECT_EQ(i2, 100);

    string_t s1 = jsonExtractAsStr(original_json, "roi");
    // ulog("Extracted quoted string: ");
    // ulog(s1.c_str());
    EXPECT_EQ(s1, "xx");

    string_t s2 = jsonExtractAsStr(original_json, "quality");
    // ulog("Extracted quoted string: ");
    // ulog(s2.c_str());
    EXPECT_EQ(s2, "100");

    // MALFORMED JSON TESTS
    string_t im1 = jsonExtractAsStr("", "roi");
    EXPECT_EQ(im1, "");
    im1 = jsonExtractAsStr("", "");
    EXPECT_EQ(im1, "");
    im1 = jsonExtractAsStr("", nullptr);
    EXPECT_EQ(im1, "");
    im1 = jsonExtractAsStr("{}", nullptr);
    EXPECT_EQ(im1, "");
    im1 = jsonExtractAsStr("{}", "");
    EXPECT_EQ(im1, "");
    im1 = jsonExtractAsStr("{}", "a");
    EXPECT_EQ(im1, "");
    im1 = jsonExtractAsStr("{a}", "a");
    EXPECT_EQ(im1, "");
    im1 = jsonExtractAsStr("{a:}", "a");
    EXPECT_EQ(im1, "");
    im1 = jsonExtractAsStr("{a:1}", "a");
    EXPECT_EQ(im1, "");
    im1 = jsonExtractAsStr(R"("{"a:1}")", "a");
    EXPECT_EQ(im1, "");
    im1 = jsonExtractAsStr(R"("{a":1}")", "a");
    EXPECT_EQ(im1, "");
    // A good one, finally :)
    im1 = jsonExtractAsStr(R"("{"a":1}")", "a");
    EXPECT_EQ(im1, "1");
}

TEST(JsonTest, ReadObjectAsStringExtractFullKey) {
    string_t original_json =
            R"({"action":"take_one_shot","payload":{"settings":{"roi":"xx","quality":100.2,"a":1}, "other":{"a":1,"b":2}}})";

    JsonWrapper parser1 = JsonWrapper(original_json);

    string_t settings = parser1.getObjectAsStr("payload.settings");

    // ulog("---------------------------------------------------------------------");
    // ulog(settings.c_str());
    // ulog("---------------------------------------------------------------------");
    EXPECT_EQ(settings, R"({"roi":"xx","quality":100.200000,"a":1})");
}

TEST(JsonTest, JsonInjectingPayloadValues) {
    const string_t original_payload =
            R"({"payload":{"hello":"world","intv":42,"floatv":3.14,"debug":true}})";
    JsonWrapper json = JsonWrapper(original_payload);
    EXPECT_TRUE(json.isValidJson());

    EXPECT_TRUE(json.has("payload"));

    bool systemChanges = false;
    bool debug_enabled = false;
    systemChanges = json.keyToBoolVar("payload.debug", debug_enabled) || systemChanges;
    EXPECT_TRUE(debug_enabled);
    EXPECT_TRUE(systemChanges);

    bool driverChanges = false;
    int intv = 11;
    float floatv = 100.20f;
    driverChanges = json.keyToFloatVar("payload.floatv", floatv) || driverChanges;
    driverChanges = json.keyToIntVar("payload.intv", intv) || driverChanges;
    EXPECT_TRUE(driverChanges);
    EXPECT_FLOAT_EQ(floatv, 100.20f);
    EXPECT_EQ(intv, 42);

    // Since the variables were set, no updates should be now processed
    driverChanges = false;
    driverChanges = json.keyToFloatVar("payload.floatv", floatv) || driverChanges;
    driverChanges = json.keyToIntVar("payload.intv", intv) || driverChanges;
    EXPECT_FALSE(driverChanges);
}

TEST(JsonTest, SerializeJsonSubObjectUsingJsonDoc) {
    JsonObject settings;
    putTojson(settings, "roi", "xx");
    putTojson(settings, "quality", 100);
    putTojson(settings, "focus", "6.5");
    putTojson(settings, "type", "jpeg");

    JsonObject mesg;
    putTojson(mesg, "action", "shot");
    putTojson(mesg, "settings", settings);

    JsonStr message_to_str;
    serializeJson(mesg, message_to_str);
    EXPECT_EQ(
            message_to_str,
            R"({"action":"shot","settings":{"roi":"xx","quality":100,"focus":"6.5","type":"jpeg"}})");
}

TEST(JsonTest, SerializeJsonSubObjectUsingJsonWrapper) {
    string_t payload =
            R"({"action":"shot","settings":{"roi":"xx","quality":100,"focus":"6.5","type":"jpeg"}})";

    JsonWrapper parser1 = JsonWrapper(payload);
    EXPECT_TRUE(parser1.isValidJson());
    EXPECT_TRUE(parser1.has("settings"));
    EXPECT_TRUE(parser1.has("settings.roi"));
    EXPECT_EQ(parser1.getStr("settings.roi"), "xx");
    EXPECT_TRUE(parser1.has("settings.quality"));
    EXPECT_EQ(parser1.getInt("settings.quality"), 100);
    EXPECT_TRUE(parser1.has("settings.focus"));
    EXPECT_EQ(parser1.getStr("settings.focus"), "6.5");
    EXPECT_TRUE(parser1.has("settings.type"));
    EXPECT_EQ(parser1.getStr("settings.type"), "jpeg");
    EXPECT_TRUE(parser1.has("action"));
    EXPECT_EQ(parser1.getStr("action"), "shot");

    // ulog("------------------------------------------------------------------------------------------");
    // parser1.printAll();
    // ulog("------------------------------------------------------------------------------------------");
    string_t test = parser1.getObjectAsStr("settings");
    EXPECT_EQ(test, R"({"roi":"xx","quality":100,"focus":"6.5","type":"jpeg"})");

    // Emulate the MQTT message serialization
    struct FakeCameraCaptureSettings {
            uint8_t count_lens_positions;
            JsonObject json_settings_payload;
    };
    FakeCameraCaptureSettings sett;
    // Inject the settings into the fake camera capture settings
    sett.count_lens_positions = 10;
    sett.json_settings_payload = parser1.getObject("settings");

    // Construct the MQTT full message
    float xh = 1.2f;
    float yv = 3.4f;
    // MQTT message
    JsonObject message = {{"action", "preview"}, {"camera", "all"}, {"x", xh}, {"y", yv}};
    putTojson(message, "settings", sett.json_settings_payload);

    // Serialize the message
    string_t out;
    serializeJson(message, out);
    EXPECT_EQ(
            out,
            R"({"action":"preview","camera":"all","x":1.200000,"y":3.400000,"settings":{"roi":"xx","quality":100,"focus":"6.5","type":"jpeg"}})");
}

TEST(JsonTest, IndividualJsonObjectsSerialization) {
    Json roi = "xx";
    Json quality = 100;
    Json focus = "6.5";
    Json type = "jpeg";

    EXPECT_EQ(roi.serialize(), "xx");
    EXPECT_EQ(roi.serialize(true), "\"xx\"");
    EXPECT_EQ(quality.serialize(), "100");
    EXPECT_EQ(quality.serialize(true), "100");
    EXPECT_EQ(focus.serialize(), "6.5");
    EXPECT_EQ(focus.serialize(true), "\"6.5\"");
    EXPECT_EQ(type.serialize(), "jpeg");
    EXPECT_EQ(type.serialize(true), "\"jpeg\"");
}

TEST(JsonTest, InjectingValuesFromJsonToVars) {
    string_t json =
            R"({"action":"take_one_shot","payload":{"settings":{"roi":"xx","quality":100}}})";
    JsonWrapper parser1 = JsonWrapper(json);
    EXPECT_TRUE(parser1.isValidJson());

    string_t roi = "";
    parser1.keyToStrVar("payload.settings.roi", roi);
    EXPECT_EQ(roi, "xx");

    int quality = 0;
    parser1.keyToIntVar("payload.settings.quality", quality);
    EXPECT_EQ(quality, 100);

    roi = "";
    jsonKeyToStrVar(json, "payload.settings.roi", roi);
    EXPECT_EQ(roi, "xx");

    quality = 0;
    jsonKeyToIntVar(json, "payload.settings.quality", quality);
    EXPECT_EQ(quality, 100);
}

void test_payload(float xh, float yv, const string_t& status, JsonStr& out,
                  const char* error_code = nullptr, const string_t* msg = nullptr) {
    const char* const DEVICE_HOSTNAME = "ESP32 I";

    // Name communicated to the MQTT server for this device (ESP32)
    const char* const DEVICE_ID = "rig";
    const char* const DEVICE_TYPE = "rig";
    const char* MQTT_STATUS_TYPE_STATUS = "status";
    const char* MQTT_STATUS_TYPE_LOG = "log";
    const char* MQTT_STATUS_TYPE_OPERATION = "operation";

    const char* DEVICE_STATUS_DISCONNECTED = "disconnected";
    const char* DEVICE_STATUS_READY = "idle";
    const char* const VERSION = "1.1.27";

    string_t IP = "192.168.1.11";

    JsonObject message;
    putTojson(message, "id", DEVICE_ID);
    putTojson(message, "type", DEVICE_TYPE);
    putTojson(message, "status", status);
    putTojson(message, "status_type", MQTT_STATUS_TYPE_OPERATION);

    putTojson(message, "ip", IP.c_str());
    putTojson(message, "hostname", DEVICE_HOSTNAME);
    putTojson(message, "ts", getEpochTime());
    putTojson(message, "version", VERSION);

    // Create default slots
    putTojson(message, "error", nullptr);
    putTojson(message, "message", nullptr);

    // Inject values using same keys as above
    if (msg)
        putTojson(message, "message", *msg);
    putTojson(message, "error", error_code == nullptr ? nullptr : Json(error_code));

    serializeJson(message, out);
}

TEST(JsonTest, EscapeJsonString) {
    string_t message = "Whatever message";

    JsonStr buffer;
    test_payload(1.2, 3.4, "ready", buffer, nullptr, &message);
    EXPECT_TRUE(
            true);  // Assuming the function works correctly, we just check if it compiles and runs
    // ulog("------------------------------------------------------------------------------------------");
    // ulog(buffer.c_str());
    // ulog("------------------------------------------------------------------------------------------");
    EXPECT_EQ(
            buffer,
            R"({"id":"rig","type":"rig","status":"ready","status_type":"operation","ip":"192.168.1.11","hostname":"ESP32 I","ts":1700000000.000000,"version":"1.1.27","error":null,"message":"Whatever message"})");
}

TEST(JsonTest, EscapeJsonStringWithSpecialChars) {
    JsonObject message = {
            {"id", "rig"},                 //
            {"type", "rig"},               //
            {"error", "Nothing"},          // Keep the error here
            {"status", "ready"},           //
            {"status_type", "operation"},  //
            {"message", nullptr}           // Keep the message here
    };

    putTojson(message, "error", "Example of error");
    putTojson(message, "id", "0001");
    putTojson(message, "message", "Now it have a message");

    JsonStr out;
    serializeJson(message, out);
    EXPECT_TRUE(
            true);  // Assuming the function works correctly, we just check if it compiles and runs

    // ulog("------------------------------------------------------------------------------------------");
    // ulog(out.c_str());
    // ulog("------------------------------------------------------------------------------------------");

    EXPECT_EQ(
            out,
            R"({"id":"0001","type":"rig","error":"Example of error","status":"ready","status_type":"operation","message":"Now it have a message"})");
}

TEST(JsonTest, TestSubJsonAsParameter) {
    float yv = 2.23;
    JsonObject message = {
            {"version", "1.0"},                             // just a key
            {"message", Json({{"xh", 1.12f}, {"yv", yv}})}  // Using Json object for xh and yv
    };

    string_t out;
    serializeJson(message, out);

    EXPECT_EQ(out, R"({"version":"1.0","message":{"xh":1.120000,"yv":2.230000}})");
}

TEST(JsonTest, TestPuttingValuesToExistingStruct) {
    struct CameraCaptureSettings {
            uint8_t count_lens_positions;
            JsonObject json_settings_payload;
    };

    CameraCaptureSettings global_shot_settings = {
            //
            .count_lens_positions = 1,
            .json_settings_payload = {}
            //
    };

    putTojson(global_shot_settings.json_settings_payload, "lens_position", "4,5,6");
    global_shot_settings.count_lens_positions = 3;

    JsonStr message_to_str;
    serializeJson(global_shot_settings.json_settings_payload, message_to_str);

    ulog("-----------------------------------------------------------------------------------------"
         "-");
    ulog(message_to_str.c_str());
    ulog("-----------------------------------------------------------------------------------------"
         "-");
}