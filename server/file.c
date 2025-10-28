#include "file.h"
#include <regex.h>
#include <fcntl.h>

/**
 * 检查给定路径是否在FTP根目录下，通过模拟路径解析来防止目录遍历。
 * 规则：路径在解析过程中，深度不能为负数。
 *    例如 "dir/.." 是合法的 (深度 1 -> 0), 但 "../dir" 是非法的 (深度 0 -> -1)。
 * @param path 要检查的路径
 * @return 如果路径安全则返回1，否则返回0
 */
int is_path_safe(const char *path)
{
    // strtok会修改字符串，所以我们必须在副本上操作
    char *path_copy = strdup(path);
    if (path_copy == NULL)
    {
        // 内存分配失败，出于安全考虑，返回不安全
        return 0;
    }

    int depth = 0;
    char *token = strtok(path_copy, "/");

    while (token != NULL)
    {
        // 忽略当前目录 "."
        if (strcmp(token, ".") == 0)
        {
            // do nothing
        }
        // 遇到上级目录 ".."
        else if (strcmp(token, "..") == 0)
            depth--;
        // 遇到普通目录
        else
            depth++;

        // 关键检查：在任何时候，深度都不能小于0
        if (depth < 0)
        {
            free(path_copy); // 清理内存
            return 0;        // 不安全
        }

        // 获取下一个token
        token = strtok(NULL, "/");
    }

    // 循环结束，如果深度从未小于0，则路径是安全的
    free(path_copy); // 清理内存
    return 1;
}

/**
 * 处理RETR命令，发送文件给客户端，也即下载
 * @param client_socket 控制连接socket
 * @param session 数据连接会话信息
 * @param filename 要下载的文件名
 * @return 0表示成功，-1表示失败
 */
int handle_retr_command(int client_socket, connection *session, const char *filename)
{
    char full_path[PATH_MAX];
    int len = snprintf(full_path, sizeof(full_path), "%s/%s", FTP_ROOT_DIR, filename);
    if (len < 0 || len >= (int)sizeof(full_path))
    {
        send_response(client_socket, 550, "Filename is too long.");
        return -1;
    }
    if (!is_path_safe(full_path))
    {
        send_response(client_socket, 550, "Permission denied or invalid path.");
        return -1;
    }
    int file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0)
    {
        send_response(client_socket, 550, "Failed to open file.");
        return -1;
    }

    // 发送代码150的初始响应，准备传输
    send_response(client_socket, 150, "Opening data connection for file transfer.");

    // 建立数据连接
    int data_socket = establish_data_connection(session);
    if (data_socket < 0)
    {
        close(file_fd);
        send_response(client_socket, 425, "Data connection failed.");
        return -1;
    }

    // 传输文件内容
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int transfer_ok = 1; // 传输状态标志
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0)
    {
        ssize_t sent_bytes = 0;
        while (sent_bytes < bytes_read)
        {
            ssize_t n = send(data_socket, buffer + sent_bytes, bytes_read - sent_bytes, 0);
            if (n < 0)
            {
                transfer_ok = 0; // 发送失败
                break;
            }
            sent_bytes += n;
        }
        if (!transfer_ok)
            break; // 退出外层循环
    }

    if (bytes_read < 0)
    {
        transfer_ok = 0; // 读取失败
    }

    // 关闭数据连接和文件
    close(data_socket);
    close(file_fd);
    session->mode = DATA_CONN_MODE_NONE; // 重置数据连接模式

    // 最终响应
    if (transfer_ok)
    {
        send_response(client_socket, 226, "Transfer complete.");
        return 0;
    }
    else
    {
        send_response(client_socket, 426, "Connection closed; transfer aborted.");
        return -1;
    }
}

/**
 * 处理 STOR (上传文件) 命令
 * @param client_socket 客户端控制连接
 * @param session 会话状态
 * @param filename 客户端要上传的文件名
 * @return 0 表示成功处理, -1 表示处理失败
 */
int handle_stor_command(int client_socket, connection *session, const char *filename)
{
    // 1. 安全检查：路径是否合法
    if (!is_path_safe(filename))
    {
        send_response(client_socket, 550, "Permission denied or invalid path.");
        return -1;
    }

    // 2. 文件检查：尝试以只写、创建、清空的方式打开文件
    // 0644 是文件权限：所有者可读写，组用户和其他用户只读
    int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0)
    {
        // 无法创建或写入文件
        send_response(client_socket, 550, "Cannot create or write to file.");
        return -1;
    }

    // 3. 发送初始响应 (Mark): 告诉客户端准备就绪
    send_response(client_socket, 150, "Ready to receive data.");

    // 4. 建立数据连接
    int data_socket = establish_data_connection(session);
    if (data_socket < 0)
    {
        // 数据连接建立失败
        send_response(client_socket, 425, "Failed to establish data connection.");
        close(file_fd);
        remove(filename); // 清理掉创建的空文件
        return -1;
    }

    // 5. 接收文件内容
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int transfer_ok = 1; // 传输状态标志

    while ((bytes_read = read(data_socket, buffer, sizeof(buffer))) > 0)
    {
        if (write(file_fd, buffer, bytes_read) != bytes_read)
        {
            // 写入本地文件失败
            transfer_ok = 0;
            break;
        }
    }

    // 检查是否是从数据连接读取时出错
    if (bytes_read < 0)
    {
        transfer_ok = 0;
    }

    // 6. 关闭数据连接和文件
    close(data_socket);
    close(file_fd);
    session->mode = DATA_CONN_MODE_NONE; // 重置数据连接模式

    // 7. 发送最终响应
    if (transfer_ok)
    {
        send_response(client_socket, 226, "Transfer complete.");
    }
    else
    {
        send_response(client_socket, 426, "Connection closed; transfer aborted.");
        remove(filename); // 清理掉传输不完整的文件
    }

    return 0;
}
