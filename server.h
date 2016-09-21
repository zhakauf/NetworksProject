// How many pending connections the queue will hold.
#define BACKLOG 10

// Size of the receive buffer.
#define MAXRCVSIZE 256

// Size of the send buffer.
#define MAXSENDSIZE 256

// Max number of users that can be connected at once.
#define MAX_USERS 2

// Max number of chat channels that can be open at once.
#define MAX_CHANNELS MAX_USERS/2

// Max length of a client's username.
#define MAX_USERNAME_LENGTH 10

// Commands.
#define CONNECT_CMD "CONNECT"
#define CONNECT_CMD_L 7
#define CHAT_CMD "CHAT"
#define CHAT_CMD_L 4
#define MSG_CMD "MSG"
#define MSG_CMD_L 3

// Struct to store information about a single connected user.
typedef struct {
    char* username; // Their username.
    int fd; // File descriptor for their socket.
    int ready; // 1 if trying to chat, 0 otherwise.
} user;

// Struct to store information about a channel where two users are talking.
typedef struct channelstruct{
    user *u_one;
    user *u_two;
    unsigned long bytes_sent;
} channel;

// Prototypes.
void start_server(void);
int user_search(int fd);
int channel_search(user* u_one, user* u_two);
int add_user(int fd, char* username);
int remove_user(int fd);
user * new_user(int fd, char* username);
channel * new_channel(user *u_one, user *u_two);
int handle_chat(int fd);
int find_available_user(user* client);

// List of user pointers.
user* user_queue[MAX_USERS] = {NULL};

// List of channel pointers.
channel * channel_queue[MAX_CHANNELS] = {NULL};

// Master fs_set, and temp copy for select() calls.
fd_set master;
fd_set read_fds;

// Maximum file descriptor number.
int fdmax;

// Listening socket descriptor.
int listener;

// Client address.
socklen_t addrlen;

// Address info.
struct addrinfo hints;

// Receiving data buffer.
char bufrcv[MAXRCVSIZE];

// Outgoing data buffer.
char bufsend[MAXSENDSIZE];
