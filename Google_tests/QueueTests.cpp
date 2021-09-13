#include <gtest/gtest.h>

#include "../queue.h"
#include <algorithm>
#include <future>
#include <vector>

struct Element {
    Element() = default;
    explicit Element(int v) :
        value{ v }
    {}

    int value = 0;
};

TEST(QueueTest, Test_Negative_Size) {
    int size = -5;
    Queue<Element> queue(size);
    EXPECT_EQ(queue.Size(), 0);
    EXPECT_EQ(queue.Count(), 0);

    queue.Push(Element{ 1 });
    EXPECT_EQ(queue.Size(), 0);
    EXPECT_EQ(queue.Count(), 0);

    auto poppedElement = queue.Pop();
    EXPECT_EQ(poppedElement.value, 0);
    EXPECT_EQ(queue.Size(), 0);
    EXPECT_EQ(queue.Count(), 0);
}

TEST(QueueTest, Test_Zero_Size) {
    int size = 0;
    Queue<Element> queue(size);
    EXPECT_EQ(queue.Size(), 0);
    EXPECT_EQ(queue.Count(), 0);

    queue.Push(Element{ 1 });
    EXPECT_EQ(queue.Size(), 0);
    EXPECT_EQ(queue.Count(), 0);

    auto poppedElement = queue.Pop();
    EXPECT_EQ(poppedElement.value, 0);
    EXPECT_EQ(queue.Size(), 0);
    EXPECT_EQ(queue.Count(), 0);
}

TEST(QueueTest, Test_1Push_And_1Pop) {
    int size = 5;
    Queue<Element> queue(size);
    EXPECT_EQ(queue.Size(), size);
    EXPECT_EQ(queue.Count(), 0);

    Element element{ 1 };
    queue.Push(element);
    EXPECT_EQ(queue.Size(), size);
    EXPECT_EQ(queue.Count(), 1);

    auto poppedElement = queue.Pop();
    EXPECT_EQ(element.value, poppedElement.value);
    EXPECT_EQ(queue.Size(), size);
    EXPECT_EQ(queue.Count(), 0);
}

TEST(QueueTest, Test_ManyPush_And_ManyPop) {
    int size = 5;
    Queue<Element> queue(size);

    for (int i = 1; i <= size; ++i) {
        queue.Push(Element{ i });
        EXPECT_EQ(queue.Size(), size);
        EXPECT_EQ(queue.Count(), i);
    }
    for (int i = 1; i <= size; ++i) {
        auto element = queue.Pop();
        EXPECT_EQ(element.value, i);
        EXPECT_EQ(queue.Size(), size);
        EXPECT_EQ(queue.Count(), (size - i));
    }
}

TEST(QueueTest, Test_ExtraPush_And_ExtraPop) {
    int size = 5;
    Queue<Element> queue(size);

    for (int i = 1; i <= (size + 2); ++i) {
        queue.Push(Element{ i });
        EXPECT_EQ(queue.Size(), size);
        EXPECT_EQ(queue.Count(), (i < size) ? i : size);
    }
    for (int i = 1; i <= (size + 2); ++i) {
        auto element = queue.Pop();
        EXPECT_EQ(element.value, (i <= size) ? i : 0);
        EXPECT_EQ(queue.Size(), size);
        EXPECT_EQ(queue.Count(), (i < size) ? (size - i) : 0);
    }
}

TEST(QueueTest, Test_Push_And_Pop_From_Multiple_Threads) {
    int queueSize = 5;
    Queue<Element> queue(queueSize);
    std::mutex readyToStartMutex;
    std::condition_variable readyToStartCV;
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    int nMaxThreads = 2 * queueSize;

    {
        std::once_flag readyToStartFlag;
        auto notifyAboutStart = [&readyToStartCV, &readyToStartFlag] () {
            auto func = [&readyToStartCV] () {
                readyToStartCV.notify_all();
            };
            std::call_once(readyToStartFlag, func);
        };
        int nStartedThreads = 0;
        auto writer = [&] () {
            {
                std::unique_lock<std::mutex> locker(readyToStartMutex);
                if (nStartedThreads == nMaxThreads) {
                    return;
                }
                ++nStartedThreads;
                auto checker = [&nStartedThreads, &nMaxThreads] () {
                    return (nStartedThreads == nMaxThreads);
                };
                readyToStartCV.wait(locker, checker);
            }
            notifyAboutStart();

            queue.Push(Element{ 1 });
        };
        std::vector<std::future<void>> futures;
        for (int i = 0; i < nMaxThreads; ++i) {
            std::future<void> future = std::async(std::launch::async, writer);
            futures.push_back(std::move(future));
        }

        std::shuffle(futures.begin(), futures.end(), generator);
        for (auto& future : futures) {
            future.wait();
        }
        EXPECT_EQ(queue.Count(), queueSize);
    }

    {
        std::once_flag readyToStartFlag;
        auto notifyAboutStart = [&readyToStartCV, &readyToStartFlag] () {
            auto func = [&readyToStartCV] () {
                readyToStartCV.notify_all();
            };
            std::call_once(readyToStartFlag, func);
        };
        int nStartedThreads = 0;
        auto reader = [&] () {
            {
                std::unique_lock<std::mutex> locker(readyToStartMutex);
                if (nStartedThreads == nMaxThreads) {
                    return;
                }
                ++nStartedThreads;
                auto checker = [&nStartedThreads, &nMaxThreads] () {
                    return (nStartedThreads == nMaxThreads);
                };
                readyToStartCV.wait(locker, checker);
            }
            notifyAboutStart();

            queue.Pop();
        };
        std::vector<std::future<void>> futures;
        for (int i = 0; i < nMaxThreads; ++i) {
            std::future<void> future = std::async(std::launch::async, reader);
            futures.push_back(std::move(future));
        }

        std::shuffle(futures.begin(), futures.end(), generator);
        for (auto& future : futures) {
            future.wait();
        }
        EXPECT_EQ(queue.Count(), 0);
    }
}

