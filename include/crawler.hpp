//
// Created by vedve on 02.06.2022.
//

#ifndef CRAWLER_HPP
#define CRAWLER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <vector>
#include <condition_variable>
#include <boost/program_options.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <utility>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/ssl.hpp>
#include <root_certificates.hpp>
#include <threadSafeQueue.hpp>
#include <gumbo.h>

namespace net {

struct url {
  std::string address;
  size_t level;
};

class crawler : public std::enable_shared_from_this<crawler> {

 public:
  crawler() = delete;
  crawler(const crawler&) = delete;
  crawler(crawler &&) = delete;
  crawler(std::string url, int depth, int network_threads, int parser_threads);
  void writeResultIntoFolder() const;
  bool stopProducers();
  bool stopConsumers();
  void decrementProdCounter() { --m_prodCounter; }
  void incrementProdCounter() { ++m_prodCounter; }
  void decrementConsCounter() { --m_consCounter; }
  void incrementConsCounter() { ++m_consCounter; }
  url getProdUrl() { return m_producers_queue.pop_front(); }
  url getConsUrl() { return m_consumers_queue.pop_front(); }

  ~crawler() = default;

 private:
  std::vector<std::thread> m_producers_pool;
  std::vector<std::thread> m_consumers_pool;
  std::atomic_int m_prodCounter{};
  std::atomic_int m_consCounter{};
  tsqueue<url> m_producers_queue;
  tsqueue<url> m_consumers_queue;
  int m_depth;
  std::atomic_bool m_prodStop = false;
  std::atomic_bool m_consStop = false;
  mutable std::mutex m_Mtx;
  mutable std::condition_variable m_CV;
};

void producerFunction(crawler* crawler_ptr);
void consumerFunction(crawler* crawler_ptr);

}

#endif //CRAWLER_HPP
