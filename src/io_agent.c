#define _XOPEN_SOURCE 400

#include <errno.h>
//#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>

#include "io_agent.h"
#include "read_line.h"

static int keepGoing;
static const char *progName;

static void ioAgent(unsigned short ioPort, const char *ioSocketPath);
static inline int max(int a, int b) { return (a > b) ? a : b; }
static void prepend(char* s, const char* t);


int main(int argc, char** argv)
{
    const char *logFilePath = 0;
    /*
     * syslog isn't installed on the target so it's disabled in this program
     * by requiring an argument to -o|--log.
     */
    int logToSyslog = 0;
    unsigned short tcpPort = 0;
    int daemonFlag = 0;
    int verboseFlag = 1;
    int enableRS485 = 0;
    int localEcho = 0;

    /* allocate memory for progName since basename() modifies it */
    const size_t nameLen = strlen(argv[0]) + 1;
    char arg0[nameLen];
    memcpy(arg0, argv[0], nameLen);
    progName = basename(arg0);

    verboseFlag = 1;

    /* set up logging to syslog or file; will be STDERR not told otherwise */
    LogOpen(progName, logToSyslog, logFilePath, verboseFlag);

    /* keep STDIO going for now */
    if (daemonFlag) {
        if (daemon(0, 1) != 0) {
            dieWithSystemMessage("daemon() failed");
        }
    }

    ioTtySetParams(localEcho, enableRS485);
    ioAgent(tcpPort, IO_AGENT_UNIX_SOCKET);

    exit(EXIT_SUCCESS);
}


static void ioInterruptHandler(int sig)
{
    keepGoing = 0;
}

/* Prepends t into s. Assumes s has enough space allocated
** for the combined string.
*/
void prepend(char* s, const char* t)
{
    size_t len = strlen(t);
    size_t i;

    memmove(s + len, s, strlen(s) + 1);

    for (i = 0; i < len; ++i)
    {
        s[i] = t[i];
    }
}


