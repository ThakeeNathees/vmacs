//
// .--.--.--------.-----.----.-----. vmacs text editor and more.
// |  |  |        |  _  |  __|__ --| version 0.1.0
//  \___/|__|__|__|__.__|____|_____| https://github.com/thakeenathees/vmacs
//
// Copyright (c) 2024 Thakee Nathees
// Licenced under: MIT

#pragma once

#include <thread>
#include <queue>


template <class T>
class ThreadSafeQueue {

public:
  void Enqueue(const T& t) {
    std::lock_guard<std::mutex>lock(mutex);
    queue.push(t);
    cond.notify_one();
  }


  T Dequeue() {
    std::unique_lock<std::mutex> lock(mutex);
    while (queue.empty()) {
      cond.wait(lock);
    }
    T value = queue.front();
    queue.pop();
    return value;
  }


  bool Empty() const {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.empty();
  }


private:
  std::queue<T> queue;
  mutable std::mutex mutex;
  std::condition_variable cond;
};


