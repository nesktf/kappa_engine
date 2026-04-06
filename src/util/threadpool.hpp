#pragma once

#include "../core.hpp"

#include <future>
#include <memory>
#include <queue>
#include <vector>

namespace kappa {

class ThreadPool {
private:
  struct ITask {
    virtual ~ITask() = default;
    virtual void do_task() = 0;
  };

  template<typename T>
  struct PromiseTaskType : public ITask {
    using promise_t = std::promise<std::invoke_result_t<T>>;

    template<typename... Args>
    PromiseTaskType(promise_t&& promise_, Args&&... args) :
        task(std::forward<Args>(args)...), promise(std::move(promise_)) {}

    void do_task() noexcept override {
#ifdef __cpp_exceptions
      try {
#endif
        if constexpr (std::is_void_v<std::invoke_result_t<T>>) {
          task();
          promise.set_value();
        } else {
          promise.set_value(task());
        }
#ifdef __cpp_exceptions
      } catch (...) {
        try {
          promise.set_exception(std::current_exception());
        } catch (...) {
        }
      }
#endif
    }

    T task;
    promise_t promise;
  };

public:
  template<typename T>
  using TaskPromise = std::promise<std::invoke_result_t<std::remove_cvref_t<T>>>;

  template<typename T>
  using TaskFuture = std::future<std::invoke_result_t<std::remove_cvref_t<T>>>;

public:
  ThreadPool(size_t n_threads = std::thread::hardware_concurrency()) {
    for (size_t i = 0; i < n_threads; ++i) {
      _threads.emplace_back([this]() {
        for (;;) {
          std::unique_ptr<ITask> task;

          {
            std::unique_lock<std::mutex> lock(_task_mtx);
            _cv.wait(lock, [this]() { return !_tasks.empty() || _stop; });

            if (_stop && _tasks.empty()) {
              return;
            }

            task = std::move(_tasks.front());
            _tasks.pop();
          }

          task->do_task();
        }
      });
    }
  }

  ~ThreadPool() noexcept {
    {
      std::unique_lock<std::mutex> lock(_task_mtx);
      _stop = true;
    }

    _cv.notify_all();

    for (auto& thread : _threads) {
      thread.join();
    }
  }

  ThreadPool(ThreadPool&&) = delete;
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

public:
  template<typename T>
  fn enqueue_future(T&& task) -> TaskFuture<T> {
    TaskPromise<T> promise;
    auto future = promise.get_future();
    {
      std::unique_lock<std::mutex> lock(_task_mtx);
      auto bundled_task = std::make_unique<PromiseTaskType<std::remove_cvref_t<T>>>(
        std::move(promise), std::forward<T>(task));
      _tasks.emplace(std::move(bundled_task));
    }
    _cv.notify_one();
    return future;
  }

  template<typename T, typename... Args>
  fn enqueue_future(std::in_place_type_t<T>, Args&&... args) -> TaskFuture<T> {
    TaskPromise<T> promise;
    auto future = promise.get_future();
    {
      std::unique_lock<std::mutex> lock(_task_mtx);
      auto bundled_task = std::make_unique<PromiseTaskType<std::remove_cvref_t<T>>>(
        std::move(promise), std::forward<Args>(args)...);
      _tasks.emplace(std::move(bundled_task));
    }
    _cv.notify_one();
    return future;
  }

private:
  bool _stop;
  std::vector<std::thread> _threads;
  std::queue<std::unique_ptr<ITask>> _tasks;
  std::mutex _task_mtx;
  std::condition_variable _cv;
};

} // namespace kappa
