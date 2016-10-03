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
        fprintf(stderr, "usage: ./client <trs_server_hostname>\n");
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
        FD_SET(stdinfile, &master);
    
        // Select for available fds.
        if(select(server_fd+1, &master, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        // Look for available data from stdin.
        if (FD_ISSET(stdinfile, &master)) {
            fgets(stdin_buffer, MAX_TRS_DATA_LEN, stdin);

            // Handle possible commands.
            if (strncmp(stdin_buffer, "/CONNECT", 8) == 0) {
                trs_handle_client_connect();
            }

            else if (strncmp(stdin_buffer, "/CHAT", 5) == 0) {
                trs_handle_client_chat();
            }

            else if (strncmp(stdin_buffer, "/QUIT", 5) == 0) {
                trs_handle_client_quit();
            }

            else if (strncmp(stdin_buffer, "/TRANSFER", 9) == 0) {
                trs_handle_client_transfer();
            }

            else if (strncmp(stdin_buffer, "/HELP", 5) == 0) {
                trs_handle_client_help();
            }

            else if (in_chat_room == 1) {
                trs_handle_client_chat_message(stdin_buffer);
            }

            else {
                printf("Invalid command. Type /HELP for possible commands.\n");
            }
        }

        // Look for available data from TRS server.
        if (FD_ISSET(server_fd, &master)) {
            int recv_count;
            if ((recv_count = trs_recv(server_fd)) <= 0) {

                // Connection closed
                if (recv_count == 0) {
                    in_chat_room = 0;
                    connected_to_trs_server = 0;

                    printf("Connection with TRS server closed. Exiting.\n");
                    close(server_fd);
                    exit(0);
                }
                else {
                    perror("recv");
                }

            // Received data from TRS server.
            } else {
                switch(command_byte) {
                    case CONNECT_ACKNOWLEDGE:
                        trs_handle_connect_acknowledge();
                        break;

                    case CHAT_ACKNOWLEDGE:
                        trs_handle_chat_acknowledge();
                        break;

                    case CHAT_MESSAGE:
                        trs_handle_chat_message();
                        break;

                    case BINARY_MESSAGE:
                        trs_handle_binary_message();
                        break;

                    case CONNECT_FAIL:
                        trs_handle_connect_fail();
                        break;

                    case CHAT_FAIL:
                        trs_handle_chat_fail();
                        break;

                    case CHAT_FINISH:
                        trs_handle_chat_finish();
                        break;

                    case HELP_ACKNOWLEDGE:
                        trs_handle_help_acknowledge();
                        break;

                    case TRANSFER_START:
                        trs_handle_transfer_start();
                        break;

                    default:
                        printf("Received message with invalid message type %zu.\n", command_byte);
                        break;
                }
            }
        }
    }
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

    // Timeout on recv, send
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
    setsockopt(server_fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));

    // Set socket fd, and stdin fd to non-blocking.
    fcntl(stdinfile, F_SETFL, O_NONBLOCK);
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    // Prompt them to get started.
    printf("Type /CONNECT <username> to start.\nType /HELP for all commands.\n");
}

// Send a TRS CONNECT_REQUEST type message.
void trs_send_connect_request(char* username, size_t username_length) {
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
    trs_send(server_fd, CHAT_FINISH, NULL, 0);
}

// Received CONNECT_ACKNOWLEDGE from server.
void trs_handle_connect_acknowledge() {
    // Track that we're currently connected.
    connected_to_trs_server = 1;

    // Prompt user to start chatting.
    printf("Connected to TRS server.\nType /CHAT to chat with a random person.\n");
}

// Received CHAT_ACKNOWLEDGE from server.
void trs_handle_chat_acknowledge() {
    // Track that we're currently in a chat room.
    in_chat_room = 1;

    // Store current chat parter's username.
    if (chat_partner != NULL) {
        free(chat_partner);
    }

    // Incoming data includes null terminator.
    char* partner_username = (char*)malloc(length_bytes);
    strncpy(partner_username, TRS_DATA, length_bytes);
    chat_partner = partner_username;

    // Notify user that they're now chatting.
    printf("Now chatting with %s.\nType /QUIT to end chat.\n", partner_username);
}

