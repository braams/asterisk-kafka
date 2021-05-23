# Asterisk CDR/CEL backend for Kafka

docker build -t asterisk-kafka-buildenv .

docker run --rm -it --net=host -v "$(pwd):/usr/src" asterisk-kafka-buildenv bash

cd /usr/src

apt-get install librdkafka-dev

make

make install

make samples

asterisk -cvvv

module load cdr_kafka.so


