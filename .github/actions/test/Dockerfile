FROM cassandra:4.0.4

RUN apt-get update && apt-get install -y postgresql
COPY entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
