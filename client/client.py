import socket
import sys

def main():
    """
    一个简单的TCP客户端，用于手动与FTP服务器交互。
    """
    if len(sys.argv) != 3:
        print("用法: python3 client.py <服务器IP地址> <端口号>")
        print("例如: python3 client.py 127.0.0.1 21021")
        sys.exit(1)

    host = sys.argv[1]
    try:
        port = int(sys.argv[2])
        if not (0 < port < 65536):
            raise ValueError
    except ValueError:
        print("错误: 端口号必须是 1-65535 之间的整数。")
        sys.exit(1)

    # 1. 创建TCP套接字
    try:
        # AF_INET 表示使用 IPv4
        # SOCK_STREAM 表示使用 TCP
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # 2. 连接到服务器
        print(f"正在连接到 {host}:{port} ...")
        client_socket.connect((host, port))
        print("连接成功！")

        # 3. 接收并打印服务器的初始欢迎消息
        initial_response = client_socket.recv(4096).decode('utf-8').strip()
        print(f"服务器: {initial_response}")

        # 4. 进入交互式命令循环
        while True:
            # 提供命令行提示
            command = input("ftp> ")

            if not command:
                continue
            
            # 输入 quit 或 exit 时退出程序
            if command.lower() in ["quit", "exit"]:
                # 最好也向服务器发送QUIT命令
                client_socket.sendall("QUIT\r\n".encode('utf-8'))
                response = client_socket.recv(4096).decode('utf-8').strip()
                print(f"服务器: {response}")
                break

            # 5. 发送用户输入的命令 (FTP协议要求以 \r\n 结尾)
            client_socket.sendall(f"{command}\r\n".encode('utf-8'))

            # 6. 接收并打印服务器的响应
            response = client_socket.recv(4096).decode('utf-8').strip()
            print(f"服务器: {response}")

    except ConnectionRefusedError:
        print(f"连接被拒绝。请确保服务器正在 {host}:{port} 上运行。")
    except socket.gaierror:
        print(f"错误: 无法解析主机名 '{host}'。")
    except Exception as e:
        print(f"发生了一个错误: {e}")
    finally:
        # 7. 关闭套接字
        if 'client_socket' in locals() and client_socket.fileno() != -1:
            client_socket.close()
            print("连接已关闭。")

if __name__ == "__main__":
    main()
