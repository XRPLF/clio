FROM ubuntu:22.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        libatomic1 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -g 10001 clio \
    && useradd -u 10000 -g 10001 -s /bin/bash clio

COPY ./clio_server /opt/clio/bin/clio_server

RUN ln -s /opt/clio/bin/clio_server /usr/local/bin/clio_server \
    && mkdir -p /opt/clio/etc/ \
    && mkdir -p /opt/clio/log/ \
    && chown clio:clio /opt/clio/log

USER clio
ENTRYPOINT ["/opt/clio/bin/clio_server"]
CMD ["--conf", "/opt/clio/etc/config.json"]
