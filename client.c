/*
 * client.c
 * TRS Client
 *
 * Usage:
 * ./client hostname
 */

#include "trs.h"
#include "client.h"

int sockfd, numbytes;

int connected_to_trs_server = 0;
int in_chat_room = 0;
char* client_username;
char* chat_partner;

int main(int argc, char *argv[])
{

    int sentcount; // result from send() command
    struct addrinfo *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    int i;// for loops

    //file descriptors of readable files
    fd_set read_fds;
    //file descriptors of writeable files
    fd_set master;
    //currently chatting?
    int chatting = 0;

    if (argc != 2) {
        fprintf(stderr,"usage: ./client trs_server_hostname\n");
        exit(1);
    }

    printf("\nTRS Client Started.\n");

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], SERVER_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Loop through all the results and connect to the first we can.
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Could not connect to TRS server. Exiting.\n");
        return 2;
    }


    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
//    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure


    //set socket and stdin to nonblocking
    fcntl(stdin->_fileno, F_SETFL, O_NONBLOCK);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);


    printf("Type /CONNECT <username> to start.\nType /HELP for all commands.\n");

    while(1) {
    
    //clear sent and received buffers
    memset(&bufsend, 0, MAXSENDSIZE);

    //clear the sets
    FD_ZERO(&read_fds);
    FD_ZERO(&master);

    //add server socket to readable and writeable sets
    FD_SET(sockfd, &read_fds);
    FD_SET(sockfd, &master);

    //add user input to readable and writeable sets
    FD_SET(stdin->_fileno, &read_fds);
    FD_SET(stdin->_fileno, &master);
    

        if(select(sockfd+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        //run through connections looking for data to read
        int i_fd;
        for(i_fd = 0; i_fd <= sockfd; i_fd++) {
            if (FD_ISSET(i_fd, &read_fds)){//data incoming

                // Zero the receiving buffer.
                memset(&bufrcv, 0, MAXRCVSIZE);

                // Data coming to stdin from user.
                if (i_fd == stdin->_fileno) {

                    fgets(bufrcv, MAXRCVSIZE, stdin);

                    if (strncmp(bufrcv, "/CONNECT", 8) == 0) {
                        trs_handle_client_connect(&bufrcv[9]);
                    }

                    else if (strncmp(bufrcv, "/CHAT", 5) == 0) {
                        trs_handle_client_chat();
                    }

                    else if (strncmp(bufrcv, "/QUIT", 5) == 0) {
                        trs_handle_client_quit();
                    }

                    else if (strncmp(bufrcv, "/TRANSFER", 9) == 0) {
                        trs_handle_client_transfer();
                    }

                    else if (strncmp(bufrcv, "/HELP", 5) == 0) {
                        trs_handle_client_help();
                    }

                    else if (in_chat_room == 1) {
                        trs_handle_client_chat_message(bufrcv);
                    }

                    else {
                        printf("Invalid command. Type /HELP for possible commands.\n");
                    }
                }

                // closed connection
                else if ((sentcount = recv(i_fd, bufrcv, MAXRCVSIZE, 0)) <=0) {
                    //connection closed
                    if (sentcount == 0) {
                        printf("Connection with server closed\n");
                        chatting = 0;
                        exit(0);
                    }
                    else {
                        perror("recv");
                    }
                    close(i_fd);
                    FD_CLR(i_fd, &read_fds);
                }

                // Received data from TRS server.
                else {
                    // TODO: Do I need to save the remaining data in the buffer, after parsing one message?
                    // Probably. Remaining data start of next message.

                    // First byte of the TRS header specifies type of message.
                    size_t command_byte = bufrcv[0];

                    // Second byte of the TRS head is the length of data remaining.
                    size_t length_byte = bufrcv[1];

                    switch(command_byte) {

                        case CONNECT_ACKNOWLEDGE:
                            trs_handle_connect_acknowledge(i_fd, &bufrcv[2], length_byte);
                            break;

                        case CHAT_ACKNOWLEDGE:
                            trs_handle_chat_acknowledge(i_fd, &bufrcv[2], length_byte);
                            break;

                        case CHAT_MESSAGE:
                            trs_handle_chat_message(i_fd, &bufrcv[2], length_byte);
                            break;

                        default:
                            //printf("Received message with invalid message type %zu.\n", command_byte);
                            break;
                    }
                }
            }
        }
    }

    close(sockfd);
    return 0;
}

void trs_send_connect_request(int fd, char* username, unsigned char username_length) {
    memset(&bufsend,0,sizeof(bufsend));

    unsigned char message_type = CONNECT_REQUEST;
    unsigned char data_length = username_length;
    char* data = username;

    strncpy(&bufsend[0], &message_type, 1);
    strncpy(&bufsend[1], &data_length, 1);
    strncpy(&bufsend[2], data, data_length);

    size_t total_len = 2 + data_length;

    int sent;
    if ((sent = send(sockfd, bufsend, total_len, 0)) == -1) {
        perror("send");
    }
}

void trs_send_chat_request(int fd) {
    memset(&bufsend,0,sizeof(bufsend));

    unsigned char message_type = CHAT_REQUEST;
    unsigned char data_length = 0;
    char* data = NULL;

    strncpy(&bufsend[0], &message_type, 1);
    strncpy(&bufsend[1], &data_length, 1);
    strncpy(&bufsend[2], data, data_length);

    size_t total_len = 2 + data_length;

    int sent;
    if ((sent = send(fd, bufsend, total_len, 0)) == -1) {
        perror("send");
    }
}

// Received CONNECT_ACKNOWLEDGE from server.
void trs_handle_connect_acknowledge(int sender_fd, char* data, size_t length) {
    connected_to_trs_server = 1;
    printf("Connected to TRS server.\nType /CHAT to chat with a random person.\n");
}

// Received CHAT_ACKNOWLEDGE from server.
void trs_handle_chat_acknowledge(int sender_fd, char* data, size_t length) {
    in_chat_room = 1;

    char* partner_username = (char*)malloc(length);
    strncpy(partner_username, data, length);
    chat_partner = partner_username;

    printf("Now chatting with %s.\n", partner_username);
}

// Received CHAT_MESSAGE from server.
void trs_handle_chat_message(int sender_fd, char* data, size_t length) {
    if (in_chat_room) {
        printf("%s: %s", chat_partner, data);
    }
}

// Received CHAT_FINISH from server.
void trs_handle_chat_finish(int sender_fd, char* data, size_t length) {
    in_chat_room = 0;
    printf("Disconnected from chat room. Type /CHAT to chat with another random person.\n");
}

// Client typed /CONNECT <username>
void trs_handle_client_connect(char* username) {
    if (connected_to_trs_server == 1) {
        printf("You are already connected to the TRS server.\n");
        return;
    }

    // Parse username.
    char* newline_pos = strchr(username, '\n');
    *newline_pos = '\0';

    // Store for later.
    client_username = username;

    // Send only up to null terminator (no null terminator needed).
    size_t username_len = newline_pos - username;

    // Send the server a connect request with our username.
    trs_send_connect_request(sockfd, username, username_len);
}

// Client typed /CHAT
void trs_handle_client_chat() {
    if (connected_to_trs_server == 0) {
        printf("Your are not connected to the TRS server.\nType /CONNECT <username> to start.\n");
        return;
    }

    if (in_chat_room == 1) {
        printf("You are already in a chat room.\nType /QUIT to exit the current chat room first.\n");
        return;
    }

    // Send the server a chat request.
    trs_send_chat_request(sockfd);
}

void trs_handle_client_help() {

}

void trs_handle_client_quit() {

}

void trs_handle_client_transfer() {

}

void trs_handle_client_chat_message(char * data) {

    char* null_term_loc = strchr(data, '\0');
    if (null_term_loc == NULL) {
        printf("Message too long.\n");
        return;
    }

    unsigned char message_length = null_term_loc - data + 1;
    trs_send_chat_message(sockfd, data, message_length);
}
