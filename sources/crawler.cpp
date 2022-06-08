// Copyright 2022 Andrey Vedeneev vedvedved2003@gmail.com

#include <crawler.hpp>
#include <utility>

void net::producerFunction(crawler* crawler_ptr) {
    while (true) {  // if true, all the threads will be detached

        if (crawler_ptr->stopProducers())
            return;

        net::webPage current = crawler_ptr->getProdUrl();  // blocks the thread
        crawler_ptr->decrementProdCounter();
        ++crawler_ptr->m_count;

        current.html = net::DownloadPage(current.address);
        if (current.html.empty()) {
            std::cout << "Empty page\n";
            crawler_ptr->incrementProdCounter();
            continue;
        }
        std::cout << std::this_thread::get_id() << "\tLevel: " << current.level << "\tParsing the page: " << current.address << std::endl;

        crawler_ptr->m_consumers_queue.push_back(current);

        if (current.level + 1 < crawler_ptr->m_depth) {
            GumboOutput* output = gumbo_parse(current.html.c_str());
            std::vector<std::string> LinksFound;
            net::search_for_links(output->root, LinksFound);
            for (auto& i: LinksFound) {
                if (!i.empty()) {
                    if ((net::startWith(i, "http") || net::startWith(i, "www"))) {
                        crawler_ptr->m_producers_queue.push_back(webPage{i, current.level + 1, ""});
                    } else if (!net::endsWith(current.address, ".html")
                               && !net::endsWith(current.address, ".php")) {
                        crawler_ptr->m_producers_queue.push_back(webPage{current.address + i, current.level + 1, ""});
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
                if ((net::startWith(i, "http") || net::startWith(i, "www"))) {
                    crawler_ptr->m_result.push_back(i);
                } else if (!net::endsWith(current.address, ".html")
                           && !net::endsWith(current.address, ".php")) {
                    crawler_ptr->m_result.push_back(current.address + i);
                }
            }
        }
        gumbo_destroy_output(&kGumboDefaultOptions, output);

        std::cout << std::this_thread::get_id() << ": Consumer is parsing page: " << current.address << std::endl;
        crawler_ptr->incrementConsCounter();
    }
}

net::crawler::crawler(std::string url, int depth, int network_threads, int parser_threads) {
  if (network_threads < 1 || parser_threads < 1 || depth < 1)
    throw std::runtime_error("Invalid arguments have been passed!\n");
  m_producers_pool.reserve(network_threads);
  m_consumers_pool.reserve(parser_threads);
  m_depth = depth;
  m_prodCounter = network_threads;
  m_producers_num = network_threads;
  m_consCounter = parser_threads;
  m_consumers_num = parser_threads;
  m_count = 0;
  net::webPage startPoint = { std::move(url), 0, ""};
  m_producers_queue.push_back(startPoint);

  for (int i = 0; i < network_threads; ++i) {
    m_producers_pool.emplace_back(producerFunction, this);
  }
  for (int i = 0; i < parser_threads; ++i) {
    m_consumers_pool.emplace_back(consumerFunction, this);
  }
}

bool net::crawler::stopProducers() {
    if (m_producers_queue.empty() && (m_prodCounter == m_producers_num)) {
        m_prodStop = true;
        stopConsumers();
        return true;
    }
    return false;
}

bool net::crawler::stopConsumers() {
    if (m_prodStop) {
        if (m_consumers_queue.empty()) {
            if (m_consCounter == m_consumers_num) {
                m_consStop = true;
                m_CV.notify_one();
                return true;
            }
        }
    }
    return false;
}

void net::crawler::writeResultIntoFolder() {
    if (!m_consStop) {
        std::unique_lock<std::mutex> ul(m_Mtx);
        m_CV.wait(ul);
    }
    for (auto &i: m_producers_pool) {
        i.detach();
    }
    for (auto &i: m_consumers_pool) {
        i.detach();
    }
    //TODO
    std::cout << std::this_thread::get_id() << ": Writing to the folder!\nTotal pages: " << m_count << std::endl;
    for (auto &i: m_result) {
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
        std::cout << "Found img: " << href->value << "\n";
    } else if (node->v.element.tag == GUMBO_TAG_IMG &&
               (href = gumbo_get_attribute(&node->v.element.attributes, "src"))) {
        images.emplace_back(href->value);
        std::cout << "Found img: " << href->value << "\n";
    } else if (node->v.element.tag == GUMBO_TAG_IMAGE &&
               (href = gumbo_get_attribute(&node->v.element.attributes, "src"))) {
        images.emplace_back(href->value);
        std::cout << "Found img: " << href->value << "\n";
    } else if (node->v.element.tag == GUMBO_TAG_LINK &&
               (href = gumbo_get_attribute(&node->v.element.attributes, "type"))) {
        if (href->value == str.c_str()) {
            images.emplace_back(href->value);
            std::cout << "Found img: " << href->value << "\n";
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
