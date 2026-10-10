#include "ThreadPool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t num_threads) : running(true), activeCount(0) {
    for (size_t i = 0; i < num_threads; i++) {
        workers.emplace_back([this]() {
            while (true) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [this]() {
                        return !tasks.empty() || !running;
                    });
                    
                    if (!running && tasks.empty()) {
                        return;
                    }
                    
                    task = std::move(tasks.front());
                    tasks.pop();
                    activeCount++;
                }
                
                task();
                
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    activeCount--;
                }
            }
        });
    }
    
    std::cout << "线程池启动，线程数: " << num_threads << std::endl;
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        tasks.push(std::move(task));
    }
    cv.notify_one();
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        running = false;
    }
    cv.notify_all();
    
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

size_t ThreadPool::queueSize() const {
    std::lock_guard<std::mutex> lock(mtx);
    return tasks.size();
}

size_t ThreadPool::activeThreads() const {
    return activeCount.load();
}