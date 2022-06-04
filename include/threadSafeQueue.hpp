// Copyright 2021 Andrey Vedeneev vedvedved2003@gmail.com

#ifndef INCLUDE_THREADSAFEQUEUE_HPP_
#define INCLUDE_THREADSAFEQUEUE_HPP_

#include <mutex>
#include <deque>
#include <thread>
#include <semaphore.h>
#include <memory>
#include <utility>

template<class T>
class tsqueue
{
     public:
      tsqueue() {
        sem_init(&m_SemCount, 0, 0);
      }
      tsqueue(const tsqueue<T>&) = delete;

      const T& front() {
        std::scoped_lock lock(_mtx);
        return _data.front();
      }

      void push_back(const T& item) {
        std::scoped_lock lock(_mtx);
        _data.push_back(std::move(item));
        sem_post(&m_SemCount);
      }

      size_t size() {
        std::scoped_lock lock(_mtx);
        return _data.size();
      }

      void clear() {
        std::scoped_lock lock(_mtx);
        _data.clear();
      }

      bool empty() {
        std::scoped_lock lock(_mtx);
        return _data.empty();
      }

      T pop_front() {
        sem_wait(&m_SemCount);
        std::scoped_lock lock(_mtx);
        auto tmp = std::move(_data.front());
        _data.pop_front();
        return tmp;
      }

      ~tsqueue() {
        clear();
        sem_destroy(&m_SemCount);
      }

      bool Exists(const T& t)
      {
        std::scoped_lock lock(_mtx);
        if (!_data.empty())
        {
          for (auto& it : _data)
          {
            if (it == t)
              return true;
          }
        }
        return false;
      }

     private:
      sem_t m_SemCount;
      std::mutex _mtx;
      std::deque<T> _data;
};

#endif //INCLUDE_THREADSAFEQUEUE_HPP_
