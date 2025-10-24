#include "utils.h"

/**
 * 向指定的客户端套接字发送响应消息
 * @param client_socket 客户端套接字
 * @param code 响应代码
 * @param message 响应消息
 */
void send_response(int client_socket, int code, const char *message)
{
    char response[LINE_MAX_SIZE];
    snprintf(response, sizeof(response), "%d %s\r\n", code, message);
    if (send(client_socket, response, strlen(response), 0) == -1)
    {
        perror("send failed");
    }
}

/**
 * 向指定的客户端发送多行响应消息
 * @param client_socket 客户端套接字
 * @param code 响应代码
 * @param messages 响应消息数组，以 NULL 结尾
 */
void send_multiline_response(int client_socket, int code, const char *messages[])
{
    char response[LINE_MAX_SIZE];
    int i = 0;
    // 发送除最后一行外的所有行，格式为 "%d-%s\r\n"
    for (i = 0; messages[i] != NULL && messages[i + 1] != NULL; i++)
    {
        snprintf(response, sizeof(response), "%d-%s\r\n", code, messages[i]);
        if (send(client_socket, response, strlen(response), 0) == -1)
        {
            perror("send failed");
            return;
        }
    }
    // 发送最后一行
    if (messages[i] != NULL)
    {
        snprintf(response, sizeof(response), "%d %s\r\n", code, messages[i]);
        if (send(client_socket, response, strlen(response), 0) == -1)
        {
            perror("send failed");
            return;
        }
    }
}

/**
 * 从客户端套接字读取一行数据，并存储到缓冲区
 * @param client_socket 客户端套接字
 * @param buffer 存储读取数据的缓冲区
 * @param max_len 缓冲区的最大长度
 * @return 读取的字节数，失败时返回-1
 */
int read_line(int client_socket, char *buffer, size_t max_len)
{
    size_t total_read = 0;
    while (total_read < max_len - 1)
    {
        char ch;
        ssize_t bytes_read = recv(client_socket, &ch, 1, 0);
        if (bytes_read <= 0)
        {
            perror("recv failed");
            return -1;
        }
        if (ch == '\n')
        {
            break; // 行结束
        }
        if (ch != '\r') // 忽略回车符
        {
            buffer[total_read++] = ch;
        }
    }
    buffer[total_read] = '\0'; // 添加字符串结束符
    return total_read;
}

/**
 * 解析命令行，提取命令和参数。暂时只支持单个参数的情况。
 * @param line 输入的命令行
 * @param cmd 输出的命令
 * @param arg 输出的参数，可为空字符串
 */
void parse_cmd_param(const char *line, char *cmd, char *arg)
{
    while (*line && isspace((unsigned char)*line))
        line++; // 跳过前导空白字符

    // 提取命令
    const char *cmd_start = line;
    while (*line && !isspace((unsigned char)*line))
        line++;
    size_t cmd_len = line - cmd_start;
    strncpy(cmd, cmd_start, cmd_len);
    cmd[cmd_len] = '\0';

    // 提取参数
    while (*line && isspace((unsigned char)*line))
        line++; // 跳过空白字符
    strcpy(arg, line);
}