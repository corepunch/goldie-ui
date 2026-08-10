#include "tasks.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define VIBE_TASK_DIR ".tasks"
#define VIBE_TASK_FILE_MAX (VIBE_TASK_OUTPUT_MAX * 2 + 4096)

const char *vibe_task_status_name(vibe_task_status_t status) {
  switch (status) {
    case VIBE_TASK_PENDING: return "pending";
    case VIBE_TASK_BUSY:    return "busy";
    case VIBE_TASK_DONE:    return "done";
    case VIBE_TASK_ERROR:   return "error";
    default:                return "available";
  }
}

static vibe_task_status_t task_status_from_name(const char *name) {
  if (!strcmp(name, "pending")) return VIBE_TASK_PENDING;
  if (!strcmp(name, "busy"))    return VIBE_TASK_BUSY;
  if (!strcmp(name, "done"))    return VIBE_TASK_DONE;
  if (!strcmp(name, "error"))   return VIBE_TASK_ERROR;
  return VIBE_TASK_AVAILABLE;
}

static bool task_path(char *path, size_t size, int desk_id, const char *suffix) {
  int n = snprintf(path, size, "%s/desk-%d.json%s", VIBE_TASK_DIR, desk_id,
                   suffix ? suffix : "");
  return n > 0 && (size_t)n < size;
}

static bool task_dir_ensure(void) {
#ifdef _WIN32
  return mkdir(VIBE_TASK_DIR) == 0 || errno == EEXIST;
#else
  return mkdir(VIBE_TASK_DIR, 0755) == 0 || errno == EEXIST;
#endif
}

static void json_write_string(FILE *file, const char *text) {
  fputc('"', file);
  for (const unsigned char *p = (const unsigned char *)(text ? text : ""); *p; p++) {
    switch (*p) {
      case '"':  fputs("\\\"", file); break;
      case '\\': fputs("\\\\", file); break;
      case '\b': fputs("\\b", file);  break;
      case '\f': fputs("\\f", file);  break;
      case '\n': fputs("\\n", file);  break;
      case '\r': fputs("\\r", file);  break;
      case '\t': fputs("\\t", file);  break;
      default:
        if (*p < 0x20) fprintf(file, "\\u%04x", *p);
        else fputc(*p, file);
        break;
    }
  }
  fputc('"', file);
}

static bool task_write(int desk_id, vibe_task_status_t status,
                       const char *model, const char *input, const char *output) {
  char path[128], temp[128];
  if (!task_dir_ensure() || !task_path(path, sizeof(path), desk_id, NULL) ||
      !task_path(temp, sizeof(temp), desk_id, ".tmp")) return false;
  FILE *file = fopen(temp, "wb");
  if (!file) return false;
  fprintf(file, "{ \"desk_id\": %d, \"status\": ", desk_id);
  json_write_string(file, vibe_task_status_name(status));
  if (model && *model) { fputs(", \"model\": ", file); json_write_string(file, model); }
  fputs(", \"input\": ", file); json_write_string(file, input);
  if (output) { fputs(", \"output\": ", file); json_write_string(file, output); }
  fputs(" }\n", file);
  bool ok = !ferror(file) && fclose(file) == 0;
  if (!ok) { remove(temp); return false; }
  if (rename(temp, path) != 0) { remove(temp); return false; }
  return true;
}

static const char *json_field(const char *json, const char *name) {
  char key[64];
  int n = snprintf(key, sizeof(key), "\"%s\"", name);
  if (n <= 0 || (size_t)n >= sizeof(key)) return NULL;
  const char *p = strstr(json, key);
  if (!p) return NULL;
  p += strlen(key);
  while (isspace((unsigned char)*p)) p++;
  if (*p++ != ':') return NULL;
  while (isspace((unsigned char)*p)) p++;
  return p;
}

static bool json_read_int(const char *json, const char *name, int *value) {
  const char *p = json_field(json, name);
  if (!p) return false;
  char *end = NULL;
  long parsed = strtol(p, &end, 10);
  if (end == p) return false;
  *value = (int)parsed;
  return true;
}

static int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool json_read_string(const char *json, const char *name, char *out, size_t size) {
  const char *p = json_field(json, name);
  if (!p || *p++ != '"' || !out || !size) return false;
  size_t len = 0;
  while (*p && *p != '"') {
    unsigned char c = (unsigned char)*p++;
    if (c == '\\') {
      c = (unsigned char)*p++;
      switch (c) {
        case 'b': c = '\b'; break; case 'f': c = '\f'; break;
        case 'n': c = '\n'; break; case 'r': c = '\r'; break;
        case 't': c = '\t'; break; case '"': case '\\': case '/': break;
        case 'u': {
          int h0 = hex_value(p[0]), h1 = hex_value(p[1]);
          int h2 = hex_value(p[2]), h3 = hex_value(p[3]);
          if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) return false;
          int code = (h0 << 12) | (h1 << 8) | (h2 << 4) | h3;
          c = (unsigned char)(code <= 0x7f ? code : '?'); p += 4;
          break;
        }
        default: return false;
      }
    }
    if (len + 1 < size) out[len++] = (char)c;
  }
  if (*p != '"') return false;
  out[len] = '\0';
  return true;
}

