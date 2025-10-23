#include "main.h"

int main(int argc, char **argv)
{
    int listen_socket;              // 监听socket
    int connected_socket;           // 连接socket
    struct sockaddr_in server_addr; // 服务器地址结构体

    // 创建监听socket
    if ((listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("listen socket creation failed!");
        exit(EXIT_FAILURE);
    }

    // 配置服务器地址结构体
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(CONTROL_PORT);

    // 绑定地址和端口到socket
    if (bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind failed!");
        close(listen_socket);
        exit(EXIT_FAILURE);
    }

    // 开始监听连接请求
    if (listen(listen_socket, WAITING_QUEUE_SIZE) == -1)
    {
        perror("listen failed!");
        close(listen_socket);
        exit(EXIT_FAILURE);
    }

    // 监听主循环
    while (1)
    {
        if ((connected_socket = accept(listen_socket, NULL, NULL)) == -1) // 尝试接受连接
        {
            perror("accept failed!");
            continue;
        }

        handle_connection(connected_socket); // 处理连接
    }

    close(listen_socket); // 关闭监听socket
    return 0;
}