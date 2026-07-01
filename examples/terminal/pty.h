// PTY (pseudo-terminal) abstraction layer.
// Unix: forkpty()
// Windows: ConPTY ( stubs for now)

#ifndef __VGAT_PTY_H__
#define __VGAT_PTY_H__

#include <stdbool.h>

// Open a new PTY and fork a shell process.
// shell: path to shell binary (NULL = $SHELL or /bin/sh)
// rows, cols: initial terminal size
// pid_out: set to child process ID on success
// Returns: master fd on success, -1 on failure
int vgat_pty_open(const char *shell, int rows, int cols, int *pid_out);

// Open a new PTY and exec an arbitrary program.
// argv: NULL-terminated argument vector (argv[0] = program path)
// rows, cols: initial terminal size
// pid_out: set to child process ID on success
// Returns: master fd on success, -1 on failure
int vgat_pty_exec(const char *const *argv, int rows, int cols, int *pid_out);

// Close the PTY and terminate the child process.
void vgat_pty_close(int pid);

// Resize the terminal (signal child via TIOCSWINSZ on Unix).
bool vgat_pty_resize(int master_fd, int rows, int cols);

// Read bytes from the PTY master (non-blocking).
// Returns: number of bytes read, 0 if nothing available, -1 on error.
int vgat_pty_read(int master_fd, void *buf, int sz);

// Write bytes to the PTY master (to send to the shell).
// Returns: number of bytes written, -1 on error.
int vgat_pty_write(int master_fd, const void *buf, int sz);

#endif // __VGAT_PTY_H__
