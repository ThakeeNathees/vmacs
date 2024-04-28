//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#if defined(__unix__) || defined(__APPLE__)
#pragma once

#include "platform.hpp"

#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

// 2 kilo bytes will be read from child's stdout each iteration.
#define READ_BUFF_SIZE 2048


// Callback function to send stdin for the child process, where the fd will be
// stdin for the child process. This should return the output of the write call.
// The additional parameter `pending` should be set to true if we're not done
// with writing to the child process' stdin.
//
// A possible implementation:
//
// int write_to_child_stdin(void* user_data, int fd, bool* pending) {
//   return write(fd, buff, buff_len);
// }
//
// ***************************************************************
// WARNING: in the above write call, the `buff_len` should be > 0.
// ***************************************************************
//
typedef int (*cb_stdin_f)(void* user_data, int fd, bool* pending);

// Callback function to "flush" the stdout pipe of the child process.
typedef void (*cb_stdout_f)(void* user_data, const char* buff, size_t length);

// Callback function will be called once the child process is terminated.
// exit type will be either 0 = success or 1 = failure or 2 = timeout.
typedef void (*cb_exit_f)(void* user_data, int exit_type, int status_code);


typedef struct {
  void*        user_data;
  const char*  file;
  char* const* argv;
  int          timeout_sec; // Set -1 for no timeout.

  // Callbacks.
  cb_stdin_f  cb_stdin;
  cb_stdout_f cb_stdout;
  cb_stdout_f cb_stderr;
  cb_exit_f   cb_exit;
} exec_options_t;


typedef enum {
  EXIT_TYPE_SUCCESS = 0,
  EXIT_TYPE_FAILURE = 1,
  EXIT_TYPE_TIMEOUT = 2,
} exec_exit_type;

// By convention argv[0] should be the same as file.
bool SpawnProcess(
    const char* file,   // The executable file we'll be executing.
    char* const argv[], // Arguments (last elements *MUST* be NULL).
    pid_t* pid,         // Return value process id.
    int* fdwrite_in,    // Return file descriptor for stdin.
    int* fdread_out,    // Return file descriptor for stdout.
    int* fdread_err);   // Return file descriptor for stderr.

bool ShellExec(exec_options_t opt, pid_t* pid);

#endif // defined(__unix__) || defined(__APPLE__)
