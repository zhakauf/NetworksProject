// Send messages to the server.
void trs_send_connect_request(int fd, char* username, unsigned char username_length);

// Handle messages from the server.
void trs_handle_connect_acknowledge(int sender_fd, char* data, size_t length);
void trs_handle_chat_acknowledge(int sender_fd, char* data, size_t length);
void trs_handle_chat_message(int sender_fd, char* data, size_t length);
void trs_handle_chat_finish(int sender_fd, char* data, size_t length);

// Handle input from user.
void trs_handle_client_connect(char* username);
void trs_handle_client_chat(void);
void trs_handle_client_quit(void);
void trs_handle_client_transfer(void);
void trs_handle_client_chat_message(char* data);
