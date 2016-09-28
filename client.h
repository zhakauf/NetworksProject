// File descriptor for communicating with the TRS server.
int server_fd;

// Whether client is currently connected to TRS server.
int connected_to_trs_server = 0;

// Whether client is currently in a chat room.
int in_chat_room = 0;

// This client's username.
char* client_username = NULL;

// Current chat partner's username.
char* chat_partner = NULL;

// Initialization.
void initialize_trs(char* hostname);

// Send messages to the server.
void trs_send_connect_request(char* username, unsigned char username_length);
void trs_send_chat_request(void);
void trs_send_help_request(void);
void trs_send_chat_finish(void);

// Handle messages from the server.
void trs_handle_connect_acknowledge(char* data, size_t length);
void trs_handle_chat_acknowledge(char* data, size_t length);
void trs_handle_chat_message(char* data, size_t length);
void trs_handle_chat_finish(char* data, size_t length);
void trs_handle_help_acknowledge(char* data, size_t length);
void trs_handle_connect_fail(char* data, size_t length);
void trs_handle_chat_fail(char* data, size_t length);
void trs_handle_binary_message(char* data, size_t length);

// Handle input from user.
void trs_handle_client_connect(char* stdinbuf);
void trs_handle_client_chat(void);
void trs_handle_client_quit(void);
void trs_handle_client_transfer(void);
void trs_handle_client_help(void);
void trs_handle_client_chat_message(char* data);
