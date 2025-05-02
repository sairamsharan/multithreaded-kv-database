#include "thread_pool.hpp"

ThreadPool::ThreadPool(size_t threads)
    : stop(false)
{
    for(size_t i = 0;i < threads; ++i) {
        workers.emplace_back(
            [this] {
                for(;;) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                            [this]{ return this->stop.load() || !this->tasks.empty(); });
                        
                        // if stopped and tasks queue is empty, exit thread
                        if(this->stop.load() && this->tasks.empty())
                            return;
                        
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    // execute the task outside the lock
                    task();
                }
            }
        );
    }
}

ThreadPool::~ThreadPool() {
    stop.store(true);
    condition.notify_all();
    for(std::thread &worker: workers) {
        if(worker.joinable()) {
            worker.join();
        }
    }
}
