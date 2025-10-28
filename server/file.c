#include "file.h"
#include <regex.h>
#include <fcntl.h>

/**
 * 确保会话的当前工作目录已初始化
 * 如果未初始化，则设置为根目录 "/"
 * @param session 会话指针
 */
// static void ensure_session_cwd(connection *session)
// {
//     if (session == NULL)
//         return;
//     if (session->cwd[0] == '\0')
//     {
//         session->cwd[0] = '/';
//         session->cwd[1] = '\0';
//     }
// }

// /**
//  * 规范化虚拟路径，处理 . 和 .. 组件
//  * @param base 当前工作目录
//  * @param input 输入路径（可能是相对或绝对路径）
//  * @param out 输出缓冲区，用于存储规范化后的路径
//  * @param outsz 输出缓冲区大小
//  */
// static void normalize_virtual_path(const char *base, const char *input, char *out, size_t outsz)
// {
//     // 选择起点：绝对路径用根 /，相对路径用 base
//     const char *use_base = (input && input[0] == '/') ? "/" : (base && base[0] ? base : "/");

//     // 合并 base 与 input
//     char joined[PATH_MAX * 2];
//     if (input && input[0] == '/')
//     {
//         snprintf(joined, sizeof(joined), "%s", input);
//     }
//     else if (input && input[0] != '\0')
//     {
//         if (strcmp(use_base, "/") == 0)
//             snprintf(joined, sizeof(joined), "/%s", input);
//         else
//             snprintf(joined, sizeof(joined), "%s/%s", use_base, input);
//     }
//     else
//     {
//         snprintf(joined, sizeof(joined), "%s", use_base);
//     }

//     // 分词并用栈处理 . 和 ..
//     char buf[PATH_MAX * 2];
//     snprintf(buf, sizeof(buf), "%s", joined);

//     char *stack[PATH_MAX / 2];
//     int top = 0;

//     for (char *tok = strtok(buf, "/"); tok; tok = strtok(NULL, "/"))
//     {
//         if (strcmp(tok, ".") == 0 || tok[0] == '\0')
//         {
//             continue;
//         }
//         else if (strcmp(tok, "..") == 0)
//         {
//             if (top > 0)
//                 top--; // 不允许超出根
//         }
//         else
//         {
//             stack[top++] = tok;
//         }
//     }

//     // 生成标准化虚拟路径
//     if (top == 0)
//     {
//         snprintf(out, outsz, "/");
//         return;
//     }
//     size_t pos = 0;
//     out[0] = '\0';
//     for (int i = 0; i < top; ++i)
//     {
//         int n = snprintf(out + pos, (pos < outsz ? outsz - pos : 0), "%s%s", (i == 0 ? "/" : "/"), stack[i]);
//         if (n < 0)
//             break;
//         pos += (size_t)n;
//         if (pos >= outsz)
//             break;
//     }
//     if (pos >= outsz && outsz > 0)
//         out[outsz - 1] = '\0';
// }

// /**
//  * 将虚拟路径转换为文件系统路径：FTP根目录 + 虚拟路径
//  * @param vpath 虚拟路径（必须以 / 开头），也即规范化后的路径
//  * @param out 输出缓冲区，用于存储文件系统路径
//  * @param outsz 输出缓冲区大小
//  */
// static int vpath_to_fspath(const char *vpath, char *out, size_t outsz)
// {
//     if (!vpath || vpath[0] != '/')
//         return -1;
//     if (strcmp(vpath, "/") == 0)
//     {
//         int n = snprintf(out, outsz, "%s", FTP_ROOT_DIR);
//         return (n < 0 || n >= (int)outsz) ? -1 : 0;
//     }
//     int n = snprintf(out, outsz, "%s%s", FTP_ROOT_DIR, vpath);
//     return (n < 0 || n >= (int)outsz) ? -1 : 0;
// }

// /**
//  * 检查给定的文件系统路径是否在FTP根目录下
//  * @param fspath 文件系统路径
//  * @param real_out 输出缓冲区，用于存储解析后的真实路径
//  * @param outsz 输出缓冲区大小
//  */
// static int safe_realpath_existing(const char *fspath, char *real_out, size_t outsz)
// {
//     char realbuf[PATH_MAX];
//     if (!realpath(fspath, realbuf))
//         return -1;
//     size_t rootlen = strlen(FTP_ROOT_DIR);
//     if (strncmp(realbuf, FTP_ROOT_DIR, rootlen) != 0)
//         return -1;
//     int n = snprintf(real_out, outsz, "%s", realbuf);
//     return (n < 0 || n >= (int)outsz) ? -1 : 0;
// }

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
    int len = snprintf(full_path, sizeof(full_path), "%s/%s", session->cwd, filename);
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
