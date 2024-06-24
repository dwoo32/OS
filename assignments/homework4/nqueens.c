#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include "stack.h"

#ifndef BOARD_SIZE
#define BOARD_SIZE 15 // 체스판 크기 정의
#endif

// 바운디드 버퍼 구조체 정의
typedef struct
{
	pthread_mutex_t lock;	  // 버퍼 접근을 위한 뮤텍스
	pthread_cond_t not_full;  // 버퍼가 가득 찼을 때 대기 조건 변수
	pthread_cond_t not_empty; // 버퍼가 비어 있을 때 대기 조건 변수
	char **elem;			  // 버퍼 요소들을 저장하는 배열
	int capacity;			  // 버퍼 용량
	int num;				  // 현재 버퍼에 저장된 요소의 수
	int front;				  // 버퍼에서 요소를 꺼낼 위치
	int rear;				  // 버퍼에 요소를 추가할 위치
} bounded_buffer;

bounded_buffer *buf = 0x0;		// 공유 버퍼
int solution_count = 0;			// 솔루션 카운터
pthread_mutex_t print_lock;		// 출력 동기화를 위한 뮤텍스
pthread_mutex_t count_lock;		// 솔루션 카운터 동기화를 위한 뮤텍스
volatile sig_atomic_t stop = 0; // 프로그램 종료 플래그

// SIGINT 핸들러: 프로그램 종료를 위해 stop 플래그를 설정하고 모든 조건 변수를 브로드캐스트함
void handle_sigint(int sig)
{
	stop = 1;
	pthread_cond_broadcast(&(buf->not_empty));
	pthread_cond_broadcast(&(buf->not_full));
}

// 바운디드 버퍼 초기화 함수
void bounded_buffer_init(bounded_buffer *buf, int capacity)
{
	pthread_mutex_init(&(buf->lock), 0x0);				   // 뮤텍스 초기화
	pthread_cond_init(&(buf->not_full), 0x0);			   // not_full 조건 변수 초기화
	pthread_cond_init(&(buf->not_empty), 0x0);			   // not_empty 조건 변수 초기화
	buf->capacity = capacity;							   // 버퍼 용량 설정
	buf->elem = (char **)calloc(capacity, sizeof(char *)); // 버퍼 요소 배열 할당
	buf->num = 0;										   // 현재 요소 수 초기화
	buf->front = 0;										   // front 초기화
	buf->rear = 0;										   // rear 초기화
}

// 버퍼에 메시지 추가 함수
void bounded_buffer_queue(bounded_buffer *buf, char *msg)
{
	pthread_mutex_lock(&(buf->lock));
	while (buf->num == buf->capacity && !stop)
	{
		pthread_cond_wait(&(buf->not_full), &(buf->lock)); // 버퍼가 가득 찬 경우 대기
	}
	if (stop)
	{
		pthread_mutex_unlock(&(buf->lock));
		return;
	}
	buf->elem[buf->rear] = msg;					 // 메시지를 버퍼에 추가
	buf->rear = (buf->rear + 1) % buf->capacity; // rear 포인터 갱신
	buf->num++;									 // 요소 수 증가
	pthread_cond_signal(&(buf->not_empty));		 // 버퍼가 비어 있지 않음을 신호
	pthread_mutex_unlock(&(buf->lock));
}

// 버퍼에서 메시지 제거 함수
char *bounded_buffer_dequeue(bounded_buffer *buf)
{
	char *r = 0x0;
	pthread_mutex_lock(&(buf->lock));
	while (buf->num == 0 && !stop)
	{
		pthread_cond_wait(&(buf->not_empty), &(buf->lock)); // 버퍼가 비어 있는 경우 대기
	}
	if (buf->num > 0)
	{
		r = buf->elem[buf->front];					   // 버퍼에서 메시지 제거
		buf->front = (buf->front + 1) % buf->capacity; // front 포인터 갱신
		buf->num--;									   // 요소 수 감소
		pthread_cond_signal(&(buf->not_full));		   // 버퍼가 가득 차 있지 않음을 신호
	}
	pthread_mutex_unlock(&(buf->lock));
	return r;
}

// 셀의 행 번호를 반환하는 함수
int row(int cell)
{
	return cell / BOARD_SIZE;
}

// 셀의 열 번호를 반환하는 함수
int col(int cell)
{
	return cell % BOARD_SIZE;
}

// 현재 스택에 있는 퀸 배치가 유효한지 검사하는 함수
int is_feasible(struct stack_t *queens)
{
	int board[BOARD_SIZE][BOARD_SIZE] = {0}; // 체스판 배열 초기화
	for (int i = 0; i < get_size(queens); i++)
	{
		int cell;
		get_elem(queens, i, &cell);
		int r = row(cell);
		int c = col(cell);

		if (board[r][c] != 0)
		{
			return 0; // 충돌 발생 시 유효하지 않음
		}

		int x, y;

		// 행과 열을 채우기
		for (y = 0; y < BOARD_SIZE; y++)
		{
			board[y][c] = 1;
		}
		for (x = 0; x < BOARD_SIZE; x++)
		{
			board[r][x] = 1;
		}

		// 대각선을 채우기
		for (y = r + 1, x = c + 1; y < BOARD_SIZE && x < BOARD_SIZE; y++, x++)
		{
			board[y][x] = 1;
		}
		for (y = r + 1, x = c - 1; y < BOARD_SIZE && x >= 0; y++, x--)
		{
			board[y][x] = 1;
		}
		for (y = r - 1, x = c + 1; y >= 0 && x < BOARD_SIZE; y--, x++)
		{
			board[y][x] = 1;
		}
		for (y = r - 1, x = c - 1; y >= 0 && x >= 0; y--, x--)
		{
			board[y][x] = 1;
		}
	}
	return 1; // 유효한 배치
}

