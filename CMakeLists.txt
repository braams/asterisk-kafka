cmake_minimum_required(VERSION 3.16)
project(asterisk-kafka C)
set(CMAKE_C_STANDARD 99)

add_library(cdr_kafka SHARED cdr_kafka.c)
set_target_properties(cdr_kafka PROPERTIES PREFIX "")
target_link_libraries (cdr_kafka LINK_PUBLIC rdkafka)

install(TARGETS cdr_kafka DESTINATION /usr/lib/asterisk/modules/)
install(FILES cdr_kafka.conf DESTINATION /etc/asterisk/)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME "asterisk-kafka")
set(CPACK_PACKAGE_CONTACT "braams@braams.ru")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "asterisk,librdkafka1")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)