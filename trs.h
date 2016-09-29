#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>

// Size of the receive buffer.
#define MAXRCVSIZE 1024

// Size of the send buffer.
#define MAXSENDSIZE 1024

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
#define HELP_REQUEST 0x70
#define HELP_ACKNOWLEDGE 0x71

// Receiving data buffer.
char bufrcv[MAXRCVSIZE];

// Outgoing data buffer.
char bufsend[MAXSENDSIZE];

// Address info.
struct addrinfo hints;

// Master fd_set, and temp copy for select() calls.
fd_set master;
fd_set read_fds;

//trs packet to send to handler
char trs_packet[MAXSENDSIZE];
//byte signifying message command
size_t command_byte; 
//byte signifying message length
size_t length_byte;

// Get sockaddr, IPv4 or IPv6:
// Borrowed directly from Beej: http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Generalized function for sending any TRS message.
void trs_send(int destination_fd, unsigned char message_type, char* data, unsigned char data_length) {
    // TRS header is 2 bytes.
    size_t total_len = 2 + data_length;

    // Zero the send buffer.
    memset(&bufsend, 0, sizeof(bufsend));

    // Copy message type, length, and data.
    strncpy(&bufsend[0], &message_type, 1);
    strncpy(&bufsend[1], &data_length, 1);
    strncpy(&bufsend[2], data, data_length);

    // Send it off.
    int sent_count;
    if ((sent_count = send(destination_fd, bufsend, total_len, 0)) == -1) {
        perror("send");
    }
}

// Send a TRS CHAT_MESSAGE type message.
// Client and server have the same exact implementation for trs_send_chat_message.
// Because server just forwards what client sends.
void trs_send_chat_message(int destination_fd, char* message, unsigned char message_length) {
    trs_send(destination_fd, CHAT_MESSAGE, message, message_length);
}

// Send a TRS BINARY_MESSAGE type message.
// Client and server have the same exact implementation for trs_send_binary_message.
// Because server just forwards what client sends.
void trs_send_binary_message(int destination_fd, char* message, unsigned char message_length) {
    // TODO: This function isn't called properly from anywhere.
    trs_send(destination_fd, BINARY_MESSAGE, message, message_length);
}



//Receive packet and parse the receive buffer
//If the buffer contains more than one message, send the first to a handler and shift to parse the rest
//If the buffer contains only part of one message, receive again and send the full message to the handler
int trs_recv(int sender_fd){
    //delete contents of trs_packet
    int trs_len = 0;
    char * trsend = NULL;
    if ((trsend = strrchr(trs_packet, '\0')) == NULL) {
        trs_len = trsend - trs_packet;
    }
    memset(&trs_packet, 0, trs_len);
    int got;
    //get command type from message
    if((got = recv(sender_fd, &command_byte, 1, 0)) <= 0) {
        if (got == 0){
            return 0;
        }
        else {
            return -1;
        }
    }

    //get length of message
    if((got = recv(sender_fd, &length_byte, 1, 0)) <= 0) {
        if (got == 0){
            return 0;
        }
        else {
            return -1;
        }
    }

    //get message data
    if(length_byte > 0) {
        if((got = recv(sender_fd, bufrcv, length_byte, 0)) <= 0) {
            if (got == 0){
                return 0;
            }
            else {
                return -1;
            }
        }
    }

    //copy data to trs_packet
    memcpy(&trs_packet, &command_byte,1);
    memcpy(&trs_packet[1], &length_byte, 1);
    memcpy(&trs_packet[2], &bufrcv, length_byte);
    //successful data transfer
    return 1;
} 