// Received CHAT_MESSAGE from server.
void trs_handle_chat_message() {
    // Output the chat message, if we're supposed to be in a chat room.
    if (in_chat_room) {
        printf("%s: %s", chat_partner, TRS_DATA);
    }
}

void trs_handle_transfer_start() {
    // Ignore this if we're not in a chatroom, or are already receiving a file.
    if (in_chat_room != 1) {
        printf("Received TRANSFER_START, but not in a chat room.\n");
        return;
    }
    if (receiving_file == 1) {
        printf("Received TRANSFER_START, but currently receiving file.\n");
        return;
    }

    size_t int_size = sizeof(int);
    size_t filename_length = length_bytes - int_size;

    // Determine file length and name.
    int file_length = -1;
    char* filename = malloc(filename_length + 1);
    memcpy(filename, TRS_DATA, length_bytes - int_size);
    memcpy(&file_length, &trs_packet[TRS_HEADER_LEN + length_bytes - int_size], int_size);
    filename[filename_length] = '\0';

    printf("Now receiving file %s - will save as trs_%s.\n", filename, filename);

    // Prepend trs_ to filename.
    char *longer_filename = malloc(file_length + 5);
    memcpy(longer_filename, "trs_", 4);
    memcpy(&longer_filename[4], filename, filename_length);
    longer_filename[file_length + 4] = '\0';

    // Open the file we'll write to.
    file = fopen(longer_filename, "w");
    if (file == NULL) {
        printf("Could not open file.\n");
        return;
    }

    // Track that we're currently receiving a file.
    receiving_file = 1;
    file_bytes_expected = file_length;
    file_bytes_received = 0;
    receiving_filename = longer_filename;
}

// Received CHAT_FINISH from server.
void trs_handle_chat_finish() {
    // Track that we're no longer in a chat room.
    in_chat_room = 0;

    // Notify the user.
    printf("Disconnected from chat room.\nType /CHAT to chat with another random person.\n");
}

// Received HELP_ACKNOWLEDGE from server.
void trs_handle_help_acknowledge() {
    // Print out the content.
    printf("%s", TRS_DATA);
}

// Received CONNECT_FAIL from server.
void trs_handle_connect_fail() {
    printf("Error message from server: %s", TRS_DATA);
}

// Received BINARY_MESSAGE from server.
void trs_handle_binary_message() {
    if (receiving_file == 0) {
        printf("Ignoring BINARY_MESSAGE because we are not currently receiving a file.\n");
        return;
    }

    size_t written;
    written = fwrite(TRS_DATA, 1, length_bytes, file);
    if (written != length_bytes) {
        printf("Wrote different amount than received.\n");
    }

    file_bytes_received += written;

    if (file_bytes_received == file_bytes_expected) {
        printf("Done receiving %s.\n", receiving_filename);
        free(receiving_filename);

        receiving_file = 0;
        file_bytes_expected = -1;
        file_bytes_received = -1;
        fclose(file);
    }
}

// Received CHAT_FAIL from server.
void trs_handle_chat_fail() {
    printf("Error message from server: %s", TRS_DATA);
}

// Client typed /CONNECT <username>
void trs_handle_client_connect() {
    if (connected_to_trs_server == 1) {
        printf("You are already connected to the TRS server.\n");
        return;
    }

    // Make sure a space character is before the username
    char * space_pos = strrchr(stdin_buffer, ' ');
    if (space_pos == NULL) {
        printf("Usage: /CONNECT <username>\n");
        return;
    }

    // Parse username.
    char* newline_pos = strchr(stdin_buffer, '\n');
    if((newline_pos - space_pos) < 2) {
        printf("Usage: /CONNECT <username>\n");
        return;
    }
    *newline_pos = '\0';

    // Store for later.
    client_username = space_pos + 1;

    // Send only up to null terminator (no null terminator needed).
    size_t username_len = newline_pos - space_pos - 1;

    // Send the server a connect request with our username.
    trs_send_connect_request(client_username, username_len);
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

    // Notify user that we are waiting for a chat partner.
    printf("Finding chat partner...\n");
}