// 퀸 배치를 출력하는 함수
void print_placement(struct stack_t *queens)
{
	pthread_mutex_lock(&print_lock);
	for (int i = 0; i < get_size(queens); i++)
	{
		int queen;
		get_elem(queens, i, &queen);
		printf("[%d,%d] ", row(queen), col(queen)); // 퀸의 위치 출력
	}
	printf("\n");
	pthread_mutex_unlock(&print_lock);
}

// N-Queens 문제를 해결하는 생산자 스레드 함수
void *find_n_queens_thread(void *arg)
{
	int N = *(int *)arg;
	struct stack_t *queens = create_stack(BOARD_SIZE); // 스택 생성

	push(queens, 0);
	while (!is_empty(queens) && !stop)
	{
		int latest_queen;
		top(queens, &latest_queen);

		if (latest_queen == BOARD_SIZE * BOARD_SIZE)
		{
			pop(queens, &latest_queen);
			if (!is_empty(queens))
			{
				pop(queens, &latest_queen);
				push(queens, latest_queen + 1);
			}
			else
			{
				break;
			}
			continue;
		}

		if (is_feasible(queens))
		{
			if (get_size(queens) == N)
			{
				char *solution = malloc(256);
				int offset = 0;
				for (int i = 0; i < get_size(queens); i++)
				{
					int queen;
					get_elem(queens, i, &queen);
					offset += sprintf(solution + offset, "[%d,%d] ", row(queen), col(queen)); // 솔루션 생성
				}
				bounded_buffer_queue(buf, solution); // 버퍼에 솔루션 추가
				pthread_mutex_lock(&count_lock);
				solution_count++;
				pthread_mutex_unlock(&count_lock);

				pop(queens, &latest_queen);
				push(queens, latest_queen + 1);
			}
			else
			{
				top(queens, &latest_queen);
				push(queens, latest_queen + 1);
			}
		}
		else
		{
			pop(queens, &latest_queen);
			push(queens, latest_queen + 1);
		}
	}

	bounded_buffer_queue(buf, NULL); // 메인 스레드에게 종료 신호 보내기
	delete_stack(queens);
	return NULL;
}

// 소비자 스레드 함수: 버퍼에서 결과를 꺼내어 출력
void *consumer(void *ptr)
{
	char *result;
	while (1)
	{
		result = bounded_buffer_dequeue(buf);
		if (result == NULL)
		{
			break;
		}
		printf("Solution: %s\n", result); // 솔루션 출력
		free(result);
	}
	return NULL;
}

// 주어진 사전 배치(prepositions)를 사용하여 N-Queens 문제를 해결하는 함수
int find_n_queens_with_prepositions(int N, struct stack_t *prep)
{
	pthread_t thread;
	pthread_create(&thread, NULL, find_n_queens_thread, &N);
	pthread_join(thread, NULL);
	return 0;
}

// 다중 스레드로 N-Queens 문제를 해결하는 함수
int find_n_queens(int N)
{
	pthread_t thread;
	pthread_create(&thread, NULL, find_n_queens_thread, &N);
	pthread_join(thread, NULL);
	return 0;
}

// 메인 함수
int main(int argc, char *argv[])
{
	int opt;
	int num_threads = 1;
	int N = 4; // 기본값

	// 명령줄 인자 처리: -t 옵션을 사용하여 스레드 개수 설정, -n 옵션을 사용하여 N 설정
	while ((opt = getopt(argc, argv, "t:n:")) != -1)
	{
		switch (opt)
		{
		case 't':
			num_threads = atoi(optarg);
			break;
		case 'n':
			N = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Usage: %s -t <number of threads> -n <number of queens>\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	buf = malloc(sizeof(bounded_buffer)); // 공유 버퍼 할당
	bounded_buffer_init(buf, 10);		  // 공유 버퍼 초기화

	pthread_mutex_init(&print_lock, NULL); // 출력 뮤텍스 초기화
	pthread_mutex_init(&count_lock, NULL); // 카운트 뮤텍스 초기화
	signal(SIGINT, handle_sigint);		   // SIGINT 핸들러 설정

	pthread_t threads[num_threads];
	pthread_t consumer_thread;

	// 생산자 스레드 생성
	for (int i = 0; i < num_threads; i++)
	{
		pthread_create(&threads[i], NULL, find_n_queens_thread, &N);
	}

	// 소비자 스레드 생성
	pthread_create(&consumer_thread, NULL, consumer, NULL);

	// 모든 생산자 스레드가 작업을 완료할 때까지 대기
	for (int i = 0; i < num_threads; i++)
	{
		pthread_join(threads[i], NULL);
	}

	// 소비자 스레드가 작업을 완료할 때까지 대기
	pthread_join(consumer_thread, NULL);

	// 총 솔루션 수 출력
	printf("Total solutions found: %d\n", solution_count);

	// 자원 해제
	free(buf->elem);
	free(buf);
	pthread_mutex_destroy(&print_lock);
	pthread_mutex_destroy(&count_lock);

	return EXIT_SUCCESS;
}
