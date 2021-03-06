cmake_minimum_required(VERSION 2.8)
project(asterisk-kafka C)
set(CMAKE_C_STANDARD 99)

add_library(res_kafka SHARED res_kafka.c)
add_library(cdr_kafka SHARED cdr_kafka.c)
add_library(cel_kafka SHARED cel_kafka.c)
add_library(app_kafka SHARED app_kafka.c)

set_target_properties(res_kafka PROPERTIES PREFIX "")
set_target_properties(cdr_kafka PROPERTIES PREFIX "")
set_target_properties(cel_kafka PROPERTIES PREFIX "")
set_target_properties(app_kafka PROPERTIES PREFIX "")

target_link_libraries(res_kafka LINK_PUBLIC rdkafka)
target_link_libraries(cdr_kafka LINK_PUBLIC rdkafka)
target_link_libraries(cel_kafka LINK_PUBLIC rdkafka)
target_link_libraries (app_kafka LINK_PUBLIC rdkafka)

install(TARGETS res_kafka DESTINATION /usr/lib/asterisk/modules/)
install(TARGETS cdr_kafka DESTINATION /usr/lib/asterisk/modules/)
install(TARGETS cel_kafka DESTINATION /usr/lib/asterisk/modules/)
install(TARGETS app_kafka DESTINATION /usr/lib/asterisk/modules/)

install(FILES res_kafka.conf DESTINATION /etc/asterisk/)
install(FILES cdr_kafka.conf DESTINATION /etc/asterisk/)
install(FILES cel_kafka.conf DESTINATION /etc/asterisk/)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME "asterisk-kafka")
set(CPACK_PACKAGE_CONTACT "braams@braams.ru")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "asterisk,librdkafka1")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CONFFILES_FILE "${CMAKE_CURRENT_BINARY_DIR}/conffiles")
file(WRITE "${CONFFILES_FILE}" "")
file(APPEND "${CONFFILES_FILE}" "/etc/asterisk/res_kafka.conf\n")
file(APPEND "${CONFFILES_FILE}" "/etc/asterisk/cdr_kafka.conf\n")
file(APPEND "${CONFFILES_FILE}" "/etc/asterisk/cel_kafka.conf\n")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA};${CONFFILES_FILE}")

include(CPack)