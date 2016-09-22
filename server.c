/*
 * server.c
 * TRS Server
 *
 * Usage:
 * ./server
 */

#include "trs.h"
#include "server.h"

int main(void) {
    initialize_trs();
    printf("TRS Admin: Type /START to open chat rooms.\n");


    // Remote IP.
    char remoteIP[INET6_ADDRSTRLEN];

    // Bytes received.
    int nbytes;

    // Main loop.
    while(1) {
        // Copy master list.
        read_fds = master;

        // Populate read_fds.
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // Run through the existing connections looking for data to read.
        int i_fd, new_fd;
        struct sockaddr_storage remoteaddr;
        for(i_fd = 0; i_fd <= fdmax; i_fd++) {

            // Here's a connection.
            if (FD_ISSET(i_fd, &read_fds)) {

                // Zero out receive buffer.
                memset(&bufrcv, 0, MAXRCVSIZE);

                // Handle stdin message.
                if(i_fd == stdin->_fileno) {
                    fgets(bufrcv, MAXRCVSIZE, stdin);

                    if (strncmp(bufrcv, "/START\n", 7) == 0) {
                        trs_handle_admin_start();
                    }
                }

                // Handle new connection.
                else if (i_fd == listener) {
                    addrlen = sizeof remoteaddr;
                    new_fd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

                    if (new_fd == -1) {
                        perror("accept");
                    } else {
                        // Add fd to master set.
                        FD_SET(new_fd, &master);

                        // Keep track of max.
                        if (new_fd > fdmax) {
                            fdmax = new_fd;
                        }

                        printf("New connection from %s on socket %d.\n",
                               inet_ntop(remoteaddr.ss_family,
                                         get_in_addr((struct sockaddr*)&remoteaddr),
                                         remoteIP,
                                         INET6_ADDRSTRLEN),
                               new_fd );
                    }

                // Handle data from existing client.
                } else {
                    if ((nbytes = recv(i_fd, bufrcv, MAXRCVSIZE, 0)) <= 0) {

                        // Error or connection closed by client.
                        if (nbytes == 0) {
                            printf("Socket %d hung up.\n", i_fd);
                        } else {
                            perror("recv");
                        }
                        close(i_fd);

                        trs_disconnect_user(i_fd);

                        FD_CLR(i_fd, &master); // remove from master set

                    // Got some data from an already connected client
                    } else {

                        // TODO: Do I need to save the remaining data in the buffer, after parsing one message?
                        // Probably. Remaining data start of next message.

                        // First byte of the TRS header specifies type of message.
                        size_t command_byte = bufrcv[0];

                        // Second byte of the TRS head is the length of data remaining.
                        size_t length_byte = bufrcv[1];

                        switch(command_byte) {

                            case CONNECT_REQUEST:
                                trs_handle_connect_request(i_fd, &bufrcv[2], length_byte);
                                break;

                            case CHAT_REQUEST:
                                trs_handle_chat_request(i_fd, &bufrcv[2], length_byte);
                                break;

                            case CHAT_MESSAGE:
                                trs_handle_chat_message(i_fd, &bufrcv[2], length_byte);
                                break;

                            case CHAT_FINISH:
                                trs_handle_chat_finish(i_fd, &bufrcv[2], length_byte);
                                break;

                            case BINARY_MESSAGE:
                                trs_handle_binary_message(i_fd, &bufrcv[2], length_byte);
                                break;

                            default:
                                printf("Received message with invalid message type %zu.\n", command_byte);
                        }
                    }
                }
            }
        }
    }
    
    return 0;
}

// Start listening for admin commands.
void initialize_trs() {
    // Set stdin fd to nonblocking.
    fcntl(stdin->_fileno, F_SETFL, O_NONBLOCK);

    // Zero the master and temp file descriptor sets.
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // Add stdin fd to master fd_set.
    FD_SET(stdin->_fileno, &master);

    // Keep track of the biggest file descriptor.
    fdmax = stdin->_fileno;
}

// Get the server listening.
void start_server() {
    // For setsockopt() SO_REUSEADDR
    int yes = 1;

    // Address infos.
    struct addrinfo *ai, *p;

    // Bind to a TCP socket.
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv;
    if ((rv = getaddrinfo(NULL, SERVER_PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "%s\n", gai_strerror(rv));
        exit(1);
    }

    // Prevent "address already in use" error message on successive executions.
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // If p is null, failed to bind to socket.
    if (p == NULL) {
        fprintf(stderr, "ERROR: Failed to bind.\n");
        exit(2);
    }

    // Done with ai.
    freeaddrinfo(ai);

    // Start listening.
    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        exit(3);
    }

    // Add listener to master fd set.
    FD_SET(listener, &master);

    // Set listener socket to nonblocking.
    // TODO: Need to do this for each new connection's socket?
    fcntl(listener, F_SETFL, O_NONBLOCK);

    // Keep track of the biggest file descriptor.
    if (listener > fdmax){
        fdmax = listener;
    }

    printf("TRS Server Started.\n");
}

