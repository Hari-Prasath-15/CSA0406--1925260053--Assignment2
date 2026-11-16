#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_TRAINS 8
#define COACHES_PER_TRAIN 10
#define SEATS_PER_COACH 72
#define NUM_AGENTS 8
#define BOOKINGS_PER_AGENT 50000

typedef struct {
    int booked_seats;
    pthread_mutex_t coach_lock;
} Coach;

typedef struct {
    Coach coaches[COACHES_PER_TRAIN];
    pthread_mutex_t train_lock;
} Train;

static Train trains[NUM_TRAINS];
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

typedef enum {
    LOCK_GLOBAL = 0,
    LOCK_PER_TRAIN = 1,
    LOCK_PER_COACH = 2
} LockMode;

static LockMode current_mode;

static double elapsed_sec(struct timespec s, struct timespec e)
{
    return (double)(e.tv_sec - s.tv_sec) + (double)(e.tv_nsec - s.tv_nsec) / 1e9;
}

static void reset_trains(void)
{
    for (int t = 0; t < NUM_TRAINS; t++) {
        for (int c = 0; c < COACHES_PER_TRAIN; c++) {
            trains[t].coaches[c].booked_seats = 0;
        }
    }
}

static void *agent_worker(void *arg)
{
    unsigned int seed = (unsigned int)(*(int *)arg + 1) * 1337U;
    volatile int sink = 0;

    for (int i = 0; i < BOOKINGS_PER_AGENT; i++) {
        int t = (int)(rand_r(&seed) % NUM_TRAINS);
        int c = (int)(rand_r(&seed) % COACHES_PER_TRAIN);

        if (current_mode == LOCK_GLOBAL) {
            pthread_mutex_lock(&global_lock);
        } else if (current_mode == LOCK_PER_TRAIN) {
            pthread_mutex_lock(&trains[t].train_lock);
        } else {
            pthread_mutex_lock(&trains[t].coaches[c].coach_lock);
        }

        /* Simulate seat check and booking validation work */
        trains[t].coaches[c].booked_seats++;
        for (int w = 0; w < 50; w++) {
            sink += w;
        }

        if (current_mode == LOCK_GLOBAL) {
            pthread_mutex_unlock(&global_lock);
        } else if (current_mode == LOCK_PER_TRAIN) {
            pthread_mutex_unlock(&trains[t].train_lock);
        } else {
            pthread_mutex_unlock(&trains[t].coaches[c].coach_lock);
        }
    }
    (void)sink;
    return NULL;
}

static void run_benchmark(const char *label, LockMode mode)
{
    current_mode = mode;
    reset_trains();

    pthread_t agents[NUM_AGENTS];
    int ids[NUM_AGENTS];
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_AGENTS; i++) {
        ids[i] = i;
        pthread_create(&agents[i], NULL, agent_worker, &ids[i]);
    }
    for (int i = 0; i < NUM_AGENTS; i++) {
        pthread_join(agents[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    long total_ops = (long)NUM_AGENTS * BOOKINGS_PER_AGENT;
    double sec = elapsed_sec(start, end);
    double throughput = (double)total_ops / sec;

    long total_booked = 0;
    for (int t = 0; t < NUM_TRAINS; t++) {
        for (int c = 0; c < COACHES_PER_TRAIN; c++) {
            total_booked += trains[t].coaches[c].booked_seats;
        }
    }

    printf("%-22s | %8.3f ms | %12.0f ops/sec | booked=%ld\n",
           label, sec * 1000.0, throughput, total_booked);
}

int main(void)
{
    for (int t = 0; t < NUM_TRAINS; t++) {
        pthread_mutex_init(&trains[t].train_lock, NULL);
        for (int c = 0; c < COACHES_PER_TRAIN; c++) {
            pthread_mutex_init(&trains[t].coaches[c].coach_lock, NULL);
        }
    }

    printf("Railway Booking Lock Benchmark (%d agents, %d ops/agent)\n",
           NUM_AGENTS, BOOKINGS_PER_AGENT);
    printf("%-22s | %-11s | %-18s | %s\n",
           "Locking Design", "Time (ms)", "Throughput", "Total Check");
    printf("------------------------------------------------------------------------\n");

    run_benchmark("1. Global Lock", LOCK_GLOBAL);
    run_benchmark("2. One Lock per Train", LOCK_PER_TRAIN);
    run_benchmark("3. One Lock per Coach", LOCK_PER_COACH);

    for (int t = 0; t < NUM_TRAINS; t++) {
        pthread_mutex_destroy(&trains[t].train_lock);
        for (int c = 0; c < COACHES_PER_TRAIN; c++) {
            pthread_mutex_destroy(&trains[t].coaches[c].coach_lock);
        }
    }
    pthread_mutex_destroy(&global_lock);
    return 0;
}
