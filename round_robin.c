#include <stdio.h>

#define N 4
int main(void)
{
    const char *name[N] = {"P1", "P2", "P3", "P4"};
    int arrival[N] = {0, 1, 2, 3};
    int burst[N] = {8, 4, 9, 5};
    int quantum = 3;

    int left[N], finish[N], queue[64];
    int head = 0, tail = 0, added[N] = {0};
    for (int i = 0; i < N; i++) left[i] = burst[i];
    int time = 0, done = 0;
    queue[tail++] = 0;
    added[0] = 1;
    printf("Gantt:");
    while (done < N) {
        int p = queue[head++];
        // TODO 1: Run p for min(quantum, left[p]) time units:
        // print " P(time-newtime)", advance time, reduce left[p];
        // add every process that has now arrived and is not yet
        // added to the queue; then put p back at the end of the
        // queue if it still has time left, otherwise record
        // finish[p] and count it as done
        int run = (left[p] < quantum) ? left[p] : quantum;
        int newtime = time + run;
        printf(" %s(%d-%d)", name[p], time, newtime);
        time = newtime;
        left[p] -= run;
        for (int i = 0; i < N; i++) {
            if (!added[i] && arrival[i] <= time) {
                queue[tail++] = i;
                added[i] = 1;
            }
        }
        if (left[p] > 0) {
            queue[tail++] = p;
        } else {
            finish[p] = time;
            done++;
        }
    }
    double tw = 0, tt = 0;
    printf("\nProcess TAT WT\n");
    for (int i = 0; i < N; i++) {
        int tat = finish[i] - arrival[i], wt = tat - burst[i];
        tt += tat;
        tw += wt;
        printf("%-7s %4d %3d\n", name[i], tat, wt);
    }
    printf("Average TAT = %.2f, average WT = %.2f\n", tt / N, tw / N);
    return 0;
}
