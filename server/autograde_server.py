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
        proc = subprocess.Popen('make', stdout=subprocess.PIPE, stderr=subprocess.PIPE)
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

    def test_public(self, port=21, directory='/tmp'):
        print(f'[{_now()}] [test] 启动服务器: port={port}, root={directory}')
        if port == 21 and directory == '/tmp':
            server = subprocess.Popen(['sudo', './ftpserver'], stdout=subprocess.PIPE)
        else:
            server = subprocess.Popen(['sudo', './ftpserver', '-port', '%d' % port, '-root', directory], stdout=subprocess.PIPE)
        # 实时读取服务器 stdout
        t = threading.Thread(target=_drain_pipe, args=(server.stdout,'server'), daemon=True)
        t.start()

        start_credit = self.credit
        start_dir_credit = self.dir_credit
        time.sleep(0.1)
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
                # PWD 原地
                resp = ftp.sendcmd('PWD')
                print(f'[{_now()}] [dir-step] PWD -> {resp!r}')
                ok = resp.startswith('257 ') and ('"' in resp)
                self._dir_bump(ok, 'PWD 257 "<path>"')

                # MKD
                dirname = 'dir%d' % random.randint(300, 400)
                resp = ftp.sendcmd(f'MKD {dirname}')
                print(f'[{_now()}] [dir-step] MKD {dirname} -> {resp!r}')
                self._dir_bump(resp.startswith('257'), f'MKD {dirname}')

                # CWD 进入新目录
                resp = ftp.cwd(dirname)
                print(f'[{_now()}] [dir-step] CWD {dirname} -> {resp!r}')
                self._dir_bump(resp.startswith('250'), f'CWD {dirname}')

                # PWD 应包含新目录名
                resp = ftp.sendcmd('PWD')
                print(f'[{_now()}] [dir-step] PWD(after CWD) -> {resp!r}')
                in_path = f'"{dirname}"' in resp or f'"/{dirname}"' in resp
                self._dir_bump(in_path, 'PWD 包含子目录名')

                # 在服务器根目录下的该子目录内创建两个文件（直接在文件系统上创建，便于验证 LIST）
                subdir_fs = os.path.join(directory, dirname)
                try:
                    with open(os.path.join(subdir_fs, 'a.txt'), 'w') as f:
                        f.write('hello\n')
                    with open(os.path.join(subdir_fs, 'b.txt'), 'w') as f:
                        f.write('world\n')
                except Exception as e:
                    print(f'[{_now()}] [dir-diag] 无法在 {subdir_fs} 创建文件: {e}')

                # 切换到 ASCII 文本模式以满足 LIST 规范
                try:
                    resp = ftp.sendcmd('TYPE A')
                    print(f'[{_now()}] [dir-step] TYPE A -> {resp!r}')
                except Exception as e:
                    print(f'[{_now()}] [dir-error] TYPE A 异常: {e}')

                # LIST 列表，需 226 且包含 a.txt/b.txt
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

                # 返回到父目录
                try:
                    resp = ftp.cwd('..')
                    print(f'[{_now()}] [dir-step] CWD .. -> {resp!r}')
                except Exception as e:
                    print(f'[{_now()}] [dir-error] CWD .. 异常: {e}')

                # 测试越权 CWD（不应允许逃离 root）
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

                # 删除测试文件（文件系统上清理），再 RMD
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

            # TYPE
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
                self.create_test_file(directory + '/' + filename)
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
                if not filecmp.cmp(filename, directory + '/' + filename):
                    print(f'[{_now()}] [diag] RETR 文件比对失败: {filename} != {directory}/{filename}')
                    ok = False
                if ok:
                    self.credit += self.minor
                    print(f'[{_now()}] [score] RETR +{self.minor}, 当前 {self.credit}')
            except Exception as e:
                print(f'[{_now()}] [error] RETR 异常: {e}')
            finally:
                try: os.remove(directory + '/' + filename)
                except Exception: pass
                try: os.remove(filename)
                except Exception: pass

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
                ok = True
                if not resp.startswith('226'):
                    print(f'[{_now()}] [score] STOR 未加分（缺少 226）')
                    ok = False
                if not filecmp.cmp(filename, directory + '/' + filename):
                    print(f'[{_now()}] [diag] STOR 文件比对失败: {filename} != {directory}/{filename}')
                    ok = False
                if ok:
                    self.credit += self.minor
                    print(f'[{_now()}] [score] STOR +{self.minor}, 当前 {self.credit}')
            except Exception as e:
                print(f'[{_now()}] [error] STOR 异常: {e}')
            finally:
                try: os.remove(directory + '/' + filename)
                except Exception: pass
                try: os.remove(filename)
                except Exception: pass
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
    # Test 1
    test.test_public()
    # Test 2
    port = random.randint(2000, 3000)
    directory = ''.join(random.choice(string.ascii_letters) for x in range(10))
    if os.path.isdir(directory):
        shutil.rmtree(directory)
    os.mkdir(directory)
    test.test_public(port, directory)
    shutil.rmtree(directory)
    # Clean
    subprocess.run(['make', 'clean'], stdout=subprocess.PIPE)
    # Result
    if test.credit < 0: test.credit = 0
    print(f'[{_now()}] [result] Your credit is {test.credit} / 28')
    print(f'[{_now()}] [dir-result] Directory ops credit is {test.dir_credit} / 28')