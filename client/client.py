#!/usr/bin/env python3
import sys
import socket
import re

ENC = "utf-8"

def send_cmd(sock, line: str):
    # 发送以 CRLF 结尾的命令
    if not line.endswith("\r\n"):
        line = line.rstrip("\r\n") + "\r\n"
    sock.sendall(line.encode(ENC))

def recv_line(sock_file):
    # 从控制连接读取一行并打印，返回去掉换行的字符串
    line = sock_file.readline()
    if not line:
        return None
    line = line.rstrip("\r\n")
    print(line, flush=True)
    return line

def recv_response(sock_file):
    """
    读取并打印一个完整响应（支持多行）。
    规则：若首行形如 ddd-，则直到读到同一 ddd + 空格 的行结束；否则首行即为完整响应。
    返回：本响应的所有行（不含换行符）。
    """
    lines = []
    first = sock_file.readline()
    if not first:
        return lines
    first = first.rstrip("\r\n")
    print(first, flush=True)
    lines.append(first)
    m = re.match(r"^(\d{3})([ -])", first)
    if m and m.group(2) == "-":
        code = m.group(1)
        end_pat = re.compile(rf"^{re.escape(code)}\s")
        while True:
            L = sock_file.readline()
            if not L:
                break
            L = L.rstrip("\r\n")
            print(L, flush=True)
            lines.append(L)
            if end_pat.match(L):
                break
    return lines

def parse_pasv_227(resp_line):
    # 解析: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
    m = re.search(r"\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\)", resp_line)
    if not m:
        return None
    h = ".".join(m.group(i) for i in range(1, 5))
    p = int(m.group(5)) * 256 + int(m.group(6))
    return (h, p)

def main():
    # 解析参数: -ip <ip> -port <port>
    ip, port = "127.0.0.1", 21
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "-ip" and i + 1 < len(args):
            ip = args[i + 1]
            i += 2
        elif args[i] == "-port" and i + 1 < len(args):
            port = int(args[i + 1])
            i += 2
        else:
            i += 1

    # 建立控制连接
    ctrl = socket.create_connection((ip, port))
    ctrl_file = ctrl.makefile("r", encoding=ENC, newline="\n", errors="replace")

    # 打印欢迎语（可能多行，比如 220- ... 220 ...）
    if not recv_response(ctrl_file):
        return

    pasv_target = None          # (host, port) for PASV
    active_listener = None      # listening socket for PORT

    try:
        for raw in sys.stdin:
            cmd = raw.strip()
            if not cmd:
                continue

            upper = cmd.upper()

            if upper.startswith("PASV"):
                # 被动模式：请求并解析 227
                send_cmd(ctrl, cmd)
                lines = recv_response(ctrl_file)
                # 取本次响应中出现的 227 行进行解析
                for line in reversed(lines):
                    if line.startswith("227"):
                        pasv_target = parse_pasv_227(line)
                        break
                # 关闭主动监听（如果此前开启过）
                if active_listener:
                    try:
                        active_listener.close()
                    except Exception:
                        pass
                    active_listener = None

            elif upper.startswith("PORT"):
                # 解析命令中的端口，准备监听
                try:
                    addr = cmd.split()[1]
                    h1, h2, h3, h4, p1, p2 = [int(x) for x in addr.split(",")]
                    data_port = p1 * 256 + p2
                    if active_listener:
                        try:
                            active_listener.close()
                        except Exception:
                            pass
                    ls = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    # 允许快速复用，减少“地址已被占用”
                    ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                    # 绑定回环地址，监听地址和端口由 PORT 指定
                    ls.bind((f"{h1}.{h2}.{h3}.{h4}", data_port))
                    ls.listen(1)
                    active_listener = ls
                    pasv_target = None
                    # 主动模式：透传并准备监听端口
                    send_cmd(ctrl, cmd)
                    _ = recv_response(ctrl_file)  # 打印完整响应（一般 200）
                except Exception:
                    # 解析/监听失败，留给后续 RETR 让服务器报错
                    pass

            elif upper.startswith("RETR "):
                # 文件下载流程
                filename = cmd.split(maxsplit=1)[1]
                send_cmd(ctrl, cmd)
                pre = recv_response(ctrl_file)  # 150/125（可能多行）
                # 建立数据连接
                data_sock = None
                try:
                    if pasv_target:
                        data_sock = socket.create_connection(pasv_target)
                    elif active_listener:
                        data_sock, _peer = active_listener.accept()
                    else:
                        # 未设置数据连接模式，等待服务器错误回应
                        # 读取并打印完整错误响应后继续
                        _ = recv_response(ctrl_file)
                        continue

                    # 接收数据并保存到同名文件
                    with open(filename, "wb") as f:
                        while True:
                            buf = data_sock.recv(8192)
                            if not buf:
                                break
                            f.write(buf)
                finally:
                    if data_sock:
                        try:
                            data_sock.close()
                        except Exception:
                            pass
                    # 主动模式的监听只用于下一次 RETR 前复用，这里保留

                # 打印完成响应（通常 226，可能是多行）
                post = recv_response(ctrl_file)

            elif upper.startswith("LIST"):
                # 目录列表流程
                send_cmd(ctrl, cmd)

                # 对于主动模式，必须先准备好接受连接
                data_sock = None
                if active_listener:
                    data_sock, _peer = active_listener.accept()

                # 读取服务器的初步响应（150）
                pre = recv_response(ctrl_file)
                
                # 检查初步响应是否成功。如果失败（例如服务器直接返回5xx错误），则直接继续
                if not pre or not (pre[0].startswith("150") or pre[0].startswith("125")):
                    if data_sock: data_sock.close()
                    continue

                # 检查客户端自身是否能建立连接
                can_connect = active_listener or pasv_target
                if not can_connect:
                    # 如果客户端没有连接方式，我们知道服务器接下来会发送一个错误码（如425）
                    # 我们必须读取这个错误码来完成这次事务，而不是直接 continue
                    if data_sock: data_sock.close() # 如果 accept 碰巧成功了，关闭它
                    _ = recv_response(ctrl_file) # 读取并打印服务器的最终错误响应
                    continue # 现在可以安全地继续了

                try:
                    # 对于被动模式，在这里建立连接
                    if pasv_target:
                        data_sock = socket.create_connection(pasv_target)

                    # 此时 data_sock 应该已经建立
                    if not data_sock:
                        # 理论上不应该到这里，但作为保险
                        _ = recv_response(ctrl_file)
                        continue

                    # 接收数据并直接打印到屏幕
                    while True:
                        buf = data_sock.recv(8192)
                        if not buf:
                            break
                        print(buf.decode(ENC, errors='ignore'), end='', flush=True)
                finally:
                    if data_sock:
                        data_sock.close()
                    if active_listener:
                        active_listener.close()
                        active_listener = None
                    pasv_target = None

                # 打印传输完成的最终响应（226）
                post = recv_response(ctrl_file)

            else:
                # 其他命令透传并打印完整响应
                send_cmd(ctrl, cmd)
                recv_response(ctrl_file)

    finally:
        try:
            if active_listener:
                active_listener.close()
        except Exception:
            pass
        try:
            ctrl.close()
        except Exception:
            pass

if __name__ == "__main__":
    main()