#pragma once

//single-producer, single-consumer lockless queue
//includes await functions for spin-loops

namespace nall {

template<typename T> struct queue_spsc;

template<typename T, u32 Size>
struct queue_spsc<T[Size]> {
  auto flush() -> void {
    _read  = 0;
    _write = 2 * Size;
  }

  auto size() const -> u32 {
    return (_write - _read) % (2 * Size);
  }

  auto empty() const -> bool {
    return size() == 0;
  }

  auto full() const -> bool {
    return size() == Size;
  }

  auto read() -> maybe<T> {
    if(empty()) return nothing;
    auto value = _data[_read % Size];
    _read = _read + 1 < 2 * Size ? _read + 1 : 0;
    return value;
  }

  auto write(const T& value) -> bool {
    if(full()) return false;
    _data[_write % Size] = value;
    _write = _write + 1 < 4 * Size ? _write + 1 : 2 * Size;
    return true;
  }

  auto await_empty() -> void {
    while(!empty()) spinloop();
  }

  auto await_read() -> T {
    T value;
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _notEmpty.wait(lock, [&] { return !empty(); });
      value = _data[_read % Size];
    }
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _read = _read + 1 < 2 * Size ? _read + 1 : 0;
    }
    _notFull.notify_one();
    return value;
  }

  auto await_write(const T& value) -> void {
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _notFull.wait(lock, [&] { return !full(); });
      _data[_write % Size] = value;
    }
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _write = _write + 1 < 4 * Size ? _write + 1 : 2 * Size;
    }
    _notEmpty.notify_one();
  }

private:
  T _data[Size];
  std::atomic<u32> _read  = 0;
  std::atomic<u32> _write = 2 * Size;
  std::mutex _mutex;
  std::condition_variable _notEmpty;
  std::condition_variable _notFull;
};

}
