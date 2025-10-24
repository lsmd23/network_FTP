#include "connect.h"

/**
 * 处理PORT命令，设置数据连接的地址和端口
 * @param client_socket 客户端控制连接的socket
 * @param arg PORT命令的参数，格式为 h1,h2,h3,h4,p1,p2
 * @param session 会话状态结构体，包含客户端套接字识别码，IP地址和端口以及连接模式
 * @return 0 成功，-1 参数错误
 */
int handle_port_command(int client_socket, const char *arg, connection *session)
{
    // 解析PORT命令的参数，格式为 h1,h2,h3,h4,p1,p2
    int h1, h2, h3, h4, p1, p2;
    if (sscanf(arg, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6)
        return -1; // 参数格式错误

    // 计算端口号
    int port = (p1 << 8) | p2;

    // 设置数据连接的地址结构体
    memset(&session->data_addr, 0, sizeof(session->data_addr));
    session->data_addr.sin_family = AF_INET;
    session->data_addr.sin_addr.s_addr = htonl((h1 << 24) | (h2 << 16) | (h3 << 8) | h4);
    session->data_addr.sin_port = htons(port);

    // 更新会话状态为PORT模式
    session->mode = DATA_CONN_MODE_PORT;
    return 0; // 成功
}

/**
 * 处理PASV命令，设置被动模式的数据连接
 * @param client_socket 客户端控制连接的socket
 * @param session 会话状态结构体，包含客户端套接字识别码，IP地址和端口以及连接模式
 * @return 0 成功，-1 创建socket失败，-2 绑定失败，-3 获取端口号失败，-4 监听失败
 */
int handle_pasv_command(int client_socket, connection *session)
{
    // 创建一个新的socket用于监听数据连接
    int pasv_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (pasv_socket < 0)
        return -1; // 创建socket失败

    // 绑定一个随机端口
    struct sockaddr_in pasv_addr;
    memset(&pasv_addr, 0, sizeof(pasv_addr));
    pasv_addr.sin_family = AF_INET;
    pasv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    pasv_addr.sin_port = 0; // 让系统分配一个随机端口

    if (bind(pasv_socket, (struct sockaddr *)&pasv_addr, sizeof(pasv_addr)) < 0)
    {
        close(pasv_socket);
        return -2; // 绑定失败
    }

    // 获取分配的端口号
    socklen_t addr_len = sizeof(pasv_addr);
    if (getsockname(pasv_socket, (struct sockaddr *)&pasv_addr, &addr_len) < 0)
    {
        close(pasv_socket);
        return -3; // 获取端口号失败
    }

    // 开始监听
    if (listen(pasv_socket, 1) < 0)
    {
        close(pasv_socket);
        return -4; // 监听失败
    }

    // 处理成功响应
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    if (getsockname(client_socket, (struct sockaddr *)&server_addr, &server_addr_len) < 0)
    {
        close(pasv_socket);
        return -5; // 获取服务器地址失败
    }
    unsigned int ip = ntohl(server_addr.sin_addr.s_addr);
    unsigned int port = ntohs(pasv_addr.sin_port);
    char response[128];
    snprintf(response, sizeof(response), "Entering Passive Mode (%u,%u,%u,%u,%u,%u).",
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
             (port >> 8) & 0xFF, port & 0xFF);
    send_response(client_socket, 227, response);

    // 更新会话状态为PASV模式
    // session->data_addr = pasv_addr;
    session->client_data_socket = pasv_socket;
    session->mode = DATA_CONN_MODE_PASV;
    return 0; // 成功
}

/**
 * 建立数据连接，根据会话状态中的连接模式进行处理
 * @param session 会话状态结构体，包含客户端套接字识别码，IP地址和端口以及连接模式
 * @return 数据连接的socket，成功返回socket，失败返回-1，未设置有效的连接模式返回-2
 */
int establish_data_connection(connection *session)
{
    if (session->mode == DATA_CONN_MODE_PORT)
    {
        // 主动模式，连接客户端指定的地址和端口
        int data_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (data_socket < 0)
            return -1; // 创建socket失败

        if (connect(data_socket, (struct sockaddr *)&session->data_addr, sizeof(session->data_addr)) < 0)
        {
            close(data_socket);
            return -1; // 连接失败
        }
        return data_socket; // 返回数据连接socket
    }
    else if (session->mode == DATA_CONN_MODE_PASV)
    {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        // 被动模式，接受客户端的连接请求，并返回数据连接socket
        int data_socket = accept(session->client_data_socket, (struct sockaddr *)&client_addr, &len);
        if (data_socket < 0)
            return -1; // 接受连接失败

        close(session->client_data_socket); // 关闭监听socket
        session->client_data_socket = -1;

        return data_socket; // 返回数据连接socket
    }
    return -2; // 未设置有效的连接模式
}