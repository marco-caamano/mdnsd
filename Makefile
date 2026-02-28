CC := cc
CFLAGS := -std=c99 -Wall -Wextra -Werror -pedantic -D_POSIX_C_SOURCE=200809L
LDFLAGS :=

SHARED_INCLUDES := -Ishared/include
SERVER_INCLUDES := -Iserver/include $(SHARED_INCLUDES)
CLIENT_INCLUDES := -Iclient/include $(SHARED_INCLUDES)

SHARED_SRC := shared/src/log.c shared/src/mdns.c shared/src/hostdb.c
SERVER_SRC := server/src/main.c server/src/args.c server/src/config.c server/src/socket.c $(SHARED_SRC)
CLIENT_SRC := client/src/main.c client/src/args.c $(SHARED_SRC)

SERVER_OBJ := $(patsubst %.c,build/%.o,$(SERVER_SRC))
CLIENT_OBJ := $(patsubst %.c,build/%.o,$(CLIENT_SRC))

SERVER_TARGET := mdnsd
CLIENT_TARGET := mdnsc

.PHONY: all clean install uninstall

all: $(SERVER_TARGET) $(CLIENT_TARGET)

build:
	mkdir -p build/shared/src build/server/src build/client/src

$(SERVER_TARGET): build $(SERVER_OBJ)
	$(CC) $(SERVER_OBJ) -o $@ $(LDFLAGS)

$(CLIENT_TARGET): build $(CLIENT_OBJ)
	$(CC) $(CLIENT_OBJ) -o $@ $(LDFLAGS)

build/shared/%.o: shared/%.c
	$(CC) $(CFLAGS) $(SHARED_INCLUDES) -c $< -o $@

build/server/%.o: server/%.c
	$(CC) $(CFLAGS) $(SERVER_INCLUDES) -c $< -o $@

build/client/%.o: client/%.c
	$(CC) $(CFLAGS) $(CLIENT_INCLUDES) -c $< -o $@

install: $(SERVER_TARGET) $(CLIENT_TARGET)
	install -m 0755 $(SERVER_TARGET) /usr/local/bin/$(SERVER_TARGET)
	install -m 0755 $(CLIENT_TARGET) /usr/local/bin/$(CLIENT_TARGET)

uninstall:
	rm -f /usr/local/bin/$(SERVER_TARGET) /usr/local/bin/$(CLIENT_TARGET)

clean:
	rm -rf build $(SERVER_TARGET) $(CLIENT_TARGET)
