/*
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

        // Zero out send and receive buffers
        memset(&bufsend, 0, MAXSENDSIZE);
        memset(&bufrcv, 0, MAXRCVSIZE);

        // Populate read_fds.
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // Run through the existing connections looking for data to read.
        int i, new_fd;
        struct sockaddr_storage remoteaddr;
        for(i = 0; i <= fdmax; i++) {

            // Here's a connection.
            if (FD_ISSET(i, &read_fds)) {

                // Handle new connection.
                if (i == listener) {
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
                    if ((nbytes = recv(i, bufrcv, MAXRCVSIZE, 0)) <= 0) {

                        // Error or connection closed by client.
                        if (nbytes == 0) {
                            printf("Socket %d hung up.\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i);

                        // TODO: disconnect_user(int fd);
                        // Does remove from channels, and remove from user_queue.
                        remove_user_from_channel(i);
                        remove_user(i);

                        FD_CLR(i, &master); // remove from master set

                    // Got some data from an already connected client
                    } else {

                        // Did they issue a CONNECT command?
                        if (strncmp(bufrcv, CONNECT_CMD, CONNECT_CMD_L) == 0) {

                            // TODO: handle_connect_cmd(int fd);

                            // Extract username.
                            // TODO respect max username len.
                            char* username_loc = (char*)&bufrcv[CONNECT_CMD_L + 1];
                            char* cr_loc = strchr(username_loc, '\r');
                            int len = cr_loc - username_loc;
                            char *username = (char*)malloc(len + 1);
                            memset(username, 0, len+1);
                            strncpy(username, username_loc, len);

                            int success = add_user(i, username);
                            if (success != -1) {
                                printf("User %s added.\n", user_queue[success]->username);
                            }
                            else {
                                printf("User %s NOT added.\n", username);
                            }
                        }

                        // Did they issue a CHAT command?
                        else if (strncmp(bufrcv, CHAT_CMD, CHAT_CMD_L) == 0) {
                            handle_chat(i);
                        }

                        else if (strncmp(bufrcv, MSG_CMD, MSG_CMD_L) == 0) {
                            handle_msg(i);
                        }

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
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
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
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    // Prevent "address already in use" error message.
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
        fprintf(stderr, "selectserver: failed to bind\n");
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
int channel_search(user* u_one, user* u_two) {
    int i;
    for(i = 0; i < MAX_CHANNELS; i++) {
        if (channel_queue[i] != NULL) {
           if ((channel_queue[i]->u_one == u_one)  || (channel_queue[i]->u_two == u_one)) {
               return i;
           }
        }
        if (channel_queue[i] != NULL) {
            if ((channel_queue[i]->u_one == u_two)  || (channel_queue[i]->u_two = u_two)) {
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

int remove_user_from_channel(int fd) {
    int i;
    for (i = 0; i < MAX_CHANNELS; i++) {
        channel* i_channel = channel_queue[i];

        if (i_channel == NULL) {
            return 0;
        }

        if ((i_channel->u_one->fd == fd) || (i_channel->u_two->fd == fd))  {
            // TODO: Notify one of the users the channel is dead.
            free(channel_queue[i]);
            channel_queue[i] = NULL;
            return 0;
        }
    }
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
    int search_result = channel_search(u_one, u_two);

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

int handle_chat(int fd) {
    // Make sure they're already in the user queue.
    int result = user_search(fd);

    if (result == -1) {
        printf("User sent CHAT but has not sent CONNECT <username>.\n");
    } else {
        user * client = user_queue[result];

        // Make sure they're not already in a channel.
        result = channel_search(client, NULL);

        if (result != -1) {
            printf("User %s is already in a channel.\n", client->username);
            return -1;
        }

        printf("User %s sent CHAT.\n", client->username);
        int available_user = find_available_user(client);
        if (available_user == -1){
            client->ready = 1;
            printf("User %s is ready to chat.\n", client->username);
        } else {
            user * available = user_queue[available_user];
            printf("User %s is already available.\n", available->username);
            available->ready = 0;

            int new_channel = add_channel(client, available);

            memset(&bufsend, 0, sizeof(bufsend));
            strncpy(bufsend,"SUCCESS\0",8);
            if (send(client->fd, bufsend, 8, 0) == -1){
                perror("send");
            }
            if (send(available->fd, bufsend, 8, 0) == -1){
                perror("send");
            }

            return new_channel;
        }
    }
}

int handle_msg(int fd) {
    int sender_index = user_search(fd);

    if (sender_index == -1) {
        printf("Got MSG from client who didn't CONNECT.\n");
        return -1;
    }

    user* sender = user_queue[sender_index];

    int channel_index = channel_search(sender, NULL);

    if (channel_index == -1) {
        printf("User was not in a channel.\n");
        return -1;
    }

    channel* chan = channel_queue[channel_index];
    user* receiver;

    if (chan->u_one == sender) {
        receiver = chan->u_two;
    } else if (chan->u_two == sender){
        receiver = chan->u_one;
    }

    char* msg_loc = (char*)&bufrcv[MSG_CMD_L + 1];
    char* cr_loc = strchr(msg_loc, '\r');
    int len = cr_loc - msg_loc;
    // TODO: len must be less than buff size

    char *msg = (char*)malloc(len + 1);
    memset(msg, 0, len+1);
    strncpy(msg, msg_loc, len);
    msg[len+1] = '\n';

    char* null_pos = strchr(msg, '\0');
    *null_pos = '\n';

    memset(&bufsend, 0, sizeof(bufsend));
    strncpy(bufsend, msg, len+1);
    if (send(receiver->fd, bufsend, len, 0) == -1){
        perror("send");
    }
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
