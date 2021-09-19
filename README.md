# Asterisk Kafka

Set of modules for producing CDR, CEL and other messages from Asterisk to Apache Kafka.

This module requires the [librdkafka](https://github.com/edenhill/librdkafka) library.

## Build for deb-based distro

Tested on Asterisk 16 @ Ubuntu 20.04

    apt-get install asterisk-dev librdkafka-dev cmake git
    git clone https://github.com/braams/asterisk-kafka
    cd asterisk-kafka
    mkdir -p build
    cd build
    cmake ..
    make package
    dpkg -i asterisk-kafka*.deb


## Build for rmp-based distro

Tested on Asterisk 16 @ Centos 8

(It's assumed you already have asterisk headers in /usr/src/asterisk and modules in /usr/lib/asterisk/modules)

    yum install -y librdkafka-devel cmake git
    git clone https://github.com/braams/asterisk-kafka    
    cd asterisk-kafka
    mkdir -p build
    cd build
    cmake ..
    make
    make install

## Single node Kafka for tests

    docker build -t kafka-single-node kafka
    docker run -it --rm --net=host kafka-single-node


## Test env 
Dive into test env

    docker build -t asterisk-kafka-buildenv .
    docker run --rm -it --net=host -v "$(pwd):/asterisk-kafka" -w /asterisk-kafka asterisk-kafka-buildenv bash

Install packages and run Asterisk

    dpkg -i build/asterisk-kafka_*.deb
    asterisk -cvvv

Check it works

    module show like kafka
    cdr show status
    cel show status
    kafka produce hello world
    channel originate Local/100@demo application KafkaProduce some,other

Consume messages

    kafkacat -C -b localhost:9092 -t asterisk_cdr -o -1

## Clickhouse

    docker run --rm -it --net=host -v "$(pwd)/clickhouse:/docker-entrypoint-initdb.d" yandex/clickhouse-server

## TODO
* Extra user fields
* Extra librdkafka configuration (https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md) 
* Compression
