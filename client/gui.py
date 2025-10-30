import sys
import os
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
    QLabel, QLineEdit, QPushButton, QTextEdit, QMessageBox
)
from PyQt5.QtCore import QProcess, Qt
from PyQt5.QtGui import QFont, QPalette, QColor

class FTPGuiClient(QWidget):
    def __init__(self):
        super().__init__()
        self.client_process = None
        self.init_ui()

    def init_ui(self):
        self.setWindowTitle('FTP 客户端图形界面')
        self.setGeometry(300, 300, 800, 600)

        # --- 整体布局 ---
        main_layout = QVBoxLayout()

        # --- 1. 连接设置区域 ---
        connect_layout = QGridLayout()
        self.ip_input = QLineEdit('127.0.0.1')
        self.port_input = QLineEdit('21')
        self.connect_button = QPushButton('连接')
        self.disconnect_button = QPushButton('断开连接')

        connect_layout.addWidget(QLabel('服务器 IP:'), 0, 0)
        connect_layout.addWidget(self.ip_input, 0, 1)
        connect_layout.addWidget(QLabel('端口:'), 0, 2)
        connect_layout.addWidget(self.port_input, 0, 3)
        connect_layout.addWidget(self.connect_button, 0, 4)
        connect_layout.addWidget(self.disconnect_button, 0, 5)

        # --- 2. 日志显示区域 ---
        self.log_display = QTextEdit()
        self.log_display.setReadOnly(True)
        # 设置一个深色背景和亮色文字，更像终端
        palette = self.log_display.palette()
        palette.setColor(QPalette.Base, QColor(40, 42, 54))
        palette.setColor(QPalette.Text, QColor(248, 248, 242))
        self.log_display.setPalette(palette)
        self.log_display.setFont(QFont('Monospace', 10))

        # --- 3. 命令输入区域 ---
        command_layout = QHBoxLayout()
        self.command_input = QLineEdit()
        self.send_button = QPushButton('发送')
        command_layout.addWidget(QLabel('命令:'))
        command_layout.addWidget(self.command_input)
        command_layout.addWidget(self.send_button)

        # --- 组合布局 ---
        main_layout.addLayout(connect_layout)
        main_layout.addWidget(self.log_display)
        main_layout.addLayout(command_layout)
        self.setLayout(main_layout)

        # --- 初始状态 ---
        self.disconnect_button.setEnabled(False)
        self.command_input.setEnabled(False)
        self.send_button.setEnabled(False)

        # --- 连接信号和槽 ---
        self.connect_button.clicked.connect(self.start_client)
        self.disconnect_button.clicked.connect(self.stop_client)
        self.send_button.clicked.connect(self.send_command)
        self.command_input.returnPressed.connect(self.send_command)

    def start_client(self):
        ip = self.ip_input.text()
        port = self.port_input.text()

        if not ip or not port:
            QMessageBox.warning(self, '输入错误', '请输入服务器 IP 和端口。')
            return

        self.log_display.clear()
        self.log_display.append(f'>>> 正在尝试连接到 {ip}:{port} ...')

        self.client_process = QProcess(self)
        self.client_process.readyReadStandardOutput.connect(self.handle_stdout)
        self.client_process.readyReadStandardError.connect(self.handle_stderr)
        self.client_process.finished.connect(self.process_finished)

        # --- 核心修改 ---
        # 构造启动命令。因为 gui.py 和 client.py 在同一目录下，可以直接引用。
        client_script_path = os.path.join(os.path.dirname(__file__), 'client.py')
        program = sys.executable  # 使用当前 Python 解释器，跨平台更稳
        args = [client_script_path, '-ip', ip, '-port', port]
        self.client_process.start(program, args)
        
        self.connect_button.setEnabled(False)
        self.disconnect_button.setEnabled(True)
        self.command_input.setEnabled(True)
        self.send_button.setEnabled(True)
        self.command_input.setFocus()

    def stop_client(self):
        if self.client_process:
            self.client_process.write(b'QUIT\n')
            if not self.client_process.waitForFinished(1000):
                self.client_process.kill()
            self.log_display.append('>>> 连接已断开。')

    def send_command(self):
        if not self.client_process or self.client_process.state() != QProcess.Running:
            return
        
        command = self.command_input.text()
        if not command:
            return
        
        self.log_display.append(f'>>> {command}')
        
        command_bytes = (command + '\n').encode('utf-8')
        self.client_process.write(command_bytes)
        
        self.command_input.clear()

    def handle_stdout(self):
        data = self.client_process.readAllStandardOutput()
        text = bytes(data).decode('utf-8', errors='ignore').strip()
        if text:
            self.log_display.append(text)

    def handle_stderr(self):
        data = self.client_process.readAllStandardError()
        text = bytes(data).decode('utf-8', errors='ignore').strip()
        if text:
            self.log_display.append(f'<font color="red">错误: {text}</font>')

    def process_finished(self):
        self.log_display.append('>>> 后台进程已结束。')
        self.connect_button.setEnabled(True)
        self.disconnect_button.setEnabled(False)
        self.command_input.setEnabled(False)
        self.send_button.setEnabled(False)
        self.client_process = None

    def closeEvent(self, event):
        self.stop_client()
        event.accept()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    ex = FTPGuiClient()
    ex.show()
    sys.exit(app.exec_())