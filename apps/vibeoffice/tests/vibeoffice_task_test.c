#include "test_framework.h"
#include "apps/vibeoffice/tasks.h"

#if !defined(_WIN32)
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
static void test_two_desks_finish_independently(void) {
  TEST("VibeOffice: two desks run independently and persist results");
  char old_cwd[1024], temp[] = "/tmp/vibeoffice-task-test-XXXXXX";
  ASSERT_NOT_NULL(getcwd(old_cwd, sizeof(old_cwd)));
  ASSERT_NOT_NULL(mkdtemp(temp));
  char fakebin[1200], opencode[1300], path[4096];
  snprintf(fakebin, sizeof(fakebin), "%s/bin", temp); mkdir(fakebin, 0755);
  snprintf(opencode, sizeof(opencode), "%s/opencode", fakebin);
  FILE *script = fopen(opencode, "wb"); ASSERT_NOT_NULL(script);
  fputs("#!/bin/sh\n[ \"$2\" = --model ] || exit 9\n"
        "sleep 0.05\nprintf 'model:%s reply:%s' \"$3\" \"$4\"\n", script);
  fclose(script); chmod(opencode, 0755);
  const char *old_path = getenv("PATH"); char *saved_path = old_path ? strdup(old_path) : NULL;
  snprintf(path, sizeof(path), "%s:%s", fakebin, old_path ? old_path : "");
  setenv("PATH", path, 1); ASSERT_EQUAL(chdir(temp), 0);

  vibe_process_t killed = {0}, healthy = {0}; char error[256];
  ASSERT_TRUE(vibe_task_submit(&killed, 1, "opencode/mimo-v2.5-free",
                               "hello one", error, sizeof(error)));
  ASSERT_TRUE(vibe_task_submit(&healthy, 2, "lmstudio/google/gemma-4-e4b",
                               "hello \"two\"", error, sizeof(error)));
  vibe_task_t first, second;
  ASSERT_TRUE(vibe_task_read(1, &first)); ASSERT_TRUE(vibe_task_read(2, &second));
  ASSERT_EQUAL(first.status, VIBE_TASK_BUSY); ASSERT_EQUAL(second.status, VIBE_TASK_BUSY);
  ASSERT_STR_EQUAL(first.model, "opencode/mimo-v2.5-free");
  ASSERT_STR_EQUAL(second.model, "lmstudio/google/gemma-4-e4b");
  kill(killed.pid, SIGKILL);
  for (int i = 0; i < 300 && (killed.pid > 0 || healthy.pid > 0); i++) {
    vibe_task_poll(&killed); vibe_task_poll(&healthy); usleep(10000);
  }
  ASSERT_EQUAL(killed.pid, 0); ASSERT_EQUAL(healthy.pid, 0);
  ASSERT_TRUE(vibe_task_read(1, &first)); ASSERT_TRUE(vibe_task_read(2, &second));
  ASSERT_EQUAL(first.status, VIBE_TASK_ERROR); ASSERT_EQUAL(second.status, VIBE_TASK_DONE);
  ASSERT_STR_EQUAL(second.input, "hello \"two\"");
  ASSERT_STR_EQUAL(second.output, "model:lmstudio/google/gemma-4-e4b reply:hello \"two\"");

  unlink(".tasks/desk-1.json"); unlink(".tasks/desk-2.json"); rmdir(".tasks");
  chdir(old_cwd); unlink(opencode); rmdir(fakebin); rmdir(temp);
  if (saved_path) { setenv("PATH", saved_path, 1); free(saved_path); }
  PASS();
}
#endif

int main(void) {
  TEST_START("VibeOffice Tasks");
#if defined(_WIN32)
  TEST("VibeOffice process test requires POSIX"); SKIP("POSIX-only process runner");
#else
  test_two_desks_finish_independently();
#endif
  TEST_END();
}
