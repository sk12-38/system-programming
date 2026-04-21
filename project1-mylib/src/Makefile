# 컴파일러 및 옵션
CC = gcc
CFLAGS = -Wall -Wextra -g -std=gnu99
LDFLAGS =
TARGET = testlib

# 소스 파일 및 오브젝트 파일 목록
SRCS = main.c list.c hash.c bitmap.c debug.c hex_dump.c
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

# 최종 실행 파일
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

# .c → .o 컴파일
%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

# 의존성 파일 포함
-include $(DEPS)

# 정리
.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)
