#pragma once

#include "utils.h"

typedef enum
{
    DATA_CONN_MODE_NONE,
    DATA_CONN_MODE_PORT,
    DATA_CONN_MODE_PASV
} data_conn_mode_t;

typedef struct
{
    int client_data_socket;       // 客户端数据连接socket
    struct sockaddr_in data_addr; // 客户端数据连接地址
    data_conn_mode_t mode;        // 数据连接模式
    char root_dir[PATH_MAX];      // FTP服务器根目录
    int bytes_transferred;        // 传输的字节数
} connection;

int handle_port_command(int client_socket, const char *arg, connection *session);
int handle_pasv_command(int client_socket, connection *session);
int establish_data_connection(connection *session);