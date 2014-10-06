TEMPLATE = app
CONFIG += console debug
CONFIG -= app_bundle
CONFIG -= qt

IO_VERSION = 1.0.1
SOURCES += \
    src/io_agent.c \
    src/read_line.c \
    src/logmsg.c \
    src/io_socket.c \
    src/die_with_message.c \
    src/io_serial.c

HEADERS += \
    src/read_line.h \
    src/io_agent.h

