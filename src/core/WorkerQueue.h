#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

class WorkerQueue {
public:
    WorkerQueue() : thread_([this] { Loop(); }) {}

    ~WorkerQueue() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        condition_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    WorkerQueue(const WorkerQueue&) = delete;
    WorkerQueue& operator=(const WorkerQueue&) = delete;

    void Submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push_back(std::move(task));
        }
        condition_.notify_one();
    }

    std::size_t Pending() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

private:
    void Loop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] { return !tasks_.empty() || stopped_; });
                if (tasks_.empty()) {
                    if (stopped_) {
                        return;
                    }
                    continue;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            task();
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::function<void()>> tasks_;
    bool stopped_ = false;
    std::thread thread_;
};
