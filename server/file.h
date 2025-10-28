#pragma once

#include "utils.h"
#include "connect.h"

#define BUFFER_SIZE 8192 // 文件传输缓冲区大小
#define FTP_ROOT_DIR "." // FTP服务器根目录
#define PATH_MAX 4096    // 最大路径长度

int is_path_safe(const char *path);
int handle_retr_command(int client_socket, connection *session, const char *filename);
int handle_stor_command(int client_socket, connection *session, const char *filename);