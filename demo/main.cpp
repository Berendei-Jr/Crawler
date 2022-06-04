//#include <example.hpp>
#include <crawler.hpp>

namespace po = boost::program_options;
namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp> // from <boost/asio.hpp>

int main(int argc, char* argv[])
{
  std::string url;
  int depth;
  int network_threads;
  int parser_threads;
  std::string output;

  po::options_description desc("Allowed options");
  desc.add_options()
          ("url", po::value<std::string>(&url), "set url (REQUIRED)")
          ("depth", po::value<int>(&depth)->default_value(1), "depth of search on the page")
          ("network_threads", po::value<int>(&network_threads)->default_value(1), "number of threads downloading pages")
          ("parser_threads", po::value<int>(&parser_threads)->default_value(1), "number of threads parsing pages")
          ("output", po::value<std::string>(&output)->default_value("output.txt"), "output file")
          ("help", "produce help message");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }

  if (!vm.count("url")) {
    std::cout << "Usage: crawler [options]\n" << desc;
    return 1;
  }

  net::crawler C(url, 1, network_threads, parser_threads);
  C.writeResultIntoFolder();
}
