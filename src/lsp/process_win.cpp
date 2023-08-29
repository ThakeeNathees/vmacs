
#ifdef _WIN32

#include "process.hpp"

#include <thread>
#include <atomic>
#include <Windows.h>


class ProcessWindows : public Process {
public:
  ~ProcessWindows();

  void Config(
    std::string_view cmd,
    std::string_view cwd,
    const CallbackOutFn& cb_stdout,
    const CallbackOutFn& cb_stderr,
    int thread_sleep_timer
  ) override;

  bool Run() override;

  bool IsRunning() const override { return is_running.load(std::memory_order_acquire); }

  bool ReadStdout(std::string& ret) override { return _ReadOut(true, ret); }
  bool ReadStderr(std::string& ret) override { return _ReadOut(false, ret); }

  bool WriteStdin(const std::string& message) override;

private:
  bool _ReadOut(bool is_stdout, std::string& ret);

private:

  std::string cmd, cwd;
  CallbackOutFn cb_stdout = NULL, cb_stderr = NULL;
  int thread_sleep_timer = 0;

  std::atomic<bool> is_running = false;

  std::thread out_listener;

  HANDLE pipe_in_w = NULL, pipe_in_r = NULL;
  HANDLE pipe_out_w = NULL, pipe_out_r = NULL;
  HANDLE pipe_err_w = NULL, pipe_err_r = NULL;

  STARTUPINFO si = { 0 };
  PROCESS_INFORMATION pi = { 0 };
};


std::unique_ptr<Process> Process::New() {
  return std::make_unique<ProcessWindows>();
}


void ProcessWindows::Config(
  std::string_view cmd,
  std::string_view cwd,
  const CallbackOutFn& cb_stdout,
  const CallbackOutFn& cb_stderr,
  int thread_sleep_timer
  ) {
  this->cmd = cmd;
  this->cwd = cwd;
  this->cb_stdout = cb_stdout;
  this->cb_stderr = cb_stderr;
  this->thread_sleep_timer = thread_sleep_timer;
}


bool ProcessWindows::Run() {

  SECURITY_ATTRIBUTES sa = { 0 };
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;

  if (!CreatePipe(&pipe_in_r, &pipe_in_w, &sa, 0)) {
    // TODO: error("Pipe creation failed", GetLastError());
    // cleanup();
    return false;
  }

  if (!CreatePipe(&pipe_out_r, &pipe_out_w, &sa, 0)) {
    // TODO: error("Pipe creation failed", GetLastError());
    // cleanup();
    return false;
  }

  if (!CreatePipe(&pipe_err_r, &pipe_err_w, &sa, 0)) {
    // TODO: error("Pipe creation failed", GetLastError());
    // cleanup();
    return false;
  }

  si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
  si.wShowWindow = SW_HIDE;
  si.hStdInput = pipe_in_r;
  si.hStdOutput = pipe_out_w;
  si.hStdError = pipe_err_w;

  const char* _cwd = (cwd.length() != 0) ? cwd.c_str() : NULL;

  is_running = (bool) CreateProcessA(
    NULL,        // Application name.
    cmd.data(),  // Command line arguments.
    NULL,        // Process attributes.
    NULL,        // Thread attributes.
    TRUE,        // Inherit handles.
    0,           // Creation flags.
    NULL,        // Environment.
    _cwd,        // Current working directory.
    &si,         // Startup info.
    &pi);        // Process information.

  // Start the thread to listen stdout/stderr.
  if (cb_stdout != nullptr || cb_stderr != nullptr) {
    out_listener = std::thread([=](ProcessWindows* cp) {
      int sleep_time = cp->thread_sleep_timer;
      std::string data;
      while (cp->IsRunning()) {
        if (sleep_time > 0) Sleep(sleep_time);
        if (cb_stdout != nullptr && cp->ReadStdout(data)) cb_stdout(data);
        if (cb_stderr != nullptr && cp->ReadStderr(data)) cb_stderr(data);
      }
    }, this);
  }

  return is_running;
}


bool ProcessWindows::WriteStdin(const std::string& message) {
  DWORD written = 0; // TODO: What if it's written less than actual size of the message.
  return (bool) WriteFile(pipe_in_w, message.c_str(), (DWORD) message.length(), &written, NULL);
}


ProcessWindows::~ProcessWindows() {
  is_running.store(false, std::memory_order_release);
  if (out_listener.joinable()) out_listener.join();
}


bool ProcessWindows::_ReadOut(bool is_stdout, std::string& ret) {

  HANDLE pipe = (is_stdout) ? pipe_out_r : pipe_err_r;

  DWORD available = 0;
  PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL);
  if (available == 0) return false;

  BYTE buffer[2048];
  DWORD read = 0;

  ret.clear();
  do {
    if (!ReadFile(pipe, buffer, sizeof buffer, &read, NULL)) {
      // TODO: error("Reading process stdout failed.");
      return false;
    }
    if (!read) break;
    ret.insert(ret.size(), (const char*)buffer, read);
  } while (read >= sizeof(buffer));

  return true;
}

#endif // _WIN32
