#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_TASKS 200 // 최대 작업 수 정의

double *data;  // 데이터 요소를 저장할 배열
int n_data;	   // 데이터 요소의 개수
int n_threads; // 스레드의 개수

// 작업 상태를 나타내는 열거형
enum task_status
{
	UNDONE,	 // 작업이 시작되지 않음
	PROCESS, // 작업이 진행 중
	DONE	 // 작업이 완료됨
};

// 정렬 작업을 나타내는 구조체
struct sorting_task
{
	double *a;	// 정렬할 배열에 대한 포인터
	int n_a;	// 배열의 요소 개수
	int status; // 작업의 상태
};

// 작업 배열 및 상태 변수들
struct sorting_task tasks[MAX_TASKS];
int n_tasks = 0;  // 총 작업 수
int n_undone = 0; // 완료되지 않은 작업 수
int n_done = 0;	  // 완료된 작업 수

// 작업 동기화를 위한 뮤텍스 및 조건 변수
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER; // lock을 위한 변수
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;  // wait을 위한 조건변수

// 함수 선언
void merge_lists(double *a1, int n_a1, double *a2, int n_a2); // 두 정렬된 배열을 병합하는 함수
void merge_sort(double *a, int n_a);						  // 병합 정렬을 수행하는 함수

// 스레드용 작업 함수
void *worker(void *ptr)
{
	while (1)
	{
		pthread_mutex_lock(&m); // 뮤텍스 잠금
		while (n_undone == 0)
		{
			pthread_cond_wait(&cv, &m); // 작업이 있을 때까지 대기
		}

		int i;
		for (i = 0; i < n_tasks; i++)
		{
			if (tasks[i].status == UNDONE)
				break;
		}

		tasks[i].status = PROCESS; // 작업 상태를 진행 중으로 변경
		n_undone--;
		pthread_mutex_unlock(&m); // 뮤텍스 잠금 해제

		printf("[Thread %ld] starts Task %d\n", pthread_self(), i);

		merge_sort(tasks[i].a, tasks[i].n_a); // 해당 작업에 대해 병합 정렬 수행

		printf("[Thread %ld] completed Task %d\n", pthread_self(), i);

		pthread_mutex_lock(&m); // 뮤텍스 잠금
		tasks[i].status = DONE; // 작업 상태를 완료로 변경
		n_done++;
		pthread_mutex_unlock(&m); // 뮤텍스 잠금 해제
	}
}

int main(int argc, char *argv[])
{
	struct timeval start, end;

	// 명령행 인수 파싱
	int opt;
	while ((opt = getopt(argc, argv, "d:t:")) != -1)
	{
		switch (opt)
		{
		case 'd':
			n_data = atoi(optarg); // 데이터 요소의 개수 설정
			break;
		case 't':
			n_threads = atoi(optarg); // 스레드의 개수 설정
			break;
		default:
			fprintf(stderr, "Usage: %s -d <# data elements> -t <# threads>\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	// 유효한 인수인지 확인
	if (n_data <= 0 || n_threads <= 0)
	{
		fprintf(stderr, "Invalid arguments. Number of data elements and threads must be positive.\n");
		exit(EXIT_FAILURE);
	}

	// 데이터 메모리 할당
	data = (double *)malloc(n_data * sizeof(double));
	if (data == NULL)
	{
		perror("Failed to allocate memory for data");
		exit(EXIT_FAILURE);
	}

	// 랜덤 값으로 데이터 초기화
	gettimeofday(&start, NULL);
	srand(start.tv_usec * start.tv_sec);

	for (int i = 0; i < n_data; i++)
	{
		int num = rand();
		int den = rand();
		if (den != 0.0)
			data[i] = ((double)num) / ((double)den);
		else
			data[i] = ((double)num);
	}

	// 스레드 생성
	pthread_t threads[n_threads];
	for (int i = 0; i < n_threads; i++)
	{
		pthread_create(&(threads[i]), NULL, worker, NULL);
	}

	// 작업 초기화
	for (int i = 0; i < MAX_TASKS; i++)
	{
		pthread_mutex_lock(&m); // 뮤텍스 잠금

		tasks[n_tasks].a = data + (n_data / MAX_TASKS) * n_tasks; // 작업에 데이터의 일부 할당
		tasks[n_tasks].n_a = n_data / MAX_TASKS;
		if (n_tasks == MAX_TASKS - 1)
			tasks[n_tasks].n_a += n_data % MAX_TASKS; // 마지막 작업은 남은 요소들을 포함
		tasks[n_tasks].status = UNDONE;				  // 작업 상태를 미완료로 설정

		n_undone++;
		n_tasks++;

		pthread_cond_signal(&cv); // 스레드에 작업 시작 신호
		pthread_mutex_unlock(&m); // 뮤텍스 잠금 해제
	}

	// 모든 작업이 완료될 때까지 대기
	pthread_mutex_lock(&m); // 뮤텍스 잠금
	while (n_done < MAX_TASKS)
	{
		pthread_mutex_unlock(&m); // 뮤텍스 잠금 해제
		pthread_mutex_lock(&m);	  // 다시 잠금
	}
	pthread_mutex_unlock(&m); // 최종적으로 잠금 해제

	// 정렬된 모든 작업 병합
	int n_sorted = n_data / n_tasks;
	for (int i = 1; i < n_tasks; i++)
	{
		merge_lists(data, n_sorted, tasks[i].a, tasks[i].n_a);
		n_sorted += tasks[i].n_a;
	}

	// 실행 시간 계산 및 출력
	gettimeofday(&end, NULL);
	double execution_time = (end.tv_sec - start.tv_sec) * 1000.0;
	execution_time += (end.tv_usec - start.tv_usec) / 1000.0;
	printf("Execution time: %f ms\n", execution_time);

#ifdef DEBUG
	// DEBUG가 정의된 경우 정렬된 데이터 출력
	for (int i = 0; i < n_data; i++)
	{
		printf("%lf ", data[i]);
	}
#endif

	free(data); // 할당된 메모리 해제
	return EXIT_SUCCESS;
}

// 두 정렬된 배열을 병합하는 함수
void merge_lists(double *a1, int n_a1, double *a2, int n_a2)
{
	double *a_m = (double *)calloc(n_a1 + n_a2, sizeof(double));
	int i = 0;

	int top_a1 = 0;
	int top_a2 = 0;

	for (i = 0; i < n_a1 + n_a2; i++)
	{
		if (top_a2 >= n_a2)
		{
			a_m[i] = a1[top_a1];
			top_a1++;
		}
		else if (top_a1 >= n_a1)
		{
			a_m[i] = a2[top_a2];
			top_a2++;
		}
		else if (a1[top_a1] < a2[top_a2])
		{
			a_m[i] = a1[top_a1];
			top_a1++;
		}
		else
		{
			a_m[i] = a2[top_a2];
			top_a2++;
		}
	}
	memcpy(a1, a_m, (n_a1 + n_a2) * sizeof(double));
	free(a_m);
}

// 병합 정렬을 수행하는 함수
void merge_sort(double *a, int n_a)
{
	if (n_a < 2)
		return;

	double *a1;
	int n_a1;
	double *a2;
	int n_a2;

	a1 = a;
	n_a1 = n_a / 2;

	a2 = a + n_a1;
	n_a2 = n_a - n_a1;

	merge_sort(a1, n_a1);
	merge_sort(a2, n_a2);

	merge_lists(a1, n_a1, a2, n_a2);
}