// Searches for a user with the given file descriptor.
// Returns the index if found, else -1.
int user_search(int fd) {
    int i;
    for(i = 0; i < MAX_USERS; i++) {
        if ((user_queue[i] != NULL) && (fd == user_queue[i]->fd)) {
            return i;
        }
    }

    return -1;
}

// Searches for a channel containing either of the given users.
// Returns the index if found, else -1.
int channel_search_users(user* u_one, user* u_two) {
    int i;
    for(i = 0; i < MAX_CHANNELS; i++) {
        if (channel_queue[i] != NULL) {
            if ((channel_queue[i]->u_one == u_one)  || (channel_queue[i]->u_two == u_one)) {
                return i;
            }
            if ((channel_queue[i]->u_one == u_two)  || (channel_queue[i]->u_two = u_two)) {
                return i;
            }
        }
    }

    return -1;
}

// Searches for a channel using the given file descriptor.
// Returns the index if found, else -1.
int channel_search_fd(int fd) {
    int i;
    for (i = 0; i < MAX_CHANNELS; i++) {
        if (channel_queue[i] != NULL) {
            if (channel_queue[i]->u_one->fd == fd) {
                return i;
            }
            if (channel_queue[i]->u_two->fd == fd) {
                return i;
            }
        }
    }

    return -1;
}

// Attempts to add a new user to the user_queue.
// Returns the index of the new user on success.
// Returns -1 on failure, if the user is already in the queue, or the queue is full.
int add_user(int fd, char* username) {
    // If this FD is already in the user queue, return failure.
    int search_result = user_search(fd);
    if (search_result != -1) {
        printf("User already in queue.\n");
        return -1;
    }

    // Try to find an empty spot in user queue.
    int empty_index = -1;
    int i;
    for(i = 0; i < MAX_USERS; i++) {
        if (user_queue[i] == NULL) {
            printf("Slot %d available.\n", i);
            empty_index = i;
            break;
        }
    }

    // Add the new user to the queue and return the index.
    if (empty_index != -1) {
        user_queue[empty_index] = new_user(fd, username);
        printf("Added to slot %d.\n", empty_index);
        return empty_index;
    }

    printf("Failed to add.\n");
    return -1;
}

// Zeroes the pointer to this user in the user_queue, if present.
int remove_user(int fd) {
    int i;
    for(i = 0; i < MAX_USERS; i++) {
        if (user_queue[i] != 0) {
            if (user_queue[i]->fd == fd) {
                printf("Removing user %s in slot %d.\n", user_queue[i]->username, i);
                free(user_queue[i]);
                user_queue[i] = NULL;
                return i;
            }
        }
    }

    return -1;
}

// Create a new user struct, get a pointer to it.
user * new_user(int fd, char* username) {
    user *u = (user *)malloc(sizeof(user));
    u->fd = fd;
    u->ready = 0;
    u->username = username;

    return u;
}

// Attempts to create a new channel for two users to talk.
// Returns the index of the new channel on success.
// Returns -1 if the channel queue is full.
int add_channel(user *u_one, user *u_two) {
    int search_result = channel_search_users(u_one, u_two);

    if (search_result != -1) {
        printf("One of the users already in channel.\n");
        return -1;
    }

    // Try to find an empty spot in channel queue.
    int empty_index = -1;
    int i;
    for(i = 0; i < MAX_CHANNELS; i++) {
        if (channel_queue[i] == NULL) {
            printf("Channel slot %d available.\n", i);
            empty_index = i;
            break;
        }
    }

    // Add the new channel to the queue and return the index.
    if (empty_index != -1) {
        channel_queue[empty_index] = new_channel(u_one, u_two);
        printf("Added channel to slot %d.\n", empty_index);
        return empty_index;
    }

    printf("Failed to add channel.\n");
    return -1;
}

// Create a new channel struct with two users in it, get a pointer to it.
channel * new_channel(user *u_one, user *u_two) {
    channel * c = (channel *)malloc(sizeof(channel));
    c->u_one = u_one;
    c->u_two = u_two;
    c->bytes_sent = 0;

    return c;
}

// Returns the index of a user in the user queue who is waiting to chat, other than client.
// Returns -1 if no one is available.
int find_available_user(user* client) {
    int i;
    for(i = 0; i < MAX_USERS; i++) {
        if ((user_queue[i] != NULL) && (user_queue[i] != client) && (user_queue[i]->ready == 1)) {
            return i;
        }
    }

    return -1;
}

