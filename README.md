# Asterisk CDR/CEL backend for Kafka

This module requires the librdkafka library, avaialble from  https://github.com/edenhill/librdkafka

## Single node Kafka for tests

docker build -t kafka-single-node kafka

docker run -it --rm --net=host kafka-single-node

## Build with cmake

mkdir build

cd build

cmake ..

make

make package


## Test module 

docker build -t asterisk-kafka-buildenv .

docker run --rm -it --net=host -v "$(pwd):/asterisk-kafka" -w /asterisk-kafka asterisk-kafka-buildenv bash

dpkg -i build/asterisk-kafka_*.deb

asterisk -cvvv

module show like kafka

cdr show status

channel originate Local/s@demo application NoOp()

!kafkacat -C -b localhost:9092 -t asterisk_cdr -o -1 -e

## TODO
* CEL
* Extra user fields
* Extra librdkafka configuration (https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md) 
* Compression
