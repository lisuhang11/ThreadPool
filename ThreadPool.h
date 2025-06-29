#pragma once

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <atomic>
#include <future>
#include <condition_variable>
#include <thread>
#include <functional>
#include <stdexcept>

namespace std {
#define THREADPOOL_MAX_NUM 16   // 线程池最大容量

    // 线程池是否可以自动增长(如果需要，且不超过THREADPOOL_MAX_NUM)
    //#define THREADPOOL_AUTO_GROW

    class ThreadPool {
        unsigned short _initSize;       // 初始化线程数量
        using Task = function<void()>;  // 定义任务类型
        vector<thread> _pool;          // 线程池
        queue<Task> _tasks;            // 任务队列
        mutex _lock;                   // 任务队列同步锁

#ifdef THREADPOOL_AUTO_GROW
        mutex _lockGrow;               // 线程池增长同步锁
#endif
        condition_variable _task_cv;    // 条件阻塞
        atomic<bool> _run{ true };      // 线程池是否执行
        atomic<int> _idlThrNum{ 0 };    // 空闲线程数量

    public:
        // 修正：使用传入的size参数创建线程
        inline ThreadPool(unsigned short size = 4) : _initSize(size) {
            addThread(size);
        }

        inline ~ThreadPool() {
            _run = false;
            _task_cv.notify_all();  // 唤醒所有线程
            for (thread& thread : _pool) {
                if (thread.joinable()) {
                    thread.join();  // 等待线程结束
                }
            }
        }

    public:
        // 提交一个任务
        template<class F, class... Args>
        auto commit(F&& f, Args&&... args) -> future<decltype(f(args...))> {
            if (!_run) {
                throw runtime_error("commit on ThreadPool is stopped.");
            }

            using RetType = decltype(f(args...));
            auto task = make_shared<packaged_task<RetType()>>(
                bind(forward<F>(f), forward<Args>(args)...)
            );
            future<RetType> future = task->get_future();

            {   // 添加任务到队列
                lock_guard<mutex> lock{ _lock };
                _tasks.emplace([task]() { (*task)(); });
            }

#ifdef THREADPOOL_AUTO_GROW
            if (_idlThrNum < 1 && _pool.size() < THREADPOOL_MAX_NUM) {
                addThread(1);
            }
#endif

            _task_cv.notify_one();  // 唤醒一个线程执行
            return future;
        }

        // 空闲线程数量
        int idlCount() { return _idlThrNum; }
        // 线程数量
        int thrCount() { return _pool.size(); }

    private:
        void addThread(unsigned short size) {
#ifdef THREADPOOL_AUTO_GROW
            // 确保锁保护整个增长过程
            unique_lock<mutex> lockGrow{ _lockGrow };
            if (!_run) throw runtime_error("线程池已停止");
#endif

            for (; _pool.size() < THREADPOOL_MAX_NUM && size > 0; --size) {
                _pool.emplace_back([this] {
                    while (true) {
                        Task task;
                        {
                            unique_lock<mutex> lock{ _lock };
                            _task_cv.wait(lock, [this] {
                                return !_run || !_tasks.empty();
                                });

                            if (!_run && _tasks.empty()) return;

                            _idlThrNum--;
                            task = move(_tasks.front());
                            _tasks.pop();
                        }

                        // 添加异常处理，防止任务异常导致线程退出
                        try {
                            task();
                        }
                        catch (const std::exception& e) {
                            // 可以记录日志或执行其他恢复操作
                        }

#ifndef THREADPOOL_AUTO_GROW
                        // 固定大小模式下，空闲线程可以退出
                        if (_idlThrNum > 0 && _pool.size() > _initSize) return;
#endif

                        {
                            unique_lock<mutex> lock{ _lock };
                            _idlThrNum++;
                        }
                    }
                    });

                {
                    unique_lock<mutex> lock{ _lock };
                    _idlThrNum++;
                }
            }
        }
    };

} // namespace std

#endif // THREAD_POOL_H
