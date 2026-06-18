# Test Framework API

Complete reference for `tests/test_framework.h`.

## Macros

| Macro | Purpose |
|-------|---------|
| `TEST_START("Suite name")` | Print suite header, must be first in `main()` |
| `TEST("description")` | Announce test case, increments `tests_run` |
| `PASS()` | Mark current test passed |
| `FAIL("reason")` | Mark current test failed with message |
| `ASSERT(cond, msg)` | Fail if cond is false, returns from function |
| `ASSERT_TRUE(cond)` | Fail if cond is false |
| `ASSERT_FALSE(cond)` | Fail if cond is true |
| `ASSERT_NULL(ptr)` | Fail if ptr is not NULL |
| `ASSERT_NOT_NULL(ptr)` | Fail if ptr is NULL |
| `ASSERT_EQUAL(a, b)` | Fail if a != b |
| `ASSERT_NOT_EQUAL(a, b)` | Fail if a == b |
| `ASSERT_STR_EQUAL(a, b)` | Fail if strings differ |
| `TEST_END()` | Print summary, returns 0 (pass) or 1 (fail) |

## Minimal Test File

```c
#include "test_framework.h"

void test_something(void) {
  TEST("description of what is tested");
  /* ... exercise code ... */
  ASSERT_EQUAL(actual, expected);
  PASS();
}

int main(void) {
  TEST_START("Module name");
  test_something();
  TEST_END();
}
```

## Helpers for Message Packing (Inline These)

```c
#define LOWORD(x)        ((uint16_t)((uint32_t)(x) & 0xffff))
#define HIWORD(x)        ((uint16_t)(((uint32_t)(x) >> 16) & 0xffff))
#define MAKEDWORD(lo,hi) ((uint32_t)(((uint16_t)(lo)) | ((uint32_t)((uint16_t)(hi))) << 16))
```

## Important Notes

- `ASSERT_*` macros call `return` on failure — they stop the test function immediately
- Always free resources before `PASS()` if test allocates anything
- Do NOT include `ui.h` unless genuinely testing window/message APIs
- Duplicate small pure-C helpers inline to stay self-contained
