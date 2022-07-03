FROM ubuntu:21.04
LABEL version="0.1"
LABEL maintainer="vedvedved2003@gmail.com"

RUN apt-get update
RUN apt-get install -y clang g++ cmake curl libgtest-dev
#RUN apt-get install -y libboost-dev
#RUN cmake . -B _build && cd _build && make && ./crawler --help
