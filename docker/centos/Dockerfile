# FROM centos:7 as deps
FROM centos:7 as build

ENV CLIO_DIR=/opt/clio/
# ENV OPENSSL_DIR=/opt/openssl

RUN yum -y install git epel-release centos-release-scl perl-IPC-Cmd openssl
RUN yum install -y devtoolset-11
ENV version=3.16
ENV build=3
# RUN curl -OJL https://cmake.org/files/v$version/cmake-$version.$build.tar.gz
COPY docker/shared/install_cmake.sh /install_cmake.sh
RUN /install_cmake.sh 3.16.3 /usr/local
RUN source /opt/rh/devtoolset-11/enable
WORKDIR /tmp
# RUN mkdir $OPENSSL_DIR && cd $OPENSSL_DIR
COPY docker/centos/build_git_centos7.sh build_git_centos7.sh

RUN ./build_git_centos7.sh
RUN git clone https://github.com/openssl/openssl
WORKDIR /tmp/openssl
RUN git checkout OpenSSL_1_1_1q
#--prefix=/usr --openssldir=/etc/ssl --libdir=lib no-shared zlib-dynamic
RUN SSLDIR=$(openssl version -d | cut -d: -f2 | tr -d [:space:]\") && ./config -fPIC --prefix=/usr --openssldir=${SSLDIR} zlib shared && \
    make -j $(nproc) && \
    make install_sw
WORKDIR /tmp
# FROM centos:7 as build

RUN git clone https://github.com/xrplf/clio.git
COPY docker/shared/build_boost.sh build_boost.sh
ENV OPENSSL_ROOT=/opt/local/openssl
ENV BOOST_ROOT=/boost
RUN source scl_source enable devtoolset-11 && /tmp/build_boost.sh 1.75.0
RUN yum install -y bison flex
RUN yum install -y rpmdevtools rpmlint
RUN source /opt/rh/devtoolset-11/enable && cd /tmp/clio && \
    cmake -B build -DBUILD_TESTS=1 && \
    cmake --build build --parallel $(nproc)
RUN mkdir output
RUN strip clio/build/clio_server && strip clio/build/clio_tests
RUN cp clio/build/clio_tests output/ && cp clio/build/clio_server output/
RUN cp clio/example-config.json output/example-config.json

FROM centos:7
COPY --from=build /tmp/output /clio
RUN mkdir -p /opt/clio/etc && mv /clio/example-config.json /opt/clio/etc/config.json

CMD ["/clio/clio_server", "/opt/clio/etc/config.json"]
