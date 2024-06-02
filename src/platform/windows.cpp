#ifdef _WIN32

#include <stdint.h>
#include <Windows.h>

#include "platform.hpp"

// Read and write end of a pipe.
#define PE_READ  0
#define PE_WRITE 1

// 4 kilo bytes will be read from child's stdout each iteration.
#define READ_BUFF_SIZE (4*1024)


uint64_t Platform::GetPid() {
  return (uint64_t) GetCurrentProcessId();
}


char Platform::GetPathSeparator() {
  return ';';
}


// https://stackoverflow.com/a/17387176/10846399
// Returns the last Win32 error, in string format. Returns an empty string if
// there is no error.
std::string ErrorAsString(DWORD errorMessageID) {

  if (errorMessageID == 0) {
    return std::string(); // No error message has been recorded.
  }

  LPSTR messageBuffer = nullptr;

  // Ask Win32 to give us the string version of that message ID. The parameters
  // we pass in, tell Win32 to create the buffer that holds the message for us
  // (because we don't yet know how long the message string will be).
  size_t size = FormatMessageA(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

  // Copy the error message into a std::string.
  std::string message(messageBuffer, size);

  // Free the Win32's string's buffer.
  LocalFree(messageBuffer);

  return message;
}


// Returns the win32 error code on failure, or 0 on success. Use ErrorAsString
// to get the string version of the returned error. Parameters p_std* will be
// set as the pipe handle for standard io which needs to be closed by the caller.
static DWORD SpawnProcess(char* command,
                   PROCESS_INFORMATION* pi,
                   HANDLE* p_stdin, HANDLE* p_stdout, HANDLE* p_stderr) {

  HANDLE pipe_stdin[2];  // Pipe to write to child's  stdin.
  HANDLE pipe_stdout[2]; // Pipe to read from child's stdout.
  HANDLE pipe_stderr[2]; // Pipe to read from child's stderr.

  SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };;

  // If creation of a pipe failed, do I have to close the previously opened
  // pipe before returning here?

  if (!CreatePipe(&pipe_stdin[PE_READ], &pipe_stdin[PE_WRITE], &sa, 0)) {
    return GetLastError();
  }

  if (!CreatePipe(&pipe_stdout[PE_READ], &pipe_stdout[PE_WRITE], &sa, 0)) {
    return GetLastError();
  }

  if (!CreatePipe(&pipe_stderr[PE_READ], &pipe_stderr[PE_WRITE], &sa, 0)) {
    return GetLastError();
  }

  STARTUPINFO si = { 0 };
  *pi = { 0 };

  si.dwFlags = STARTF_USESTDHANDLES;
  si.wShowWindow = SW_HIDE;
  si.hStdInput = pipe_stdin[PE_READ];
  si.hStdOutput = pipe_stdout[PE_WRITE];
  si.hStdError = pipe_stderr[PE_WRITE];

  if (!CreateProcess(
    NULL,    // Application name.
    command, // Command line arguments.
    NULL,    // Process attributes.
    NULL,    // Thread attributes.
    TRUE,    // Inherit handles.
    0,       // Creation flags.
    NULL,    // Environment.
    NULL,    // Current working directory.
    &si,     // Startup info.
    pi))     // Process information.
  {
    return GetLastError();
  }


  // Close handles to the stdin and stdout pipes no longer needed by the
  // child process. If they are not explicitly closed, there is no way to
  // recognize that the child process has ended.

  CloseHandle(pipe_stdin[PE_READ]);
  CloseHandle(pipe_stdout[PE_WRITE]);
  CloseHandle(pipe_stderr[PE_WRITE]);

  if (p_stdin) *p_stdin = pipe_stdin[PE_WRITE];
  if (p_stdout) *p_stdout = pipe_stdout[PE_READ];
  if (p_stderr) *p_stderr = pipe_stderr[PE_READ];

  return 0;
}

//------------------------------------------------------------------------------
// Inter Process Communication
//------------------------------------------------------------------------------

class IpcWindows : public IPC {

public:
  IpcWindows(IpcOptions opt) : IPC(opt) {

    // On windows it's just a single string of command.
    command += opt.file;
    for (const std::string& arg : opt.argv) {
      command += " " + arg;
    }

    pending_inputs = opt.sending_inputs;
  }


  ~IpcWindows() {
    loop_stop = true;
    pending_inputs = false; // Done with sending inputs.
    if (process_thread.joinable()) {
      process_thread.join();
    }
  }


