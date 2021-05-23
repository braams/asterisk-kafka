# Asterisk CDR/CEL backend for Kafka

This module requires the librdkafka library, avaialble from  https://github.com/edenhill/librdkafka

## Single node Kafka for tests

docker build -t kafka-single-node kafka

docker run -it --rm --net=host kafka-single-node

## Build and test module 

docker build -t asterisk-kafka-buildenv .

docker run --rm -it --net=host -v "$(pwd):/asterisk-kafka" -w /asterisk-kafka asterisk-kafka-buildenv bash

make

make install

make samples

asterisk -cvvv

module load cdr_kafka.so

cdr show status

channel originate Local/2565551100@Main-IVR application NoOp

!kafkacat -C -b localhost:9092 -t asterisk_cdr -o -1 -e

## TODO
* CEL
* Datetime format
* Extra user fields
* Extra librdkafka configuration (https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md) 
* Compression
