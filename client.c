/*
** client.c -- a stream socket client demo
*/

#include "trs.h"
#include "client.h"

#define MAXSENDSIZE 256 // max number of bytes we can send at once 
#define MAXRCVSIZE 256 //max number of bytes we can receive at once


int main(int argc, char *argv[])
{
    int sockfd, numbytes;  
    //char * bufsend;
    char bufsend[MAXSENDSIZE];
    char bufrec[MAXRCVSIZE];
    int sendercount; //to make sure all data is sent
    int sentcount; // result from send() command
    //char * userdata; // input from user
    struct addrinfo hints, *servinfo, *p;
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



    //ask to be part of the chat queue
    strncpy(bufsend,"CONNECT\0",8);
    if ((sentcount = send(sockfd,bufsend, 8, 0)) == -1) {
        perror("send");
        exit(1);
    }   
    memset(&bufsend,0,sizeof(bufsend));
    //ask to chat with random other partner
    strncpy(bufsend,"CHAT\0",5);
    if ((sentcount = send(sockfd,bufsend, 5, 0)) == -1) {
        perror("send");
        exit(1);
    }

     
    while(1) {
    
    //clear sent and received buffers
    memset(&bufsend, 0, MAXSENDSIZE);
    memset(&bufrec, 0, MAXRCVSIZE);

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
        for(i=0; i<=sockfd; i++) {
            if (FD_ISSET(i, &read_fds)){//data incoming
                if (i == stdin->_fileno)
                {
                    //if the data is from the user
                    fgets(bufrec, MAXRCVSIZE, stdin);
                    //send the data to the server if chatting
                    if(chatting)
                    {

                        if ((sentcount = send(sockfd,bufrec, strlen(bufrec) + 1, 0)) == -1) {
                            perror("send");
                            exit(1);
                        }    
                    }
                    memset(&bufrec,0,sizeof(bufrec));
                }

                else if (sentcount = recv(i, bufrec, MAXRCVSIZE, 0) <=0) {
                    //connection closed
                        if (sentcount == 0) {
                            printf("Connection with server closed\n");
                            chatting = 0;
                            exit(0);
                        }
                        else {
                            perror("recv");
                        }
                        close(i);
                        FD_CLR(i, &read_fds);
                }
                else {
                    //data received from server or cin
                    //
                //see if CHAT command was successful
                    if(strncmp(bufrec,"SUCCESS",7) == 0) {
                                chatting = 1;
                    }
                    if(chatting) {
                        printf("%s\n",bufrec);
                    }
                    memset(&bufrec,0,sizeof(bufrec));
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

