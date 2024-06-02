//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#if defined(__unix__) || defined(__APPLE__)

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "platform.hpp"

// Read and write end of a pipe.
#define PE_READ  0
#define PE_WRITE 1


// 4 kilo bytes will be read from child's stdout each iteration.
#define READ_BUFF_SIZE (4*1024)


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


// TODO(grep): Timeout in micro/milli seconds.
typedef struct {
  void* user_data;
  const char* file;
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


uint64_t Platform::GetPid() {
  return (uint64_t) getpid();
}


char Platform::GetPathSeparator() {
  return ':';
}


// Returns true on success. If the fdread is not NULL, it'll open pipe from child to parent to listen stdout of the child and
// if the fdwrite is not NULL it'll open pipe from parent to child to send message from parent's stdout.
static bool SpawnProcess(const char* file, char* const argv[], pid_t* pid, int* fdwrite_in, int* fdread_out, int* fdread_err) {

  int pipe_parent_to_child_in[2];  // Pipe to write to child's  stdin.
  int pipe_child_to_parent_out[2]; // Pipe to read from child's stdout.
  int pipe_child_to_parent_err[2]; // Pipe to read from child's stderr.

  if (fdwrite_in && pipe(pipe_parent_to_child_in) != 0) return false;
  if (fdread_out && pipe(pipe_child_to_parent_out) != 0) return false;
  if (fdread_err && pipe(pipe_child_to_parent_err) != 0) return false;

  *pid = fork();
  if (*pid == -1) return false;

  if (*pid == 0) { // Child process.

    // Close un-used pipes of this process and redirect to pipes if requested.
    if (fdread_out) {
      close(pipe_child_to_parent_out[PE_READ]);
      dup2(pipe_child_to_parent_out[PE_WRITE], STDOUT_FILENO);
      close(pipe_child_to_parent_out[PE_WRITE]);
    }

    if (fdread_err) {
      close(pipe_child_to_parent_err[PE_READ]);
      dup2(pipe_child_to_parent_err[PE_WRITE], STDERR_FILENO);
      close(pipe_child_to_parent_err[PE_WRITE]);
    }

    if (fdwrite_in) {
      close(pipe_parent_to_child_in[PE_WRITE]);
      dup2(pipe_parent_to_child_in[PE_READ], STDIN_FILENO);
      close(pipe_parent_to_child_in[PE_READ]);
    }

    execvp(file, argv);
    exit(EXIT_FAILURE); // Unreachable.
  }

  // Parent prcess.

  if (fdread_out) {
    close(pipe_child_to_parent_out[PE_WRITE]);
    *fdread_out = pipe_child_to_parent_out[PE_READ];
  }

  if (fdread_err) {
    close(pipe_child_to_parent_err[PE_WRITE]);
    *fdread_err = pipe_child_to_parent_err[PE_READ];
  }

  if (fdwrite_in) {
    close(pipe_parent_to_child_in[PE_READ]);
    *fdwrite_in = pipe_parent_to_child_in[PE_WRITE];
  }

  return true;
}


// Return false if it failed to spawn a child process.
static bool ShellExec(exec_options_t opt, pid_t* pid, std::atomic<bool>& loop_stop) {

  // Set of file descriptors waiting for listening.
  fd_set fds_waiting_for_read;
  struct timeval timeout;
  int read_count = 0;

  int fdwrite_in = -1, fdread_out = -1, fdread_err = -1;
  exec_exit_type exit_type = EXIT_TYPE_SUCCESS;

  bool reading_out = (opt.cb_stdout != NULL);
  bool reading_err = (opt.cb_stderr != NULL);
  bool writing_in  = (opt.cb_stdin  != NULL);

  if (!SpawnProcess(
        opt.file,
        opt.argv,
        pid,
        (writing_in)  ? &fdwrite_in : NULL,
        (reading_out) ? &fdread_out : NULL,
        (reading_err) ? &fdread_err : NULL
      )) {
    return false;
  }

  // Blocking IO loop.
  while (true) {

    // The buffer where we'll be reading the stdout and stderr to.
    char buff[READ_BUFF_SIZE];

    // If we're exiting don't run this anymore.
    if (loop_stop.load()) break;

    // Write to the child process. This is non blocking.
    if (fdwrite_in >= 0 && writing_in) {
      int write_count = opt.cb_stdin(opt.user_data, fdwrite_in, &writing_in);
      if (write_count < 0) {
        exit_type = EXIT_TYPE_FAILURE;
        goto l_loop_end;
      }

      // If no more data to write, close the pipe to send EOF to the child.
      if (!writing_in) {
        close(fdwrite_in);
        fdwrite_in = -1;
      }
    }

    // Read from stdout / stderr.
    int fds_out_err[2] = {fdread_out, fdread_err};
    cb_stdout_f cbs[2] = {opt.cb_stdout, opt.cb_stderr};
    for (int i = 0; i < 2; i++) {

      int fdread = fds_out_err[i];
      cb_stdout_f cb = cbs[i];
      bool is_stdout = (i == 0);

      if (cb == NULL) continue;

      // FIXME: Need to support for milli/micro second timeout since this is
      // only bound to seconds and we cannot do fraction here.
      //
      // Since select will modify the timeout struct, we have to reset it before
      // calling select each time. Event if we're listening wihtout timeout
      // (timeout < 0) we set the timout to 0.01 seconds so the stdin in the
      // above will not be blocked.
      //
      // Note that for  stderr we don't wait for too long since it'll block the
      // stdout (same as timeout = -1).
      if (opt.timeout_sec < 0 || !is_stdout) {
        timeout.tv_sec  = 0;      // Seconds.
        timeout.tv_usec = 10000;  // Microseconds.
      } else {
        timeout.tv_sec  = opt.timeout_sec; // Seconds.
        timeout.tv_usec = 0;               // Microseconds.
      }

      FD_ZERO(&fds_waiting_for_read);
      FD_SET(fdread, &fds_waiting_for_read);

      // https://stackoverflow.com/a/2918709/10846399
      int rv = select(fdread + 1, &fds_waiting_for_read, NULL, NULL, &timeout);
      if (rv < 0) {
        exit_type = EXIT_TYPE_FAILURE;
        goto l_loop_end;
      }

      if (rv == 0) { // Timed out we don't stop on stderr timeout (only for stdout).
        if (opt.timeout_sec == -1 || !is_stdout) continue; // Continue listening.
        exit_type = EXIT_TYPE_TIMEOUT;
        goto l_loop_end;
      }

      // File descriptor is ready to read immediately.
      read_count = read(fdread, buff, READ_BUFF_SIZE-1); // -1 So we'll add \0 character at the end.

      if (read_count < 0) {
        exit_type = EXIT_TYPE_FAILURE;
        goto l_loop_end;

      } else if (read_count > 0) {
        buff[read_count] = '\0';
        cb(opt.user_data, buff, read_count);
      }

      // If nothing in the stdout of the child and we're not writing anymore,
      // we're done with the process.
      if (is_stdout && read_count == 0 && !writing_in) {
        goto l_loop_end;
      }
    }

  }

l_loop_end:

  // FIXME: Note that SIGTERM should be the proper way to end the process however
  // the lsp will takes like 2 seconds at the end to terminate in the bellow waitpid
  // call and SIGKILL will takes at most 10 microseconds.  This kill method should
  // be a parameter and we should use sigkill for lsp and sigterm for other processes.
  //
  // SIGKILL: kills the process immediately without letting the child process to cleanup.
  // SIGTERM: Stop the process peacefully letting the child cleanup.
  kill(*pid, SIGKILL);

  // Wait for the child process to finish it's thing and cleanup resources.
  int status;
  if (waitpid(*pid, &status, 0) == -1) {
    // TODO: Check errno and report / log.
  } else if (opt.cb_exit) {
    opt.cb_exit(opt.user_data, exit_type, WEXITSTATUS(status));
  }

  // Now that the child is finished we can safely close all the pipes.
  if (fdwrite_in >= 0) close(fdwrite_in);
  if (fdread_out >= 0) close(fdread_out);
  if (fdread_err >= 0) close(fdread_err);

  return true;
}

//------------------------------------------------------------------------------
// Inter Process Communication
//------------------------------------------------------------------------------

class IpcPosix : public IPC {

public:
  IpcPosix(IpcOptions opt) : IPC(opt) {

    // Construct argv.
    int argv_size = 2 + this->opt.argv.size();
    argv = (char**) malloc(sizeof(char*) * argv_size);
    argv[0] = this->opt.file.data(); // Convention.
    for (int i = 0; i < this->opt.argv.size(); i++) {
      argv[i+1] = this->opt.argv[i].data();
    }
    argv[argv_size-1] = NULL;

    // FIXME(grep): We have tow options struct that does the same thing and a
    // callback calls another callback. These two needs to be merged.
    memset(&execopt, 0, sizeof(execopt));
    execopt.user_data   = this;
    execopt.file        = this->opt.file.c_str();
    execopt.argv        = argv;
    execopt.timeout_sec = this->opt.timeout_sec;
    execopt.cb_stdin    = (opt.sending_inputs) ? IpcPosix::StdinCallback : NULL;
    execopt.cb_stdout   = IpcPosix::StdoutCallback;
    execopt.cb_stderr   = IpcPosix::StderrCallback;
    execopt.cb_exit     = IpcPosix::ExitCallback;

    pending_inputs = opt.sending_inputs;
  }


  ~IpcPosix() {
    if (argv != nullptr) free(argv);
    loop_stop = true;
    pending_inputs = false;
    if (server_io_thread.joinable()) {
      server_io_thread.join();
    }
  }


  // Note that this method isn't supposed to be called multiple times.
  void Run() override {
    server_io_thread = std::thread([this] () {
      ShellExec(execopt, &pid, loop_stop);
    });
  }


  void StopListening() override {
    pending_inputs = false;
  }


  void WriteToStdin(const std::string& data) override {
    ASSERT(opt.sending_inputs, "Trying to send stdin to child process\n"
        "Did you forget to set opt.sending_inputs to true?");
    queue_to_stdin.Enqueue(data);
  }


private:
  // Buffer all the write to stdin request in this queue and we dispatch them
  // in the stdin callback.
  ThreadSafeQueue<std::string> queue_to_stdin;

  // This manually needs to be cleaned up as it will be mallocd.
  char** argv = nullptr;

  exec_options_t execopt;
  pid_t pid;

  std::atomic<bool> pending_inputs = false;
  std::atomic<bool> loop_stop = false;
  std::thread server_io_thread;

private:
  static int StdinCallback(void* user_data, int fd, bool* pending) {
    IpcPosix* self = static_cast<IpcPosix*>(user_data);

    int count = 0;
    while (!self->queue_to_stdin.Empty()) {
      std::string data = self->queue_to_stdin.Dequeue();

      // TODO: Handle if count is != data.size();
      if (data.size() > 0) {
        int wc = write(fd, data.c_str(), data.size());
        if (wc < 0) { // Something went wrong.
          *pending = false;
          return wc;
        }
        count += wc;
      }
    }

    *pending = (self->pending_inputs || !self->queue_to_stdin.Empty());
    return count;
  }


  static void StdoutCallback(void* user_data, const char* buff, size_t length) {
    IpcPosix* self = static_cast<IpcPosix*>(user_data);
    FuncStdoutCallback cb = self->opt.stdout_cb;
    if (cb) cb(self->opt.user_data, buff, length);
  }


  static void StderrCallback(void* user_data, const char* buff, size_t length) {
    IpcPosix* self = static_cast<IpcPosix*>(user_data);
    FuncStdoutCallback cb = self->opt.stderr_cb;
    if (cb) cb(self->opt.user_data, buff, length);
  }


  static void ExitCallback(void* user_data, int exit_type, int exit_code) {
    IpcPosix* self = static_cast<IpcPosix*>(user_data);
    FuncExitCallback cb = self->opt.exit_cb;
    if (cb) cb(self->opt.user_data, exit_code);
  }

};


std::unique_ptr<IPC> IPC::New(IpcOptions opt) {
  return std::make_unique<IpcPosix>(opt);
}


#endif // defined(__unix__) || defined(__APPLE__)
