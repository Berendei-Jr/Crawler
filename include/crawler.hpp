// Copyright 2022 Andrey Vedeneev vedvedved2003@gmail.com

#ifndef CRAWLER_HPP
#define CRAWLER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <mutex>
#include <curl/curl.h>
#include <memory>
#include <vector>
#include <condition_variable>
#include <boost/program_options.hpp>
//#include <boost/beast/core.hpp>
//include <boost/beast/http.hpp>
#include <utility>
/*#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/ssl.hpp>
#include <root_certificates.hpp>*/
#include <threadSafeQueue.hpp>
#include <gumbo.h>

namespace net {

    struct webPage {
        std::string address;
        int level;
        std::string html;
    };

    class crawler {

    public:
        crawler() = delete;

        crawler(const crawler &) = delete;

        crawler(crawler &&) = delete;

        crawler(std::string url, int depth, int network_threads, int parser_threads);

        void writeResultIntoFolder();

        ~crawler() = default;

    private:
        bool stopProducers();

        bool stopConsumers();

        void decrementProdCounter() { --m_prodCounter; }

        void incrementProdCounter() { ++m_prodCounter; }

        void decrementConsCounter() { --m_consCounter; }

        void incrementConsCounter() { ++m_consCounter; }

        webPage getProdUrl() { return m_producers_queue.pop_front(); }

        webPage getConsUrl() { return m_consumers_queue.pop_front(); }

        friend void producerFunction(crawler *crawler_ptr);

        friend void consumerFunction(crawler *crawler_ptr);

        std::vector<std::thread> m_producers_pool;
        std::vector<std::thread> m_consumers_pool;
        std::atomic_int m_prodCounter{};
        std::atomic_int m_consCounter{};
        tsqueue<webPage> m_producers_queue;
        tsqueue<webPage> m_consumers_queue;
        int m_depth;
        int m_producers_num;
        int m_consumers_num;
        std::atomic_bool m_prodStop = false;
        std::atomic_bool m_consStop = false;
        mutable std::mutex m_Mtx;
        mutable std::condition_variable m_CV;
        std::mutex m_prodMtx;
        std::mutex m_consMtx;

        std::atomic_int m_count{};
        std::vector<std::string> m_result;
    };

    std::string DownloadPage(std::string &url);

    void producerFunction(crawler *crawler_ptr);

    void consumerFunction(crawler *crawler_ptr);

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
        ((std::string *) userp)->append((char *) contents, size * nmemb);
        return size * nmemb;
    }

    static inline void search_for_links(GumboNode *node, std::vector<std::string>& urls);
    static inline void search_for_img(GumboNode* node, std::vector<std::string>& images);

    bool startWith(const std::string& s1, const std::string& s2);
    bool endsWith(const std::string& s1, const std::string& s2);
}

#endif //CRAWLER_HPP
