// Copyright 2022 Andrey Vedeneev vedvedved2003@gmail.com

#include <crawler.hpp>

void net::producerFunction(crawler* crawler_ptr) {
    while (true) {  // if true, all the threads will be detached

        if (crawler_ptr->stopProducers())
            return;

        net::webPage current = crawler_ptr->getProdUrl();  // blocks the thread
        crawler_ptr->decrementProdCounter();
        ++crawler_ptr->mCount;

        current.html = net::DownloadPage(current.address);
        if (current.html.empty()) {
            std::cout << "Empty page\n";
            crawler_ptr->incrementProdCounter();
            continue;
        }
        std::cout << std::this_thread::get_id() << "\tLevel: " << current.level << "\tParsing the page: " << current.address << std::endl;

        crawler_ptr->mConsumersQueue.push_back(current);

        if (current.level + 1 < crawler_ptr->mDepth) {
            GumboOutput* output = gumbo_parse(current.html.c_str());
            std::vector<std::string> LinksFound;
            net::search_for_links(output->root, LinksFound);
            for (auto& i: LinksFound) {
                if (!i.empty()) {
                  //  std::cout << "URL found, root: " << current.root << "; current: " << current.root << ";   i: " << i << std::endl;
                    if (i[0] == '/') {
                        crawler_ptr->mProducersQueue.push_back(webPage{current.root + i, current.level + 1, "", current.root} );
                    } else if (net::startWith(i, "http")) {
                        crawler_ptr->mProducersQueue.push_back(webPage{i, current.level + 1, "", net::getRoot(i)});
                    } else if (net::endsWith(current.address, ".php") || net::endsWith(current.address, "html")) {
                        crawler_ptr->mProducersQueue.push_back(webPage{net::cleanBackUntilSlash(current.address) + i, current.level + 1, "", current.root });
                    } else if (*current.address.end() == '/') {
                        crawler_ptr->mProducersQueue.push_back(webPage{current.address + i, current.level + 1, "", current.root} );
                    } else {
                        crawler_ptr->mProducersQueue.push_back(webPage{current.address + "/" + i, current.level + 1, "", current.root} );
                    }
                }
            }
            gumbo_destroy_output(&kGumboDefaultOptions, output);
        }
        crawler_ptr->incrementProdCounter();
    }
}

void net::consumerFunction(crawler* crawler_ptr) {
    while (!crawler_ptr->stopConsumers()) {  // if true, all the threads will be detached
        net::webPage current = crawler_ptr->getConsUrl();  // blocks the thread
        crawler_ptr->decrementConsCounter();

        GumboOutput *output = gumbo_parse(current.html.c_str());
        std::vector<std::string> ImgFound;
        net::search_for_img(output->root, ImgFound);
        for (auto &i: ImgFound) {
            if (!i.empty()) {
               // std::cout << "IMG WITH URL root: " << current.root << "; current: " << current.address << ";   i: " << i << std::endl;
                if (i[0] == '/') {
                    crawler_ptr->mResult.insert(current.root + i );
                } else if (net::startWith(i, "http")) {
                    crawler_ptr->mResult.insert(i);
                } else if (net::endsWith(current.address, ".php") || net::endsWith(current.address, "html")) {
                    crawler_ptr->mResult.insert(net::cleanBackUntilSlash(current.address) + i );
                } else if (*current.address.end() == '/') {
                    crawler_ptr->mResult.insert(current.address + i);
                } else {
                    crawler_ptr->mResult.insert(current.address + "/" + i);
                }
            }
        }
        gumbo_destroy_output(&kGumboDefaultOptions, output);

        std::cout << std::this_thread::get_id() << ": Consumer is parsing page: " << current.address << std::endl;
        crawler_ptr->incrementConsCounter();
    }
}

void net::imgDownloaderFunction(net::crawler *crawler_ptr) {
    while (!crawler_ptr->stopDownloaders()) {  // if true, all the threads will be detached

        std::string current = crawler_ptr->getDownloadersUrl();  // blocks the thread
        crawler_ptr->decrementDownCounter();

        std::string header;
        std::string body;

        CURL *curl_handle = curl_easy_init();
        if(curl_handle)
        {
            curl_easy_setopt(curl_handle, CURLOPT_URL, current.c_str());
            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, net::WriteCallback);
            curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
            curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEHEADER, &header);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &body);

            CURLcode res = curl_easy_perform(curl_handle);
            if(res != CURLE_OK)
                std::cout<< "curl_easy_perform() failed: %s\n" << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl_handle);
        }

        //TODO writing to files

        crawler_ptr->incrementDownCounter();
    }
}

net::crawler::crawler(std::string& url, int depth, int network_threads, int parser_threads) {
  if (network_threads < 1 || parser_threads < 1 || depth < 1)
    throw std::runtime_error("Invalid arguments have been passed!");
  if (!net::startWith(url, "http"))
      throw std::runtime_error("Use format 'http://' or 'https://' for url");
  mProducersPool.reserve(network_threads);
  mConsumersPool.reserve(parser_threads);
    mDepth = depth;
    mProdCounter = network_threads;
    mProducersNum = network_threads;
    mConsCounter = parser_threads;
    mConsumersNum = parser_threads;
    mCount = 0;
  net::webPage startPoint = { url, 0, "", net::getRoot(url)};
  mProducersQueue.push_back(startPoint);

  for (int i = 0; i < network_threads; ++i) {
    mProducersPool.emplace_back(producerFunction, this);
  }
  for (int i = 0; i < parser_threads; ++i) {
    mConsumersPool.emplace_back(consumerFunction, this);
  }
}

bool net::crawler::stopProducers() {
    if (mProducersQueue.empty() && (mProdCounter == mProducersNum)) {
        mProdStop = true;
        stopConsumers();
        return true;
    }
    return false;
}

bool net::crawler::stopConsumers() {
    if (mProdStop) {
        if (mConsumersQueue.empty()) {
            if (mConsCounter == mConsumersNum) {
                mConsStop = true;
                mCv.notify_one();
                return true;
            }
        }
    }
    return false;
}



void net::crawler::writeResultIntoFolder() {
    if (!mConsStop) {
        std::unique_lock<std::mutex> ul(mMtx);
        mCv.wait(ul);
    }
    for (auto &i: mProducersPool) {
        i.detach();
    }
    for (auto &i: mConsumersPool) {
        i.detach();
    }
    //TODO
    std::cout << std::this_thread::get_id() << ": Writing to the folder!\nTotal pages: " << mCount << "\nResult << " << mResult.size() << std::endl;
    for (auto &i: mResult) {
        std::cout << i << std::endl;
    }
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

size_t net::write_data( char *ptr, size_t size, size_t nmemb, FILE* data)
{
    return fwrite(ptr, size, nmemb, data);
}
