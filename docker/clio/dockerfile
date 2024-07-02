FROM ubuntu:22.04

COPY ./clio_server /opt/clio/bin/clio_server

RUN ln -s /opt/clio/bin/clio_server /usr/local/bin/clio_server && \
    mkdir -p /opt/clio/etc/ && \
    mkdir -p /opt/clio/log/ && \
    groupadd -g 10001 clio && \
    useradd -u 10000 -g 10001 -s /bin/bash clio && \
    chown clio:clio /opt/clio/log && \
    apt update && \
    apt install -y libatomic1

USER clio
ENTRYPOINT ["/opt/clio/bin/clio_server"]
CMD ["--conf", "/opt/clio/etc/config.json"]
