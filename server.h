// How many pending connections the queue will hold.
#define BACKLOG 10

// Max number of users that can be connected at once.
#define MAX_USERS 2

// Max number of chat channels that can be open at once.
#define MAX_CHANNELS MAX_USERS/2

// Struct to store information about a single connected user.
typedef struct {
    char* username; // Their username.
    int fd; // File descriptor for their socket.
    int ready; // 1 if trying to chat, 0 otherwise.
} user;

// Struct to store information about a channel where two users are talking.
typedef struct {
    user *u_one;
    user *u_two;
    unsigned long bytes_sent;
} channel;

// List of user pointers.
user* user_queue[MAX_USERS] = {NULL};

// List of channel pointers.
channel * channel_queue[MAX_CHANNELS] = {NULL};

// Maximum file descriptor number.
int fdmax;

// Listening socket descriptor.
int listener;

// Client address.
socklen_t addrlen;

// Internal maintenance functions.
void initialize_trs(void);
void start_server(void);
int user_search(int fd);
int channel_search_users(user* u_one, user* u_two);
int channel_search_fd(int fd);
int add_user(int fd, char* username);
int remove_user(int fd);
user * new_user(int fd, char* username);
channel * new_channel(user *u_one, user *u_two);
int handle_chat(int fd);
int find_available_user(user* client);
void disconnect_user(int fd);

// Functions to send messages to remote clients.
void trs_send_chat_finish(user* u);
void trs_send_chat_fail(int fd);
void trs_send_connect_fail(int fd);

// Functions to handle messages from remote clients.
void trs_handle_connect_request(int sender_fd, char* data, size_t length);
void trs_handle_chat_request(int sender_fd, char* data, size_t length);
void trs_handle_chat_message(int sender_fd, char* data, size_t length);
void trs_handle_chat_finish(int sender_fd, char* data, size_t length);
void trs_handle_binary_message(int sender_fd, char* data, size_t length);
void trs_handle_help_request(int sender_fd, char* data, size_t length);

// Functions to handle commands from local user.
void trs_handle_admin_start(void);

