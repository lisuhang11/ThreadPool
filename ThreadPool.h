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
#define THREADPOOL_MAX_NUM 16   // �̳߳��������

    // �̳߳��Ƿ�����Զ�����(�����Ҫ���Ҳ�����THREADPOOL_MAX_NUM)
    //#define THREADPOOL_AUTO_GROW

    class ThreadPool {
        unsigned short _initSize;       // ��ʼ���߳�����
        using Task = function<void()>;  // ������������
        vector<thread> _pool;          // �̳߳�
        queue<Task> _tasks;            // �������
        mutex _lock;                   // �������ͬ����

#ifdef THREADPOOL_AUTO_GROW
        mutex _lockGrow;               // �̳߳�����ͬ����
#endif
        condition_variable _task_cv;    // ��������
        atomic<bool> _run{ true };      // �̳߳��Ƿ�ִ��
        atomic<int> _idlThrNum{ 0 };    // �����߳�����

    public:
        // ������ʹ�ô����size���������߳�
        inline ThreadPool(unsigned short size = 4) : _initSize(size) {
            addThread(size);
        }

        inline ~ThreadPool() {
            _run = false;
            _task_cv.notify_all();  // ���������߳�
            for (thread& thread : _pool) {
                if (thread.joinable()) {
                    thread.join();  // �ȴ��߳̽���
                }
            }
        }

    public:
        // �ύһ������
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

            {   // ������񵽶���
                lock_guard<mutex> lock{ _lock };
                _tasks.emplace([task]() { (*task)(); });
            }

#ifdef THREADPOOL_AUTO_GROW
            if (_idlThrNum < 1 && _pool.size() < THREADPOOL_MAX_NUM) {
                addThread(1);
            }
#endif

            _task_cv.notify_one();  // ����һ���߳�ִ��
            return future;
        }

        // �����߳�����
        int idlCount() { return _idlThrNum; }
        // �߳�����
        int thrCount() { return _pool.size(); }

    private:
        void addThread(unsigned short size) {
#ifdef THREADPOOL_AUTO_GROW
            // ȷ��������������������
            unique_lock<mutex> lockGrow{ _lockGrow };
            if (!_run) throw runtime_error("�̳߳���ֹͣ");
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

                        // ����쳣������ֹ�����쳣�����߳��˳�
                        try {
                            task();
                        }
                        catch (const std::exception& e) {
                            // ���Լ�¼��־��ִ�������ָ�����
                        }

#ifndef THREADPOOL_AUTO_GROW
                        // �̶���Сģʽ�£������߳̿����˳�
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
