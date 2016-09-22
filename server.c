﻿/*
 * server.c
 * TRS Server
 *
 * Usage:
 * ./server
 */

#include "trs.h"
#include "server.h"

int main(void)
{
    printf("TRS Admin: Type /START to open chat rooms.\n");
    
    // Boilerplate setup to start selecting connections.
    start_server();

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

                // Handle new connection.
                if (i_fd == listener) {
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

                    // Zero out send and receive buffers
                    memset(&bufrcv, 0, MAXRCVSIZE);

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

                        // Did they issue a CONNECT command?
//                        if (strncmp(bufrcv, CONNECT_CMD, CONNECT_CMD_L) == 0) {
//
//                            // TODO: handle_connect_cmd(int fd);
//
//                            // Extract username.
//                            // TODO respect max username len.
//                            char* username_loc = (char*)&bufrcv[CONNECT_CMD_L + 1];
//                            char* cr_loc = strchr(username_loc, '\r');
//                            int len = cr_loc - username_loc;
//                            char *username = (char*)malloc(len + 1);
//                            memset(username, 0, len+1);
//                            strncpy(username, username_loc, len);
//
//                            int success = add_user(i, username);
//                            if (success != -1) {
//                                printf("User %s added.\n", user_queue[success]->username);
//                            }
//                            else {
//                                printf("User %s NOT added.\n", username);
//                            }
//                        }
//
//                        // Did they issue a CHAT command?
//                        else if (strncmp(bufrcv, CHAT_CMD, CHAT_CMD_L) == 0) {
//                            handle_chat(i);
//                        }
//
//                        else if (strncmp(bufrcv, MSG_CMD, MSG_CMD_L) == 0) {
//                            handle_msg(i);
//                        }

//                        else {
//                            printf("got here two\n");
//                            //the client is in the chat queue is the client asking to chat
//                            if(strncmp(bufrcv,"CHAT",4) == 0){
//                                printf("Got here three\n");
//                                //is there anyone else ready to chat
//                                user_queue[qsr]->ready = 1;
//                                for(j=0;j<MAX_USERS;j++){
//                                    if((user_queue[j]!=0) && (user_queue[j]->ready == 1) && (user_queue[j]->fd != i)){
//                                        //add the two clients to a chat channel
//                                        l = 0;
//                                        user_queue[qsr]->ready = 0;
//                                        user_queue[j]->ready = 0;
//                                        printf("Got here four\n");
//                                        while(l<MAX_CHANNELS){
//                                            if(channel_queue[l] == 0){
//                                                channel_queue[l] = new_channel(user_queue[qsr],user_queue[j]);
//                                                break;
//                                            }
//                                            l++;
//                                        }
//                                        printf("Got here four\n");
//                                        memset(&bufsend, 0, sizeof(bufsend));
//                                        strncpy(bufsend,"SUCCESS\0",8);
//                                        if (send(i, bufsend, 8, 0) == -1){
//                                            perror("send");
//                                        }
//                                        if (send(j, bufsend, 8, 0) == -1){
//                                            perror("send");
//                                        }
//
//                                    }
//                                }
//                            } else {
//                                printf("Got here four\n");
//                                //the client is not asking to chat
//                                // we got some data from a client
//                                // is the client in a channel?
//                                if(qsr = channel_search(channel_queue,MAX_CHANNELS, i) == -1){
//                                    printf("Got here five\n");
//                                    //client is not in a chat channel, send a fail
//                                    memset(&bufsend, 0, MAXSENDSIZE);
//                                    strncpy(bufsend,"FAIL\0",5);
//                                    if (send(i, bufsend, 5, 0) == -1) {
//                                        perror("send");
//                                    }
//                                }
//                                else{
//                                    //who are we chatting with?
//                                    if(channel_queue[qsr]->u_one->fd == i) {
//                                        if(FD_ISSET(channel_queue[qsr]->u_two->fd, &master)){
//                                            if (send(channel_queue[qsr]->u_two->fd, bufrcv, nbytes, 0) == -1){ // send data to partner
//                                                perror("send");
//                                            }
//                                        }
//                                        else{
//                                            //partner not readable, send a fail
//                                            memset(&bufsend, 0, MAXSENDSIZE);
//                                            strncpy(bufsend,"FAIL\0",5);
//                                            if (send(i, bufsend, 5, 0) == -1) {
//                                                perror("send");
//                                            }
//                                        }
//                                    }
//                                    if(channel_queue[qsr]->u_two->fd == i){
//                                        if(FD_ISSET(channel_queue[qsr]->u_one->fd, &master)){
//                                            if (send(channel_queue[qsr]->u_one->fd, bufrcv, nbytes, 0) == -1){ // send data to partner
//                                                perror("send");
//                                            }
//                                        }
//                                        else{
//                                            //partner not readable, send a fail
//                                            memset(&bufsend, 0, MAXSENDSIZE);
//                                            strncpy(bufsend,"FAIL\0",5);
//                                            if (send(i, bufsend, 5, 0) == -1) {
//                                                perror("send");
//                                            }
//                                        }
//                                    } else {
//                                        if(FD_ISSET(channel_queue[qsr]->u_one->fd, &master)){
//                                            if (send(channel_queue[qsr]->u_one->fd, bufrcv, nbytes, 0) == -1){ // send data to partner
//                                                perror("send");
//                                            }
//                                        }
//                                        else{
//                                            //partner not readable, send a fail
//                                            memset(&bufsend, 0, MAXSENDSIZE);
//                                            strncpy(bufsend,"FAIL\0",5);
//                                            if (send(i, bufsend, 5, 0) == -1){
//                                                perror("send");
//                                            }
//                                        }
//                                    }
//                                }
//                            }
//                        }
                    }
                }
            }
        }
    }
    
    return 0;
}

