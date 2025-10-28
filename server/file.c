#include "file.h"
#include <regex.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>

/**
 * 检查给定的相对路径相对于给定的根目录是否安全，防止目录遍历攻击
 *  - 对这组代码，root路径是在程序启动时确定的FTP根目录，一般为绝对路径
 *  - 在运行过程中，因为CWD、PWD等的命令，实际工作目录不一定等于root
 *  - path参数是绝对路径，表示待检查的文件路径
 * @param root FTP服务器根目录（绝对路径）
 * @param path 要检查的路径（绝对路径）
 * @return 如果路径安全则返回1，否则返回0
 */
int is_path_safe(const char *root, const char *path)
{
    // strtok会修改字符串，所以我们必须在副本上操作
    char *path_copy = strdup(path);
    if (path_copy == NULL)
    {
        // 内存分配失败，出于安全考虑，返回不安全
        return 0;
    }

    char *root_copy = strdup(root);
    if (root_copy == NULL)
    {
        free(path_copy);
        return 0;
    }

    // 规范化路径：移除末尾的斜杠
    size_t root_len = strlen(root_copy);
    if (root_len > 1 && (root_copy[root_len - 1] == '/' || root_copy[root_len - 1] == '\\'))
    {
        root_copy[root_len - 1] = '\0';
        root_len--;
    }

    // 将路径分解为组件，处理 "." 和 ".."
    char normalized[1024] = {0};
    char *components[256];
    int count = 0;

    // 分解路径
    char *token = strtok(path_copy, "/\\");
    while (token != NULL && count < 256)
    {
        if (strcmp(token, ".") == 0)
        {
            // 跳过当前目录
        }
        else if (strcmp(token, "..") == 0)
        {
            // 返回上一级目录
            if (count > 0)
            {
                count--;
            }
        }
        else
        {
            // 正常目录名
            components[count++] = token;
        }
        token = strtok(NULL, "/\\");
    }

    // 重建规范化路径
    normalized[0] = '\0';
    for (int i = 0; i < count; i++)
    {
        strcat(normalized, "/");
        strcat(normalized, components[i]);
    }

    // 如果路径为空，设置为根目录
    if (normalized[0] == '\0')
    {
        strcpy(normalized, "/");
    }

    // 检查规范化后的路径是否以 root 开头
    int is_safe = (strncmp(normalized, root_copy, root_len) == 0) &&
                  (normalized[root_len] == '\0' ||
                   normalized[root_len] == '/' ||
                   normalized[root_len] == '\\');

    free(path_copy);
    free(root_copy);

    return is_safe;
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
    char *current_dir = getcwd(NULL, 0);
    int len = snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, filename);
    if (len < 0 || len >= (int)sizeof(full_path))
    {
        send_response(client_socket, 550, "Filename is too long.");
        return -1;
    }
    if (!is_path_safe(session->root_dir, full_path))
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
    // 拼接文件的完整路径（绝对路径）
    char *current_dir = getcwd(NULL, 0);
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, filename);
    // 1. 安全检查：路径是否合法
    if (!is_path_safe(session->root_dir, full_path))
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

/**
 * 处理 CWD (Change Working Directory) 命令
 * @param client_socket 客户端控制连接
 * @param path 客户端请求切换到的目录路径，可以是相对路径或绝对路径
 * @return 0 表示成功处理, -1 表示处理失败
 */
int handle_cwd_command(int client_socket, connection *session, const char *path)
{
    char new_dir[PATH_MAX];
    // 分别处理绝对路径和相对路径
    if (path[0] == '/')
    {
        // 绝对路径
        if (is_path_safe(session->root_dir, path))
        {
            chdir(path);
            send_response(client_socket, 250, "Directory successfully changed.");
            return 0;
        }
        else
        {
            send_response(client_socket, 550, "Permission denied or invalid path.");
            return -1;
        }
    }
    else
    {
        // 相对路径
        char *current_dir = getcwd(NULL, 0);
        snprintf(new_dir, sizeof(new_dir), "%s/%s", current_dir, path);
        free(current_dir);

        if (is_path_safe(session->root_dir, new_dir))
        {
            chdir(new_dir);
            send_response(client_socket, 250, "Directory successfully changed.");
            return 0;
        }
        else
        {
            send_response(client_socket, 550, "Permission denied or invalid path.");
            return -1;
        }
    }
}

/**
 * 处理 PWD (Print Working Directory) 命令
 * @param client_socket 客户端控制连接
 * @param session 会话状态
 * @return 0 表示成功处理, -1 表示处理失败
 */
int handle_pwd_command(int client_socket, connection *session)
{
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        char *return_cwd = (char *)malloc(strlen(cwd) + 3);
        sprintf(return_cwd, "\"%s\"", cwd);
        send_response(client_socket, 257, return_cwd);
        free(return_cwd);
        return 0;
    }
    else
    {
        send_response(client_socket, 550, "Failed to get current directory.");
        return -1;
    }
}

