#include "main.h"
#include "utils.h"
#include "file.h"
#include <signal.h>
#include <unistd.h>
#include <limits.h>

int main(int argc, char **argv)
{
    int listen_socket;              // 监听socket
    int connected_socket;           // 连接socket
    struct sockaddr_in server_addr; // 服务器地址结构体

    // 解析命令行参数
    int port = CONTROL_PORT; // 默认控制端口21
    char root_dir[PATH_MAX]; // 服务器根目录，也即FTP应当工作的根目录
    // 默认根目录与测试一致：/tmp（首轮不传 -root）
    strncpy(root_dir, "/tmp", sizeof(root_dir) - 1);
    root_dir[sizeof(root_dir) - 1] = '\0';

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-port") == 0 && i + 1 < argc)
        {
            port = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-root") == 0 && i + 1 < argc)
        {
            strncpy(root_dir, argv[++i], sizeof(root_dir) - 1);
            root_dir[sizeof(root_dir) - 1] = '\0';
        }
    }
    if (chdir(root_dir) != 0)
    {
        perror("chdir to root directory failed!");
        exit(EXIT_FAILURE);
    }

    // 获取绝对路径
    char abs_root[PATH_MAX];
    if (getcwd(abs_root, sizeof(abs_root)) == NULL)
    {
        perror("getcwd failed!");
        exit(EXIT_FAILURE);
    }

    // 创建监听socket
    if ((listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("listen socket creation failed!");
        exit(EXIT_FAILURE);
    }

    // 允许端口复用，便于快速重启
    int opt = 1;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt SO_REUSEADDR failed");
    }

    // 配置服务器地址结构体
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

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

    // 避免子进程成为僵尸
    signal(SIGCHLD, SIG_IGN);

    // 监听主循环
    while (1)
    {
        if ((connected_socket = accept(listen_socket, NULL, NULL)) == -1)
        {
            perror("accept failed!");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0)
        {
            // 子进程：处理一个连接
            close(listen_socket);
            handle_connection(connected_socket, abs_root);
            close(connected_socket);
            _exit(0);
        }
        else if (pid > 0)
        {
            // 父进程：继续 accept
            close(connected_socket);
        }
        else
        {
            perror("fork failed!");
            close(connected_socket);
        }
    }

    close(listen_socket);
    return 0;
}