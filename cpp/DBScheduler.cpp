#include "DBScheduler.h"
#include <chrono>

namespace turbo_db {

DBScheduler::DBScheduler() {
    worker_ = std::thread(&DBScheduler::worker_loop, this);
}

DBScheduler::~DBScheduler() {
    shutdown();
}

void DBScheduler::schedule(std::function<void()> task, Priority priority) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_.load(std::memory_order_relaxed)) return;
        uint64_t seq = seq_counter_.fetch_add(1, std::memory_order_relaxed);
        queue_.push(Task{std::move(task), priority, seq});
        pending_.fetch_add(1, std::memory_order_relaxed);
    }
    cv_.notify_one();
}

void DBScheduler::waitUntilIdle() {
    std::unique_lock<std::mutex> lock(mutex_);
    idle_cv_.wait(lock, [this] {
        return stop_.load(std::memory_order_relaxed) || queue_.empty();
    });
}

void DBScheduler::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_.store(true, std::memory_order_relaxed);
    }
    cv_.notify_all();
    idle_cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void DBScheduler::worker_loop() {
    while (true) {
        std::function<void()> fn;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return stop_.load(std::memory_order_relaxed) || !queue_.empty();
            });

            if (stop_.load(std::memory_order_relaxed) && queue_.empty()) {
                idle_cv_.notify_all();
                return;
            }

            if (!queue_.empty()) {
                fn = std::move(const_cast<Task&>(queue_.top()).fn);
                queue_.pop();
            }
        }

        if (fn) {
            try {
                fn();
            } catch (...) {
                // Exceptions must be caught inside individual tasks and forwarded
                // to the Promise reject callback. This is a last-resort safety net.
            }
            size_t remaining = pending_.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (remaining == 0) {
                idle_cv_.notify_all();
            }
        }
    }
}

} // namespace turbo_db
