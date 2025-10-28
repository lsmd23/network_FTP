#pragma once

#include "utils.h"
#include "connect.h"

#define BUFFER_SIZE 8192 // 文件传输缓冲区大小
// #define FTP_ROOT_DIR "." // FTP服务器根目录

// static void ensure_session_cwd(connection *session);
// static void normalize_virtual_path(const char *base, const char *input, char *out, size_t outsz);
int is_path_safe(const char *path);
int handle_retr_command(int client_socket, connection *session, const char *filename);
int handle_stor_command(int client_socket, connection *session, const char *filename);
// int handle_cwd_command(int client_socket, const char *path);