TEST(QueueTest, Test_Simultaneously_Push_And_Pop_From_Multiple_Threads_For_Empty_Queue) {
    int queueSize = 5;
    Queue<Element> queue(queueSize);
    std::mutex readyToStartMutex;
    std::condition_variable readyToStartCV;
    int nStartedThreads = 0;
    int nMaxThreads = 4 * queueSize;
    std::vector<std::future<void>> futures;
    std::once_flag readyToStartFlag;

    auto notifyAboutStart = [&readyToStartCV, &readyToStartFlag] () {
        auto func = [&readyToStartCV] () {
            readyToStartCV.notify_all();
        };
        std::call_once(readyToStartFlag, func);
    };

    {
        auto writer = [&] () {
            {
                std::unique_lock<std::mutex> locker(readyToStartMutex);
                if (nStartedThreads == nMaxThreads) {
                    return;
                }
                ++nStartedThreads;
                auto checker = [&nStartedThreads, &nMaxThreads] () {
                    return (nStartedThreads == nMaxThreads);
                };
                readyToStartCV.wait(locker, checker);
            }
            notifyAboutStart();

            queue.Push(Element{ 1 });
        };
        for (int i = 0; i < (2 * queueSize); ++i) {
            std::future<void> future = std::async(std::launch::async, writer);
            futures.push_back(std::move(future));
        }
    }
    {
        auto reader = [&] () {
            {
                std::unique_lock<std::mutex> locker(readyToStartMutex);
                if (nStartedThreads == nMaxThreads) {
                    return;
                }
                ++nStartedThreads;
                auto checker = [&nStartedThreads, &nMaxThreads] () {
                    return (nStartedThreads == nMaxThreads);
                };
                readyToStartCV.wait(locker, checker);
            }
            notifyAboutStart();

            queue.Pop();
        };
        for (int i = 0; i < (2 * queueSize); ++i) {
            std::future<void> future = std::async(std::launch::async, reader);
            futures.push_back(std::move(future));
        }
    }

    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::shuffle(futures.begin(), futures.end(), generator);

    for (auto& future : futures) {
        future.wait();
    }
    EXPECT_EQ(1, 1);
}

TEST(QueueTest, Test_Simultaneously_Push_And_Pop_From_Multiple_Threads_For_Full_Queue) {
    int queueSize = 5;
    Queue<Element> queue(queueSize);
    for (int i = 1; i <= queueSize; ++i) {
        queue.Push(Element{ i });
    }

    std::mutex readyToStartMutex;
    std::condition_variable readyToStartCV;
    int nStartedThreads = 0;
    int nMaxThreads = 4 * queueSize;
    std::vector<std::future<void>> futures;
    std::once_flag readyToStartFlag;

    auto notifyAboutStart = [&readyToStartCV, &readyToStartFlag] () {
        auto func = [&readyToStartCV] () {
            readyToStartCV.notify_all();
        };
        std::call_once(readyToStartFlag, func);
    };

    {
        auto writer = [&] () {
            {
                std::unique_lock<std::mutex> locker(readyToStartMutex);
                if (nStartedThreads == nMaxThreads) {
                    return;
                }
                ++nStartedThreads;
                auto checker = [&nStartedThreads, &nMaxThreads] () {
                    return (nStartedThreads == nMaxThreads);
                };
                readyToStartCV.wait(locker, checker);
            }
            notifyAboutStart();

            queue.Push(Element{ 1 });
        };
        for (int i = 0; i < (2 * queueSize); ++i) {
            std::future<void> future = std::async(std::launch::async, writer);
            futures.push_back(std::move(future));
        }
    }
    {
        auto reader = [&] () {
            {
                std::unique_lock<std::mutex> locker(readyToStartMutex);
                if (nStartedThreads == nMaxThreads) {
                    return;
                }
                ++nStartedThreads;
                auto checker = [&nStartedThreads, &nMaxThreads] () {
                    return (nStartedThreads == nMaxThreads);
                };
                readyToStartCV.wait(locker, checker);
            }
            notifyAboutStart();

            queue.Pop();
        };
        for (int i = 0; i < (2 * queueSize); ++i) {
            std::future<void> future = std::async(std::launch::async, reader);
            futures.push_back(std::move(future));
        }
    }

    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::shuffle(futures.begin(), futures.end(), generator);

    for (auto& future : futures) {
        future.wait();
    }
    EXPECT_EQ(1, 1);
}