// Client typed /HELP
void trs_handle_client_help() {
    // Send the server a help request.
    trs_send_help_request();
}

// Client typed /QUIT
void trs_handle_client_quit() {
    // Track that we're no longer in a chat room.
    in_chat_room = 0;

    // Notify the server that we're quitting this chat.
    trs_send_chat_finish();

    // Notify user that chat has ended.
    printf("No longer chatting with %s.\nType /CHAT to chat with another random person.\n", chat_partner);
}

// Client typed /TRANSFER
void trs_handle_client_transfer() {
    if (in_chat_room != 1) {
        printf("You are not in a chat room.\nType /CHAT to chat with a random person.\n");
        return;
    }

    // Make sure a space character is before the filepath.
    char * space_pos = strrchr(stdin_buffer, ' ');
    if (space_pos == NULL) {
        printf("Usage: /TRANSFER <filepath>\n");
        return;
    }

    // Parse filepath.
    char* newline_pos = strchr(stdin_buffer, '\n');
    size_t filename_len = newline_pos - space_pos - 1;
    if(filename_len < 1) {
        printf("Usage: /TRANSFER <filepath>\n");
        return;
    }
    *newline_pos = '\0';

    char* filepath = space_pos + 1;
    struct stat file_stat;
    int success = stat(filepath, &file_stat);

    if (success != 0) {
        printf("Could not find file.\n");
        return;
    }

    // Get file length in bytes.
    int file_length = (int)file_stat.st_size;

    if (file_length > MAX_FILE_TRANSFER_BYTES) {
        printf("File too large. Max is %d MB.\n", MAX_FILE_TRANSFER_MB);
        return;
    }

    // Begin transferring file.
    FILE *to_transfer = NULL;
    size_t int_size = sizeof(int);
    unsigned int bytes_transferred = 0;
    char transfer_buf[MAX_TRS_DATA_LEN];

    to_transfer = fopen(filepath, "rb");
    if (to_transfer == NULL) {
        printf("Could not open file.\n");
        return;
    }

    printf("Starting to transfer %s.\n", filepath);

    // Notify server with TRANSFER_START message.
    char* data = malloc(filename_len + int_size);
    memcpy(data, filepath, filename_len);
    memcpy(&data[filename_len], (char*)&file_length, int_size);
    trs_send_transfer_start(server_fd, data, filename_len + int_size);

    // Send BINARY_MESSAGE messages until we've sent the whole file.
    size_t bytes_read;
    unsigned int sent;
    printf("Transfer file length %d.\n", file_length);
    while (bytes_transferred < file_length) {
        // Delay in between sending chunks guaranteed.
        useconds_t millis = 10;
        usleep(1000*millis);

        bytes_read = fread(transfer_buf, 1, MAX_TRS_DATA_LEN, to_transfer);
        printf("Read %zu bytes.\n", bytes_read);

        sent = 0;
        sent = trs_send_binary_message(server_fd, &transfer_buf[sent], bytes_read);
        while (sent < bytes_read) {
            usleep(1000*5*millis);
            sent = sent + trs_send_binary_message(server_fd, &transfer_buf[sent], bytes_read);
            printf("Send %d bytes.\n", sent);
        }

        bytes_transferred += bytes_read;
        printf("Bytes transferred now %d\n", bytes_transferred);
    }

    printf("Done transferring %s.\n", filepath);
    fclose(to_transfer);
}

// Client typed something, presumed to be a message for their chat room.
void trs_handle_client_chat_message(char * data) {

    // Validate length.
    char* null_term_loc = strchr(data, '\0');
    if (null_term_loc == NULL) {
        printf("Message too long.\n");
        return;
    }

    // Send to server. Length includes null terminator.
    size_t message_length = null_term_loc - data + 1;
    trs_send_chat_message(server_fd, data, message_length);
}
