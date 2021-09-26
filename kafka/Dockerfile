FROM openjdk:11-jre-buster

ARG SCALA_VERSION=2.13
ARG KAFKA_VERSION=2.8.1
ARG KCJ_VERSION=10.2.1
ARG MCJ_VERSION=8.0.26

ARG KAFKA_URL=https://mirror.yandex.ru/mirrors/apache/kafka/"$KAFKA_VERSION"/kafka_"$SCALA_VERSION"-"$KAFKA_VERSION".tgz
ARG CONNECT_JDBC_URL=https://d1i4a15mxbxib1.cloudfront.net/api/plugins/confluentinc/kafka-connect-jdbc/versions/"$KCJ_VERSION"/confluentinc-kafka-connect-jdbc-"$KCJ_VERSION".zip
ARG MYSQL_JDBC_URL=https://mirror.yandex.ru/mirrors/ftp.mysql.com/Downloads/Connector-J/mysql-connector-java-"$MCJ_VERSION".zip

RUN echo "$KAFKA_URL" && wget -q "$KAFKA_URL" -O /tmp/kafka_"$SCALA_VERSION"-"$KAFKA_VERSION".tgz && \
    tar xfz /tmp/kafka_"$SCALA_VERSION"-"$KAFKA_VERSION".tgz -C /opt && \
    rm /tmp/kafka_"$SCALA_VERSION"-"$KAFKA_VERSION".tgz

RUN mkdir /opt/connectors && \
    wget -q "$CONNECT_JDBC_URL" -O confluentinc-kafka-connect-jdbc-"$KCJ_VERSION".zip && \
    unzip confluentinc-kafka-connect-jdbc-"$KCJ_VERSION".zip && \
    cp confluentinc-kafka-connect-jdbc-"$KCJ_VERSION"/lib/*.jar /opt/connectors/ && \
    wget -q "$MYSQL_JDBC_URL" -O mysql-connector-java_"$MCJ_VERSION".zip && \
    unzip mysql-connector-java_"$MCJ_VERSION".zip && \
    cp mysql-connector-java-"$MCJ_VERSION"/mysql-connector-java-"$MCJ_VERSION".jar /opt/connectors/ && \
    rm -r confluentinc-kafka-connect-jdbc-"$KCJ_VERSION" mysql-connector-java-"$MCJ_VERSION" && \
    rm confluentinc-kafka-connect-jdbc-"$KCJ_VERSION".zip mysql-connector-java_"$MCJ_VERSION".zip

WORKDIR /opt/kafka_"$SCALA_VERSION"-"$KAFKA_VERSION"/

RUN ./bin/kafka-storage.sh format -t NotReallyUniqueLetters -c ./config/kraft/server.properties

CMD ["./bin/kafka-server-start.sh", "./config/kraft/server.properties"]