static void ioAgent(unsigned short tcpPort, const char *ioSocketPath)
{
    fd_set currFdSet;
    int connectedFd = -1;  /* not currently connected */
    struct LineBuffer fromQv;
    fromQv.pos = 0;

    {
        /* install a signal handler to remove the socket file */
        struct sigaction a;
        memset(&a, 0, sizeof(a));
        a.sa_handler = ioInterruptHandler;
        if (sigaction(SIGINT, &a, 0) != 0) {
            LogMsg(LOG_ERR, "sigaction() failed, errno = %d\n", errno);
            exit(1);
        }
    }

    /* open the server socket */
    int addressFamily = 0;
    const int listenFd = ioQvSocketInit(tcpPort, &addressFamily,
        ioSocketPath);
    if (listenFd < 0) {
        /* open failed, can't continue */
        LogMsg(LOG_ERR, "could not open server socket\n");
        return;
    }

    FD_ZERO(&currFdSet);
    FD_SET(listenFd, &currFdSet);

    /* execution remains in this loop until a fatal error or SIGINT */
    keepGoing = 1;
    while (keepGoing) {
        int nfds = 0;
        /* serial port 1 */
        off_t serialPos1 = 0;
        char ttyBuff1[READ_BUF_SIZE];
        struct FdPair serialFds1;

        /* serial port 2 */
        off_t serialPos2 = 0;
        char ttyBuff2[READ_BUF_SIZE];
        struct FdPair serialFds2;

        /* try opening serial device 1*/
        serialFds1.inFd = ioTtyInit(DEFAULT_SERIAL_DEVICE1, DEFAULT_SERIAL_RATE1);
        if (serialFds1.inFd < 0) {
            /* open failed, can't continue */
            LogMsg(LOG_ERR, "could not open serial port %s\n", DEFAULT_SERIAL_DEVICE1);
            break;
        } else {
            serialFds1.outFd = serialFds1.maxFd = serialFds1.inFd;
        }

        FD_SET(serialFds1.inFd, &currFdSet);
        if (serialFds1.inFd != serialFds1.outFd) {
            FD_SET(serialFds1.outFd, &currFdSet);
        }

        /* try opening serial device 2 */
        serialFds2.inFd = ioTtyInit(DEFAULT_SERIAL_DEVICE2, DEFAULT_SERIAL_RATE2);
        if (serialFds2.inFd < 0) {
            /* open failed, can't continue */
            LogMsg(LOG_ERR, "could not open serial port %s\n", DEFAULT_SERIAL_DEVICE2);
            break;
        } else {
            serialFds2.outFd = serialFds2.maxFd = serialFds2.inFd;
        }

        FD_SET(serialFds2.inFd, &currFdSet);
        if (serialFds2.inFd != serialFds2.outFd) {
            FD_SET(serialFds2.outFd, &currFdSet);
        }

        nfds = max( (serialFds1.maxFd > serialFds2.maxFd) ? serialFds1.maxFd : serialFds2.maxFd,
            (connectedFd >= 0) ? connectedFd : listenFd) + 1;

        /*
         * This is the select loop which waits for characters to be received on
         * the serial/pty descriptor and on either the listen socket (meaning
         * an incoming connection is queued) or on a connected socket
         * descriptor.
         */
        while (1) {
            /* wait indefinitely for someone to blink */
            fd_set readFdSet = currFdSet;
            const int sel = select(nfds, &readFdSet, 0, 0, 0);

            if (sel == -1) {
                if (errno == EINTR) {
                    break;  /* drop out of inner while */
                } else {
                    LogMsg(LOG_ERR, "select() returned -1, errno = %d\n", errno);
                    exit(1);
                }
            } else if (sel <= 0) {
                continue;
            }

            /* check for a new connection to accept */
            if (FD_ISSET(listenFd, &readFdSet)) {
                /* new connection is here, accept it */
                connectedFd = ioQvSocketAccept(listenFd, addressFamily);
                if (connectedFd >= 0) {
                    FD_CLR(listenFd, &currFdSet);
                    FD_SET(connectedFd, &currFdSet);
                    nfds = max(serialFds1.maxFd, connectedFd) + 1;
                }
            }

            /* check for packet received from qml-viewer */
            if ((connectedFd >= 0) && FD_ISSET(connectedFd, &readFdSet)) {
                /* connected qml-viewer has something to say */
                char inMsg[READ_BUF_SIZE];
                const int readCount = readLine2(connectedFd, inMsg,
                                                sizeof(inMsg), &fromQv, "qml-viewer");
                if (readCount < 0) {
                    /* socket closed, stop watching this file descriptor */
                    FD_CLR(connectedFd, &currFdSet);
                    FD_SET(listenFd, &currFdSet);
                    connectedFd = -1;
                    nfds = listenFd + 1;
                } else if (readCount > 0) {
                    /* We need to determine what serial port to send to */
                    char token[3];
                    strncpy(token, inMsg, 3);

                    if (strncmp(token, "S1.", 3) == 0 && serialFds1.outFd >= 0)
                    {
                        memmove(inMsg, inMsg+3, strlen(inMsg));
                        ioTtyWrite(serialFds1.outFd, inMsg, readCount-4);
                    }
                    else if (strncmp(token, "S2.", 3) == 0 && serialFds2.outFd >= 0)
                    {                        
                        memmove(inMsg, inMsg+3, strlen(inMsg));
                        ioTtyWrite(serialFds2.outFd, inMsg, readCount-4);
                    }
                }
            }

            /* check for a character on the serial port 1 */
            if (FD_ISSET(serialFds1.inFd, &readFdSet)) {
                /*
                 * serial port has something to send to the qml-viewer,
                 * if connected
                 */
                int serialRet = ioTtyRead(serialFds1.inFd, ttyBuff1,
                    sizeof(ttyBuff1), &serialPos1);
                if (serialRet < 0) {
                    /* fall out of this loop to reopen serial port or pts */
                    break;
                } else if ((serialRet > 0) && (connectedFd >= 0)) {
                    //Write to qml viewer
                    prepend(ttyBuff1, "S1.");
                    ioQvSocketWrite(connectedFd, ttyBuff1);
                }
            }

            /* check for a character on the serial port 2 */
            if (FD_ISSET(serialFds2.inFd, &readFdSet)) {
                /*
                 * serial port has something to send to the qml-viewer,
                 * if connected
                 */
                int serialRet = ioTtyRead(serialFds2.inFd, ttyBuff2,
                    sizeof(ttyBuff2), &serialPos2);
                if (serialRet < 0) {
                    /* fall out of this loop to reopen serial port or pts */
                    break;
                } else if ((serialRet > 0) && (connectedFd >= 0)) {
                    //Write to qml viewer
                    prepend(ttyBuff2, "S2.");
                    ioQvSocketWrite(connectedFd, ttyBuff2);
                }
            }

        }  /* End while(1) */


        close(serialFds1.inFd);
        FD_CLR(serialFds1.inFd, &currFdSet);
        close(serialFds2.inFd);
        FD_CLR(serialFds2.inFd, &currFdSet);


    }

    LogMsg(LOG_INFO, "cleaning up\n");

    if (connectedFd >= 0) {
        close(connectedFd);
    }
    if (listenFd >= 0) {
        close(listenFd);
    }

    if (tcpPort == 0) {
        /* best effort removal of socket */
        const int rv = unlink(ioSocketPath);
        if (rv == 0) {
            LogMsg(LOG_INFO, "socket file %s unlinked\n", ioSocketPath);
        } else {
            LogMsg(LOG_INFO, "socket file %s unlink failed\n", ioSocketPath);
        }
    }

}


