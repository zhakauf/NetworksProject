#include <errno.h>
#include <fcntl.h>

void trs_send_connect_request(int fd, char* username, unsigned char username_length);

void trs_handle_connect_acknowledge(int sender_fd, char* data, size_t length);
void trs_handle_chat_acknowledge(int sender_fd, char* data, size_t length);
void trs_handle_chat_message(int sender_fd, char* data, size_t length);
