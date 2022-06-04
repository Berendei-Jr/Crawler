// Copyright 2021 Andrey Vedeneev

#ifndef INCLUDE_EXAMPLE_HPP_
#define INCLUDE_EXAMPLE_HPP_

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
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

namespace po = boost::program_options;
namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

struct page_t {
  std::string _url;
  int _level;
  explicit page_t(std::string url) : _url(std::move(url)), _level(1) {}
  page_t(std::string url, int level) : _url(std::move(url)), _level(level) {}
  bool operator==(const page_t& p) const { return _url == p._url; }
};

struct page1_t {
  std::string _url;
  std::string _body;
  page1_t(const std::string& url, const std::string& body) : _url(url),
                                                             _body(body) {}
  bool operator==(const page1_t& p) const { return _url == p._url; }
};

class control_t : public std::enable_shared_from_this<control_t> {
 public:
  control_t(int thr, int depth, std::string url, std::condition_variable& cv,
            tsqueue<page1_t>& m, std::atomic<bool>& flag);
  ~control_t() = default;

  bool Stop();
  bool Exists(const page_t& p) { return m_queue.Exists(p); }
  page_t GetPage() { return m_queue.pop_front(); }
  int GetDepth() const { return m_depth; }
  void IncrementCounter() { m_counter++; }
  void DecrementCounter() { m_counter--; }
  void NotifyMain() { m_StopToMain.notify_one();
    m_flag = true; }
  void AddLink(const page_t& p) { m_queue.push_back(p); }
  void AddToMainQueue(const page1_t& url) { m_main_queue.push_back(url); }

 private:
  int m_threads;
  int m_depth;
  std::atomic<int> m_counter;
  std::condition_variable& m_StopToMain;
  tsqueue<page_t> m_queue = tsqueue<page_t>();
  tsqueue<page1_t>& m_main_queue;
  std::atomic<bool>& m_flag;
};

class cons_control_t : public std::enable_shared_from_this<cons_control_t> {
 public:
  cons_control_t(int thr, tsqueue<page1_t>& queue,
                 tsqueue<std::string>& img, std::condition_variable& cv,
                 std::atomic<bool>& flag);
  ~cons_control_t() = default;

  page1_t GetPage() { return m_queue.pop_front(); }
  void DecrementCounter() { m_counter--; }
  void IncrementCounter() { m_counter++; }
  void AddLink(const std::string& s) { m_img.push_back(s); }
  bool Stop() {  return m_queue.empty() && m_counter == m_threads && m_flag; }
  bool Exists(const std::string& s) { return m_img.Exists(s); }
  void NotifyMain() { m_StopToMain.notify_one(); }

 private:
  int m_threads;
  tsqueue<page1_t>& m_queue;
  tsqueue<std::string>& m_img;
  std::atomic<int> m_counter;
  std::condition_variable& m_StopToMain;
  std::atomic<bool>& m_flag;
};

void NetworkThreadFunction(std::shared_ptr<control_t> control);
void ConsumerThreadFunction(std::shared_ptr<cons_control_t> control);

std::string Download(std::string& url);

static void search_for_links(GumboNode* node, std::vector<std::string>& links) {
  if (node->type != GUMBO_NODE_ELEMENT) {
    throw std::invalid_argument("");
  }

  GumboAttribute* href;
  if (node->v.element.tag == GUMBO_TAG_A &&
      (href = gumbo_get_attribute(&node->v.element.attributes, "href"))) {
    links.emplace_back(href->value);
  }

  GumboVector* children = &node->v.element.children;
  for (unsigned int i = 0; i < children->length; ++i) {
    try {
      search_for_links(static_cast<GumboNode*>(children->data[i]), links);
    } catch (std::invalid_argument&) {
      continue;
    }
  }
}

static void search_for_img(GumboNode* node, std::vector<std::string>& links) {
  if (node->type != GUMBO_NODE_ELEMENT) {
    throw std::invalid_argument("");
  }

  std::string str = "image/png";
  GumboAttribute* href;
  if (node->v.element.tag == GUMBO_TAG_IMG &&
      (href = gumbo_get_attribute(&node->v.element.attributes, "href"))) {
    links.emplace_back(href->value);
    std::cout << "Found img: " << href->value << "\n";
  } else if (node->v.element.tag == GUMBO_TAG_IMG &&
          (href = gumbo_get_attribute(&node->v.element.attributes, "src"))) {
    links.emplace_back(href->value);
    std::cout << "Found img: " << href->value << "\n";
  } else if (node->v.element.tag == GUMBO_TAG_LINK &&
          (href = gumbo_get_attribute(&node->v.element.attributes, "type"))) {
    if (href->value == str.c_str()) {
      links.emplace_back(href->value);
      std::cout << "Found img: " << href->value << "\n";
    }
  }

  GumboVector* children = &node->v.element.children;
  for (unsigned int i = 0; i < children->length; ++i) {
    try {
      search_for_img(static_cast<GumboNode*>(children->data[i]), links);
    } catch (std::invalid_argument&) {
      continue;
    }
  }
}

#endif // INCLUDE_EXAMPLE_HPP_
