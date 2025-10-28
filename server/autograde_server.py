#! /usr/bin/python3
import subprocess
import random
import time
import filecmp
import struct
import os
import shutil
import string
from ftplib import FTP, error_perm
import threading
import traceback
from datetime import datetime
import re

def _now():
    return datetime.now().strftime('%H:%M:%S')

def _drain_pipe(pipe, prefix='server'):
    try:
        for line in iter(pipe.readline, b''):
            if not line:
                break
            try:
                print(f'[{_now()}] [{prefix}] {line.decode(errors="replace").rstrip()}')
            except Exception:
                pass
    except Exception:
        pass

class TestServer:
    def __init__(self) -> None:
        self.credit = 0
        self.minor  = 2
        self.major  = 8
        # 目录操作单独计分（不影响 self.credit）
        self.dir_credit = 0
        self.dir_minor = 2
    
    def build(self):
        print(f'[{_now()}] [build] 开始编译...')
        server_dir = os.path.dirname(os.path.abspath(__file__))
        proc = subprocess.Popen('make', cwd=server_dir,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        have_error = False
        stdout, stderr = proc.communicate()
        if b'-Wall' not in stdout and b'-Wall' not in stderr:
            print(f'[{_now()}] [build] 未检测到 -Wall（脚本原逻辑会直接判 0 分并退出，但这里不改变原行为）')
        if stderr:
            print(f'[{_now()}] [build] 编译存在警告/错误，可能扣 {self.major} 分')
            have_error = True
        else:
            print(f'[{_now()}] [build] 编译无警告（不加分，只在有警告才扣分）')
        if have_error:
            self.credit -= self.major
            print(f'[{_now()}] [build] 当前得分 {self.credit}')

    def create_test_file(self, filename, filesize = 10000):
        f = open(filename, 'wb')
        for i in range(filesize):
            data = struct.pack('d', random.random())
            f.write(data)
        f.close()

    def _dir_bump(self, passed: bool, label: str):
        if passed:
            self.dir_credit += self.dir_minor
            print(f'[{_now()}] [dir-score] {label} +{self.dir_minor}, 当前 {self.dir_credit}')
        else:
            print(f'[{_now()}] [dir-score] {label} 未加分')

    def _start_server(self, port: int, root: str):
        server_dir = os.path.dirname(os.path.abspath(__file__))
        abs_root = os.path.abspath(root)
        args = ['./ftpserver', '-port', str(port), '-root', abs_root]
        print(f'[{_now()}] [test] 启动服务器: port={port}, root={abs_root}')
        return subprocess.Popen(['sudo'] + args, cwd=server_dir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    def _pwd_last_component_is(self, resp: str, dirname: str) -> bool:
        # 允许 257 "<任何路径>"，只要最后一个组件等于 dirname 即通过
        # 例如 257 "/tmp/dir389" 或 257 "/dir389" 或 257 "dir389"
        m = re.search(r'^257\s+"([^"]+)"', resp)
        if not m:
            return False
        path = m.group(1)
        last = path.rstrip('/').split('/')[-1] if path else ''
        return last == dirname

    def test_public(self, port: int, directory: str):
        server = self._start_server(port, directory)
        # 实时读取服务器 stdout
        t = threading.Thread(target=_drain_pipe, args=(server.stdout,'server'), daemon=True)
        t.start()

        start_credit = self.credit
        start_dir_credit = self.dir_credit
        time.sleep(0.2)
        try:
            ftp = FTP()
            # connect
            try:
                resp = ftp.connect('127.0.0.1', port)
                print(f'[{_now()}] [step] CONNECT -> {resp!r}')
                if not resp.startswith('220'):
                    print(f'[{_now()}] [score] CONNECT 未加分（缺少 220）')
                else:
                    self.credit += self.minor
                    print(f'[{_now()}] [score] CONNECT +{self.minor}, 当前 {self.credit}')
            except Exception as e:
                print(f'[{_now()}] [error] CONNECT 异常: {e}')

            # login
            try:
                resp = ftp.login()
                print(f'[{_now()}] [step] LOGIN -> {resp!r}')
                if not resp.startswith('230'):
                    print(f'[{_now()}] [score] LOGIN 未加分（缺少 230）')
                else:
                    self.credit += self.minor
                    print(f'[{_now()}] [score] LOGIN +{self.minor}, 当前 {self.credit}')
            except Exception as e:
                print(f'[{_now()}] [error] LOGIN 异常: {e}')

            # SYST
            try:
                resp = ftp.sendcmd('SYST')
                print(f'[{_now()}] [step] SYST -> {resp!r}')
                if resp != '215 UNIX Type: L8':
                    print(f'[{_now()}] [score] SYST 未加分（需完全匹配 215 UNIX Type: L8）')
                else:
                    self.credit += self.minor
                    print(f'[{_now()}] [score] SYST +{self.minor}, 当前 {self.credit}')
            except Exception as e:
                print(f'[{_now()}] [error] SYST 异常: {e}')

            # ===== 目录操作测试开始（不计入原有 credit，总分单独统计） =====
            try:
                abs_root = os.path.abspath(directory)

                # PWD 原地：应为 257 "<pathname>"
                resp = ftp.sendcmd('PWD')
                print(f'[{_now()}] [dir-step] PWD -> {resp!r}')
                ok = resp.startswith('257 ') and ('"' in resp)
                self._dir_bump(ok, 'PWD 257 "<path>"')

                # MKD：257 "<pathname>"
                dirname = 'dir%d' % random.randint(300, 400)
                resp = ftp.sendcmd(f'MKD {dirname}')
                print(f'[{_now()}] [dir-step] MKD {dirname} -> {resp!r}')
                ok = resp.startswith('257') and ('"' in resp)
                self._dir_bump(ok, f'MKD {dirname}')

                # CWD：250
                resp = ftp.cwd(dirname)
                print(f'[{_now()}] [dir-step] CWD {dirname} -> {resp!r}')
                self._dir_bump(resp.startswith('250'), f'CWD {dirname}')

                # PWD：最后路径组件应为该目录名（容忍绝对物理路径或虚拟路径）
                resp = ftp.sendcmd('PWD')
                print(f'[{_now()}] [dir-step] PWD(after CWD) -> {resp!r}')
                ok = self._pwd_last_component_is(resp, dirname)
                self._dir_bump(ok, 'PWD 包含子目录名')

                # 在该子目录内创建两个文件（用于 LIST 校验）
                subdir_fs = os.path.join(abs_root, dirname)
                try:
                    os.makedirs(subdir_fs, exist_ok=True)
                    with open(os.path.join(subdir_fs, 'a.txt'), 'w') as f:
                        f.write('hello\n')
                    with open(os.path.join(subdir_fs, 'b.txt'), 'w') as f:
                        f.write('world\n')
                except Exception as e:
                    print(f'[{_now()}] [dir-diag] 无法在 {subdir_fs} 创建文件: {e}')

                # LIST（客户端ONLY）：不强制 TYPE A；应通过数据连接返回并 226 结束，且包含 a.txt/b.txt
                try:
                    lines = []
                    resp = ftp.retrlines('LIST', lines.append)
                    print(f'[{_now()}] [dir-step] LIST -> {resp!r}')
                    listing = '\n'.join(lines)
                    ok = resp.startswith('226') and ('a.txt' in listing) and ('b.txt' in listing)
                    if not ok:
                        print(f'[{_now()}] [dir-diag] LIST 内容:\n{listing}')
                    self._dir_bump(ok, 'LIST 226 且包含文件名')
                except Exception as e:
                    print(f'[{_now()}] [dir-error] LIST 异常: {e}')
                    self._dir_bump(False, 'LIST 226 且包含文件名')

                # 返回父目录
                try:
                    resp = ftp.cwd('..')
                    print(f'[{_now()}] [dir-step] CWD .. -> {resp!r}')
                except Exception as e:
                    print(f'[{_now()}] [dir-error] CWD .. 异常: {e}')

                # 越权 CWD（应拒绝）
                try:
                    ftp.cwd('../..')
                    print(f'[{_now()}] [dir-step] CWD ../.. 成功（不应允许）')
                    self._dir_bump(False, '阻止越出 root 的 CWD')
                except error_perm:
                    print(f'[{_now()}] [dir-step] CWD ../.. 被拒绝（预期）')
                    self._dir_bump(True, '阻止越出 root 的 CWD')
                except Exception as e:
                    print(f'[{_now()}] [dir-error] CWD ../.. 异常: {e}')
                    self._dir_bump(False, '阻止越出 root 的 CWD')

                # 清理文件，RMD 子目录：250
                try:
                    for fn in ('a.txt', 'b.txt'):
                        p = os.path.join(subdir_fs, fn)
                        if os.path.exists(p):
                            os.remove(p)
                except Exception as e:
                    print(f'[{_now()}] [dir-diag] 清理子目录文件失败: {e}')

                try:
                    resp = ftp.sendcmd(f'RMD {dirname}')
                    print(f'[{_now()}] [dir-step] RMD {dirname} -> {resp!r}')
                    self._dir_bump(resp.startswith('250'), f'RMD {dirname}')
                except Exception as e:
                    print(f'[{_now()}] [dir-error] RMD 异常: {e}')
                    self._dir_bump(False, f'RMD {dirname}')

                dir_subtotal = self.dir_credit - start_dir_credit
                print(f'[{_now()}] [test] 目录操作子总分 +{dir_subtotal}（本轮最高 14）')
            except Exception as e:
                print(f'[{_now()}] [dir-fatal] 目录操作测试异常: {e}')
            # ===== 目录操作测试结束 =====

            # TYPE I
            try:
                resp = ftp.sendcmd('TYPE I')
                print(f'[{_now()}] [step] TYPE I -> {resp!r}')
                if resp != '200 Type set to I.':
                    print(f'[{_now()}] [score] TYPE I 未加分（需完全匹配 200 Type set to I.）')
                else:
                    self.credit += self.minor
                    print(f'[{_now()}] [score] TYPE I +{self.minor}, 当前 {self.credit}')
            except Exception as e:
                print(f'[{_now()}] [error] TYPE I 异常: {e}')

            # PORT download
            try:
                filename = 'test%d.data' % random.randint(100, 200)
                abs_root = os.path.abspath(directory)
                self.create_test_file(os.path.join(abs_root, filename))
                ftp.set_pasv(False)
                print(f'[{_now()}] [step] RETR 文件: {filename} (主动模式)')
                localf = open(filename, 'wb')
                try:
                    resp = ftp.retrbinary('RETR %s' % filename, localf.write)
                finally:
                    localf.close()
                print(f'[{_now()}] [step] RETR 响应 -> {resp!r}')
                ok = True
                if not resp.startswith('226'):
                    print(f'[{_now()}] [score] RETR 未加分（缺少 226）')
                    ok = False
                if not filecmp.cmp(filename, os.path.join(abs_root, filename)):
                    print(f'[{_now()}] [diag] RETR 文件比对失败: {filename} != {abs_root}/{filename}')
                    ok = False
                if ok:
                    self.credit += self.minor
                    print(f'[{_now()}] [score] RETR +{self.minor}, 当前 {self.credit}')
            except Exception as e:
                print(f'[{_now()}] [error] RETR 异常: {e}')
            finally:
                try:
                    os.remove(os.path.join(abs_root, filename))
                except Exception:
                    pass
                try:
                    os.remove(filename)
                except Exception:
                    pass

            # PASV upload
            try:
                ftp2 = FTP()
                resp = ftp2.connect('127.0.0.1', port)
                print(f'[{_now()}] [step] CONNECT2 -> {resp!r}')
                resp = ftp2.login()
                print(f'[{_now()}] [step] LOGIN2 -> {resp!r}')
                filename = 'test%d.data' % random.randint(100, 200)
                self.create_test_file(filename)
                print(f'[{_now()}] [step] STOR 文件: {filename} (被动模式)')
                with open(filename, 'rb') as f:
                    resp = ftp2.storbinary('STOR %s' % filename, f)
                print(f'[{_now()}] [step] STOR 响应 -> {resp!r}')
                abs_root = os.path.abspath(directory)
                ok = True
                if not resp.startswith('226'):
                    print(f'[{_now()}] [score] STOR 未加分（缺少 226）')
                    ok = False
                if not filecmp.cmp(filename, os.path.join(abs_root, filename)):
                    print(f'[{_now()}] [diag] STOR 文件比对失败: {filename} != {abs_root}/{filename}')
                    ok = False
                if ok:
                    self.credit += self.minor
                    print(f'[{_now()}] [score] STOR +{self.minor}, 当前 {self.credit}')
            except Exception as e:
                print(f'[{_now()}] [error] STOR 异常: {e}')
            finally:
                try:
                    os.remove(os.path.join(abs_root, filename))
                except Exception:
                    pass
                try:
                    os.remove(filename)
                except Exception:
                    pass
                try:
                    ftp2.quit()
                except Exception:
                    pass

            # QUIT
            try:
                resp = ftp.quit()
                print(f'[{_now()}] [step] QUIT -> {resp!r}')
                if not resp.startswith('221'):
                    print(f'[{_now()}] [score] QUIT 未加分（缺少 221）')
                else:
                    self.credit += self.minor
                    print(f'[{_now()}] [score] QUIT +{self.minor}, 当前 {self.credit}')
            except Exception as e:
                print(f'[{_now()}] [error] QUIT 异常: {e}')

            subtotal = self.credit - start_credit
            print(f'[{_now()}] [test] 子总分 +{subtotal}（本轮最高 14）')

        except Exception as e:
            print(f'[{_now()}] [fatal] 测试异常: {e}')
            traceback.print_exc()
        finally:
            try:
                server.kill()
            except Exception:
                pass

if __name__ == "__main__":
    test = TestServer()
    print(f'[{_now()}] [info] 本脚本单轮最高 14 分，两轮合计最高 28 分；build 阶段只扣分不加分。')
    test.build()
    # Test 1：固定根 /tmp + 随机高端口
    port1 = random.randint(20000, 40000)
    test.test_public(port1, "/tmp")
    # Test 2：随机新建“绝对路径”根目录 + 随机高端口
    server_dir = os.path.dirname(os.path.abspath(__file__))
    rand_dir = ''.join(random.choice(string.ascii_letters) for _ in range(10))
    abs_dir = os.path.abspath(os.path.join(server_dir, rand_dir))
    if os.path.isdir(abs_dir):
        shutil.rmtree(abs_dir)
    os.mkdir(abs_dir)
    try:
        port2 = random.randint(20000, 40000)
        test.test_public(port2, abs_dir)
    finally:
        shutil.rmtree(abs_dir)
    # Clean
    subprocess.run(['make', 'clean'], cwd=server_dir, stdout=subprocess.PIPE)
    # Result
    if test.credit < 0: test.credit = 0
    print(f'[{_now()}] [result] Your credit is {test.credit} / 28')
    print(f'[{_now()}] [dir-result] Directory ops credit is {test.dir_credit} / 28')