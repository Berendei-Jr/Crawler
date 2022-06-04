//
// Created by vedve on 04.06.2022.
//

#include <crawler.hpp>
#include <utility>

net::crawler::crawler(std::string url, int depth, int network_threads, int parser_threads) {
  if (network_threads < 1 || parser_threads < 1 || depth < 1)
    throw std::runtime_error("Invalid arguments have been passed!\n");
  m_producers_pool.reserve(network_threads);
  m_consumers_pool.reserve(parser_threads);
  m_depth = depth;
  m_prodCounter = network_threads;
  m_consCounter = network_threads;
  struct url startPoint = { std::move(url), 1 };
  m_producers_queue.push_back(startPoint);
  m_consumers_queue.push_back(startPoint);

  for (int i = 0; i < network_threads; ++i) {
    m_producers_pool.emplace_back(producerFunction, this);
  }
  for (int i = 0; i < parser_threads; ++i) {
    m_consumers_pool.emplace_back(consumerFunction, this);
  }
}

bool net::crawler::stopProducers() {
  if (m_producers_queue.empty()) {
    if ((size_t)m_prodCounter == m_producers_pool.size()) {
      for (auto &t : m_producers_pool) {
        t.detach();
      }
      m_prodStop = true;
      stopConsumers();
      return true;
    }
  }
  return false;
}

bool net::crawler::stopConsumers() {
  if (m_prodStop) {
    if (m_consumers_queue.empty()) {
      if ((size_t)m_consCounter == m_consumers_pool.size()) {
        for (auto &t : m_consumers_pool) {
          t.detach();
        }
        m_consStop = true;
        m_CV.notify_one();
        return true;
      }
    }
  }
  return false;
}

void net::crawler::writeResultIntoFolder() const {
 // sleep(1);
  if (!m_consStop) {
    std::unique_lock<std::mutex> ul(m_Mtx);
    m_CV.wait(ul);
  }
  //TODO
  std::cout << "Writing to the folder!\n";
}

void net::producerFunction(crawler* crawler_ptr) {
  while(!crawler_ptr->stopProducers()) {  // if true, all the threads will be detached
    url current = crawler_ptr->getProdUrl();  // blocks the thread
    crawler_ptr->decrementProdCounter();
    //TODO logic
    std::cout << "Producer is working!\n";
    crawler_ptr->incrementProdCounter();
  }
}

void net::consumerFunction(crawler* crawler_ptr) {
  while (!crawler_ptr->stopConsumers()) {  // if true, all the threads will be detached
    url current = crawler_ptr->getConsUrl();  // blocks the thread
    crawler_ptr->decrementConsCounter();
    // TODO logic
    std::cout << "Consumer is working!\n";
    crawler_ptr->incrementConsCounter();
  }
}
