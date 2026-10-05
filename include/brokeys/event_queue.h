#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace bro::keys {

template <class T>
class MessageQueue {
public:
    void push(T value) {
        std::function<void()> wake;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            items_.push_back(std::move(value));
            wake = wake_;
        }
        cv_.notify_all();
        if (wake) wake();
    }

    std::vector<T> drain() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T> out;
        out.swap(items_);
        return out;
    }

    bool wait_for(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return !items_.empty(); });
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }

    void set_wake(std::function<void()> wake) {
        std::lock_guard<std::mutex> lock(mutex_);
        wake_ = std::move(wake);
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<T> items_;
    std::function<void()> wake_;
};

} // namespace bro::keys
