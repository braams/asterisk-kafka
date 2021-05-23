INSTALL_PREFIX=
HEADERS_DIR=/asterisk/include
MODULES_DIR=$(INSTALL_PREFIX)/usr/lib/asterisk/modules
CONFIGS_DIR=$(INSTALL_PREFIX)/etc/asterisk

INSTALL = install
SAMPLENAME = cdr_kafka.conf.sample
CONFNAME = $(basename $(SAMPLENAME))

INCLUDE+= -I$(HEADERS_DIR)
#INCLUDE+= -I/usr/include/

TARGET = cdr_kafka.so
OBJECTS = cdr_kafka.o

CFLAGS += -I. $(INCLUDE)
CFLAGS += -DHAVE_STDINT_H=1
CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Winit-self -Wmissing-format-attribute\
          -Wformat=2 -g -fPIC -D_GNU_SOURCE -D'AST_MODULE="cdr_kafka"' -D'AST_MODULE_SELF_SYM=__internal_cdr_kafka_self'
LIBS += -lrdkafka
LDFLAGS = -Wall -shared

.PHONY: install clean

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) -o $@ $<

install: $(TARGET)
	mkdir -p $(DESTDIR)$(MODULES_DIR)
	install -m 644 $(TARGET) $(DESTDIR)$(MODULES_DIR)
	@echo " +----------- cdr_kafka installed ------------+"
	@echo " +                                           +"
	@echo " + cdr_kafka has successfully been installed  +"
	@echo " + If you would like to install the sample   +"
	@echo " + configuration file run:                   +"

	@echo " +              make samples                 +"
	@echo " +-------------------------------------------+"

clean:
	rm -f $(OBJECTS)
	rm -f $(TARGET)

samples:
	$(INSTALL) -m 644 $(SAMPLENAME) $(DESTDIR)$(CONFIGS_DIR)/$(CONFNAME)
	@echo " ------- cdr_kafka config installed ---------"