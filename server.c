/*
 * server.c
 * TRS Server
 *
 * Usage:
 * ./server
 */

#include "trs.h"
#include "server.h"

//TODO: Make sure that if you're trying to connect to an unknown server, the error that pops up is ours. Right now it's some socket error.
//TODO: Make sure that the server starts on /START. Change that debug flag. Right now /START literally quits the server and that's not great haha.
//TODO: Get file transfer to work without error
//TODO: Have a drink and play some overwatch

int main(void) {
    // Start monitoring stdin, and give the admin a prompt.
    initialize_trs();

    // Remote IP.
    char remoteIP[INET6_ADDRSTRLEN];

    // Speed things up in dev.
    if (DEBUG == 1) {
        trs_handle_admin_start();
    }

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

                // Handle stdin message.
                if(i_fd == stdinfile) {
                    fgets(stdin_buffer, MAX_TRS_DATA_LEN, stdin);

                    // User typed /START
                    if (strncmp(stdin_buffer, "/START\n", 7) == 0) {
                        trs_handle_admin_start();
                    }

                    // User typed /END
                    else if (strncmp(stdin_buffer, "/END\n", 5) == 0) {
                        trs_handle_admin_end();
                    }

                    // User typed /STATS
                    else if (strncmp(stdin_buffer, "/STATS\n", 7) == 0) {
                        trs_handle_admin_stats();
                    }

                    // User typed /THROWOUT
                    else if (strncmp(stdin_buffer, "/THROWOUT", 9) == 0) {
                        trs_handle_admin_throwout();
                    }

                    // User typed /BLOCK
                    else if (strncmp(stdin_buffer, "/BLOCK", 6) == 0) {
                        trs_handle_admin_block();
                    }

                    // User typed /UNBLOCK
                    else if (strncmp(stdin_buffer, "/UNBLOCK", 8) == 0) {
                        trs_handle_admin_unblock();
                    }

                    // User typed /UNBLOCK
                    else if (strncmp(stdin_buffer, "/HELP", 5) == 0) {
                        trs_handle_admin_help();
                    }

                    else {
                        printf("Illegal command. Type /HELP to see commands.\n");
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

                        // Set new socket to non-blocking.
                        fcntl(new_fd, F_SETFL, O_NONBLOCK);

                        // Keep track of max.
                        if (new_fd > fdmax) {
                            fdmax = new_fd;
                        }
                    }

                // Handle data from existing client.
                } else {

                    // Error or connection closed by client.
                    int trs_recv_result;
                    if ((trs_recv_result = trs_recv(i_fd)) < 1) {

                        // Connection closed.
                        if (trs_recv_result == 0) {
                            // Close socket.
                            close(i_fd);

                            // Remove user if they were in the system.
                            disconnect_user(i_fd);

                            // Stop tracking this fd.
                            FD_CLR(i_fd, &master);
                        }
                        // Error in recv.
                        else {
                            perror("recv");
                        }

                    // Handle some data from an already connected client
                    } else {

                        switch(command_byte) {

                            case CONNECT_REQUEST:
                                trs_handle_connect_request(i_fd);
                                break;

                            case CHAT_REQUEST:
                                trs_handle_chat_request(i_fd);
                                break;

                            case CHAT_MESSAGE:
                                trs_handle_chat_message(i_fd);
                                break;

                            case CHAT_FINISH:
                                trs_handle_chat_finish(i_fd);
                                break;

                            case BINARY_MESSAGE:
                                trs_handle_binary_message(i_fd);
                                break;

                            case HELP_REQUEST:
                                trs_handle_help_request(i_fd);
                                break;

                            case TRANSFER_START:
                                trs_handle_transfer_start(i_fd);
                                break;

                            default:
                                printf("Received message with invalid message type %zu.\n", command_byte);
                        }
                    }
                }
            }
        }
    }
}

// Start listening for admin commands.
void initialize_trs() {
    // Set stdin fd to nonblocking.
    fcntl(stdinfile, F_SETFL, O_NONBLOCK);

    // Zero the master and temp file descriptor sets.
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // Add stdin fd to master fd_set.
    FD_SET(stdinfile, &master);

    // Keep track of the biggest file descriptor.
    fdmax = stdinfile;

    printf("\nTRS Started. \nType /START to open chat rooms.\nType /HELP for other commands.\n");
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


        // Set a min timeout before erroring.
        struct timeval tv;
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
        setsockopt(listener, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));

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

    // Set listener socket to non-blocking.
    fcntl(listener, F_SETFL, O_NONBLOCK);

    // Keep track of the biggest file descriptor.
    if (listener > fdmax){
        fdmax = listener;
    }

    printf("Now listening on port %s.\n", SERVER_PORT);
}

