sources = die_with_message.c \
	read_line.c \
	logmsg.c \
	io_serial.c \
	io_socket.c \
	io_agent.c \
	
headers = read_line.h \
	io_agent.h \

LDFLAGS=-pthread

CFLAGS=-Wall

ifeq ($(DEBUG_DEF),1)
	DEBUG = -g -O0 -DDEBUG
else
	DEBUG = -O2
endif

all: io-agent

io-agent: $(sources) $(headers)
	$(CC) -DTIO_VERSION='"$(AGENT_VERSION)"' $(CFLAGS) $(LDFLAGS) $(DEBUG) -o $@ $(sources)

clean:
	$(RM) io-agent

.PHONY: all clean
