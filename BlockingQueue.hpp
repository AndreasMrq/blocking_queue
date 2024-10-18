#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <concepts>

template<std::move_constructible T>
class BlockingQueue
{
    public:
        void Push(const T &item);

        void Push(T&& item);

        template<typename ...Args>
            void Emplace(Args... args);

        bool IsEmpty() const;

        std::optional<T> TryPop();

        T BlockAndPop();
        std::optional<T> BlockAndPop(std::stop_token stopToken);

        std::optional<T> WaitAndTryPop(std::chrono::microseconds timeout);

    private:
        std::queue<T> queue;
        mutable std::mutex guard;
        std::condition_variable_any signal;

        std::optional<T> TryPopInternal();
};

    template<std::move_constructible T>
std::optional<T> BlockingQueue<T>::TryPopInternal()
{
    if(queue.empty())
    {
        return std::nullopt;
    }
    else
    {
        auto result = std::make_optional<T>(std::move(queue.front()));
        queue.pop();
        return result;
    }
}

    template<std::move_constructible T>
void BlockingQueue<T>::Push(const T &item)
{
    {
        std::lock_guard<std::mutex> lock(guard);
        queue.push(item);
    }
    signal.notify_one();
}

    template<std::move_constructible T>
void BlockingQueue<T>::Push(T &&item)
{
    {
        std::lock_guard<std::mutex> lock(guard);
        queue.push(std::move(item));
    }
    signal.notify_one();
}

template<std::move_constructible T>
    template<typename ...Args>
void BlockingQueue<T>::Emplace(Args... args)
{
    {
        std::lock_guard<std::mutex> lock(guard);
        queue.emplace(args...);
    }
    signal.notify_one();
}

template<std::move_constructible T>
bool BlockingQueue<T>::IsEmpty() const
{
    std::lock_guard<std::mutex> lock(guard);
    return queue.empty();
}

    template<std::move_constructible T>
std::optional<T> BlockingQueue<T>::TryPop()
{
    std::lock_guard<std::mutex> lock(guard);
    return TryPopInternal();
}

    template<std::move_constructible T>
T BlockingQueue<T>::BlockAndPop()
{
    std::unique_lock<std::mutex> lock(guard);

    signal.wait(lock, [&](){return !queue.empty();});

    T result{std::move(queue.front())};
    queue.pop();
    return result;
}

    template<std::move_constructible T>
std::optional<T> BlockingQueue<T>::BlockAndPop(std::stop_token stopToken)
{
    std::unique_lock<std::mutex> lock(guard);

    signal.wait(lock, stopToken, [&](){return !queue.empty();});
    return TryPopInternal();
}

    template<std::move_constructible T>
std::optional<T> BlockingQueue<T>::WaitAndTryPop(std::chrono::microseconds timeout)
{
    std::unique_lock<std::mutex> lock(guard);
    signal.wait_for(lock, timeout, [&](){return !queue.empty();});
    return TryPopInternal();
}

