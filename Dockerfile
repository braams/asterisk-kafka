FROM ubuntu:focal

RUN     apt-get update && \
        export DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true && \
        apt-get -y install debconf-utils && \
        echo "tzdata tzdata/Areas select Etc"        | debconf-set-selections && \
        echo "tzdata tzdata/Zones/Etc select UTC"    | debconf-set-selections && \
        echo "Etc/UTC" > /etc/timezone && \
        apt-get -y install asterisk-dev librdkafka-dev cmake kafkacat

# ARG     ASTERISK_VERSION=18.3.0
# RUN     wget -O asterisk.tar.gz http://downloads.asterisk.org/pub/telephony/asterisk/releases/asterisk-${ASTERISK_VERSION}.tar.gz && \
#         tar xzf asterisk.tar.gz && \
#         rm asterisk.tar.gz && \
#         mv asterisk-${ASTERISK_VERSION} asterisk

# WORKDIR /asterisk

# RUN     ./configure --without-dahdi --with-pjproject-bundled \
#         && make menuselect.makeopts \
#         && menuselect/menuselect --disable BUILD_NATIVE menuselect.makeopts \
#         && make -j$(grep -c ^processor /proc/cpuinfo) \
#         && make install && make basic-pbx \
#         && sed -i '$ a load = res_clioriginate.so' /etc/asterisk/modules.conf


CMD ["asterisk", "-cvvv"]