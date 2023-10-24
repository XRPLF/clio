FROM centos:7 as builder

# https://github.com/docker/buildx/issues/379
RUN ulimit -n 1024000 && yum update -y

COPY docker/build.sh /tmp/build.sh
RUN /tmp/build.sh

COPY docker/build_git.sh /tmp/build_git.sh
RUN /tmp/build_git.sh

COPY docker/install_cmake.sh /tmp/install_cmake.sh
RUN /tmp/install_cmake.sh

FROM builder as build_clio
COPY . /clio
COPY docker/build_clio.sh /tmp/build_clio.sh
WORKDIR /clio
RUN /tmp/build_clio.sh

FROM debian:11-slim as clio
COPY --from=build_clio /opt/clio /opt/clio
COPY docker/test.sh /test.sh
