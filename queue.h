#ifndef QUEUE_H
#define QUEUE_H

// #include <iostream>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace QueueConstants {
    using namespace std::chrono_literals;
    constexpr auto WRITING_WAIT_TIME = 100ms;
    constexpr auto READING_WAIT_TIME = 100ms;
}

template<class T>
class Queue {
public:
    explicit Queue(int size) {
        if (size > 0) {
            m_size = size;
        }
        static_assert(std::is_default_constructible<T>::value, "Queue requires default-constructable elements");
        static_assert(std::is_copy_constructible<T>::value, "Queue requires copy-constructable elements");
        static_assert(std::is_move_constructible<T>::value, "Queue requires move-constructable elements");
        static_assert(std::is_move_assignable<T>::value, "Queue requires move-assignable elements");
        static_assert(std::is_destructible<T>::value, "Queue requires destructible elements");
    }

    int Count() {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_count;
    }
    int Size() { return m_size; }

    void Push(T element) {
        using namespace QueueConstants;
        if (m_size <= 0) {
            return;
        }
        {
            std::unique_lock<std::mutex> locker(m_mutex);
            if (m_count == m_size) {
                auto checker = [this] {
                    return m_count < m_size;
                };
                auto result = m_writingCV.wait_for(locker, WRITING_WAIT_TIME, checker);
                if (result) {
                    // std::cout << "writing thread finished waiting" << std::endl;
                } else {
                    // std::cout << "writing thread timed out" << std::endl;
                    return;
                }
            }
            bool isEmpty = (m_count == 0);
            m_queue.push_back(std::move(element));
            ++m_count;
            if (!isEmpty) {
                return;
            }
        }
        m_readingCV.notify_one();
    }

    T Pop() {
        using namespace QueueConstants;
        if (m_size <= 0) {
            return T{};
        }
        {
            std::unique_lock<std::mutex> locker(m_mutex);
            if (m_count == 0) {
                auto& count = m_count;
                auto checker = [&count] {
                    return count > 0;
                };
                auto result = m_readingCV.wait_for(locker, READING_WAIT_TIME, checker);
                if (result) {
                    // std::cout << "reading thread finished waiting" << std::endl;
                } else {
                    // std::cout << "reading thread timed out" << std::endl;
                    return T{};
                }
            }
            bool isFull = (m_count == m_size);
            if (isFull && m_isInited) {
                // std::cout << "popping of previous element is not finished yet" << std::endl;
                return T{};
            }
            auto element = m_queue.front();
            m_queue.pop_front();
            --m_count;
            if (!isFull) {
                return element;
            }
            m_poppedElement = std::move(element);
            m_isInited = true;
        }
        m_writingCV.notify_one();
        {
            std::lock_guard<std::mutex> locker(m_mutex);
            m_isInited = false;
            return m_poppedElement;
        }
    }

private:
    int m_count = 0;
    int m_size = 0;
    std::deque<T> m_queue;

    std::condition_variable m_readingCV;
    std::condition_variable m_writingCV;
    std::mutex m_mutex;

    T m_poppedElement;
    bool m_isInited = false;
};

#endif //QUEUE_H