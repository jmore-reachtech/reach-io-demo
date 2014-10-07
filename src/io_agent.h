/*
 * translate_agent.h
 *
 *  Created on: Oct 7, 2011
 *      Author: jhorn
 */

#ifndef IO_AGENT_H_
#define IO_AGENT_H_

#include <syslog.h>
#include <sys/stat.h>
#include <termios.h>

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define BACKLOG 5
#define READ_BUF_SIZE 2048

struct LineBuffer
{
    char store[READ_BUF_SIZE];
    off_t pos;
};

struct FdPair {
    int inFd;
    int outFd;
    int maxFd;
};

/* functions in sio_serial.c */
void ioTtySetParams(int localEcho, int enableRS485);
speed_t getTtySerialRate(unsigned int serialRate);
int ioTtyInit(const char *tty_dev, unsigned int serialRate);
int ioTtyRead(int fd, char *msgBuff, size_t bufSize, off_t *currPos);
void ioTtyWrite(int serialFd, char *msgBuff, int buffSize);

/* functions exported from io_socket.c */
int ioQvSocketInit(unsigned short port, int *addressFamily,
    const char *socketPath);
int ioQvSocketAccept(int listenFd, int addressFamily);
void ioQvSocketWrite(int socketFd, const char *buf);

/* functions exported from die_with_message.c */
void dieWithSystemMessage(const char *msg);

/* functions exported from logmsg.c */
void LogOpen(const char *ident, int logToSyslog, const char *logFilePath,
    int verboseFlag);
void LogMsg(int level, const char *fmt, ...);

/* qml-viewer should use these same socket specifications */
#define	IO_DEFAULT_AGENT_PORT 7885
#define IO_AGENT_UNIX_SOCKET "/tmp/tioSocket"
/* Set up serial ports */
#define DEFAULT_SERIAL_RATE1 115200
#define DEFAULT_SERIAL_RATE2 115200
#define DEFAULT_SERIAL_DEVICE1 "/dev/ttySP1"
#define DEFAULT_SERIAL_DEVICE2 "/dev/ttySP2"

/* handy constants */
#define LOCALHOST_ADDR "127.0.0.1"

#endif /* IO_AGENT_H_ */
