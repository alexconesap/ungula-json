// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#ifndef __cplusplus
#error UngulaJson requires a C++ compiler
#endif

// Ungula Embedded JSON Library — single include for the Arduino discovery
// chain. Real code should include the specific headers it needs:
//
//   #include <json/json_types.h>   // Json + JsonObject value types
//   #include <json/json.h>         // JsonWrapper full parser
//   #include <json/json_utils.h>   // helpers + single-key extractors
//
// Pulling in <util/string_types.h> here keeps Arduino CLI's library
// discovery happy: if a sketch only includes <ungula_json.h>, the
// transitive dependency on UngulaCore still gets discovered.

#include <util/string_types.h>

#include "json/json.h"
#include "json/json_types.h"
#include "json/json_utils.h"
