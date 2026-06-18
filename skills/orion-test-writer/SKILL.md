---
name: "Orion Test Writer"
description: "Write and review headless C unit tests for the Orion UI framework. Uses tests/test_framework.h. Focuses on correctness, edge cases, and keeping tests fast and display-free."
globs:
  - "tests/**/*.c"
  - "tests/test_framework.h"
alwaysApply: false
---

# Orion Test Writer

Write, review, and improve C unit tests for the Orion UI framework. Tests must be headless, fast, and self-contained.

## Quick Start

```c
#include "test_framework.h"

void test_example(void) {
  TEST("description of assertion");
  ASSERT_EQUAL(actual, expected);
  PASS();
}

int main(void) {
  TEST_START("Suite name");
  test_example();
  TEST_END();
}
```

## Core Rules

1. **Headless by default** — Do NOT include `ui.h` unless testing window/message APIs
2. **One behavior per function** — Split if >20 lines
3. **Test public API** — Never reach into struct internals or call `static` helpers
4. **Assert eagerly** — `ASSERT_*` macros return on failure; free resources before `PASS()`
5. **Name clearly** — `test_<thing>` function, English `TEST("...")` description

## Test Framework Reference

See `reference/framework-api.md` for complete macro reference.

## Common Patterns

See `reference/patterns.md` for examples of testing:
- Pure logic (HIWORD/LOWORD, MAKEDWORD)
- Edge cases (NULL,边界 values)
- Accelerator tables
- Window procedures

## Review Checklist

See `reference/review-checklist.md` for review findings with severity ratings.

## Workflow

1. Understand the module under test
2. Identify public API surface
3. List edge cases (NULL, 0, INT_MAX, empty, max)
4. Write one test function per behavior
5. Include inline helpers (do NOT pull in ui.h for pure logic)
6. Verify: `make test` passes
