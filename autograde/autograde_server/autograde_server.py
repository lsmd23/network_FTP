#! /usr/bin/python3
import subprocess
import random
import time
import filecmp
import struct
import os
import shutil
import string
from ftplib import FTP

class TestServer:
    def __init__(self) -> None:
        self.credit = 0
        self.minor  = 2
        self.major  = 8
    
    def build(self):
        # 在 server 目录下执行 make
        proc = subprocess.Popen(['make'], cwd='server', stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        have_error = False
        while True:
            stdout_line = proc.stdout.readline()
            stderr_line = proc.stderr.readline()
            if not stdout_line and not stderr_line:
                break
            
            # 将 bytes 解码为 str
            stdout = stdout_line.decode()
            stderr = stderr_line.decode()

            if stdout and 'gcc' in stdout and '-Wall' not in stdout:
                print('No -Wall argument')
                print('Your credit is 0')
                exit(0)
            if stderr and not have_error:
                print('There are warnings when compiling your program')
                have_error = True
        if have_error:
            self.credit -= self.major

    def create_test_file(self, filename, filesize = 10000):
        f = open(filename, 'wb')
        for i in range(filesize):
            data = struct.pack('d', random.random())
            f.write(data)
        f.close()

    def test_public(self, port=21, directory='/tmp'):
        # 使用正确的路径 ./server/server 启动服务器
        if port == 21 and directory == '/tmp':
            server = subprocess.Popen(['sudo', './server/server'], stdout=subprocess.PIPE)
        else:
            server = subprocess.Popen(['sudo', './server/server', '-port', '%d' % port, '-root', directory], stdout=subprocess.PIPE)
        time.sleep(0.1)
        try:
            ftp = FTP()
            # connect
            if not ftp.connect('127.0.0.1', port).startswith('220'):
                print('You missed response 220')
            else:
                self.credit += self.minor
            # login
            # 为 login 提供一个符合电子邮件格式的密码
            if not ftp.login('anonymous', 'test@test.com').startswith('230'):
                print('You missed response 230')
            else:
                self.credit += self.minor
            # SYST
            if ftp.sendcmd('SYST') != '215 UNIX Type: L8':
                print('Bad response for SYST')
            else:
                self.credit += self.minor
            # TYPE
            if ftp.sendcmd('TYPE I') != '200 Type set to I.':
                print('Bad response for TYPE I')
            else:
                self.credit += self.minor

            # PORT download
            filename = 'test%d.data' % random.randint(100, 200)
            self.create_test_file(directory + '/' + filename)
            ftp.set_pasv(False)
            if not ftp.retrbinary('RETR %s' % filename, open(filename, 'wb').write).startswith('226'):
                print('Bad response for RETR')
            elif not filecmp.cmp(filename, directory + '/' + filename):
                print('Something wrong with RETR')
            else:
                self.credit += self.minor
            os.remove(directory + '/' + filename)
            os.remove(filename)
            
            # PASV upload
            ftp2 = FTP()
            ftp2.connect('127.0.0.1', port)
            # 同样修改这里的 login
            ftp2.login('anonymous', 'test@test.com')
            filename = 'test%d.data' % random.randint(100, 200)
            self.create_test_file(filename)
            if not ftp2.storbinary('STOR %s' % filename, open(filename, 'rb')).startswith('226'):
                print('Bad response for STOR')
            if not filecmp.cmp(filename, directory + '/' + filename):
                print('Something wrong with STOR')
            else:
                self.credit += self.minor
            os.remove(directory + '/' + filename)
            os.remove(filename)

            # QUIT
            if not ftp.quit().startswith('221'):
                print('Bad response for QUIT')
            else:
                self.credit += self.minor
            ftp2.quit()
            # 使用 sudo 权限杀死服务器进程
            subprocess.run(['sudo', 'kill', str(server.pid)])

        except Exception as e:
            print('Exception occurred:', e)
        
        # 确保在异常情况下也能用 sudo 杀死服务器
        if server.poll() is None: # 检查进程是否还在运行
            subprocess.run(['sudo', 'kill', str(server.pid)])

if __name__ == "__main__":
    # 在开始前，清理可能残留的旧进程
    subprocess.run("sudo pkill -f './server/server' || true", shell=True)
    test = TestServer()
    test.build()
    # Test 1
    test.test_public(port=10021)
    # Test 2
    port = random.randint(2000, 3000)
    directory = ''.join(random.choice(string.ascii_letters) for x in range(10))
    if os.path.isdir(directory):
        shutil.rmtree(directory)
    os.mkdir(directory)
    test.test_public(port, directory)
    shutil.rmtree(directory)
    # Clean
    # 在 server 目录下执行 make clean
    subprocess.run(['make', 'clean'], cwd='server', stdout=subprocess.PIPE)
    # Result
    if test.credit < 0: test.credit = 0
    print(f'Your credit is {test.credit}')