
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

#define NUM_CAMERAS 64
#define WORKLOAD_STEPS 2000000UL
#define COUNTER_THREADS 8
#define INCREMENTS_PER_THREAD 100000

/* Global array to store each camera's computed plate hash result */
static volatile uint64_t plate_results[NUM_CAMERAS];

/* Shared variables for thread-pool task distribution & protected counter */
static int next_camera_index = 0;
static int safe_frames_processed = 0;
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Shared counters for Part D (Unprotected vs. Mutex-Protected test) */
static volatile long unprotected_counter = 0;
static long protected_counter = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Helper function: Returns elapsed wall-clock time in milliseconds
 * between two struct timespec timestamps using CLOCK_MONOTONIC.
 */
static double elapsed_ms(struct timespec start, struct timespec end)
{
    double sec = (double)(end.tv_sec - start.tv_sec);
    double nsec = (double)(end.tv_nsec - start.tv_nsec);
    return (sec * 1000.0) + (nsec / 1e6);
}

/*
 * Fair, deterministic CPU-bound workload simulating number-plate
 * image filtering and OCR feature extraction for a single camera.
 * Uses volatile accumulator so compiler optimizations never skip work.
 */
static uint64_t process_camera_frame(int cam_id)
{
    volatile uint64_t hash = (uint64_t)(cam_id + 1) * 2654435761ULL;
    for (unsigned long step = 0; step < WORKLOAD_STEPS; step++) {
        uint64_t h = hash;
        h ^= (h << 13) ^ (step + (unsigned long)cam_id);
        h = (h * 1099511628211ULL) ^ (h >> 7);
        hash = h;
    }
    return hash;
}

/* ====================================================================
 * 1. SEQUENTIAL PROCESSING (BASELINE)
 * ==================================================================== */
static double run_sequential(void)
{
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int cam = 0; cam < NUM_CAMERAS; cam++) {
        plate_results[cam] = process_camera_frame(cam);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    return elapsed_ms(t0, t1);
}

/* ====================================================================
 * 2. ONE THREAD PER CAMERA (64 THREADS)
 * ==================================================================== */
static void *one_thread_per_camera_worker(void *arg)
{
    int cam_id = *(int *)arg;
    uint64_t res = process_camera_frame(cam_id);
    plate_results[cam_id] = res;

    pthread_mutex_lock(&pool_mutex);
    safe_frames_processed++;
    pthread_mutex_unlock(&pool_mutex);

    return NULL;
}

static double run_64_threads(void)
{
    pthread_t threads[NUM_CAMERAS];
    int cam_ids[NUM_CAMERAS];
    safe_frames_processed = 0;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < NUM_CAMERAS; i++) {
        cam_ids[i] = i;
        if (pthread_create(&threads[i], NULL,
                           one_thread_per_camera_worker, &cam_ids[i]) != 0) {
            perror("Error: pthread_create failed in 64-thread mode");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < NUM_CAMERAS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Error: pthread_join failed in 64-thread mode");
            exit(EXIT_FAILURE);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    return elapsed_ms(t0, t1);
}

/* ====================================================================
 * 3. THREAD POOL PROCESSING (4, 8, AND 16 WORKER THREADS)
 * ==================================================================== */
static void *thread_pool_worker(void *arg)
{
    (void)arg;
    while (1) {
        int cam_id = -1;

        /* Acquire mutex to safely claim the next unprocessed camera */
        pthread_mutex_lock(&pool_mutex);
        if (next_camera_index < NUM_CAMERAS) {
            cam_id = next_camera_index++;
        }
        pthread_mutex_unlock(&pool_mutex);

        /* Exit loop when all 64 cameras have been claimed */
        if (cam_id == -1) {
            break;
        }

        /* Process the assigned camera frame outside the lock */
        plate_results[cam_id] = process_camera_frame(cam_id);

        /* Safely update the shared completed frame counter */
        pthread_mutex_lock(&pool_mutex);
        safe_frames_processed++;
        pthread_mutex_unlock(&pool_mutex);
    }
    return NULL;
}

static double run_thread_pool(int pool_size)
{
    pthread_t workers[16];
    next_camera_index = 0;
    safe_frames_processed = 0;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < pool_size; i++) {
        if (pthread_create(&workers[i], NULL,
                           thread_pool_worker, NULL) != 0) {
            perror("Error: pthread_create failed in thread pool");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < pool_size; i++) {
        if (pthread_join(workers[i], NULL) != 0) {
            perror("Error: pthread_join failed in thread pool");
            exit(EXIT_FAILURE);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    return elapsed_ms(t0, t1);
}

/* ====================================================================
 * 4. SHARED COUNTER DEMONSTRATION (UNPROTECTED VS. MUTEX-PROTECTED)
 * ==================================================================== */
static void *unsafe_counter_worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        /* Intentional race condition: non-atomic Read-Modify-Write */
        long temp = unprotected_counter;
        temp = temp + 1;
        unprotected_counter = temp;
    }
    return NULL;
}

static void *safe_counter_worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
        pthread_mutex_lock(&counter_mutex);
        protected_counter++;
        pthread_mutex_unlock(&counter_mutex);
    }
    return NULL;
}

