#ifdef _WIN32

#include <stdint.h>
#include <Windows.h>

#include "platform.hpp"


uint64_t Platform::GetPid() {
  return (uint64_t) GetCurrentProcessId();
}


char Platform::GetPathSeparator() {
  return ';';
}


class IpcWindows : public IPC {

public:
  IpcWindows(IpcOptions opt) : IPC(opt) {
  }


  ~IpcWindows() {
  }


  // Note that this method isn't supposed to be called multiple times.
  void Run() override {
  }


  void StopListening() override {
  }


  void WriteToStdin(const std::string& data) override {
  }


private:

};


std::unique_ptr<IPC> IPC::New(IpcOptions opt) {
  return std::make_unique<IpcWindows>(opt);
}


#endif
