// Copyright 2022 Andrey Vedeneev vedvedved2003@gmail.com

#include <crawler.hpp>
namespace fs = std::filesystem;

void net::producerFunction(crawler* crawler_ptr) {
    while (true) {

        if (crawler_ptr->stopProducers())
            return;

        net::webPage current = crawler_ptr->getProdUrl();  // blocks the thread
        crawler_ptr->decrementProdCounter();

        current.html = net::DownloadPage(current.address);
        if (current.html.empty()) {
            crawler_ptr->incrementProdCounter();
            continue;
        }

        crawler_ptr->mConsumersQueue.push_back(current);

        if (current.level + 1 < crawler_ptr->mDepth) {
            GumboOutput* output = gumbo_parse(current.html.c_str());
            std::vector<std::string> LinksFound;
            net::search_for_links(output->root, LinksFound);
            for (auto& i: LinksFound) {
                if (!i.empty()) {
                    if (i[0] == '/') {
                        crawler_ptr->mProducersQueue.push_back(webPage{ current.root + i, current.level + 1, "", current.root} );
                    } else if (net::startWith(i, "http")) {
                        crawler_ptr->mProducersQueue.push_back(webPage{ i, current.level + 1, "", net::getRoot(i) });
                    } else if (net::endsWith(current.address, ".php") || net::endsWith(current.address, "html")) {
                        crawler_ptr->mProducersQueue.push_back(webPage{ net::cleanBackUntilSlash(current.address) + i, current.level + 1, "", current.root });
                    } else if (*current.address.end() == '/') {
                        crawler_ptr->mProducersQueue.push_back(webPage{ current.address + i, current.level + 1, "", current.root} );
                    } else {
                        crawler_ptr->mProducersQueue.push_back(webPage{ current.address + "/" + i, current.level + 1, "", current.root} );
                    }
                }
            }
            gumbo_destroy_output(&kGumboDefaultOptions, output);
        }
        crawler_ptr->incrementProdCounter();
    }
}

void net::consumerFunction(crawler* crawler_ptr) {
    while (!crawler_ptr->stopConsumers()) {
        net::webPage current = crawler_ptr->getConsUrl();  // blocks the thread
        crawler_ptr->decrementConsCounter();

        GumboOutput *output = gumbo_parse(current.html.c_str());
        std::vector<std::string> ImgFound;
        net::search_for_img(output->root, ImgFound);
        for (auto &i: ImgFound) {
            if (!i.empty()) {
                if (i[0] == '/') {
                    crawler_ptr->mImgDownloadersQueue.push_back(current.root + i );
                } else if (net::startWith(i, "http")) {
                    crawler_ptr->mImgDownloadersQueue.push_back(i);
                } else if (net::endsWith(current.address, ".php") || net::endsWith(current.address, "html")) {
                    crawler_ptr->mImgDownloadersQueue.push_back( net::cleanBackUntilSlash(current.address) + i );
                } else if (*current.address.end() == '/') {
                    crawler_ptr->mImgDownloadersQueue.push_back(current.address + i);
                } else {
                    crawler_ptr->mImgDownloadersQueue.push_back(current.address + "/" + i);
                }
            }
        }
        gumbo_destroy_output(&kGumboDefaultOptions, output);

        crawler_ptr->incrementConsCounter();
    }
}

void net::imgDownloaderFunction(net::crawler *crawler_ptr) {
    while (!crawler_ptr->stopDownloaders()) {

        std::string current = crawler_ptr->getDownloadersUrl();  // blocks the thread
        crawler_ptr->decrementDownCounter();

        if (!crawler_ptr->existsInListTS(current)) {
            crawler_ptr->addToListTS(current);
        } else {
            crawler_ptr->incrementDownCounter();
            continue;
        }
        std::string header;
        std::string body;

        CURL *curl_handle = curl_easy_init();
        if (curl_handle) {
            curl_easy_setopt(curl_handle, CURLOPT_URL, current.c_str());
            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, net::WriteCallback);
            curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
            curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEHEADER, &header);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &body);

            CURLcode res = curl_easy_perform(curl_handle);
            if (res != CURLE_OK)
                std::cout << "curl_easy_perform() failed: %s\n" << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl_handle);
        }

        auto n = header.find("image/");
        if (n == std::string::npos) {
            crawler_ptr->incrementDownCounter();
            continue;
        }
        std::string format;
        for (int i = 0; i < 3; ++i) {
            format.push_back(header.substr(n + 6)[i]);
        }
        if (header.substr(n + 6)[3] == 'g')
            format.push_back('g');

        std::ofstream fout{
                crawler_ptr->imgPath / (std::to_string(crawler_ptr->mCount++) + "." + format)}; // create regular file
        if (fout) {
            fout << body;
        }
        fout.close();

        if (crawler_ptr->mCount % 5 == 0)
            std::cout << "Count = " << crawler_ptr->mCount << std::endl;
        crawler_ptr->incrementDownCounter();
    }
}

net::crawler::crawler(std::string& url, int depth, int network_threads, int parser_threads, int downloaders_threads) {
    if (network_threads < 1 || parser_threads < 1 || depth < 1)
        throw std::runtime_error("Invalid arguments have been passed!");
    if (!net::startWith(url, "http"))
        throw std::runtime_error("Use format 'http://' or 'https://' for url");
    std::cout << "The job has started..." << std::endl;
    mProducersPool.reserve(network_threads);
    mConsumersPool.reserve(parser_threads);
    mImgDownloadersPool.reserve(downloaders_threads);
    mDepth = depth;
    mProdCounter = network_threads;
    mProducersNum = network_threads;
    mConsCounter = parser_threads;
    mConsumersNum = parser_threads;
    mDownCounter = downloaders_threads;
    mDownloadersNum = downloaders_threads;
    mCount = 0;
    net::webPage startPoint = {url, 0, "", net::getRoot(url)};
    mProducersQueue.push_back(startPoint);

    for (int i = 0; i < network_threads; ++i) {
        mProducersPool.emplace_back(producerFunction, this);
    }
    for (int i = 0; i < parser_threads; ++i) {
        mConsumersPool.emplace_back(consumerFunction, this);
    }
    for (int i = 0; i < downloaders_threads; ++i) {
        mImgDownloadersPool.emplace_back(imgDownloaderFunction, this);
    }
    imgPath = "Output";
    fs::remove_all(imgPath);
    fs::create_directory(imgPath);
}

