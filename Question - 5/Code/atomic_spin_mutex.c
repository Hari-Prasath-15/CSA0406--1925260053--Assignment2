#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>

#define TOTAL_UPDATES 1600000L

static atomic_long atomic_counter = 0;
static long shared_counter = 0;
static pthread_spinlock_t spin_lock;
static pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
static sem_t sem_lock;

typedef enum {
    MODE_ATOMIC = 0,
    MODE_SPINLOCK = 1,
    MODE_MUTEX = 2,
    MODE_SEMAPHORE = 3
} SyncMode;

typedef struct {
    long iters;
    SyncMode mode;
} WorkerArg;

static double elapsed_ns(struct timespec s, struct timespec e)
{
    return (double)(e.tv_sec - s.tv_sec) * 1e9 + (double)(e.tv_nsec - s.tv_nsec);
}

static void *worker(void *arg)
{
    WorkerArg *wa = (WorkerArg *)arg;
    for (long i = 0; i < wa->iters; i++) {
        switch (wa->mode) {
            case MODE_ATOMIC:
                atomic_fetch_add_explicit(&atomic_counter, 1, memory_order_relaxed);
                break;
            case MODE_SPINLOCK:
                pthread_spin_lock(&spin_lock);
                shared_counter++;
                pthread_spin_unlock(&spin_lock);
                break;
            case MODE_MUTEX:
                pthread_mutex_lock(&mutex_lock);
                shared_counter++;
                pthread_mutex_unlock(&mutex_lock);
                break;
            case MODE_SEMAPHORE:
                sem_wait(&sem_lock);
                shared_counter++;
                sem_post(&sem_lock);
                break;
        }
    }
    return NULL;
}

static double benchmark_mode(int num_threads, SyncMode mode)
{
    pthread_t threads[16];
    WorkerArg args[16];
    long per_thread = TOTAL_UPDATES / num_threads;

    atomic_store(&atomic_counter, 0);
    shared_counter = 0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_threads; i++) {
        args[i].iters = per_thread;
        args[i].mode = mode;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    return elapsed_ns(start, end) / (double)(per_thread * num_threads);
}

int main(void)
{
    pthread_spin_init(&spin_lock, PTHREAD_PROCESS_PRIVATE);
    sem_init(&sem_lock, 0, 1);

    const int thread_counts[] = {1, 4, 16};
    printf("Counter Update Benchmark (%ld total updates per run)\n", TOTAL_UPDATES);
    printf("%-10s | %-14s | %-14s | %-14s | %-14s\n",
           "Threads", "Atomic (ns)", "Spinlock (ns)", "Mutex (ns)", "Semaphore (ns)");
    printf("----------------------------------------------------------------------------\n");

    for (int i = 0; i < 3; i++) {
        int t = thread_counts[i];
        double t_atomic = benchmark_mode(t, MODE_ATOMIC);
        double t_spin   = benchmark_mode(t, MODE_SPINLOCK);
        double t_mutex  = benchmark_mode(t, MODE_MUTEX);
        double t_sem    = benchmark_mode(t, MODE_SEMAPHORE);

        printf("%4d       | %11.2f ns | %11.2f ns | %11.2f ns | %11.2f ns\n",
               t, t_atomic, t_spin, t_mutex, t_sem);
    }

    pthread_spin_destroy(&spin_lock);
    pthread_mutex_destroy(&mutex_lock);
    sem_destroy(&sem_lock);
    return 0;
}
