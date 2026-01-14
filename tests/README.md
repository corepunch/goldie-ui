# Goldie UI Test Suite

This directory contains the test suite for the Goldie UI framework. The tests are inspired by the testing approach used for Windows 1.0, focusing on core functionality: window management, event handling, and common controls.

## Test Organization

### Test Files

- **test_framework.h** - Simple test framework providing macros for assertions and test reporting
- **basic_test.c** - Basic functionality tests (macros, constants, structures)

## Running Tests

### Build and Run All Tests

```bash
make test
```

This will:
1. Build all test executables
2. Run each test in sequence
3. Report results

### Run Individual Tests

```bash
./build/bin/test_basic
```

## Test Philosophy

The test suite follows the Windows 1.0 testing philosophy:

1. **Simple and Direct** - Tests are straightforward and easy to understand
2. **No Dependencies** - Each test is self-contained
3. **Fast Execution** - Tests run quickly without UI rendering
4. **Core Functionality** - Focus on essential features that must work

## What We Test

### Basic Functionality
- LOWORD/HIWORD/MAKEDWORD macros
- MIN/MAX macros
- Rectangle structures
- Window message constants (WM_CREATE, WM_DESTROY, WM_PAINT, etc.)
- Control message constants (BM_SETCHECK, CB_ADDSTRING, etc.)
- Notification constants (BN_CLICKED, EN_UPDATE, etc.)
- Window flags (WINDOW_NOTITLE, WINDOW_TRANSPARENT, etc.)

## Test Framework

The custom test framework (`test_framework.h`) provides:

### Macros
- `TEST_START(name)` - Begin a test suite
- `TEST_END()` - End suite and report results
- `TEST(name)` - Begin individual test
- `PASS()` - Mark test as passed
- `FAIL(msg)` - Mark test as failed with message

### Assertions
- `ASSERT_TRUE(condition)` - Verify condition is true
- `ASSERT_FALSE(condition)` - Verify condition is false
- `ASSERT_NULL(ptr)` - Verify pointer is NULL
- `ASSERT_NOT_NULL(ptr)` - Verify pointer is not NULL
- `ASSERT_EQUAL(a, b)` - Verify values are equal
- `ASSERT_NOT_EQUAL(a, b)` - Verify values are not equal
- `ASSERT_STR_EQUAL(a, b)` - Verify strings are equal

### Output
Tests produce colored output:
- Blue: Test suite headers
- Green: Passed tests
- Red: Failed tests
- Summary statistics at end

## Adding New Tests

To add a new test file:

1. Create `tests/yourtest.c`
2. Include `test_framework.h` and `../ui.h`
3. Write test functions
4. Create `main()` function with `TEST_START()` and `TEST_END()`
5. Run `make test` - the Makefile will automatically pick it up

Example:

```c
#include "test_framework.h"
#include "../ui.h"

void test_something(void) {
    TEST("Your test name");
    
    // Test code here
    ASSERT_TRUE(1 == 1);
    
    PASS();
}

int main(void) {
    TEST_START("Your Test Suite");
    
    test_something();
    
    TEST_END();
}
```

## CI/CD Integration

Tests are automatically run in GitHub Actions for:
- Linux (Ubuntu)
- macOS

Failed tests will cause the build to fail, preventing broken code from being merged.

## Future Tests

Planned additions:
- Window creation and management tests
- Message queue and event handling tests
- Common controls tests (button, checkbox, edit, label, list, combobox)
- Focus and keyboard navigation tests
- Mouse event simulation tests
- Text rendering tests
