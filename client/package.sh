#!/usr/bin/env bash
set -euo pipefail

# 配置
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CLIENT_PY="$SCRIPT_DIR/client.py"
TARGET_NAME="client"
DIST_BIN="$SCRIPT_DIR/dist/$TARGET_NAME"
AUTOGRADE_DIR="$SCRIPT_DIR/../autograde/autograde_client"
AUTOGRADE_TARGET="$AUTOGRADE_DIR/$TARGET_NAME"

# 检查 Python 与源码
command -v python3 >/dev/null 2>&1 || { echo "需要 python3"; exit 1; }
[ -f "$CLIENT_PY" ] || { echo "未找到 $CLIENT_PY"; exit 1; }

# 安装/启用 PyInstaller
if ! command -v pyinstaller >/dev/null 2>&1; then
  echo "安装 PyInstaller..."
  python3 -m pip install --user pyinstaller
  export PATH="$HOME/.local/bin:$PATH"
fi

# 清理旧构建
rm -rf "$SCRIPT_DIR/build" "$SCRIPT_DIR/dist" "$SCRIPT_DIR/$TARGET_NAME.spec"

# 打包
echo "开始打包..."
pyinstaller -F -n "$TARGET_NAME" --clean "$CLIENT_PY"

# 显示产物
[ -x "$DIST_BIN" ] || { echo "打包失败：未生成 $DIST_BIN"; exit 1; }
echo "打包完成: $DIST_BIN"

# 拷贝到当前目录（若存在）
if [ -d "$AUTOGRADE_DIR" ]; then
  cp -f "$DIST_BIN" "$AUTOGRADE_TARGET"
  chmod +x "$AUTOGRADE_TARGET"
  echo "已拷贝到: $AUTOGRADE_TARGET"
else
  echo "未找到评分目录 $AUTOGRADE_DIR，已跳过拷贝。"
fi

# 赋予执行权限
chmod +x "$DIST_BIN"
chmod +x "$AUTOGRADE_TARGET" || true

echo "完成。"