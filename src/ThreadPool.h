#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();
    
    void enqueue(std::function<void()> task);
    void stop();
    
    size_t queueSize() const;
    size_t activeThreads() const;

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    
    mutable std::mutex mtx;
    std::condition_variable cv;
    std::atomic<bool> running;
    std::atomic<size_t> activeCount;
};

#endif