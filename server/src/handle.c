// #include "main.h"
#include "utils.h"
#include "connect.h"
#include "file.h"
#include <regex.h>

// 处理每一个来自客户端的连接
void handle_connection(int client_socket)
{
    int logged_in = 0;                                      // 登录状态标志
    int awaiting_password = 0;                              // 等待密码标志
    char line[LINE_MAX_SIZE], cmd[128], arg[LINE_MAX_SIZE]; // 命令和参数缓冲区

    // 初始化一个会话状态结构体
    connection session;
    memset(&session, 0, sizeof(session));
    session.client_data_socket = -1;
    session.mode = DATA_CONN_MODE_NONE; // 初始无数据连接模式

    // 发送欢迎消息
    send_response(client_socket, 220, "Anonymous FTP server ready.");

    // 主循环，处理客户端命令
    while (1)
    {
        int bytes_read = read_line(client_socket, line, sizeof(line));
        if (bytes_read <= 0)
            break; // 读取失败或连接关闭，退出循环

        // 解析命令和参数
        parse_cmd_param(line, cmd, arg);

        // 处理命令

        // 3.1 登录
        if (logged_in == 0) // 尚未登录，其他命令均不合法
        {
            if (strcmp(cmd, "USER") == 0)
            {
                if (awaiting_password == 0)
                {
                    if (strcmp(arg, "anonymous") == 0)
                    {
                        send_response(client_socket, 331, "Anonymous login ok, send your complete email as password.");
                        awaiting_password = 1; // 等待密码输入
                    }
                    else
                    {
                        send_response(client_socket, 530, "Only anonymous login is allowed.");
                    }
                }
                else
                {
                    send_response(client_socket, 430, "Already sent USER command, please provide password with PASS command.");
                }
            }
            else if (strcmp(cmd, "PASS") == 0)
            {
                if (awaiting_password == 1)
                {
                    // 接受一个简单的邮箱格式作为密码
                    regex_t regex;
                    regcomp(&regex, "^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$", REG_EXTENDED);
                    if (regexec(&regex, arg, 0, NULL, 0) == 0)
                    {

                        const char *lines[] = {
                            "Login successful.",
                            "Welcome to the FTP server! You are logged in as anonymous.",
                            NULL};
                        // int num_lines = sizeof(lines) / sizeof(lines[0]);
                        send_multiline_response(client_socket, 230, lines);
                        logged_in = 1;         // 设置为已登录状态
                        awaiting_password = 0; // 登录完成
                    }
                    else
                    {
                        send_response(client_socket, 530, "Invalid email format for password.");
                    }
                    regfree(&regex);
                }
                else
                {
                    send_response(client_socket, 430, "Please provide USER command before PASS.");
                }
            }
            else
            {
                send_response(client_socket, 530, "Please login with USER and PASS.");
            }
        }
        else // 已登录状态下的其他命令处理
        {
            // 3.2 连接模式
            if (strcmp(cmd, "PORT") == 0) // 处理PORT命令
            {
                if (handle_port_command(client_socket, arg, &session) == 0)
                    send_response(client_socket, 200, "PORT command successful.");
                else
                    send_response(client_socket, 501, "Syntax error in parameters or arguments.");
            }
            else if (strcmp(cmd, "PASV") == 0) // 处理PASV命令
            {
                switch (handle_pasv_command(client_socket, &session))
                {
                case -1:
                    send_response(client_socket, 425, "Can't create socket for PASV.");
                    break;
                case -2:
                    send_response(client_socket, 425, "Can't bind PASV socket.");
                    break;
                case -3:
                    send_response(client_socket, 425, "Can't get PASV port.");
                    break;
                case -4:
                    send_response(client_socket, 425, "Can't listen on PASV socket.");
                    break;
                case -5:
                    send_response(client_socket, 425, "Can't get server address.");
                default: // 成功连接
                    break;
                }
            }

            // 3.3 文件传输命令处理
            else if (strcmp(cmd, "RETR") == 0)
            {
                handle_retr_command(client_socket, &session, arg);
            }
            else if (strcmp(cmd, "STOR") == 0)
            {
                handle_stor_command(client_socket, &session, arg);
            }

            // 3.5 其他系统命令处理
            else if (strcmp(cmd, "SYST") == 0)
            {
                send_response(client_socket, 215, "UNIX Type: L8");
            }
            else if (strcmp(cmd, "TYPE") == 0)
            {
                if (strcmp(arg, "I") == 0)
                {
                    send_response(client_socket, 200, "Type set to I.");
                }
                else
                {
                    // 对于 "TYPE A" 等其他类型，返回错误
                    send_response(client_socket, 504, "Command not implemented for that parameter.");
                }
            }
            else if (strcmp(cmd, "QUIT") == 0)
            {
                send_response(client_socket, 221, "Goodbye.");
                break; // 退出循环，这将导致子进程结束，从而关闭连接
                // TODO: 可以统计一下其它的数据在这里一并返回
            }
            else
            {
                // 对于其他未实现的命令
                send_response(client_socket, 500, "Command not implemented.");
            }
            // TODO: 处理其他FTP命令，如 LIST, RETR, STOR 等
        }
    }
}
