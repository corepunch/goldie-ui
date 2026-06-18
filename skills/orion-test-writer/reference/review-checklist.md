# Test Review Checklist

Findings when reviewing test files. Each tagged with severity.

## 🔴 Critical (Must Fix)

- **Missing `TEST_END()`** — test results won't be printed, exit code wrong
- **`ASSERT` after resource allocation without cleanup** — leak on failure
- **Missing `#include "test_framework.h"`** — won't compile

## 🟡 Important (Should Fix)

- **Testing internals instead of public API** — brittle; refactor to use public entry points
- **Tests that require SDL init but don't document it** — will fail in CI without a display
- **Overly large test functions (>20 lines)** — split into smaller, focused functions
- **Missing edge case coverage** — test NULL, 0, boundary values

## 🔵 Style (Nice to Have)

- **Vague `TEST("...")` descriptions** — descriptions should read like assertions, not code
- **Inconsistent naming** — use `test_<thing_being_tested>` pattern
- **Missing file-header comment** — explain what module is under test
- **Inline helpers not marked** — clearly mark duplicated helpers with comment

## Example Review Comments

```c
// Missing TEST_END()
int main(void) {
  TEST_START("My tests");
  test_something();
  // ❌ Missing TEST_END()
}
```
> 🔴 Missing `TEST_END()` — test results won't be printed and exit code is wrong.

```c
void test_with_alloc(void) {
  char *str = malloc(100);
  TEST("test with allocation");
  ASSERT_NOT_NULL(str);
  // ❌ Missing free before PASS()
  PASS();
}
```
> 🔴 Resource allocated but not freed before `PASS()`. If `ASSERT` fails, memory leaks.

```c
void test_internal_struct(void) {
  TEST("accesses struct internals");
  window_t *win = create_window(...);
  ASSERT_EQUAL(win->internal_field, 42); // 🟡 Testing internals
  PASS();
}
```
> 🟡 Testing internals instead of public API. Use public entry points to avoid brittleness.
