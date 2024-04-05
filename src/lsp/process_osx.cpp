// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2023 Thakee Nathees
// Licenced under: MIT

#ifdef __APPLE__

#include "process.hpp"


class ProcessOSX : public Process {
public:
  ~ProcessOSX();

  void Config(
    std::string_view cmd,
    std::string_view cwd,
    const CallbackOutFn& cb_stdout,
    const CallbackOutFn& cb_stderr,
    int thread_sleep_timer
  ) override;

  bool Run() override;

  bool IsRunning() const override { }

  bool ReadStdout(std::string& ret) override { }
  bool ReadStderr(std::string& ret) override { }

  bool WriteStdin(const std::string& message) override;


private:

};


std::unique_ptr<Process> Process::New() {
  return std::make_unique<ProcessOSX>();
}


void ProcessOSX::Config(
  std::string_view cmd,
  std::string_view cwd,
  const CallbackOutFn& cb_stdout,
  const CallbackOutFn& cb_stderr,
  int thread_sleep_timer
  ) {
  TODO;
}


bool ProcessOSX::Run() {
  TODO;
}


bool ProcessOSX::WriteStdin(const std::string& message) {
  TODO;
}


ProcessOSX::~ProcessOSX() {
  TODO;
}


#endif // __APPLE__
