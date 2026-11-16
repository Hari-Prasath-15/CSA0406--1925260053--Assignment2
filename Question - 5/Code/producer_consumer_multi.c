#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>

#define SIZE 5
#define NUM_PRODUCERS 4
#define NUM_CONSUMERS 4
#define ITEMS_PER_THREAD 25
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_THREAD)

int buffer[SIZE], in = 0, out = 0;
sem_t empty, full;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

long consumed_sum = 0;
int consumed_total = 0;
int item_seen[TOTAL_ITEMS + 1];
int use_mutex = 1;

void *producer(void *arg)
{
    int id = *(int *)arg;
    int start_item = id * ITEMS_PER_THREAD + 1;

    for (int i = 0; i < ITEMS_PER_THREAD; i++) {
        int item = start_item + i;
        sem_wait(&empty);
        if (use_mutex) pthread_mutex_lock(&lock);

        buffer[in] = item;
        in = (in + 1) % SIZE;

        if (use_mutex) pthread_mutex_unlock(&lock);
        sem_post(&full);
    }
    return NULL;
}

void *consumer(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITEMS_PER_THREAD; i++) {
        sem_wait(&full);
        if (use_mutex) pthread_mutex_lock(&lock);

        int item = buffer[out];
        out = (out + 1) % SIZE;

        consumed_sum += item;
        consumed_total++;
        if (item >= 1 && item <= TOTAL_ITEMS) {
            item_seen[item]++;
        }

        if (use_mutex) pthread_mutex_unlock(&lock);
        sem_post(&empty);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "--no-mutex") == 0) {
        use_mutex = 0;
    }

    memset(item_seen, 0, sizeof(item_seen));
    sem_init(&empty, 0, SIZE);
    sem_init(&full, 0, 0);

    pthread_t prod[NUM_PRODUCERS], cons[NUM_CONSUMERS];
    int prod_ids[NUM_PRODUCERS];

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        prod_ids[i] = i;
        pthread_create(&prod[i], NULL, producer, &prod_ids[i]);
    }
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_create(&cons[i], NULL, consumer, NULL);
    }

    for (int i = 0; i < NUM_PRODUCERS; i++) pthread_join(prod[i], NULL);
    for (int i = 0; i < NUM_CONSUMERS; i++) pthread_join(cons[i], NULL);

    int missing = 0, duplicates = 0;
    for (int i = 1; i <= TOTAL_ITEMS; i++) {
        if (item_seen[i] == 0) missing++;
        else if (item_seen[i] > 1) duplicates++;
    }

    long expected_sum = (long)TOTAL_ITEMS * (TOTAL_ITEMS + 1) / 2;
    printf("Mode: %s | Producers: %d, Consumers: %d\n",
           use_mutex ? "WITH MUTEX" : "NO MUTEX", NUM_PRODUCERS, NUM_CONSUMERS);
    printf("items consumed: %d (expected %d), sum = %ld (expected %ld)\n",
           consumed_total, TOTAL_ITEMS, consumed_sum, expected_sum);
    printf("verification: missing = %d, duplicates = %d\n", missing, duplicates);

    sem_destroy(&empty);
    sem_destroy(&full);
    pthread_mutex_destroy(&lock);
    return 0;
}
