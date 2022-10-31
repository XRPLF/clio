FROM ubuntu:20.04 AS boost

RUN apt-get update && apt-get install -y build-essential
ARG BOOST_VERSION_=1_75_0
ARG BOOST_VERSION=1.75.0
COPY docker/shared/build_boost.sh .
RUN apt install -y curl
RUN ./build_boost.sh ${BOOST_VERSION}
ENV BOOST_ROOT=/boost

FROM ubuntu:20.04 AS build
ENV BOOST_ROOT=/boost
COPY --from=boost /boost /boost
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --no-install-recommends -y build-essential software-properties-common pkg-config libssl-dev wget curl gpg git zlib1g-dev bison flex autoconf lsb-release
RUN apt install -y gpg-agent
RUN wget https://apt.llvm.org/llvm.sh
RUN chmod +x llvm.sh && ./llvm.sh 14 && ./llvm.sh 15
# COPY . /clio
## Install cmake
ARG CMAKE_VERSION=3.16.3
COPY docker/shared/install_cmake.sh .
RUN ./install_cmake.sh ${CMAKE_VERSION}
ENV PATH="/opt/local/cmake/bin:$PATH"
