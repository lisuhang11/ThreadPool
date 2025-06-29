#include "threadpool.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <stdexcept>

// ������ͨ����
int add(int a, int b) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return a + b;
}

// ����Lambda���ʽ
std::string greet(const std::string& name) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return "Hello, " + name;
}

// �����쳣���
void throwException() {
    throw std::runtime_error("�����쳣");
}

// �����ྲ̬��Ա����
class TestClass {
public:
    static int multiply(int a, int b) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return a * b;
    }
};

int main() {
    try {
        // �����̳߳أ���ʼ4���߳�
        std::cout << "�����̳߳أ���ʼ�߳���: 4" << std::endl;
        std::ThreadPool pool(4);

        // ����1: �ύ��ͨ��������
        std::cout << "\n=== ����1: ��ͨ�������� ===" << std::endl;
        auto future1 = pool.commit(add, 10, 20);
        std::cout << "�ȴ�����ִ�н��...";
        int result1 = future1.get();
        std::cout << "10 + 20 = " << result1 << std::endl;

        // ����2: �ύLambda���ʽ����
        std::cout << "\n=== ����2: Lambda���ʽ���� ===" << std::endl;
        auto future2 = pool.commit([](int a, double b) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            return a * b;
            }, 5, 3.5);
        double result2 = future2.get();
        std::cout << "5 * 3.5 = " << result2 << std::endl;

        // ����3: �ύ���ַ�������������
        std::cout << "\n=== ����3: �ַ����������� ===" << std::endl;
        std::string name = "ThreadPool";
        auto future3 = pool.commit(greet, name);
        std::string result3 = future3.get();
        std::cout << result3 << std::endl;

        // ����4: �����ྲ̬��Ա����
        std::cout << "\n=== ����4: �ྲ̬��Ա���� ===" << std::endl;
        auto future4 = pool.commit(TestClass::multiply, 6, 7);
        int result4 = future4.get();
        std::cout << "6 * 7 = " << result4 << std::endl;

        // ����5: �����ύ����
        std::cout << "\n=== ����5: �����ύ���� ===" << std::endl;
        const int TASK_COUNT = 10;
        std::vector<std::future<int>> futures;

        std::cout << "�ύ" << TASK_COUNT << "������..." << std::endl;
        for (int i = 0; i < TASK_COUNT; ++i) {
            futures.emplace_back(pool.commit([i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50 + i * 10));
                return i * i;
                }));
        }

        std::cout << "�ȴ�����������ɲ���ȡ���: ";
        for (auto& future : futures) {
            std::cout << future.get() << " ";
        }
        std::cout << std::endl;

        // ����6: �����̳߳��Զ��������ܣ�ȡ��ע�ͺ궨�����ã�
#ifdef THREADPOOL_AUTO_GROW
        std::cout << "\n=== ����6: �Զ��������� ===" << std::endl;
        std::cout << "��ǰ�߳���: " << pool.thrCount() << ", �����߳���: " << pool.idlCount() << std::endl;

        // �ύ�������񴥷��߳�����
        std::vector<std::future<void>> growFutures;
        for (int i = 0; i < 20; ++i) {
            growFutures.emplace_back(pool.commit([]() {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                }));
        }

        // �ȴ�һ��ʱ�����߳�����
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "�����������߳���: " << pool.thrCount() << ", �����߳���: " << pool.idlCount() << std::endl;
#endif

        // ����7: �����쳣����
        std::cout << "\n=== ����7: �쳣���� ===" << std::endl;
        try {
            auto futureEx = pool.commit(throwException);
            futureEx.get(); // ������׳��쳣
        }
        catch (const std::exception& e) {
            std::cout << "�����쳣: " << e.what() << std::endl;
        }

        // ����8: �����̳߳�����
        std::cout << "\n=== ����8: �̳߳����� ===" << std::endl;
        {
            std::ThreadPool localPool(2);
            std::cout << "�����̳߳ش������߳���: " << localPool.thrCount() << std::endl;
            auto future = localPool.commit([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                return 42;
                });
            // �̳߳������������ʱ���٣���ȴ��������
        }
        std::cout << "�����̳߳�������" << std::endl;

        // ����9: �����̳߳�ֹͣ���ύ����
        std::cout << "\n=== ����9: �̳߳�ֹͣ���ύ���� ===" << std::endl;
        {
            std::ThreadPool stopPool(1);
            stopPool.commit([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                });
            // �ֶ�ֹͣ�̳߳�
            stopPool.~ThreadPool();
            try {
                // Ӧ���׳��쳣
                stopPool.commit([]() {});
            }
            catch (const std::exception& e) {
                std::cout << "Ԥ���쳣: " << e.what() << std::endl;
            }
        }

        std::cout << "\n���в�����ɣ�" << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "�����쳣: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}