/**
 * 处理 MKD (Make Directory) 命令
 * @param client_socket 客户端控制连接
 * @param session 会话状态
 * @param dirname 客户端请求创建的目录名
 * @return 0 表示成功处理, -1 表示处理失败
 */
int handle_mkd_command(int client_socket, connection *session, const char *dirname)
{
    char full_path[PATH_MAX];
    char *current_dir = getcwd(NULL, 0);
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, dirname);
    if (!is_path_safe(session->root_dir, full_path))
    {
        send_response(client_socket, 550, "Permission denied or invalid path.");
        return -1;
    }
    if (mkdir(full_path, 0755) == 0)
    {
        char *return_path = (char *)malloc(strlen(full_path) + 3);
        sprintf(return_path, "\"%s\"", full_path);
        send_response(client_socket, 257, return_path);
        free(return_path);
        return 0;
    }
    else
    {
        send_response(client_socket, 550, "Failed to create directory.");
        return -1;
    }
}

/**
 * 处理 RMD (Remove Directory) 命令
 * @param client_socket 客户端控制连接
 * @param session 会话状态
 * @param dirname 客户端请求删除的目录名
 * @return 0 表示成功处理, -1 表示处理失败
 */
int handle_rmd_command(int client_socket, connection *session, const char *dirname)
{
    char full_path[PATH_MAX];
    char *current_dir = getcwd(NULL, 0);
    snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, dirname);
    if (!is_path_safe(session->root_dir, full_path))
    {
        send_response(client_socket, 550, "Permission denied or invalid path.");
        return -1;
    }
    if (rmdir(full_path) == 0)
    {
        send_response(client_socket, 250, "Directory removed successfully.");
        return 0;
    }
    else
    {
        send_response(client_socket, 550, "Failed to remove directory.");
        return -1;
    }
}

/**
 * 处理 LIST 命令，将目录内容以 ls -l 格式发送给客户端
 * @param client_socket 客户端控制连接
 * @param session 会话状态
 * @param arg 客户端请求列出的目录路径 (可选)
 * @return 0 表示成功处理, -1 表示处理失败
 */
int handle_list_command(int client_socket, connection *session, const char *arg)
{
    char command[PATH_MAX + 16]; // "ls -l " + path + null terminator
    char target_path[PATH_MAX];

    // 1. 确定要列出的目标路径
    // 如果客户端没有提供路径参数，则列出当前工作目录
    if (arg == NULL || strlen(arg) == 0)
    {
        // getcwd 已经返回了绝对路径
        if (getcwd(target_path, sizeof(target_path)) == NULL)
        {
            send_response(client_socket, 550, "Failed to get current directory.");
            return -1;
        }
    }
    else
    {
        // 如果客户端提供了路径，需要解析
        if (arg[0] == '/')
        {
            // 绝对路径
            strncpy(target_path, arg, sizeof(target_path) - 1);
        }
        else
        {
            // 相对路径，需要拼接
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL)
            {
                send_response(client_socket, 550, "Failed to get current directory.");
                return -1;
            }

            // --- 核心修复：检查 snprintf 的返回值 ---
            int required_len = snprintf(target_path, sizeof(target_path), "%s/%s", cwd, arg);
            if (required_len < 0 || required_len >= (int)sizeof(target_path))
            {
                // 如果 snprintf 发生错误或输出被截断，则路径太长
                send_response(client_socket, 550, "Resulting path is too long.");
                return -1;
            }
            // --- 修复结束 ---
        }
    }
    target_path[PATH_MAX - 1] = '\0'; // 确保字符串以 null 结尾

    // 2. 安全检查
    if (!is_path_safe(session->root_dir, target_path))
    {
        send_response(client_socket, 550, "Permission denied or invalid path.");
        return -1;
    }

    // 3. 发送初始响应
    send_response(client_socket, 150, "Here comes the directory listing.");

    // 4. 建立数据连接
    int data_socket = establish_data_connection(session);
    if (data_socket < 0)
    {
        send_response(client_socket, 425, "Failed to establish data connection.");
        return -1;
    }

    // 5. 构造并执行 ls -l 命令
    // 使用 -a 显示隐藏文件，-l 显示详细信息
    snprintf(command, sizeof(command), "ls -al \"%s\"", target_path);
    FILE *pipe = popen(command, "r");
    if (!pipe)
    {
        close(data_socket);
        send_response(client_socket, 550, "Failed to execute list command.");
        return -1;
    }

    // 6. 将命令输出通过数据连接发送
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), pipe)) > 0)
    {
        if (send(data_socket, buffer, bytes_read, 0) < 0)
        {
            // 发送失败，跳出循环
            break;
        }
    }

    // 7. 清理和收尾
    pclose(pipe);
    close(data_socket);
    session->mode = DATA_CONN_MODE_NONE; // 重置数据连接模式

    // 8. 发送最终响应
    send_response(client_socket, 226, "Directory send OK.");
    return 0;
}
