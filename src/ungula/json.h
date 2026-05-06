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
//   #include <ungula/json/json_types.h>   // Json + JsonObject value types
//   #include <ungula/json/json.h>         // JsonWrapper full parser
//   #include <ungula/json/json_utils.h>   // helpers + single-key extractors
//
// Pulling in <ungula/core/util/string_types.h> here keeps Arduino CLI's library
// discovery happy: if a sketch only includes <ungula_json.h>, the
// transitive dependency on UngulaCore still gets discovered.

#include <ungula/core/util/string_types.h>

#include "ungula/json/json.h"
#include "ungula/json/json_types.h"
#include "ungula/json/json_utils.h"
