#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHM_NAME "/weather_station_shm"

/* Shared memory structure holding one sensor reading and two semaphores */
typedef struct {
    int reading;
    int done;           /* 0 = valid reading, 1 = end of data */
    sem_t can_write;    /* 1 when slot is empty (parent can write) */
    sem_t can_read;     /* 1 when slot is full (child can read) */
} SharedWeatherData;

int main(void)
{
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return 1;
    }

    if (ftruncate(shm_fd, sizeof(SharedWeatherData)) == -1) {
        perror("ftruncate");
        shm_unlink(SHM_NAME);
        return 1;
    }

    SharedWeatherData *shm = mmap(NULL, sizeof(SharedWeatherData),
                                  PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        shm_unlink(SHM_NAME);
        return 1;
    }

    /* Initialize process-shared semaphores (pshared = 1) */
    if (sem_init(&shm->can_write, 1, 1) == -1 ||
        sem_init(&shm->can_read, 1, 0) == -1) {
        perror("sem_init");
        munmap(shm, sizeof(SharedWeatherData));
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Child process: analyser */
        int count = 0, sum = 0, max = 0;

        while (1) {
            sem_wait(&shm->can_read);
            if (shm->done) {
                sem_post(&shm->can_write);
                break;
            }
            int r = shm->reading;
            if (count == 0 || r > max)
                max = r;
            sum += r;
            count++;
            sem_post(&shm->can_write);
        }

        printf("analyser: %d readings, average %.1f, max %d\n", count,
               count ? (double)sum / count : 0.0, max);
        fflush(stdout);

        munmap(shm, sizeof(SharedWeatherData));
        close(shm_fd);
        _exit(0);
    }

    /* Parent process: sensor reader */
    int readings[] = {31, 33, 36, 34, 41, 38};
    for (int i = 0; i < 6; i++) {
        sem_wait(&shm->can_write);
        shm->reading = readings[i];
        shm->done = 0;
        sem_post(&shm->can_read);
    }

    /* Signal end of data to child */
    sem_wait(&shm->can_write);
    shm->done = 1;
    sem_post(&shm->can_read);

    waitpid(pid, NULL, 0);
    printf("reader: done\n");

    sem_destroy(&shm->can_write);
    sem_destroy(&shm->can_read);
    munmap(shm, sizeof(SharedWeatherData));
    close(shm_fd);
    shm_unlink(SHM_NAME);

    return 0;
}
