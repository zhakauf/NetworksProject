/*
** selectserver.c -- a cheezy multiperson chat server
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "9034"   // port we're listening on
#define BACKLOG 10

#define MAXRCVSIZE 256 // size of receive buffer
#define MAXSENDSIZE 256 // size of send buffer

#define QSIZE 10 //size of chat queue

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


//struct to hold fds of servers in chat queue and their partners
struct queuestruct{
    int fdesc;
    int ready;
};

//struct to hold fds of servers in chatting queue
struct channelstruct{
    struct queuestruct one;
    struct queuestruct two;
};


//search function for file descriptor in chat queue array
int qssearch(struct queuestruct ** qs, int qssize, int fd)
{
    printf("I GOT HERE TOO\n");
    int k;
    for(k=0; k<qssize; k++)
    {
        //printf("%p\n",qs[k]);
        if ((qs[k] != 0) && (fd == qs[k]->fdesc)) {
            return k; 
        }
    }
    return -1;
}

//search function for file descriptor in chatting array
int channelsearch(struct channelstruct **cs, int cssize, int fd)
{
    int k;
    for(k=0; k<cssize; k++)
    {
        if (fd == cs[k]->one.fdesc)
            return k;
        if (fd == cs[k]->two.fdesc)
            return k;
    }
}

//create and populate a queuestruct with a client
struct queuestruct * newqs(int fda)
{
    struct queuestruct * newstruct = (struct queuestruct *)malloc(sizeof(struct queuestruct));
    newstruct->fdesc = fda;
    newstruct->ready = 0;
    return newstruct;
}

//create and populate a channelstruct with two partner clients
struct channelstruct * newcs(struct queuestruct fda,struct queuestruct fdb)
{
    struct channelstruct * newstruct = (struct channelstruct *)malloc(sizeof(struct channelstruct));
    newstruct->one = fda;
    newstruct->two= fdb;
    return newstruct;
}

int main(void)
{
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char bufrcv[MAXRCVSIZE];    // buffer for client data
    char bufsend[MAXSENDSIZE]; //buffer for data for client
    int nbytes;

    //struct for readyqueue
    //int readyqueue[QSIZE];
    //struct for chatqueue
    struct queuestruct * chatqueue[QSIZE];
    //struct for channelqueue
    struct channelstruct * channelqueue[QSIZE/2];
    //int for readyqueue index
    int chatnum = 0;
    //int for chatqueue index
    int channelindex = 0;
    //result for qssearch
    int qsr;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;
    int l;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    //memset(&readyqueue, 0, QSIZE); // clear chat queues
    memset(&chatqueue, 0, sizeof(chatqueue));
    memset(&channelqueue,0,sizeof(channelqueue));

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, BACKLOG) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    //initialize random number generator
    time_t t;
    srand((unsigned) time(&t));

    // main loop
    for(;;) {
        read_fds = master; // copy it
        //zero out send and receive buffers
        memset(&bufsend, 0, MAXSENDSIZE);
        memset(&bufrcv, 0, MAXRCVSIZE);

        //set up select between all connected clients
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, bufrcv, MAXRCVSIZE, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        
                        //if the client was in chatqueue, remove it
                        if(qsr = qssearch(chatqueue, QSIZE, i) != -1)
                        {
                            memset(&chatqueue[qsr], 0, 1);
                            chatnum --;
                        }
                        
                        //if the client was in channelqueue, remove it
                        if(qsr = channelsearch(channelqueue, QSIZE/2, i) != -1) {
                            memset(&channelqueue[qsr], 0, 1);
                        }

                        FD_CLR(i, &master); // remove from master set
                    } else {
                        //got some data from an already connected client
                        //is the client in the chat queue?
                        //
                        int up = 0;
                        for (up = 0; up < QSIZE; up++) {
                            printf("%p\n", chatqueue[up]);
                        }
                        if((qsr = qssearch(chatqueue, QSIZE, i)) == -1)
                        {
                            //if the client isn't in the chat queue
                            //see if it's trying to connect
                            if(strncmp(bufrcv,"CONNECT",7) == 0)
                            {   
                                //can we add it 
                                if (chatnum < QSIZE){
                                    j = 0;
                                    while(j<QSIZE){
                                        if (chatqueue[j] == 0)
                                        {
                                            chatqueue[j] = newqs(i);
                                            break;
                                        }
                                        j++;
                                    }
                                    chatnum ++;
                                }
                                else{
                                    memset(&bufsend, 0, MAXSENDSIZE);
                                    strncpy(bufsend,"FAIL\0",5);
                                    if (send(i, bufsend, 5, 0) == -1){
                                        perror("send");
                                    }
                                }
                            }
                        } else {
                            printf("got here two\n");
                            //the client is in the chat queue is the client asking to chat
                            if(strncmp(bufrcv,"CHAT",4) == 0){
                                printf("Got here three\n");
                                //is there anyone else ready to chat
                                chatqueue[qsr]->ready = 1;
                                for(j=0;j<QSIZE;j++){
                                    if((chatqueue[j]!=0) && (chatqueue[j]->ready == 1) && (chatqueue[j]->fdesc != i)){
                                        //add the two clients to a chat channel
                                        l = 0;
                                        chatqueue[qsr]->ready = 0;
                                        chatqueue[j]->ready = 0;
                                        printf("Got here four\n");
                                        while(l<QSIZE/2){
                                            if(channelqueue[l] == 0){
                                                channelqueue[l] = newcs((*chatqueue[qsr]),(*chatqueue[j]));
                                                break;
                                            }
                                            l++;
                                        }
                                        printf("Got here four\n");
                                        memset(&bufsend, 0, sizeof(bufsend));
                                        strncpy(bufsend,"SUCCESS\0",8);
                                        if (send(i, bufsend, 8, 0) == -1){
                                            perror("send");
                                        }
                                        if (send(j, bufsend, 8, 0) == -1){
                                            perror("send");
                                        }

                                    }
                                }
                            } else {
                                printf("Got here four\n");
                                //the client is not asking to chat 
                                // we got some data from a client
                                // is the client in a channel?
                                if(qsr = channelsearch(channelqueue,QSIZE/2, i) == -1){
                                    printf("Got here five\n");
                                    //client is not in a chat channel, send a fail
                                    memset(&bufsend, 0, MAXSENDSIZE);
                                    strncpy(bufsend,"FAIL\0",5);
                                    if (send(i, bufsend, 5, 0) == -1) {
                                        perror("send");
                                    }
                                }
                                else{
                                    //who are we chatting with?
                                    if(channelqueue[qsr]->one.fdesc == i) {
                                        if(FD_ISSET(channelqueue[qsr]->two.fdesc, &master)){
                                            if (send(channelqueue[qsr]->two.fdesc, bufrcv, nbytes, 0) == -1){ // send data to partner
                                                perror("send");
                                            }
                                        }
                                        else{
                                            //partner not readable, send a fail
                                            memset(&bufsend, 0, MAXSENDSIZE);
                                            strncpy(bufsend,"FAIL\0",5);
                                            if (send(i, bufsend, 5, 0) == -1) {
                                                perror("send");
                                            }
                                        }
                                    }
                                    if(channelqueue[qsr]->two.fdesc == i){
                                        if(FD_ISSET(channelqueue[qsr]->one.fdesc, &master)){
                                            if (send(channelqueue[qsr]->one.fdesc, bufrcv, nbytes, 0) == -1){ // send data to partner
                                                perror("send");
                                            }
                                        }
                                        else{
                                            //partner not readable, send a fail
                                            memset(&bufsend, 0, MAXSENDSIZE);
                                            strncpy(bufsend,"FAIL\0",5);
                                            if (send(i, bufsend, 5, 0) == -1) {
                                                perror("send");
                                            }
                                        }
                                    } else {
                                        if(FD_ISSET(channelqueue[qsr]->one.fdesc, &master)){
                                            if (send(channelqueue[qsr]->one.fdesc, bufrcv, nbytes, 0) == -1){ // send data to partner
                                                perror("send");
                                            }
                                        }
                                        else{
                                            //partner not readable, send a fail
                                            memset(&bufsend, 0, MAXSENDSIZE);
                                            strncpy(bufsend,"FAIL\0",5);
                                            if (send(i, bufsend, 5, 0) == -1){
                                                perror("send");
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}

