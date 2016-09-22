#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// Size of the receive buffer.
#define MAXRCVSIZE 256

// Size of the send buffer.
#define MAXSENDSIZE 256

// The TCP port this TRS server will listen on.
#define SERVER_PORT "9034"

// Commands.
#define CONNECT_REQUEST 0x61 // a
#define CONNECT_ACKNOWLEDGE 0x62 // b
#define CHAT_REQUEST 0x63 // c
#define CHAT_ACKNOWLEDGE 0x64 // d
#define CHAT_MESSAGE 0x65
#define CHAT_FINISH 0x66
#define BINARY_MESSAGE 0x67

#define CONNECT_FAIL 0x68
#define CHAT_FAIL 0x69

// Receiving data buffer.
char bufrcv[MAXRCVSIZE];

// Outgoing data buffer.
char bufsend[MAXSENDSIZE];


// Address info.
struct addrinfo hints;

// Get sockaddr, IPv4 or IPv6:
// Borrowed directly from Beej: http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Prototype
void trs_send_chat_message(int fd, char* message, unsigned char message_length);

void trs_send_chat_message(int fd, char* message, unsigned char message_length) {
    memset(&bufsend, 0, sizeof(bufsend));

    unsigned char message_type = CHAT_MESSAGE;
    unsigned char data_length = message_length;
    char* data = message;

    strncpy(&bufsend[0], &message_type, 1);
    strncpy(&bufsend[1], &data_length, 1);
    strncpy(&bufsend[2], data, data_length);

    size_t total_len = 2 + data_length;

    int sent;
    if ((sent = send(fd, bufsend, total_len, 0)) == -1) {
        perror("send");
    }
}