// Get the server listening.
void start_server() {
    // For setsockopt() SO_REUSEADDR
    int yes = 1;

    // Address infos.
    struct addrinfo *ai, *p;

    // Zero the master and temp file descriptor sets.
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

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

    // Keep track of the biggest file descriptor.
    fdmax = listener;

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

//int handle_chat(int fd) {
//    // Make sure they're already in the user queue.
//    int result = user_search(fd);
//
//    if (result == -1) {
//        printf("User sent CHAT but has not sent CONNECT <username>.\n");
//    } else {
//        user * client = user_queue[result];
//
//        // Make sure they're not already in a channel.
//        result = channel_search_users(client, NULL);
//
//        if (result != -1) {
//            printf("User %s is already in a channel.\n", client->username);
//            return -1;
//        }
//
//        printf("User %s sent CHAT.\n", client->username);
//        int available_user = find_available_user(client);
//        if (available_user == -1){
//            client->ready = 1;
//            printf("User %s is ready to chat.\n", client->username);
//        } else {
//            user * available = user_queue[available_user];
//            printf("User %s is already available.\n", available->username);
//            available->ready = 0;
//
//            int new_channel = add_channel(client, available);
//
//            memset(&bufsend, 0, sizeof(bufsend));
//            strncpy(bufsend,"SUCCESS\0",8);
//            if (send(client->fd, bufsend, 8, 0) == -1){
//                perror("send");
//            }
//            if (send(available->fd, bufsend, 8, 0) == -1){
//                perror("send");
//            }
//
//            return new_channel;
//        }
//    }
//}
//
//int handle_msg(int fd) {
//    int sender_index = user_search(fd);
//
//    if (sender_index == -1) {
//        printf("Got MSG from client who didn't CONNECT.\n");
//        return -1;
//    }
//
//    user* sender = user_queue[sender_index];
//
//    int channel_index = channel_search_users(sender, NULL);
//
//    if (channel_index == -1) {
//        printf("User was not in a channel.\n");
//        return -1;
//    }
//
//    channel* chan = channel_queue[channel_index];
//    user* receiver;
//
//    if (chan->u_one == sender) {
//        receiver = chan->u_two;
//    } else if (chan->u_two == sender){
//        receiver = chan->u_one;
//    }
//
//    char* msg_loc = (char*)&bufrcv[MSG_CMD_L + 1];
//    char* cr_loc = strchr(msg_loc, '\r');
//    int len = cr_loc - msg_loc;
//    // TODO: len must be less than buff size
//
//    char *msg = (char*)malloc(len + 1);
//    memset(msg, 0, len+1);
//    strncpy(msg, msg_loc, len);
//    msg[len+1] = '\n';
//
//    char* null_pos = strchr(msg, '\0');
//    *null_pos = '\n';
//
//    memset(&bufsend, 0, sizeof(bufsend));
//    strncpy(bufsend, msg, len+1);
//    if (send(receiver->fd, bufsend, len, 0) == -1){
//        perror("send");
//    }
//}

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
