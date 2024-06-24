# 컴파일러 설정
CC = gcc
CFLAGS = -Wall -pthread

# 타겟 이름 설정
TARGET = nqueens

# 소스 파일 설정
SRCS = nqueens.c stack.c
OBJS = $(SRCS:.c=.o)

# 기본 타겟 설정
all: $(TARGET)

# 타겟 빌드 규칙
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# 객체 파일 빌드 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 클린업 규칙
clean:
	rm -f $(TARGET) $(OBJS)
