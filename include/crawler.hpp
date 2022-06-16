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
#include <list>
#include <condition_variable>
#include <boost/program_options.hpp>
#include <utility>
#include <threadSafeQueue.hpp>
#include <gumbo.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace net {

    struct webPage {
        std::string address;
        int level;
        std::string html;
        std::string root;
    };

    class crawler {

    public:
        crawler() = delete;
        crawler(const crawler &) = delete;
        crawler(crawler &&) = delete;
        crawler(std::string& url, int depth, int network_threads, int parser_threads, int downloaders_threads);

        void writeResultIntoFolder();
        ~crawler() = default;

    private:
        bool stopProducers();
        bool stopConsumers();
        bool stopDownloaders();
        void decrementProdCounter() { --mProdCounter; }
        void incrementProdCounter() { ++mProdCounter; }
        void decrementDownCounter() { --mDownCounter; }
        void incrementDownCounter() { ++mDownCounter; }
        void decrementConsCounter() { --mConsCounter; }
        void incrementConsCounter() { ++mConsCounter; }
        webPage getProdUrl() { return mProducersQueue.pop_front(); }
        webPage getConsUrl() { return mConsumersQueue.pop_front(); }
        std::string getDownloadersUrl() { return mImgDownloadersQueue.pop_front(); }
        void addToListTS(const std::string& s);
        bool existsInListTS(const std::string& s) const;
        friend void producerFunction(crawler *crawler_ptr);
        friend void consumerFunction(crawler *crawler_ptr);
        friend void imgDownloaderFunction(crawler *crawler_ptr);

        std::vector<std::thread> mProducersPool;
        std::vector<std::thread> mConsumersPool;
        std::vector<std::thread> mImgDownloadersPool;
        std::atomic_int mProdCounter{};
        std::atomic_int mConsCounter{};
        std::atomic_int mDownCounter{};
        tsqueue<webPage> mProducersQueue;
        tsqueue<webPage> mConsumersQueue;
        tsqueue<std::string> mImgDownloadersQueue;
        std::list<std::string> mImgUrls;
        int mDepth;
        int mProducersNum;
        int mConsumersNum;
        int mDownloadersNum;
        std::atomic_bool mProdStop = false;
        std::atomic_bool mConsStop = false;
        std::atomic_bool mDownStop = false;
        mutable std::mutex mMtx;
        mutable std::condition_variable mCv;
        std::mutex mImgMtx;
        mutable std::mutex mListMtx;

        fs::path imgPath;
        std::atomic_int mCount{};
    };

    std::string DownloadPage(std::string &url);

    void producerFunction(crawler *crawler_ptr);
    void consumerFunction(crawler *crawler_ptr);
    void imgDownloaderFunction(crawler *crawler_ptr);

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
        ((std::string *) userp)->append((char *) contents, size * nmemb);
        return size * nmemb;
    }

    static inline void search_for_links(GumboNode *node, std::vector<std::string>& urls);
    static inline void search_for_img(GumboNode* node, std::vector<std::string>& images);

    bool startWith(const std::string& s1, const std::string& s2);
    bool endsWith(const std::string& s1, const std::string& s2);

    std::string getRoot(std::string& url);
    std::string cleanBackUntilSlash(std::string& url);
}

#endif //CRAWLER_HPP
