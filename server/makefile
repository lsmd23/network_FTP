# 编译器
CC = gcc

# 编译选项
# -g: 添加调试信息
# -Wall: 开启所有常用警告
# -Isrc: 告诉编译器在 src 目录下查找头文件 (.h 文件)
CFLAGS = -g -Wall -Isrc

# 链接选项
# -lregex: 链接正则表达式库 (因为 handle.c 中用到了)
LDFLAGS =

# 源文件目录
SRCDIR = .

# 目标文件（可执行文件）的名称
TARGET = ftpserver

# 所有的 .c 源文件
SRCS = $(SRCDIR)/main.c $(SRCDIR)/handle.c $(SRCDIR)/utils.c $(SRCDIR)/connect.c $(SRCDIR)/file.c

# 根据 .c 文件自动生成 .o 目标文件的列表
OBJS = $(SRCS:.c=.o)

# 默认目标：all
# 当你只输入 make 时，会执行这个目标
all: $(TARGET)

# 链接规则：如何从 .o 文件生成最终的可执行文件
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# 编译规则：如何从 .c 文件生成 .o 文件
# $< 代表第一个依赖项 (即 .c 文件)
# $@ 代表目标 (即 .o 文件)
$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理规则：删除所有生成的文件
# 当你输入 make clean 时，会执行这个目标
clean:
	rm -f $(TARGET) $(OBJS)

# .PHONY 告诉 make，all 和 clean 不是真正的文件名
.PHONY: all clean