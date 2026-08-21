// Generator contract tests for orionc.
//
// Verifies that orionc rejects invalid action manifests with actionable
// diagnostics and a non-zero exit status: unknown command references,
// duplicate action names, duplicate hotkeys, and malformed hotkey strings.
//
// These tests shell out to the orionc binary (build/bin/orionc), so they are
// POSIX-only and compiled out on Windows.

#include "test_framework.h"

#if !defined(_WIN32)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// Write fixture to a temp file, run orionc, capture combined output + exit code.
static int run_orionc(const char *fixture, const char *tag, char *out, size_t out_sz) {
    char input[512], output[512], cmd[1024];
    snprintf(input, sizeof(input), "/tmp/orionc_%s_%d.orion", tag, (int)getpid());
    snprintf(output, sizeof(output), "/tmp/orionc_%s_%d.h", tag, (int)getpid());

    FILE *f = fopen(input, "w");
    if (!f) return -1;
    fputs(fixture, f);
    fclose(f);

    snprintf(cmd, sizeof(cmd),
             "build/bin/orionc --input %s --output %s --prefix test 2>&1",
             input, output);
    FILE *p = popen(cmd, "r");
    if (!p) { remove(input); return -1; }
    size_t n = fread(out, 1, out_sz - 1, p);
    out[n] = '\0';
    int rc = pclose(p);
    FILE *generated = fopen(output, "r");
    if (generated && n + 1 < out_sz) {
        n += fread(out + n, 1, out_sz - n - 1, generated);
        out[n] = '\0';
    }
    if (generated) fclose(generated);

    remove(input);
    remove(output);
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    return -1;
}

static bool contains(const char *hay, const char *needle) {
    return strstr(hay, needle) != NULL;
}

static const char *kValid = ""
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<orion version=\"1\" name=\"t\" title=\"T\">\n"
  "  <menus>\n"
  "    <menu name=\"repo\" label=\"Repo\">\n"
  "      <item name=\"refresh\" label=\"Refresh\" shortcut=\"F5;Ctrl+F5\" />\n"
  "    </menu>\n"
  "  </menus>\n"
  "  <toolbars>\n"
  "    <toolbar name=\"main\"><Button name=\"r\" command=\"repo.refresh\" text=\"R\" /></toolbar>\n"
  "  </toolbars>\n"
  "</orion>\n";

void test_valid_manifest_accepted(void) {
    TEST("orionc: valid manifest generates with exit 0");
    char out[2048] = {0};
    int rc = run_orionc(kValid, "valid", out, sizeof(out));
    ASSERT_EQUAL(rc, 0);
    PASS();
}

void test_unknown_reference_rejected(void) {
    TEST("orionc: unknown command reference fails generation");
    const char *fix = ""
      "<orion version=\"1\" name=\"t\" title=\"T\">\n"
      "  <menus><menu name=\"repo\" label=\"Repo\"><item name=\"refresh\" label=\"Refresh\" /></menu></menus>\n"
      "  <toolbars><toolbar name=\"main\"><Button name=\"r\" command=\"repo.refreshx\" text=\"R\" /></toolbar></toolbars>\n"
      "</orion>\n";
    char out[2048] = {0};
    int rc = run_orionc(fix, "unknown", out, sizeof(out));
    ASSERT_TRUE(rc != 0);
    ASSERT_TRUE(contains(out, "unknown command reference"));
    ASSERT_TRUE(contains(out, "repo.refreshx"));
    PASS();
}

void test_duplicate_action_name_rejected(void) {
    TEST("orionc: duplicate action name fails generation");
    const char *fix = ""
      "<orion version=\"1\" name=\"t\" title=\"T\">\n"
      "  <menus><menu name=\"repo\" label=\"Repo\">\n"
      "    <item name=\"refresh\" label=\"Refresh\" />\n"
      "    <item name=\"refresh\" label=\"Refresh Again\" />\n"
      "  </menu></menus>\n"
      "</orion>\n";
    char out[2048] = {0};
    int rc = run_orionc(fix, "dupname", out, sizeof(out));
    ASSERT_TRUE(rc != 0);
    ASSERT_TRUE(contains(out, "duplicate action name"));
    PASS();
}

void test_duplicate_hotkey_rejected(void) {
    TEST("orionc: duplicate hotkey fails generation");
    const char *fix = ""
      "<orion version=\"1\" name=\"t\" title=\"T\">\n"
      "  <menus><menu name=\"repo\" label=\"Repo\">\n"
      "    <item name=\"refresh\" label=\"Refresh\" shortcut=\"F5\" />\n"
      "    <item name=\"search\" label=\"Search\" shortcut=\"F5\" />\n"
      "  </menu></menus>\n"
      "</orion>\n";
    char out[2048] = {0};
    int rc = run_orionc(fix, "duphotkey", out, sizeof(out));
    ASSERT_TRUE(rc != 0);
    ASSERT_TRUE(contains(out, "duplicate hotkey"));
    PASS();
}

void test_malformed_hotkey_rejected(void) {
    TEST("orionc: malformed hotkey fails generation");
    const char *fix = ""
      "<orion version=\"1\" name=\"t\" title=\"T\">\n"
      "  <menus><menu name=\"repo\" label=\"Repo\">\n"
      "    <item name=\"refresh\" label=\"Refresh\" shortcut=\"Ctrl+NotAKey\" />\n"
      "  </menu></menus>\n"
      "</orion>\n";
    char out[2048] = {0};
    int rc = run_orionc(fix, "badhotkey", out, sizeof(out));
    ASSERT_TRUE(rc != 0);
    ASSERT_TRUE(contains(out, "malformed hotkey"));
    PASS();
}
#endif // !_WIN32

int main(void) {
    TEST_START("orionc generator contract");
#if !defined(_WIN32)
    test_valid_manifest_accepted();
    test_unknown_reference_rejected();
    test_duplicate_action_name_rejected();
    test_duplicate_hotkey_rejected();
    test_malformed_hotkey_rejected();
#else
    (void)0;
#endif
    TEST_END();
}
