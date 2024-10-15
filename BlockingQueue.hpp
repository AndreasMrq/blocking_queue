#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

template<typename T>
class BlockingQueue
{
public:
    void Push(const T & item)
    {
        {
            std::lock_guard<std::mutex> lock(guard);
            queue.push(item);
        }
        signal.notify_one();
    }

    bool IsEmpty() const
    {
        std::lock_guard<std::mutex> lock(guard);
        return queue.empty();
    }

    std::optional<T> TryPop()
    {
        std::lock_guard<std::mutex> lock(guard);
        return TryPopInternal();
    }

    T BlockAndPop()
    {
        std::unique_lock<std::mutex> lock(guard);

        signal.wait(lock, [&](){return !queue.empty();});

        const auto result = queue.front();
        queue.pop();
        return result;
    }

    std::optional<T> WaitAndTryPop(std::chrono::microseconds timeout)
    {
        std::unique_lock<std::mutex> lock(guard);
        signal.wait_for(lock, timeout, [&](){return !queue.empty();});
        return TryPopInternal();
    }

private:
    std::queue<T> queue;
    mutable std::mutex guard;
    std::condition_variable signal;

    std::optional<T> TryPopInternal();
};

template<typename T>
std::optional<T> BlockingQueue<T>::TryPopInternal()
{
    if(queue.empty())
    {
        return std::nullopt;
    }
    else
    {
        const auto result = std::make_optional<T>(queue.front());
        queue.pop();
        return result;
    }
}