  // Note that this method isn't supposed to be called multiple times.
  void Run() override {

    process_thread = std::thread([this]() {
      DWORD err = CommandExecute(this, loop_stop);
      // TODO: Handle if error returned.
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

  std::string command;

  // Buffer all the write to stdin request in this queue and we dispatch them
  // in the stdin callback.
  ThreadSafeQueue<std::string> queue_to_stdin;
  std::thread process_thread;

  std::atomic<bool> pending_inputs = false;
  std::atomic<bool> loop_stop = false;

private:

  static DWORD CommandExecute(IpcWindows* self, std::atomic<bool>& loop_stop) {

    DWORD rv = 0;

    PROCESS_INFORMATION pi;
    HANDLE pipe_stdin, pipe_stdout, pipe_stderr;

    rv = SpawnProcess(self->command.data(), &pi, &pipe_stdin, &pipe_stdout, &pipe_stderr);
    if (rv != 0) return rv;

    while (true) {
      if (loop_stop.load()) break;

      // Write to the child process's stdin.
      if (pipe_stdin != NULL) {

        while (!self->queue_to_stdin.Empty()) {
          std::string data = self->queue_to_stdin.Dequeue();

          // TODO: Handle if written is less than data.size().
          if (data.size() > 0) {
            DWORD written;
            if (!WriteFile(pipe_stdin, data.data(), (DWORD)data.size(), &written, NULL)) {
              rv = GetLastError();
              goto l_loop_end;
            }
          }
        }

        // Check if we have more inputs oterwise close the pipe to send EOF to
        // the child process's stdin.
        if (!self->pending_inputs && self->queue_to_stdin.Empty()) {
          CloseHandle(pipe_stdin);
          pipe_stdin = NULL;
        }
      }

      // Read from the child process's stdout, stderr.
      HANDLE pipes[2] = { pipe_stdout, pipe_stderr };

      typedef void (*callback_internal_t)(void*, const char*, size_t);
      callback_internal_t cbs[2] = { IpcWindows::StdoutCallback, IpcWindows::StderrCallback };

      for (int i = 0; i < 2; i++) { // Iterate twise for stdout and stderr.
        bool is_stdout = (i == 0);

        HANDLE pipe = pipes[i];
        callback_internal_t cb = cbs[i];

        if (cb == NULL) continue;

        DWORD available;
        if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL)) {
          rv = GetLastError();
          if (rv == ERROR_BROKEN_PIPE) rv = 0; // The pipe has closed by the child.
          goto l_loop_end;
        }
        if (available <= 0) break;

        // The buffer where we'll be reading the stdout and stderr to.
        char buff[READ_BUFF_SIZE];

        DWORD read;
        if (!ReadFile(pipe, buff, READ_BUFF_SIZE - 1, &read, NULL)) {
          rv = GetLastError();
          if (rv == ERROR_BROKEN_PIPE) rv = 0; // The pipe has closed by the child.
          goto l_loop_end;
        }

        if (read >= 0) {
          buff[read] = '\0';
          cb(self, buff, (size_t)read);
        } else {
          goto l_loop_end;
        }

      }

    }

l_loop_end:
    if (pipe_stdin) CloseHandle(pipe_stdin);
    if (pipe_stdout) CloseHandle(pipe_stdout);
    if (pipe_stderr) CloseHandle(pipe_stderr);

    // WaitForSingleObject(pi.hProcess, INFINITE); // Waits for the process to end.
    TerminateProcess(pi.hProcess, 1); // Kill the process.

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return rv;
  }

private:
  static void StdoutCallback(void* user_data, const char* buff, size_t length) {
    IpcWindows* self = static_cast<IpcWindows*>(user_data);
    FuncStdoutCallback cb = self->opt.stdout_cb;
    if (cb) cb(self->opt.user_data, buff, length);
  }


  static void StderrCallback(void* user_data, const char* buff, size_t length) {
    IpcWindows* self = static_cast<IpcWindows*>(user_data);
    FuncStdoutCallback cb = self->opt.stderr_cb;
    if (cb) cb(self->opt.user_data, buff, length);
  }


  static void ExitCallback(void* user_data, int exit_type, int exit_code) {
    IpcWindows* self = static_cast<IpcWindows*>(user_data);
    FuncExitCallback cb = self->opt.exit_cb;
    if (cb) cb(self->opt.user_data, exit_code);
  }

};


std::unique_ptr<IPC> IPC::New(IpcOptions opt) {
  return std::make_unique<IpcWindows>(opt);
}


#endif
