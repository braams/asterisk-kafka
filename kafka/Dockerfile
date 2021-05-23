FROM openjdk:11-jre-buster

ARG SCALA_VERSION=2.13
ARG KAFKA_VERSION=2.8.0
ARG KAFKA_URL=https://mirror.linux-ia64.org/apache/kafka/"$KAFKA_VERSION"/kafka_"$SCALA_VERSION"-"$KAFKA_VERSION".tgz

RUN echo "$KAFKA_URL" && wget -q "$KAFKA_URL" -O /tmp/kafka_"$SCALA_VERSION"-"$KAFKA_VERSION".tgz && \
    tar xfz /tmp/kafka_"$SCALA_VERSION"-"$KAFKA_VERSION".tgz -C /opt && \
    rm /tmp/kafka_"$SCALA_VERSION"-"$KAFKA_VERSION".tgz

WORKDIR /opt/kafka_"$SCALA_VERSION"-"$KAFKA_VERSION"/

RUN ./bin/kafka-storage.sh format -t NotReallyUniqueLetters -c ./config/kraft/server.properties

CMD ["./bin/kafka-server-start.sh", "./config/kraft/server.properties"]