bool vibe_task_read(int desk_id, vibe_task_t *task) {
  if (!task) return false;
  memset(task, 0, sizeof(*task)); task->desk_id = desk_id;
  char path[128];
  if (!task_path(path, sizeof(path), desk_id, NULL)) return false;
  FILE *file = fopen(path, "rb");
  if (!file) return errno == ENOENT;
  char *json = malloc(VIBE_TASK_FILE_MAX);
  if (!json) { fclose(file); return false; }
  size_t len = fread(json, 1, VIBE_TASK_FILE_MAX - 1, file);
  bool complete = feof(file) && !ferror(file);
  fclose(file); json[len] = '\0';
  int file_desk_id = 0;
  char status[32];
  bool ok = complete && json_read_int(json, "desk_id", &file_desk_id) &&
            file_desk_id == desk_id && json_read_string(json, "status", status, sizeof(status)) &&
            json_read_string(json, "input", task->input, sizeof(task->input));
  if (ok) {
    task->exists = true; task->status = task_status_from_name(status);
    json_read_string(json, "model", task->model, sizeof(task->model));
    if (task->status == VIBE_TASK_DONE || task->status == VIBE_TASK_ERROR)
      ok = json_read_string(json, "output", task->output, sizeof(task->output));
  }
  free(json);
  if (!ok) {
    memset(task, 0, sizeof(*task)); task->exists = true; task->desk_id = desk_id;
    task->status = VIBE_TASK_ERROR;
    snprintf(task->output, sizeof(task->output), "Task file is invalid or incomplete.");
  }
  return ok;
}

void vibe_task_recover_stale(int desk_id) {
  vibe_task_t task;
  vibe_task_read(desk_id, &task);
  if (task.status == VIBE_TASK_PENDING || task.status == VIBE_TASK_BUSY)
    task_write(desk_id, VIBE_TASK_ERROR, task.model, task.input,
               "VibeOffice stopped before opencode completed.");
}

static void process_reset(vibe_process_t *process) {
  memset(process, 0, sizeof(*process));
  process->stdout_fd = process->stderr_fd = -1;
}

static void set_error(char *error, size_t size, const char *message) {
  if (error && size) { snprintf(error, size, "%s", message); error[size - 1] = '\0'; }
}

#if !defined(_WIN32)
static void fd_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void drain_fd(int *fd, char *buf, size_t *len) {
  if (*fd < 0) return;
  char discard[4096];
  while (true) {
    size_t room = *len < VIBE_TASK_OUTPUT_MAX ? VIBE_TASK_OUTPUT_MAX - *len : 0;
    void *dst = room ? (void *)(buf + *len) : (void *)discard;
    size_t amount = room ? room : sizeof(discard);
    ssize_t n = read(*fd, dst, amount);
    if (n > 0) { if (room) *len += (size_t)n; continue; }
    if (n == 0) { close(*fd); *fd = -1; }
    break;
  }
  buf[*len] = '\0';
}

static void process_close_fds(vibe_process_t *process) {
  if (process->stdout_fd >= 0) close(process->stdout_fd);
  if (process->stderr_fd >= 0) close(process->stderr_fd);
  process->stdout_fd = process->stderr_fd = -1;
}
#endif