bool net::crawler::stopProducers() {
    if (mProducersQueue.empty() && (mProdCounter == mProducersNum)) {
        mProdStop = true;
        stopConsumers();
        std::cout << "Stage 1/3 finished..." << std::endl;
        return true;
    }
    return false;
}

bool net::crawler::stopConsumers() {
    if (mProdStop) {
        if (mConsumersQueue.empty()) {
            if (mConsCounter == mConsumersNum) {
                mConsStop = true;
                std::cout << "Stage 2/3 finished..." << std::endl;
                stopDownloaders();
                return true;
            }
        }
    }
    return false;
}

bool net::crawler::stopDownloaders() {
    std::lock_guard<std::mutex> lg(mImgMtx);
    if (mConsStop) {
        if (mImgDownloadersQueue.empty()) {
            if (mDownCounter == mDownloadersNum) {
                mDownStop = true;
                mCv.notify_one();
                std::cout << "Stage 3/3 finished..." << std::endl;
                return true;
            }
        }
    }
    return false;
}

void net::crawler::writeResultIntoFolder() {
    if (!mDownStop) {
        std::unique_lock<std::mutex> ul(mMtx);
        mCv.wait(ul);
    }
    for (auto &i: mProducersPool) {
        i.detach();
    }
    for (auto &i: mConsumersPool) {
        i.detach();
    }
    for (auto &i: mImgDownloadersPool) {
        i.detach();
    }
    std::cout << "The job finished successfully!" << std::endl;
}

void net::crawler::addToListTS(const std::string &s) {
    std::scoped_lock<std::mutex> lg(mListMtx);
    mImgUrls.push_back(s);
}

bool net::crawler::existsInListTS(const std::string &s) const {
    std::scoped_lock<std::mutex> lg(mListMtx);
    return std::find(mImgUrls.begin(), mImgUrls.end(), s) != mImgUrls.end();
}

std::string net::DownloadPage(std::string& url) {
    CURL *curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, net::WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "Download of the page " << url << " failed:\n" << curl_easy_strerror(res) << std::endl;
        }

    } else {
        std::cerr << "Curl init failed" << std::endl;
    }

    return readBuffer;
}

static void net::search_for_links(GumboNode *node, std::vector<std::string>& urls) {
    if (node->type != GUMBO_NODE_ELEMENT) {
        return;
    }
    GumboAttribute *href;
    if (node->v.element.tag == GUMBO_TAG_A &&
        (href = gumbo_get_attribute(&node->v.element.attributes, "href"))) {
        urls.emplace_back(href->value);
    }

    GumboVector *children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        search_for_links(static_cast<GumboNode *>(children->data[i]), urls);
    }
}

static void net::search_for_img(GumboNode* node, std::vector<std::string>& images) {
    if (node->type != GUMBO_NODE_ELEMENT) {
        throw std::invalid_argument("");
    }

    std::string str = "image/png";
    GumboAttribute* href;
    if (node->v.element.tag == GUMBO_TAG_IMG &&
        (href = gumbo_get_attribute(&node->v.element.attributes, "href"))) {
        images.emplace_back(href->value);
    } else if (node->v.element.tag == GUMBO_TAG_IMG &&
               (href = gumbo_get_attribute(&node->v.element.attributes, "src"))) {
        images.emplace_back(href->value);
    } else if (node->v.element.tag == GUMBO_TAG_IMAGE &&
               (href = gumbo_get_attribute(&node->v.element.attributes, "src"))) {
        images.emplace_back(href->value);
    } else if (node->v.element.tag == GUMBO_TAG_LINK &&
               (href = gumbo_get_attribute(&node->v.element.attributes, "type"))) {
        if (href->value == str.c_str()) {
            images.emplace_back(href->value);
        }
    }

    GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        try {
            search_for_img(static_cast<GumboNode*>(children->data[i]), images);
        } catch (std::invalid_argument&) {
            continue;
        }
    }
}

bool net::startWith(const std::string &s1, const std::string &s2) {
    if ((s2.size() <= s1.size()) && !s1.empty() && !s2.empty()) {
        for (size_t i = 0; i < s2.size(); ++i) {
            if (s1[i] != s2[i])
                return false;
        }
        return true;
    } else {
        return false;
    }
}

bool net::endsWith(const std::string &s1, const std::string &s2) {
    if ((s2.size() <= s1.size()) && !s1.empty() && !s2.empty()) {
        for (size_t i = 0; i < s2.size(); ++i) {
            if (s1[s1.size() - 1 - i] != s2[s2.size() - 1 - i])
                return false;
        }
        return true;
    } else {
        return false;
    }
}

std::string net::getRoot(std::string &url) {
    std::string tmp;
    int counter = 0;
    for (auto i: url) {
        if (i == '/')
            ++counter;
        if (counter == 3)
            break;
        tmp.push_back(i);
    }
    return tmp;
}

std::string net::cleanBackUntilSlash(std::string &url) {
    while (*(--url.end()) != '/' && !url.empty()) {
        url.pop_back();
    }
    return url;
}
