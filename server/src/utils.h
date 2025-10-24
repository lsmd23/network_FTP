#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define CONTROL_PORT 10021   // 控制连接端口
#define WAITING_QUEUE_SIZE 5 // 监听队列大小
#define LINE_MAX_SIZE 1024   // 最大行长度

void send_response(int client_socket, int code, const char *message);
void send_multiline_response(int client_socket, int code, const char *messages[]);
int read_line(int client_socket, char *buffer, size_t max_len);
void parse_cmd_param(const char *line, char *cmd, char *arg);