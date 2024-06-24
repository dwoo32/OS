# 컴파일러와 플래그
CC = gcc
CFLAGS = -Wall -pthread

# 타겟 실행 파일 이름
TARGET = pmergesort

# 소스 파일
SRCS = pmergesort.c

# 목적 파일
OBJS = $(SRCS:.c=.o)

# 기본 타겟: 실행 파일 빌드
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# 개별 소스 파일 컴파일 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 실행 파일과 목적 파일 제거
clean:
	rm -f $(TARGET) $(OBJS)

# 실행 파일 실행
run: $(TARGET)
	./$(TARGET) -d 40000000 -t 8

.PHONY: clean run
