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
    fd_set write_fds;
    //currently chatting?
    int chatting = 0;

    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], SERVER_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);
    
    freeaddrinfo(servinfo); // all done with this structure


    //set socket and stdin to nonblocking
    fcntl(stdin->_fileno, F_SETFL, O_NONBLOCK);
    fcntl(sockfd, F_SETFL, O_NONBLOCK);


    trs_send_connect_request(sockfd, "user1", 5);

    while(1) {
    
    //clear sent and received buffers
    memset(&bufsend, 0, MAXSENDSIZE);
    memset(&bufrcv, 0, MAXRCVSIZE);

    //clear the sets
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    //add server socket to readable and writeable sets
    FD_SET(sockfd, &read_fds);
    FD_SET(sockfd, &write_fds);

    //add user input to readable and writeable sets
    FD_SET(stdin->_fileno, &read_fds);
    FD_SET(stdin->_fileno, &write_fds);
    

        if(select(sockfd+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        //run through connections looking for data to read
        int i_fd;
        for(i_fd=0; i_fd<=sockfd; i_fd++) {
            if (FD_ISSET(i_fd, &read_fds)){//data incoming
                // if the data is from the user
                if (i_fd == stdin->_fileno) {
                    fgets(bufrcv, MAXRCVSIZE, stdin);

                    char* null_term_loc = strchr(bufrcv, '\0');
                    unsigned char message_length = null_term_loc - bufrcv + 1;
                    trs_send_chat_message(sockfd, bufrcv, message_length);
                    memset(&bufrcv,0,sizeof(bufrcv));
                    printf("Sent chat\n");
                }

                // closed connection
                else if (sentcount = recv(i_fd, bufrcv, MAXRCVSIZE, 0) <=0) {
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

                // data from server
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
                            printf("Received message with invalid message type %zu.\n", command_byte);
                    }
                }
            }
        }
    }

            

    

    //if ((sentcount = send(sockfd,bufsend, strlen(bufsend) + 1, 0)) == -1) {
     //   perror("send");
    //    exit(1);
    //}
    //}

    /*if ((numbytes = recv(sockfd, buf, MAXSENDSIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }*/

    //buf[numbytes] = '\0';

    //printf("client: received '%s'\n",buf);

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

void trs_handle_connect_acknowledge(int sender_fd, char* data, size_t length) {
    printf("connect ack.\n");

    // TODO: Only want to call this when user types /CHAT
    trs_send_chat_request(sender_fd);
}

void trs_handle_chat_acknowledge(int sender_fd, char* data, size_t length) {
    char* partner_username = (char*)malloc(length);
    strncpy(partner_username, data, length);
    printf("Chatting with user:%s.\n", partner_username);
    // TODO, data should have username of partner.
}

void trs_handle_chat_message(int sender_fd, char* data, size_t length) {
    printf("Got chat:\n");
    printf("%s", data);
}
