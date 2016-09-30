#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>

// Each TRS packet header is 1 byte message type, 1 byte content length.
#define TRS_HEADER_LEN 2

// Maximum allowable TRS packet size, including 2 byte header.
#define MAX_TRS_PACKET_LEN 256

// Maximum amount of data a single TRS packet can contain.
#define MAX_TRS_DATA_LEN MAX_TRS_PACKET_LEN - TRS_HEADER_LEN

// Location in TRS packet buffer containing data.
#define TRS_DATA &trs_packet[TRS_HEADER_LEN]

// Max file transfer size is 100MB
#define MAX_FILE_TRANSFER_MB 100
#define MAX_FILE_TRANSFER_BYTES MAX_FILE_TRANSFER_MB*1024*1024

// The TCP port this TRS server will listen on.
#define SERVER_PORT "9034"

// Enable for speedier development.
#define DEBUG 0

// TRS message types.
#define CONNECT_REQUEST 0x61
#define CONNECT_ACKNOWLEDGE 0x62
#define CHAT_REQUEST 0x63
#define CHAT_ACKNOWLEDGE 0x64
#define CHAT_MESSAGE 0x65
#define CHAT_FINISH 0x66
#define BINARY_MESSAGE 0x67
#define CONNECT_FAIL 0x68
#define CHAT_FAIL 0x69
#define HELP_REQUEST 0x70
#define HELP_ACKNOWLEDGE 0x71
#define TRANSFER_START 0x72

// Outgoing data buffer.
char bufsend[MAX_TRS_PACKET_LEN];

// Stdin buffer.
char stdin_buffer[MAX_TRS_DATA_LEN];

// Incoming TRS packet buffer.
char trs_packet[MAX_TRS_PACKET_LEN];

// Message type of current TRS packet.
size_t command_byte;

// Data length of current TRS packet.
size_t length_byte;

// Address info.
struct addrinfo hints;

// Master fd_set, and temp copy for select() calls.
fd_set master;
fd_set read_fds;


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
    memcpy(&bufsend[0], &message_type, 1);
    memcpy(&bufsend[1], &data_length, 1);
    memcpy(&bufsend[2], data, data_length);

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
    trs_send(destination_fd, BINARY_MESSAGE, message, message_length);
}

// Send a TRS TRANSFER_START type message.
// Client and server have the same exact implementation for trs_send_transfer_start.
// Because server just forwards what client sends.
void trs_send_transfer_start(int destination_fd, char* message, unsigned char message_length) {
    trs_send(destination_fd, TRANSFER_START, message, message_length);
}

// Receive a single TRS packet.
int trs_recv(int sender_fd){
    // Zero the packet buffer.
    memset(&trs_packet, 0, MAX_TRS_PACKET_LEN);

    int bytes_received;

    // Receive the command byte of a TRS packet.
    if ((bytes_received = recv(sender_fd, &command_byte, 1, 0)) < 1) {
        // Return error.
        return bytes_received;
    } else {
        // Copy command byte to TRS packet buffer.
        memcpy(&trs_packet, &command_byte, 1);
    }

    // Receive the data length byte of a TRS packet.
    if((bytes_received = recv(sender_fd, &length_byte, 1, 0)) < 1) {
        // Return error.
        return bytes_received;
    } else {
        // Copy length byte to TRS packet buffer.
        memcpy(&trs_packet[1], &length_byte, 1);
    }

    // Receive the data bytes of a TRS packet.
    if(length_byte > 0) {
        if((bytes_received = recv(sender_fd, &trs_packet[2], length_byte, 0)) < length_byte) {
            printf("ERROR: Received fewer bytes than requested.\n");
            return bytes_received;
        }
    }

    // Successfully received TRS packet.
    return 1;
}
