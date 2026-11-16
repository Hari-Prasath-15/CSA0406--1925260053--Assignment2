#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHUNK_SIZE 65536 /* 64 KB transfer chunk */
#define SHM_BENCH_NAME "/weather_ipc_bench_shm"

typedef struct {
    size_t bytes;
    sem_t can_write;
    sem_t can_read;
    unsigned char buf[CHUNK_SIZE];
} BenchShm;

static double elapsed_ms(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) * 1000.0 +
           (double)(end.tv_nsec - start.tv_nsec) / 1e6;
}

static double benchmark_pipe(size_t total_bytes)
{
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(1);
    }

    unsigned char *src = malloc(CHUNK_SIZE);
    if (!src) {
        perror("malloc");
        exit(1);
    }
    memset(src, 0x2A, CHUNK_SIZE);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        close(fd[1]);
        unsigned char *dst = malloc(CHUNK_SIZE);
        if (!dst) _exit(1);
        volatile unsigned long checksum = 0;
        ssize_t n;
        while ((n = read(fd[0], dst, CHUNK_SIZE)) > 0) {
            checksum += dst[0];
        }
        (void)checksum;
        free(dst);
        close(fd[0]);
        _exit(0);
    }

    close(fd[0]);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    size_t remaining = total_bytes;
    while (remaining > 0) {
        size_t to_write = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
        size_t written = 0;
        while (written < to_write) {
            ssize_t n = write(fd[1], src + written, to_write - written);
            if (n < 0) {
                perror("write");
                exit(1);
            }
            written += (size_t)n;
        }
        remaining -= to_write;
    }
    close(fd[1]);
    waitpid(pid, NULL, 0);

    clock_gettime(CLOCK_MONOTONIC, &end);
    free(src);
    return elapsed_ms(start, end);
}

static double benchmark_shm(size_t total_bytes)
{
    int shm_fd = shm_open(SHM_BENCH_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }
    if (ftruncate(shm_fd, sizeof(BenchShm)) == -1) {
        perror("ftruncate");
        shm_unlink(SHM_BENCH_NAME);
        exit(1);
    }
    BenchShm *shm = mmap(NULL, sizeof(BenchShm), PROT_READ | PROT_WRITE,
                         MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        shm_unlink(SHM_BENCH_NAME);
        exit(1);
    }

    sem_init(&shm->can_write, 1, 1);
    sem_init(&shm->can_read, 1, 0);

    unsigned char *src = malloc(CHUNK_SIZE);
    if (!src) {
        perror("malloc");
        exit(1);
    }
    memset(src, 0x2A, CHUNK_SIZE);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        unsigned char *dst = malloc(CHUNK_SIZE);
        if (!dst) _exit(1);
        volatile unsigned long checksum = 0;
        while (1) {
            sem_wait(&shm->can_read);
            size_t n = shm->bytes;
            if (n == 0) {
                sem_post(&shm->can_write);
                break;
            }
            memcpy(dst, shm->buf, n);
            checksum += dst[0];
            sem_post(&shm->can_write);
        }
        (void)checksum;
        free(dst);
        munmap(shm, sizeof(BenchShm));
        close(shm_fd);
        _exit(0);
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    size_t remaining = total_bytes;
    while (remaining > 0) {
        size_t to_write = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
        sem_wait(&shm->can_write);
        memcpy(shm->buf, src, to_write);
        shm->bytes = to_write;
        sem_post(&shm->can_read);
        remaining -= to_write;
    }

    /* Signal end of transfer */
    sem_wait(&shm->can_write);
    shm->bytes = 0;
    sem_post(&shm->can_read);

    waitpid(pid, NULL, 0);
    clock_gettime(CLOCK_MONOTONIC, &end);

    free(src);
    sem_destroy(&shm->can_write);
    sem_destroy(&shm->can_read);
    munmap(shm, sizeof(BenchShm));
    close(shm_fd);
    shm_unlink(SHM_BENCH_NAME);
    return elapsed_ms(start, end);
}

int main(void)
{
    const size_t sizes_mb[] = {1, 10, 100};
    const int num_sizes = 3;

    printf("IPC Performance Benchmark (Chunk Size = %d KB)\n", CHUNK_SIZE / 1024);
    printf("%-10s | %-15s | %-15s | %-15s | %-15s\n",
           "Data Size", "Pipe Time (ms)", "Pipe (MB/s)", "SHM Time (ms)", "SHM (MB/s)");
    printf("--------------------------------------------------------------------------------\n");

    for (int i = 0; i < num_sizes; i++) {
        size_t mb = sizes_mb[i];
        size_t total_bytes = mb * 1024UL * 1024UL;

        double pipe_ms = benchmark_pipe(total_bytes);
        double shm_ms = benchmark_shm(total_bytes);

        double pipe_mbps = (double)mb / (pipe_ms / 1000.0);
        double shm_mbps = (double)mb / (shm_ms / 1000.0);

        printf("%3zu MB     | %12.3f ms | %10.2f MB/s | %12.3f ms | %10.2f MB/s\n",
               mb, pipe_ms, pipe_mbps, shm_ms, shm_mbps);
    }

    return 0;
}
