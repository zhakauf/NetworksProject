/*
 * client.c
 * TRS Client
 *
 * Usage:
 * ./client hostname
 */

#include "trs.h"
#include "client.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr,"usage: ./client <trs_server_hostname>\n");
        exit(1);
    }

    // Open up TCP connection to the TRS server.
    char* server_hostname = argv[1];
    initialize_trs(server_hostname);

    while(1) {
        // Zero the set of fds we're tracking.
        FD_ZERO(&master);

        // Add fd for server and stdio to fds.
        FD_SET(server_fd, &master);
        FD_SET(stdin->_fileno, &master);
    
        // Select for available fds.
        if(select(server_fd+1, &master, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        // Look for available data from stdin.
        if (FD_ISSET(stdin->_fileno, &master)) {

            // Zero the receiving buffer, then copy from stdin.
            memset(&bufrcv, 0, MAXRCVSIZE);
            fgets(bufrcv, MAXRCVSIZE, stdin);

            // Handle possible commands.
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

        // Look for available data from TRS server.
        if (FD_ISSET(server_fd, &master)) {
            int recv_count;
            if ((recv_count = recv(server_fd, bufrcv, MAXRCVSIZE, 0)) <=0) {

                // Connection closed
                if (recv_count == 0) {
                    in_chat_room = 0;
                    connected_to_trs_server = 0;

                    printf("Connection with server closed\n");
                    close(server_fd);
                    exit(0);
                }
                else {
                    perror("recv");
                }

            // Received data from TRS server.
            } else {
                // TODO: Do I need to save the remaining data in the buffer, after parsing one message?
                // Probably. Remaining data start of next message.

                // First byte of the TRS header specifies type of message.
                size_t command_byte = bufrcv[0];

                // Second byte of the TRS head is the length of data remaining.
                size_t length_byte = bufrcv[1];

                switch(command_byte) {

                    case CONNECT_ACKNOWLEDGE:
                        trs_handle_connect_acknowledge(&bufrcv[2], length_byte);
                        break;

                    case CHAT_ACKNOWLEDGE:
                        trs_handle_chat_acknowledge(&bufrcv[2], length_byte);
                        break;

                    case CHAT_MESSAGE:
                        trs_handle_chat_message(&bufrcv[2], length_byte);
                        break;

                    case BINARY_MESSAGE:
                        trs_handle_binary_message(&bufrcv[2], length_byte);
                        break;

                    case CONNECT_FAIL:
                        trs_handle_connect_fail(&bufrcv[2], length_byte);
                        break;

                    case CHAT_FAIL:
                        trs_handle_chat_fail(&bufrcv[2], length_byte);

                    case HELP_ACKNOWLEDGE:
                        trs_handle_help_acknowledge(&bufrcv[2], length_byte);

                    default:
                        printf("Received message with invalid message type %zu.\n", command_byte);
                        break;
                }
            }
        }
    }

    // TODO: Handle these in Ctrl-C signal instead?
    close(server_fd);
    return 0;
}

// Open up connection to the TRS server.
void initialize_trs(char* hostname) {
    printf("\nTRS Client Started.\n");

    // Bind to a TCP socket.
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rv;
    struct addrinfo *servinfo;
    if ((rv = getaddrinfo(hostname, SERVER_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // Loop through all the results and connect to the first we can.
    struct addrinfo *p;
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((server_fd = socket(p->ai_family, p->ai_socktype,
                                p->ai_protocol)) == -1) {
            continue;
        }

        if (connect(server_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_fd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Could not connect to TRS server. Exiting.\n");
        exit(2);
    }

    freeaddrinfo(servinfo);

    // Set socket fd, and stdin fd to non-blocking.
    fcntl(stdin->_fileno, F_SETFL, O_NONBLOCK);
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    // Prompt them to get started.
    printf("Type /CONNECT <username> to start.\nType /HELP for all commands.\n");
}

// Send a TRS CONNECT_REQUEST type message.
void trs_send_connect_request(char* username, unsigned char username_length) {
    trs_send(server_fd, CONNECT_REQUEST, username, username_length);
}

// Send a TRS CHAT_REQUEST type message.
void trs_send_chat_request() {
    trs_send(server_fd, CHAT_REQUEST, NULL, 0);
}

// Send a TRS HELP_REQUEST type message.
void trs_send_help_request() {
    trs_send(server_fd, HELP_REQUEST, NULL, 0);
}

// Send a TRS CHAT_FINISH type message.
void trs_send_chat_finish() {
    // TODO: This should be called when client types /QUIT.
    // Also need to mark that we're no longer in a chat room.
    trs_send(server_fd, CHAT_FINISH, NULL, 0);
}

// Received CONNECT_ACKNOWLEDGE from server.
void trs_handle_connect_acknowledge(char* data, size_t length) {
    // Track that we're currently connected.
    connected_to_trs_server = 1;

    // Prompt user to start chatting.
    printf("Connected to TRS server.\nType /CHAT to chat with a random person.\n");
}

// Received CHAT_ACKNOWLEDGE from server.
void trs_handle_chat_acknowledge(char* data, size_t length) {
    // Track that we're currently in a chat room.
    in_chat_room = 1;

    // Store current chat parter's username.
    if (chat_partner != NULL) {
        free(chat_partner);
    }

    char* partner_username = (char*)malloc(length);
    strncpy(partner_username, data, length);
    chat_partner = partner_username;

    // Notify user that they're now chatting.
    printf("Now chatting with %s.\n", partner_username);
}

// Received CHAT_MESSAGE from server.
void trs_handle_chat_message(char* data, size_t length) {
    // Output the chat message, if we're supposed to be in a chat room.
    if (in_chat_room) {
        printf("%s: %s", chat_partner, data);
    }
}

// Received CHAT_FINISH from server.
void trs_handle_chat_finish(char* data, size_t length) {
    // Track that we're no longer in a chat room.
    in_chat_room = 0;

    // Notify the user.
    printf("Disconnected from chat room.\nType /CHAT to chat with another random person.\n");
}

// Received HELP_ACKNOWLEDGE from server.
void trs_handle_help_acknowledge(char* data, size_t length) {
    // TODO Print out the content.
}

// Received CONNECT_FAIL from server.
void trs_handle_connect_fail(char* data, size_t length) {
    // TODO Not called from anywhere.
}

// Received BINARY_MESSAGE from server.
void trs_handle_binary_message(char* data, size_t length) {
    // TODO Implement this.
    // Should have some sort of tracker for currently_receiving_file
}

// Received CHAT_FAIL from server.
void trs_handle_chat_fail(char* data, size_t length) {
    // TODO implement this.
    // Server should should error message as data.
    // Just print the error.
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
    trs_send_connect_request(username, username_len);
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
    trs_send_chat_request();
}

// Client typed /HELP
void trs_handle_client_help() {
    // Send the server a help request.
    trs_send_help_request();
}

// Client typed /QUIT
void trs_handle_client_quit() {
    // TODO
}

// Client typed /TRANSFER
void trs_handle_client_transfer() {
    // TODO: This function needs arguments. Filename to transfer.
}

// Client typed something, presumed to be a message for their chat room.
void trs_handle_client_chat_message(char * data) {

    // Validate length.
    char* null_term_loc = strchr(data, '\0');
    if (null_term_loc == NULL) {
        printf("Message too long.\n");
        return;
    }

    // Send to server.
    unsigned char message_length = null_term_loc - data + 1;
    trs_send_chat_message(server_fd, data, message_length);
}
