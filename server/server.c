#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <errno.h>

#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	int listenfd, connfd;	 // 监听socket和连接socket
	struct sockaddr_in addr; // 服务器地址结构体
	char sentence[8192];	 // 缓冲区，长度8192字节
	int p;					 // 读写指针
	int len;				 // 数据长度

	// 创建socket
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// 设置服务器地址ip和port
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = 6789;
	addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有IP

	// 将ip和port绑定到socket
	if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// 启动监听socket
	if (listen(listenfd, 10) == -1)
	{
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	// 服务器主循环
	while (1)
	{
		// 等待client连接 -- 阻塞式
		if ((connfd = accept(listenfd, NULL, NULL)) == -1)
		{
			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
			continue;
		}

		// 读取socket数据
		p = 0;
		while (1)
		{
			int n = read(connfd, sentence + p, 8191 - p);
			if (n < 0)
			{
				printf("Error read(): %s(%d)\n", strerror(errno), errno);
				close(connfd);
				continue;
			}
			else if (n == 0)
			{
				break;
			}
			else
			{
				p += n;
				if (sentence[p - 1] == '\n')
				{
					break;
				}
			}
		}
		// socket收到的数据以'\n'结尾，替换为'\0'
		sentence[p - 1] = '\0';
		len = p - 1;

		// 字符串转大写
		for (p = 0; p < len; p++)
		{
			sentence[p] = toupper(sentence[p]);
		}

		// 写大写字符串到socket
		p = 0;
		while (p < len)
		{
			int n = write(connfd, sentence + p, len + 1 - p);
			if (n < 0)
			{
				printf("Error write(): %s(%d)\n", strerror(errno), errno);
				return 1;
			}
			else
			{
				p += n;
			}
		}

		close(connfd);
	}

	close(listenfd);
}
