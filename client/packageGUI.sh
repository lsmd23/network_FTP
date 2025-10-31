#!/usr/bin/env bash
set -euo pipefail

# 配置
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GUI_PY="$SCRIPT_DIR/gui.py"
CLIENT_PY="$SCRIPT_DIR/client.py"
TARGET_NAME="ftp_gui_client"
DIST_DIR="$SCRIPT_DIR/dist"
DIST_BIN="$DIST_DIR/$TARGET_NAME"
CLIENT_BIN="$DIST_DIR/client"

# 检查 Python 与源码
command -v python3 >/dev/null 2>&1 || { echo "需要 python3"; exit 1; }
[ -f "$GUI_PY" ] || { echo "未找到 $GUI_PY"; exit 1; }
[ -f "$CLIENT_PY" ] || { echo "未找到 $CLIENT_PY"; exit 1; }

# 安装/启用 PyInstaller
if ! command -v pyinstaller >/dev/null 2>&1; then
  echo "安装 PyInstaller..."
  python3 -m pip install --user pyinstaller
  export PATH="$HOME/.local/bin:$PATH"
fi

# 清理旧构建
rm -rf "$SCRIPT_DIR/build" "$DIST_DIR" "$SCRIPT_DIR/$TARGET_NAME.spec"

# 打包命令行程序
echo "开始打包命令行程序..."
pyinstaller -F -n "client" --clean "$CLIENT_PY"

# 检查命令行程序是否打包成功
[ -x "$CLIENT_BIN" ] || { echo "命令行程序打包失败：未生成 $CLIENT_BIN"; exit 1; }

# 打包 GUI 程序
echo "开始打包 GUI 程序..."
pyinstaller -F -n "$TARGET_NAME" --noconsole --clean \
    --add-binary "$CLIENT_BIN:." "$GUI_PY"

# 检查 GUI 程序是否打包成功
[ -x "$DIST_BIN" ] || { echo "GUI 程序打包失败：未生成 $DIST_BIN"; exit 1; }

# 将命令行程序包含到 GUI 程序的打包目录中
cp "$CLIENT_BIN" "$DIST_DIR/"
chmod +x "$DIST_DIR/client"

# 显示产物
echo "打包完成: $DIST_BIN"
echo "命令行程序已包含: $DIST_DIR/client"

# 赋予执行权限
chmod +x "$DIST_BIN"

echo "完成。"