#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define CONTROL_PORT 21021   // 控制连接端口
#define WAITING_QUEUE_SIZE 5 // 监听队列大小
#define LINE_MAX_SIZE 1024   // 最大行长度

void handle_connection(int client_socket);
