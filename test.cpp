#include "threadpool.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <stdexcept>

// 测试普通函数
int add(int a, int b) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return a + b;
}

// 测试Lambda表达式
std::string greet(const std::string& name) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return "Hello, " + name;
}

// 测试异常情况
void throwException() {
    throw std::runtime_error("测试异常");
}

// 测试类静态成员函数
class TestClass {
public:
    static int multiply(int a, int b) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return a * b;
    }
};

int main() {
    try {
        // 创建线程池，初始4个线程
        std::cout << "创建线程池，初始线程数: 4" << std::endl;
        std::ThreadPool pool(4);

        // 测试1: 提交普通函数任务
        std::cout << "\n=== 测试1: 普通函数任务 ===" << std::endl;
        auto future1 = pool.commit(add, 10, 20);
        std::cout << "等待任务执行结果...";
        int result1 = future1.get();
        std::cout << "10 + 20 = " << result1 << std::endl;

        // 测试2: 提交Lambda表达式任务
        std::cout << "\n=== 测试2: Lambda表达式任务 ===" << std::endl;
        auto future2 = pool.commit([](int a, double b) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            return a * b;
            }, 5, 3.5);
        double result2 = future2.get();
        std::cout << "5 * 3.5 = " << result2 << std::endl;

        // 测试3: 提交带字符串参数的任务
        std::cout << "\n=== 测试3: 字符串参数任务 ===" << std::endl;
        std::string name = "ThreadPool";
        auto future3 = pool.commit(greet, name);
        std::string result3 = future3.get();
        std::cout << result3 << std::endl;

        // 测试4: 测试类静态成员函数
        std::cout << "\n=== 测试4: 类静态成员函数 ===" << std::endl;
        auto future4 = pool.commit(TestClass::multiply, 6, 7);
        int result4 = future4.get();
        std::cout << "6 * 7 = " << result4 << std::endl;

        // 测试5: 批量提交任务
        std::cout << "\n=== 测试5: 批量提交任务 ===" << std::endl;
        const int TASK_COUNT = 10;
        std::vector<std::future<int>> futures;

        std::cout << "提交" << TASK_COUNT << "个任务..." << std::endl;
        for (int i = 0; i < TASK_COUNT; ++i) {
            futures.emplace_back(pool.commit([i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50 + i * 10));
                return i * i;
                }));
        }

        std::cout << "等待所有任务完成并获取结果: ";
        for (auto& future : futures) {
            std::cout << future.get() << " ";
        }
        std::cout << std::endl;

        // 测试6: 测试线程池自动增长功能（取消注释宏定义启用）
#ifdef THREADPOOL_AUTO_GROW
        std::cout << "\n=== 测试6: 自动增长功能 ===" << std::endl;
        std::cout << "当前线程数: " << pool.thrCount() << ", 空闲线程数: " << pool.idlCount() << std::endl;

        // 提交大量任务触发线程增长
        std::vector<std::future<void>> growFutures;
        for (int i = 0; i < 20; ++i) {
            growFutures.emplace_back(pool.commit([]() {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                }));
        }

        // 等待一段时间让线程增长
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "触发增长后线程数: " << pool.thrCount() << ", 空闲线程数: " << pool.idlCount() << std::endl;
#endif

        // 测试7: 测试异常处理
        std::cout << "\n=== 测试7: 异常处理 ===" << std::endl;
        try {
            auto futureEx = pool.commit(throwException);
            futureEx.get(); // 这里会抛出异常
        }
        catch (const std::exception& e) {
            std::cout << "捕获到异常: " << e.what() << std::endl;
        }

        // 测试8: 测试线程池销毁
        std::cout << "\n=== 测试8: 线程池销毁 ===" << std::endl;
        {
            std::ThreadPool localPool(2);
            std::cout << "本地线程池创建，线程数: " << localPool.thrCount() << std::endl;
            auto future = localPool.commit([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                return 42;
                });
            // 线程池在作用域结束时销毁，会等待任务完成
        }
        std::cout << "本地线程池已销毁" << std::endl;

        // 测试9: 测试线程池停止后提交任务
        std::cout << "\n=== 测试9: 线程池停止后提交任务 ===" << std::endl;
        {
            std::ThreadPool stopPool(1);
            stopPool.commit([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                });
            // 手动停止线程池
            stopPool.~ThreadPool();
            try {
                // 应该抛出异常
                stopPool.commit([]() {});
            }
            catch (const std::exception& e) {
                std::cout << "预期异常: " << e.what() << std::endl;
            }
        }

        std::cout << "\n所有测试完成！" << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "程序异常: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}