// Connection has ended with this file descriptor.
void trs_disconnect_user(int fd) {
    int user_index = user_search(fd);

    // Nothing to do if that fd wasn't one of our users.
    if (user_index == -1) {
        return;
    }

    // The user who just disconnected.
    user * to_disconnect = user_queue[user_index];

    // If this user is in a channel, notify their partner that the chat is over, and free the channel.
    int channel_index = channel_search_fd(to_disconnect->fd);
    if (channel_index != -1) {
        user * partner;
        channel * room = channel_queue[channel_index];

        if (room->u_one == to_disconnect) {
            partner = room->u_two;
        } else {
            partner = room->u_one;
        }

        // Notify partner.
        trs_send_chat_finish(partner);

        // Free channel.
        free(channel_queue[channel_index]);
        channel_queue[channel_index] = NULL;
    }

    // Free the user struct.
    printf("Freeing user:%s fd:%d.\n", to_disconnect->username, to_disconnect->fd);
    free(user_queue[user_index]);
    user_queue[user_index] = NULL;
}

void trs_send_chat_finish(user* u) {

}

void trs_send_connect_fail(int fd) {

}

void trs_send_connect_acknowledge(int fd) {
    memset(&bufsend, 0, MAXSENDSIZE);

    unsigned char message_type = CONNECT_ACKNOWLEDGE;
    unsigned char data_length = 0;
    char* data = NULL;

    strncpy(&bufsend[0], &message_type, 1);
    strncpy(&bufsend[1], &data_length, 1);
    strncpy(&bufsend[2], data, data_length);

    size_t total_len = 2 + data_length;

    int sent;
    if ((sent = send(fd, bufsend, total_len, 0)) == -1){
        perror("send");
    }
}

void trs_send_chat_fail(int fd) {

}

void trs_send_chat_acknowledge(int fd, user* chat_partner) {
    char* null_term_loc = strchr(chat_partner->username, '\0');
    unsigned char username_length = null_term_loc - chat_partner->username + 1;

    memset(&bufsend, 0, MAXSENDSIZE);

    unsigned char message_type = CHAT_ACKNOWLEDGE;
    unsigned char data_length = username_length;
    char* data = chat_partner->username;

    strncpy(&bufsend[0], &message_type, 1);
    strncpy(&bufsend[1], &data_length, 1);
    strncpy(&bufsend[2], data, data_length);

    size_t total_len = 2 + data_length;

    int sent;
    if ((sent = send(fd, bufsend, total_len, 0)) == -1){
        perror("send");
    }
}

void trs_handle_connect_request(int sender_fd, char* data, size_t length) {

    // The only data in a connect request is the username.
    char* username = (char*)malloc(length + 1);
    strncpy(username, data, length);
    username[length] = '\0';

    printf("In trs_handle_connect_request fd:%d len:%zu username:%s.\n", sender_fd, length, username);

    int success = add_user(sender_fd, username);

    if (success != -1) {
        trs_send_connect_acknowledge(sender_fd);
    }

    else {
        printf("User NOT added.\n");
        trs_send_connect_fail(sender_fd);
    }
}

void trs_handle_chat_request(int sender_fd, char* data, size_t length) {

    // Make sure they're already in the user queue.
    int result = user_search(sender_fd);

    // Nothing to do if this user isn't connected.
    if (result == -1) {
        printf("User sent CHAT but has not sent CONNECT <username>.\n");
        trs_send_chat_fail(sender_fd);
        return;
    }

    // Client who sent a chat request.
    user * client = user_queue[result];

    // Make sure they're not already in a channel.
    result = channel_search_users(client, NULL);

    if (result != -1) {
        printf("User %s is already in a channel.\n", client->username);
        trs_send_chat_fail(sender_fd);
        return;
    }

    printf("User %s sent chat request.\n", client->username);

    // Find an available chat partner.
    int available_user = find_available_user(client);

    // If there's no partner available, just mark this user as ready.
    if (available_user == -1){
        client->ready = 1;
        printf("User %s is ready to chat.\n", client->username);
        return;
    }

    // Connect the client with the available user in a channel.
    user * available = user_queue[available_user];
    printf("User %s is already available.\n", available->username);
    available->ready = 0;
    client->ready = 0;

    // If there's no room left for another channel, send back an error.
    int new_channel = add_channel(client, available);
    if (new_channel == -1) {
        printf("No room for another channel.\n");
        trs_send_chat_fail(sender_fd);
        trs_send_chat_fail(available->fd);
    }

    // Notify both users that they are in a chat.
    trs_send_chat_acknowledge(sender_fd, available);
    trs_send_chat_acknowledge(available->fd, client);

    return;
}

void trs_handle_chat_message(int sender_fd, char* data, size_t length) {
    // Make sure they're in a channel.
    int channel_index = channel_search_fd(sender_fd);

    if (channel_index == -1) {
        trs_send_chat_fail(sender_fd);
    }

    channel* room = channel_queue[channel_index];
    int recipient_fd;
    if (room->u_one->fd == sender_fd) {
        recipient_fd = room->u_two->fd;
    } else {
        recipient_fd = room->u_one->fd;
    }

    trs_send_chat_message(recipient_fd, data, length);
}

void trs_handle_chat_finish(int sender_fd, char* data, size_t length) {

}

void trs_handle_binary_message(int sender_fd, char* data, size_t length) {

}

void trs_handle_admin_start() {
    // Boilerplate setup to start selecting connections.
    start_server();
}
