// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

// Tiny helpers used by the JSON test suite. Kept here so the test sources
// stay free of implicit dependencies on the rest of the project.

#include <cstdio>

#include <util/string_types.h>

namespace test_support {

    // Trace helper used by a couple of integration-style tests. Writes to
    // stdout (gtest captures it). It's a function rather than a macro so
    // call sites read the same as `printf`.
    inline void ulog(const char* msg) {
        std::puts(msg);
    }

    // Fixed epoch timestamp used by the message-shape tests so the expected
    // string stays stable from run to run.
    inline double getEpochTime() {
        return 1700000000.0;
    }

}  // namespace test_support

using test_support::getEpochTime;
using test_support::ulog;
