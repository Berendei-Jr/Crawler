// Copyright 2021 Andrey Vedeneev vedvedved2003@gmail.com

#include <stdexcept>
#include <example.hpp>
#include <utility>

namespace po = boost::program_options;
namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>

bool control_t::Stop()
{
  return m_queue.empty() && m_counter == m_threads;
}

control_t::control_t(int thr, int depth, std::string url,
                     std::condition_variable& cv,
                     tsqueue<page1_t>& m, std::atomic<bool>& flag) :
                     m_threads(thr), m_depth(depth),
                     m_counter(thr), m_StopToMain(cv),
                     m_main_queue(m), m_flag(flag)
{
  page_t first(std::move(url));
  m_queue.push_back(first);
}

void NetworkThreadFunction(std::shared_ptr<control_t> control)
{
  while (true) {
    if (control->Stop()) {
      control->NotifyMain();
      break;
    }
    page_t url = control->GetPage();
    if (control->Exists(url))
      continue;
    control->DecrementCounter();

   // std::cout << "Got page " << url._url << " with level " << url._level
    //          << "\n";
    std::string html = Download(url._url);

    if (html.empty()) {
      control->IncrementCounter();
      continue;
    }

    page1_t tmp(url._url, html);
    control->AddToMainQueue(tmp);
    if (url._level < control->GetDepth()) {
      std::stringstream in(html, std::ios::in | std::ios::binary);
      if (!in) {
        std::cerr << "Downloading error!\n";
      }

      std::string contents;
      in.seekg(0, std::ios::end);
      contents.resize(in.tellg());
      in.seekg(0, std::ios::beg);
      in.read(&contents[0], contents.size());

      GumboOutput* output = gumbo_parse(contents.c_str());
      std::vector<std::string> LinksFound;

      search_for_links(output->root, LinksFound);
      gumbo_destroy_output(&kGumboDefaultOptions, output);

      for (auto& it : LinksFound) {
        std::string new_url = it;
        if (it.front() == '/' && url._url.back() != '/')
        {
          new_url.clear();
          new_url.append(url._url);
          new_url.append(it);
        }
        page_t page(new_url, url._level + 1);
        control->AddLink(page);
      }
    }

    control->IncrementCounter();
  }
}

std::string DownloadPage(std::string& url)
{
  try
  {
    auto port = "443";

    std::string h1 = "http://";
    std::string h2 = "https://";
    auto start_pos = url.find(h1);
    if (start_pos != std::string::npos)
    {
      url.erase(start_pos, h1.length());
      port = "80";
    }
    start_pos = url.find(h2);
    if (start_pos != std::string::npos)
    {
      url.erase(start_pos, h2.length());
    }

    auto host = url.c_str();
    auto target = "/";
    size_t i = 0;
    std::string new_host, new_target;
    if (!url.empty()) {
      while (url[i] != '/' && i < url.length() - 1) {
        i++;
      }
      if (i < url.length() - 1) {
        for (size_t j = 0; j < i; j++) {
          new_host.push_back(url[j]);
        }
        for (size_t j = i; j < url.length(); j++) {
          new_target.push_back(url[j]);
        }
        host = new_host.c_str();
        target = new_target.c_str();
      }
    }

    int version = 11;

    // The io_context is required for all I/O
    net::io_context ioc;

    // The SSL context is required, and holds certificates
    ssl::context ctx(ssl::context::tlsv12_client);

    // This holds the root certificate used for verification
    load_root_certificates(ctx);

    // Verify the remote server's certificate
    ctx.set_verify_mode(ssl::verify_peer);

    // These objects perform our I/O
    tcp::resolver resolver(ioc);
    beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(stream.native_handle(), host))
    {
      beast::error_code ec{static_cast<int>(::ERR_get_error()),
                           net::error::get_ssl_category()};
      throw beast::system_error{ec};
    }

    // Look up the domain name
    auto const results = resolver.resolve(host, port);

    // Make the connection on the IP address we get from a lookup
    beast::get_lowest_layer(stream).connect(results);

    // Perform the SSL handshake
    stream.handshake(ssl::stream_base::client);

    // Set up an HTTP GET request message
    http::request<http::string_body> req{http::verb::get, target, version};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Send the HTTP request to the remote host
    http::write(stream, req);

    // This buffer is used for reading and must be persisted
    beast::flat_buffer buffer;

    // Declare a container to hold the response
    http::response<http::dynamic_body> res;

    // Receive the HTTP response
    http::read(stream, buffer, res);

    std::stringstream s;
    s << res;
    std::string str = s.str();

    // Gracefully close the stream
    beast::error_code ec;
    stream.shutdown(ec);
    if (ec == net::error::eof)
    {
      ec = {};
    }
    if (ec)
      throw beast::system_error{ec};

    return str;
  }
  catch(std::exception const& e)
  {
    return "";
  }
}
void ConsumerThreadFunction(std::shared_ptr<cons_control_t> control) {
  while (true)
  {
    if (control->Stop()) {
      control->NotifyMain();
      break;
    }
    page1_t page = control->GetPage();
    control->DecrementCounter();

    std::stringstream in(page._body, std::ios::in | std::ios::binary);
    if (!in) {
      std::cerr << "Parsing error!\n";
    }

    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());

    GumboOutput* output = gumbo_parse(contents.c_str());
    std::vector<std::string> LinksFound;

    search_for_img(output->root, LinksFound);
    gumbo_destroy_output(&kGumboDefaultOptions, output);

    for (auto& it : LinksFound) {
      if (!(control->Exists(it) || control->Exists(page._url.append(it)))) {
        std::string new_url = it;
        if (it.front() == '/' && page._url.back() != '/')
        {
          new_url.clear();
          new_url.append(page._url);
          new_url.append(it);
        }
        control->AddLink(new_url);
      }
    }

    control->IncrementCounter();
  }
}

cons_control_t::cons_control_t(int thr, tsqueue<page1_t>& queue,
                               tsqueue<std::string>& img,
                               std::condition_variable& cv,
                               std::atomic<bool>& flag) :
                               m_threads(thr), m_queue(queue), m_img(img),
                               m_counter(thr), m_StopToMain(cv),
                               m_flag(flag) {}