static void run_shared_counter_demo(void)
{
    pthread_t t_unsafe[COUNTER_THREADS];
    pthread_t t_safe[COUNTER_THREADS];
    long expected = (long)COUNTER_THREADS * INCREMENTS_PER_THREAD;

    unprotected_counter = 0;
    protected_counter = 0;

    for (int i = 0; i < COUNTER_THREADS; i++) {
        if (pthread_create(&t_unsafe[i], NULL,
                           unsafe_counter_worker, NULL) != 0) {
            perror("Error: pthread_create failed");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < COUNTER_THREADS; i++) {
        pthread_join(t_unsafe[i], NULL);
    }

    for (int i = 0; i < COUNTER_THREADS; i++) {
        if (pthread_create(&t_safe[i], NULL,
                           safe_counter_worker, NULL) != 0) {
            perror("Error: pthread_create failed");
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < COUNTER_THREADS; i++) {
        pthread_join(t_safe[i], NULL);
    }

    printf("\n============================================================\n");
    printf("SHARED COUNTER SYNCHRONIZATION TEST (%d Threads x %d)\n",
           COUNTER_THREADS, INCREMENTS_PER_THREAD);
    printf("============================================================\n");
    printf("Expected Counter Value          : %ld\n", expected);
    printf("Unprotected Counter (No Mutex)  : %ld (Lost updates = %ld)\n",
           unprotected_counter, expected - unprotected_counter);
    printf("Protected Counter (With Mutex)  : %ld (Lost updates = %ld)\n",
           protected_counter, expected - protected_counter);
}

int main(void)
{
    printf("============================================================\n");
    printf("Q3: TRAFFIC-CAMERA NUMBER-PLATE READER (64 CAMERAS, PTHREADS)\n");
    printf("============================================================\n");

    double t_seq = run_sequential();
    double t_64  = run_64_threads();
    double t_p4  = run_thread_pool(4);
    double t_p8  = run_thread_pool(8);
    double t_p16 = run_thread_pool(16);

    printf("\n%-28s | %12s | %10s | %8s\n",
           "Execution Model", "Time (ms)", "Speed-up", "Cameras");
    printf("------------------------------------------------------------\n");
    printf("%-28s | %12.2f | %9.2fx | %8d\n",
           "1. Sequential (Baseline)", t_seq, 1.00, NUM_CAMERAS);
    printf("%-28s | %12.2f | %9.2fx | %8d\n",
           "2. One Thread/Camera (64)", t_64, t_seq / t_64, safe_frames_processed);
    printf("%-28s | %12.2f | %9.2fx | %8d\n",
           "3. Thread Pool (4 Threads)", t_p4, t_seq / t_p4, safe_frames_processed);
    printf("%-28s | %12.2f | %9.2fx | %8d\n",
           "4. Thread Pool (8 Threads)", t_p8, t_seq / t_p8, safe_frames_processed);
    printf("%-28s | %12.2f | %9.2fx | %8d\n",
           "5. Thread Pool (16 Threads)", t_p16, t_seq / t_p16, safe_frames_processed);

    run_shared_counter_demo();

    return 0;
}
