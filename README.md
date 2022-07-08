# PhotoHunter
## A Web Image Crawler built with [CURL](https://curl.se/libcurl), boost & [gumbo-parser](https://github.com/google/gumbo-parser) (C/C++)
This multithreaded crawler will help you to download images from html-pages. It gets an url and recursively goes through
the web-pages it finds with the fixed depth.

Usage: crawler [options]  
Allowed options:  
--url arg                     set url (REQUIRED)  
--depth arg (default = 1)              depth of search on the page  
--network_threads arg (default = 1)    number of threads downloading pages  
--parser_threads arg (default = 1)     number of threads parsing pages  
--downloader_threads arg (default = 1) number of threads downloading images  
--help                        produce help message
