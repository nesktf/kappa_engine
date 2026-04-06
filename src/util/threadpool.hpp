#pragma once

#include <future>
#include <memory>
#include <queue>
#include <vector>

namespace kappa {

class thread_pool {
private:
  struct task_interface {
    virtual ~task_interface() = default;
    virtual void do_task() = 0;
  };

  template<typename T>
  struct promise_task_type : public task_interface {
    using promise_t = std::promise<std::invoke_result_t<T>>;

    template<typename... Args>
    promise_task_type(promise_t&& promise_, Args&&... args) :
        task(std::forward<Args>(args)...), promise(std::move(promise_)) {}

    void do_task() noexcept override {
      try {
        if constexpr (std::is_void_v<std::invoke_result_t<T>>) {
          task();
          promise.set_value();
        } else {
          promise.set_value(task());
        }
      } catch (...) {
        try {
          promise.set_exception(std::current_exception());
        } catch (...) {
        }
      }
    }

    T task;
    promise_t promise;
  };

public:
  template<typename T>
  using task_promise = std::promise<std::invoke_result_t<std::remove_cvref_t<T>>>;

  template<typename T>
  using task_future = std::future<std::invoke_result_t<std::remove_cvref_t<T>>>;

public:
  thread_pool(size_t n_threads = std::thread::hardware_concurrency()) {
    for (size_t i = 0; i < n_threads; ++i) {
      _threads.emplace_back([this]() {
        for (;;) {
          std::unique_ptr<task_interface> task;

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

  ~thread_pool() noexcept {
    {
      std::unique_lock<std::mutex> lock(_task_mtx);
      _stop = true;
    }

    _cv.notify_all();

    for (auto& thread : _threads) {
      thread.join();
    }
  }

  thread_pool(thread_pool&&) = delete;
  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(thread_pool&&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;

public:
  template<typename T>
  auto enqueue_future(T&& task) -> task_future<T> {
    task_promise<T> promise;
    auto future = promise.get_future();
    {
      std::unique_lock<std::mutex> lock(_task_mtx);
      auto bundled_task = std::make_unique<promise_task_type<std::remove_cvref_t<T>>>(
        std::move(promise), std::forward<T>(task));
      _tasks.emplace(std::move(bundled_task));
    }
    _cv.notify_one();
    return future;
  }

  template<typename T, typename... Args>
  auto enqueue_future(std::in_place_type_t<T>, Args&&... args) -> task_future<T> {
    task_promise<T> promise;
    auto future = promise.get_future();
    {
      std::unique_lock<std::mutex> lock(_task_mtx);
      auto bundled_task = std::make_unique<promise_task_type<std::remove_cvref_t<T>>>(
        std::move(promise), std::forward<Args>(args)...);
      _tasks.emplace(std::move(bundled_task));
    }
    _cv.notify_one();
    return future;
  }

private:
  bool _stop;
  std::vector<std::thread> _threads;
  std::queue<std::unique_ptr<task_interface>> _tasks;
  std::mutex _task_mtx;
  std::condition_variable _cv;
};

} // namespace kappa