bool vibe_task_submit(vibe_process_t *process, int desk_id, const char *model, const char *input,
                      char *error, size_t error_size) {
  if (!process || !model || !*model || !input || !*input || process->pid > 0) {
    set_error(error, error_size, "This desk cannot accept that task."); return false;
  }
  if (strlen(input) > VIBE_TASK_INPUT_MAX) {
    set_error(error, error_size, "The message is too long."); return false;
  }
  if (strlen(model) > VIBE_TASK_MODEL_MAX) {
    set_error(error, error_size, "The model ID is too long."); return false;
  }
  if (!task_write(desk_id, VIBE_TASK_PENDING, model, input, NULL)) {
    set_error(error, error_size, "Could not write the pending task file."); return false;
  }
#if defined(_WIN32)
  task_write(desk_id, VIBE_TASK_ERROR, model, input, "opencode spawning is not supported on Windows yet.");
  set_error(error, error_size, "opencode spawning is not supported on Windows yet.");
  return false;
#else
  int out_pipe[2] = {-1, -1}, err_pipe[2] = {-1, -1};
  if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
    if (out_pipe[0] >= 0) { close(out_pipe[0]); close(out_pipe[1]); }
    if (err_pipe[0] >= 0) { close(err_pipe[0]); close(err_pipe[1]); }
    task_write(desk_id, VIBE_TASK_ERROR, model, input, strerror(errno));
    set_error(error, error_size, strerror(errno)); return false;
  }
  pid_t pid = fork();
  if (pid == 0) {
    dup2(out_pipe[1], STDOUT_FILENO); dup2(err_pipe[1], STDERR_FILENO);
    close(out_pipe[0]); close(out_pipe[1]); close(err_pipe[0]); close(err_pipe[1]);
    execlp("opencode", "opencode", "run", "--model", model, input, (char *)NULL);
    fprintf(stderr, "opencode: %s\n", strerror(errno)); _exit(127);
  }
  close(out_pipe[1]); close(err_pipe[1]);
  if (pid < 0) {
    close(out_pipe[0]); close(err_pipe[0]);
    task_write(desk_id, VIBE_TASK_ERROR, model, input, strerror(errno));
    set_error(error, error_size, strerror(errno)); return false;
  }
  process_reset(process); process->desk_id = desk_id; process->pid = (int)pid;
  process->stdout_fd = out_pipe[0]; process->stderr_fd = err_pipe[0];
  fd_nonblocking(process->stdout_fd); fd_nonblocking(process->stderr_fd);
  if (!task_write(desk_id, VIBE_TASK_BUSY, model, input, NULL)) {
    kill(pid, SIGTERM); waitpid(pid, NULL, 0); process_close_fds(process); process_reset(process);
    task_write(desk_id, VIBE_TASK_ERROR, model, input, "Could not write busy state.");
    set_error(error, error_size, "Could not write busy state."); return false;
  }
  return true;
#endif
}

bool vibe_task_poll(vibe_process_t *process) {
#if defined(_WIN32)
  (void)process; return false;
#else
  if (!process || process->pid <= 0) return false;
  drain_fd(&process->stdout_fd, process->stdout_buf, &process->stdout_len);
  drain_fd(&process->stderr_fd, process->stderr_buf, &process->stderr_len);
  int status = 0;
  pid_t result = waitpid((pid_t)process->pid, &status, WNOHANG);
  if (result == 0) return false;
  if (result < 0) {
    snprintf(process->stderr_buf, sizeof(process->stderr_buf), "waitpid: %s", strerror(errno));
    process->stderr_len = strlen(process->stderr_buf);
  } else {
    drain_fd(&process->stdout_fd, process->stdout_buf, &process->stdout_len);
    drain_fd(&process->stderr_fd, process->stderr_buf, &process->stderr_len);
  }
  process_close_fds(process);
  vibe_task_t task;
  vibe_task_read(process->desk_id, &task);
  bool success = result > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  if (!success && process->stderr_len == 0) {
    if (result > 0 && WIFSIGNALED(status))
      snprintf(process->stderr_buf, sizeof(process->stderr_buf), "opencode terminated by signal %d.", WTERMSIG(status));
    else if (result > 0 && WIFEXITED(status))
      snprintf(process->stderr_buf, sizeof(process->stderr_buf), "opencode exited with status %d.", WEXITSTATUS(status));
    process->stderr_len = strlen(process->stderr_buf);
  }
  task_write(process->desk_id, success ? VIBE_TASK_DONE : VIBE_TASK_ERROR,
             task.model, task.input, success ? process->stdout_buf :
             (process->stderr_len ? process->stderr_buf : process->stdout_buf));
  process_reset(process);
  return true;
#endif
}

void vibe_task_abort(vibe_process_t *process, const char *reason) {
#if defined(_WIN32)
  (void)process; (void)reason;
#else
  if (!process || process->pid <= 0) return;
  pid_t pid = (pid_t)process->pid;
  kill(pid, SIGTERM);
  bool stopped = false;
  for (int i = 0; i < 50; i++) {
    if (waitpid(pid, NULL, WNOHANG) != 0) { stopped = true; break; }
    usleep(10000);
  }
  if (!stopped) { kill(pid, SIGKILL); waitpid(pid, NULL, 0); }
  process_close_fds(process);
  vibe_task_t task;
  vibe_task_read(process->desk_id, &task);
  task_write(process->desk_id, VIBE_TASK_ERROR, task.model, task.input,
             reason ? reason : "opencode was stopped.");
  process_reset(process);
#endif
}
