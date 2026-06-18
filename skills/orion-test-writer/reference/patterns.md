# Test Patterns

Common testing patterns for Orion framework modules.

## Pure Logic Tests (No ui.h)

```c
#include "test_framework.h"

// Inline helpers — do NOT include ui.h
#define LOWORD(x)        ((uint16_t)((uint32_t)(x) & 0xffff))
#define HIWORD(x)        ((uint16_t)(((uint32_t)(x) >> 16) & 0xffff))
#define MAKEDWORD(lo,hi) ((uint32_t)(((uint16_t)(lo)) | ((uint32_t)((uint16_t)(hi))) << 16))

void test_hiword_loword_packing(void) {
  TEST("HIWORD returns high 16 bits of uint32");
  uint32_t val = MAKEDWORD(0x1234, 0xABCD);
  ASSERT_EQUAL(HIWORD(val), 0xABCD);
  PASS();
}

void test_makerect_fields(void) {
  TEST("MAKERECT sets x, y, w, h correctly");
  irect16_t r = MAKERECT(10, 20, 100, 50);
  ASSERT_EQUAL(r.x, 10);
  ASSERT_EQUAL(r.y, 20);
  ASSERT_EQUAL(r.w, 100);
  ASSERT_EQUAL(r.h, 50);
  PASS();
}

int main(void) {
  TEST_START("Message packing helpers");
  test_hiword_loword_packing();
  test_makerect_fields();
  TEST_END();
}
```

## Edge Case Testing

For every function, test:
- **NULL inputs** — what happens?
- **Boundary values** — 0, INT_MAX, empty string, max-length string
- **Failure paths** — allocation failure, invalid arguments
- **Happy path** — does it return the right value?

```c
void test_accel_translate_null_table(void) {
  TEST("translate_accelerator with NULL table returns false");
  bool result = translate_accelerator(NULL, NULL, NULL);
  ASSERT_FALSE(result);
  PASS();
}

void test_accel_translate_no_match(void) {
  TEST("translate_accelerator returns false when no accelerator matches");
  // ... setup ...
  bool result = translate_accelerator(win, table, &evt);
  ASSERT_FALSE(result);
  PASS();
}
```

## Accelerator Table Tests

```c
void test_accel_fshift_fcontrol_combination(void) {
  TEST("FSHIFT+FCONTROL modifier combination recognized");
  accel_t table[] = { { FSHIFT | FCONTROL, SDL_SCANCODE_S, 100 } };
  // ... test that Ctrl+Shift+S triggers command 100 ...
  PASS();
}
```

## Coverage Checklist

For a module like `accel.c`, write tests for:

| Behavior | Example test name |
|----------|-------------------|
| Happy path | `test_accel_translate_finds_match` |
| No match | `test_accel_translate_no_match` |
| NULL safety | `test_accel_translate_null_table` |
| Flag combinations | `test_accel_fshift_fcontrol_combination` |
| Command packing | `test_accel_command_packing` |
| Free null safety | `test_accel_free_null_safe` |
