#pragma once

#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <stdexcept>

namespace turbo_db {

/**
 * DBScheduler — serialized single-threaded background worker.
 *
 * All DB operations (reads, writes, compaction) are executed on the DBWorker
 * thread, never on the JS thread. Completion callbacks are posted back via
 * js_invoker_->invokeAsync() so Promise resolution happens on the JS thread.
 *
 * Priority lanes (higher priority executes first):
 *   COMPACTION < WRITE < READ
 */
class DBScheduler {
public:
    enum class Priority : uint8_t {
        COMPACTION = 0,
        WRITE      = 1,
        READ       = 2,
    };

    explicit DBScheduler();
    ~DBScheduler();

    DBScheduler(const DBScheduler&) = delete;
    DBScheduler& operator=(const DBScheduler&) = delete;

    /**
     * Schedule a task on the background DB thread.
     * @param task     The work to execute on the DB thread (must NOT touch jsi::Runtime).
     * @param priority Higher priority tasks jump ahead of lower priority ones.
     */
    void schedule(std::function<void()> task, Priority priority = Priority::WRITE);

    /**
     * Block the calling thread until all pending tasks are drained.
     * Uses condition_variable — not a spin wait.
     */
    void waitUntilIdle();

    /**
     * Request shutdown. Finishes in-flight task, then exits.
     */
    void shutdown();

    bool isRunning() const { return !stop_.load(std::memory_order_relaxed); }

private:
    struct Task {
        std::function<void()> fn;
        Priority priority;
        uint64_t seq; // FIFO ordering within same priority

        bool operator<(const Task& other) const {
            if (priority != other.priority)
                return static_cast<uint8_t>(priority) < static_cast<uint8_t>(other.priority);
            return seq > other.seq; // lower seq = older = higher prio within same level
        }
    };

    void worker_loop();

    std::priority_queue<Task> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable idle_cv_;
    std::atomic<bool> stop_{false};
    std::atomic<size_t> pending_{0};
    std::atomic<uint64_t> seq_counter_{0};
    std::thread worker_;
};

} // namespace turbo_db