// Searches for a user with the given file descriptor.
// Returns the index if found, else -1.
int user_search(int fd) {
    int i;
    for(i = 0; i < MAX_USERS; i++) {
        if (user_queue[i] != NULL) {
            if (user_queue[i]->fd == fd) {
                return i;
            }
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
            if ((channel_queue[i]->u_one == u_two)  || (channel_queue[i]->u_two == u_two)) {
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
        return -1;
    }

    // Try to find an empty spot in user queue.
    int empty_index = -1;
    int i;
    for(i = 0; i < MAX_USERS; i++) {
        if (user_queue[i] == NULL) {
            empty_index = i;
            break;
        }
    }

    // Add the new user to the queue and return the index.
    if (empty_index != -1) {
        user_queue[empty_index] = new_user(fd, username);
        return empty_index;
    }

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
    u->blocked = 0;

    return u;
}

// Attempts to create a new channel for two users to talk.
// Returns the index of the new channel on success.
// Returns -1 if the channel queue is full.
int add_channel(user *u_one, user *u_two) {
    int search_result = channel_search_users(u_one, u_two);

    if (search_result != -1) {
        return -1;
    }

    // Try to find an empty spot in channel queue.
    int empty_index = -1;
    int i;
    for(i = 0; i < MAX_CHANNELS; i++) {
        if (channel_queue[i] == NULL) {
            empty_index = i;
            break;
        }
    }

    // Add the new channel to the queue and return the index.
    if (empty_index != -1) {
        if (u_one == NULL) {
            printf("u_one NULL?!\n");
        }
        if (u_two == NULL) {
            printf("u_two NULL?!\n");
        }
        channel_queue[empty_index] = new_channel(u_one, u_two);
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

    if (c->u_one == NULL) {
        printf("u_one NULL?!\n");
    }
    if (c->u_two == NULL) {
        printf("u_two NULL?!\n");
    }

    return c;
}

// Returns the index of a user in the user queue who is waiting to chat, other than client.
// Returns -1 if no one is available.
int find_available_user(user* client) {
    int i;
    for(i = 0; i < MAX_USERS; i++) {
        if ((user_queue[i] != NULL) &&
                (user_queue[i] != client) &&
                (user_queue[i]->ready == 1) &&
                (user_queue[i]->blocked == 0)) {
            return i;
        }
    }

    return -1;
}

// Connection has ended with this file descriptor.
void disconnect_user(int fd) {
    int user_index = user_search(fd);

    // Nothing to do if that fd wasn't one of our users.
    if (user_index == -1) {
        return;
    }

    // The user who just disconnected.
    user * to_disconnect = user_queue[user_index];

    // If this user is in a channel, notify their partner that the chat is over, and free the channel.
    int channel_index = channel_search_fd(fd);

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
    free(user_queue[user_index]);
    user_queue[user_index] = NULL;
}

// Send a TRS CHAT_FINISH type message.
void trs_send_chat_finish(user* u) {
    trs_send(u->fd, CHAT_FINISH, NULL, 0);
}

// Send a TRS CONNECT_FAIL type message.
void trs_send_connect_fail(int fd) {
    char* error = "Too many users already connected.\n\0";

    // Length includes null terminator.
    size_t error_len = strchr(error, '\0') - error + 1;

    trs_send(fd, CONNECT_FAIL, error, error_len);
}

// Send a TRS CONNECT_ACKNOWLEDGE type message.
void trs_send_connect_acknowledge(int fd) {
    trs_send(fd, CONNECT_ACKNOWLEDGE, NULL, 0);
}

// Send a TRS CHAT_FAIL type message.
void trs_send_chat_fail(int fd) {
    char* error = "Unable to request chat room.\n\0";

    // Length includes null terminator.
    size_t error_len = strchr(error, '\0') - error + 1;

    trs_send(fd, CHAT_FAIL, error, error_len);
}

// Send a TRS HELP_ACKNOWLEDGE type message.
void trs_send_help_acknowledge(int fd, char* data, size_t length) {
    trs_send(fd, HELP_ACKNOWLEDGE, data, length);
}

// Send a TRS CHAT_ACKNOWLEDGE type message.
void trs_send_chat_acknowledge(int fd, user* chat_partner) {
    // Figure out the username length.
    char* null_term_loc = strchr(chat_partner->username, '\0');

    // Length includes null terminator.
    size_t username_length = null_term_loc - chat_partner->username + 1;

    trs_send(fd, CHAT_ACKNOWLEDGE, chat_partner->username, username_length);
}

// Received CONNECT_REQUEST from a client.
void trs_handle_connect_request(int sender_fd) {

    // The only data in a connect request is the username.
    char* username = (char*)malloc(length_bytes + 1);
    strncpy(username, TRS_DATA, length_bytes);
    username[length_bytes] = '\0';

    // Try to add a new chat user.
    int success = add_user(sender_fd, username);
    if (success != -1) {
        trs_send_connect_acknowledge(sender_fd);
    }

    else {
        trs_send_connect_fail(sender_fd);
    }
}

// Received HELP_REQUEST from a client.
void trs_handle_help_request(int sender_fd) {
    // Data is list of accepted commands and their descriptions.
    char *data = "Commands:\n\
/CONNECT <username>\tConnect as <username>.\n\
/CHAT\t\t\tAsk to chat with random partner.\n\
/QUIT\t\t\tQuit chat session.\n\
/TRANSFER <filepath>\tTransfer a file to a chat partner.\n\
/HELP\t\t\tReprint this help message.\n\0";

    // Length does not include null terminator.
    size_t len = strrchr(data, '\0') - data;

    trs_send_help_acknowledge(sender_fd, data, len);
}

// Received CHAT_REQUEST from a client.
void trs_handle_chat_request(int sender_fd) {

    // Make sure they're already in the user queue.
    int result = user_search(sender_fd);

    // Nothing to do if this user isn't connected.
    if (result == -1) {
        trs_send_chat_fail(sender_fd);
        return;
    }

    // Client who sent a chat request.
    user * client = user_queue[result];

    // Make sure they're not already in a channel.
    result = channel_search_users(client, NULL);

    if (result != -1) {
        trs_send_chat_fail(sender_fd);
        return;
    }

    // Find an available chat partner.
    int available_user = find_available_user(client);

    // If there's no partner available, just mark this user as ready.
    if (available_user == -1){
        client->ready = 1;
        return;
    }

    // If the sender is blocked, don't connect them.
    if (client->blocked == 1) {
        return;
    }

    // Connect the client with the available user in a channel.
    user * available = user_queue[available_user];
    available->ready = 0;
    client->ready = 0;

    // If there's no room left for another channel, send back an error.
    int new_channel = add_channel(client, available);
    if (new_channel == -1) {
        trs_send_chat_fail(sender_fd);
        trs_send_chat_fail(available->fd);
        return;
    }

    // Notify both users that they are in a chat.
    trs_send_chat_acknowledge(sender_fd, available);
    trs_send_chat_acknowledge(available->fd, client);
}

// Received CHAT_MESSAGE from a client.
void trs_handle_chat_message(int sender_fd) {

    // Make sure they're in a channel.
    int channel_index = channel_search_fd(sender_fd);

    // Ignore chat message if sender isn't in a channel.
    if (channel_index == -1) {
        return;
    }

    // Determine the recipient of this chat message.
    int recipient_fd;
    channel* room = channel_queue[channel_index];
    if (room->u_one->fd == sender_fd) {
        recipient_fd = room->u_two->fd;
    } else {
        recipient_fd = room->u_one->fd;
    }

    // Forward the chat message.
    trs_send_chat_message(recipient_fd, TRS_DATA, length_bytes);

    // Update the data usage in this channel.
    room->bytes_sent = room->bytes_sent + (length_bytes + TRS_HEADER_LEN);
}

void trs_handle_transfer_start(int sender_fd) {
    // Make sure they're in a channel.
    int channel_index = channel_search_fd(sender_fd);

    // Ignore chat message if sender isn't in a channel.
    if (channel_index == -1) {
        return;
    }

    // Determine the recipient of this chat message.
    int recipient_fd;
    channel* room = channel_queue[channel_index];
    if (room->u_one->fd == sender_fd) {
        recipient_fd = room->u_two->fd;
    } else {
        recipient_fd = room->u_one->fd;
    }

    // Forward the message.
    trs_send_transfer_start(recipient_fd, TRS_DATA, length_bytes);
}

// Received CHAT_FINISH from a client.
void trs_handle_chat_finish(int sender_fd) {
    // Make sure they're in a channel.
    int channel_index = channel_search_fd(sender_fd);

    // Ignore this message if they're not in a chat channel.
    if (channel_index == -1) {
        return;
    }

    // Determine the chat partner of the sender.
    user* partner;
    channel* room = channel_queue[channel_index];
    if (room->u_one->fd == sender_fd) {
        partner = room->u_two;
    } else {
        partner = room->u_one;
    }

    // Notify the partner that chat has ended.
    trs_send_chat_finish(partner);

    // Destroy the channel.
    free(channel_queue[channel_index]);
    channel_queue[channel_index] = NULL;
}

// Received BINARY_MESSAGE from a client.
void trs_handle_binary_message(int sender_fd) {
    // Make sure they're in a channel.
    int channel_index = channel_search_fd(sender_fd);

    // Ignore chat message if sender isn't in a channel.
    if (channel_index == -1) {
        return;
    }

    // Determine the recipient of this chat message.
    int recipient_fd;
    channel* room = channel_queue[channel_index];
    if (room->u_one->fd == sender_fd) {
        recipient_fd = room->u_two->fd;
    } else {
        recipient_fd = room->u_one->fd;
    }

    // Forward the message.
    trs_send_binary_message(recipient_fd, TRS_DATA, length_bytes);

    // Update the data usage in this channel.
    room->bytes_sent = room->bytes_sent + (length_bytes + TRS_HEADER_LEN);
}

// Local user typed /START
void trs_handle_admin_start() {
    // Boilerplate setup to start selecting connections.
    start_server();
}

// Local user typed /END
void trs_handle_admin_end() {

    // Notify all chatting users that chat is ending, and destroy channels.
    int channel_index;
    for (channel_index = 0; channel_index < MAX_CHANNELS; channel_index++) {
        channel *i_channel = channel_queue[channel_index];

        if (i_channel != NULL) {
            trs_send_chat_finish(i_channel->u_one);
            trs_send_chat_finish(i_channel->u_two);
        }

        free(i_channel);
        channel_queue[channel_index] = NULL;
    }

    // Notify user that all chats have been disconnected.
    printf("All chat rooms ended.\n");
}

// Local user typed /STATS
void trs_handle_admin_stats() {
    int connected_users = 0;
    int user_index;
    for (user_index = 0; user_index < MAX_USERS; user_index++) {
        user* i_user = user_queue[user_index];

        if (i_user != NULL) {
            if (connected_users == 0) {
                printf("Connected users:\n");
            }

            printf("\t%d  username:%s  fd:%d  ready:%d  blocked:%d\n",
                   connected_users++,
                   i_user->username,
                   i_user->fd,
                   i_user->ready,
                   i_user->blocked
            );
        }
    }

    if (connected_users == 0) {
        printf("No users connected.\n");
        return;
    }

    int active_channels = 0;
    int channel_index;
    for (channel_index = 0; channel_index < MAX_CHANNELS; channel_index++) {
        channel *i_channel = channel_queue[channel_index];

        if (i_channel != NULL) {
            if (active_channels == 0) {
                printf("Active channels:\n");
            }

            printf("\t%d  u_one:%s  u_one_fd:%d  u_two:%s  u_two_fd:%d  data_used:%lu\n",
                   active_channels++,
                   i_channel->u_one->username,
                   i_channel->u_one->fd,
                   i_channel->u_two->username,
                   i_channel->u_two->fd,
                   i_channel->bytes_sent
            );
        }
    }

    if (active_channels == 0) {
        printf("No active channels.\n");
    }
}

// Local user typed /THROWOUT
void trs_handle_admin_throwout() {

    // Make sure a space character is before a file descriptor.
    char * space_pos = strrchr(stdin_buffer, ' ');
    if (space_pos == NULL) {
        printf("Usage: /THROWOUT <user_fd>\n");
        return;
    }

    // Make sure there's an argument.
    char* newline_pos = strchr(stdin_buffer, '\n');
    if((newline_pos - space_pos) < 2) {
        printf("Usage: /THROWOUT <user_fd>\n");
        return;
    }
    *newline_pos = '\0';

    // Make sure parsed fd is legal.
    int target_fd = atoi(&stdin_buffer[10]);
    if (target_fd == 0) {
        printf("Invalid argument given.\n");
        return;
    }

    // Make sure this user exists.
    int user_index = user_search(target_fd);
    if (user_index == -1) {
        printf("No user with file descriptor %d.\n", target_fd);
        return;
    }

    // Check if they're in a channel.
    user* target_user = user_queue[user_index];
    int channel_index = channel_search_fd(target_fd);

    if (channel_index == -1) {
        printf("User %s with fd %d is not in any chat channel.\n", target_user->username, target_fd);
        return;
    }
    // Notify admin.
    printf("Ended chat for user %s with fd %d.\n", target_user->username, target_fd);

    // Notify both users that chat is ending.
    channel *target_channel = channel_queue[channel_index];
    trs_send_chat_finish(target_channel->u_one);
    trs_send_chat_finish(target_channel->u_two);

    // Destroy channel.
    free(target_channel);
    channel_queue[channel_index] = NULL;
}

// Local user typed /BLOCK
void trs_handle_admin_block() {
    // Make sure a space character is before a file descriptor.
    char * space_pos = strrchr(stdin_buffer, ' ');
    if (space_pos == NULL) {
        printf("Usage: /BLOCK <user_fd>\n");
        return;
    }

    // Make sure there's an argument.
    char* newline_pos = strchr(stdin_buffer, '\n');
    if((newline_pos - space_pos) < 2) {
        printf("Usage: /BLOCK <user_fd>\n");
        return;
    }
    *newline_pos = '\0';

    // Make sure parsed fd is legal.
    int target_fd = atoi(&stdin_buffer[7]);
    if (target_fd == 0) {
        printf("Invalid argument given.\n");
        return;
    }

    // Make sure this user exists.
    int user_index = user_search(target_fd);
    if (user_index == -1) {
        printf("No user with file descriptor %d.\n", target_fd);
        return;
    }

    // Block the user.
    user *target_user = user_queue[user_index];
    target_user->blocked = 1;

    // Notify admin.
    printf("User %s with fd %d blocked from joining new chat rooms.\n",
           target_user->username,
           target_fd
    );
}

// Local user typed /UNBLOCK
void trs_handle_admin_unblock() {
    // Make sure a space character is before a file descriptor.
    char * space_pos = strrchr(stdin_buffer, ' ');
    if (space_pos == NULL) {
        printf("Usage: /UNBLOCK <user_fd>\n");
        return;
    }

    // Make sure there's an argument.
    char* newline_pos = strchr(stdin_buffer, '\n');
    if((newline_pos - space_pos) < 2) {
        printf("Usage: /UNBLOCK <user_fd>\n");
        return;
    }
    *newline_pos = '\0';

    // Make sure parsed fd is legal.
    int target_fd = atoi(&stdin_buffer[9]);
    if (target_fd == 0) {
        printf("Invalid argument given.\n");
        return;
    }

    // Make sure this user exists.
    int user_index = user_search(target_fd);
    if (user_index == -1) {
        printf("No user with file descriptor %d.\n", target_fd);
        return;
    }

    // Unblock the user.
    user *target_user = user_queue[user_index];
    target_user->blocked = 0;

    // Notify admin.
    printf("User %s with fd %d not blocked from joining new chat rooms.\n",
           target_user->username,
           target_fd
    );
}

// Local user typed /HELP
void trs_handle_admin_help() {
    printf("Commands:\n");
    printf("/START\t\t\tOpen up chat rooms.\n");
    printf("/STATS\t\t\tSee stats about users and chat rooms.\n");
    printf("/END\t\t\tClose all active chat rooms.\n");
    printf("/THROWOUT <user_fd>\tEnd user's chat room.\n");
    printf("/BLOCK <user_fd>\tPrevent user from joining any chat room.\n");
    printf("/UNBLOCK <user_fd>\tAllow user to join chat rooms.\n");
    printf("/HELP\t\t\tShow this message.\n");
}
