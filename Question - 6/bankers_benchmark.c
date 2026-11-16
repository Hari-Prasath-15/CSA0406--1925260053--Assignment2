#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    int num_p;
    int num_r;
    int *avail;
    int *maxm;
    int *alloc;
    int *need;
} BankerState;

static int *cell(int *mat, int num_r, int p, int r)
{
    return &mat[p * num_r + r];
}

static int ccell(const int *mat, int num_r, int p, int r)
{
    return mat[p * num_r + r];
}

static BankerState *create_state(int num_p, int num_r)
{
    BankerState *st = malloc(sizeof(BankerState));
    if (!st) return NULL;
    st->num_p = num_p;
    st->num_r = num_r;
    st->avail = calloc((size_t)num_r, sizeof(int));
    st->maxm  = calloc((size_t)(num_p * num_r), sizeof(int));
    st->alloc = calloc((size_t)(num_p * num_r), sizeof(int));
    st->need  = calloc((size_t)(num_p * num_r), sizeof(int));
    if (!st->avail || !st->maxm || !st->alloc || !st->need) {
        free(st->avail); free(st->maxm); free(st->alloc); free(st->need); free(st);
        return NULL;
    }
    return st;
}

static void free_state(BankerState *st)
{
    if (!st) return;
    free(st->avail);
    free(st->maxm);
    free(st->alloc);
    free(st->need);
    free(st);
}

static void compute_need(BankerState *st)
{
    for (int p = 0; p < st->num_p; p++) {
        for (int r = 0; r < st->num_r; r++) {
            int n = ccell(st->maxm, st->num_r, p, r) - ccell(st->alloc, st->num_r, p, r);
            *cell(st->need, st->num_r, p, r) = (n >= 0) ? n : 0;
        }
    }
}

static int is_safe(const BankerState *st, int *seq, int *work_buf, int *finish_buf)
{
    int n = st->num_p;
    int m = st->num_r;
    memcpy(work_buf, st->avail, (size_t)m * sizeof(int));
    memset(finish_buf, 0, (size_t)n * sizeof(int));

    int count = 0;
    while (count < n) {
        int found = 0;
        for (int p = 0; p < n; p++) {
            if (finish_buf[p]) continue;
            int ok = 1;
            for (int r = 0; r < m; r++) {
                if (ccell(st->need, m, p, r) > work_buf[r]) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                for (int r = 0; r < m; r++) {
                    work_buf[r] += ccell(st->alloc, m, p, r);
                }
                finish_buf[p] = 1;
                if (seq) seq[count] = p;
                count++;
                found = 1;
            }
        }
        if (!found) return 0;
    }
    return 1;
}

static void request_resources(BankerState *st, int p, const int *req)
{
    if (p < 0 || p >= st->num_p) {
        printf("Invalid process index: %d\n", p);
        return;
    }
    for (int r = 0; r < st->num_r; r++) {
        if (req[r] < 0) {
            printf("P%d request: invalid negative request\n", p);
            return;
        }
        if (req[r] > ccell(st->need, st->num_r, p, r)) {
            printf("P%d request: error (exceeds maximum claim)\n", p);
            return;
        }
        if (req[r] > st->avail[r]) {
            printf("P%d request: must wait (not available)\n", p);
            return;
        }
    }

    for (int r = 0; r < st->num_r; r++) {
        st->avail[r] -= req[r];
        *cell(st->alloc, st->num_r, p, r) += req[r];
        *cell(st->need, st->num_r, p, r) -= req[r];
    }

    int *seq = malloc((size_t)st->num_p * sizeof(int));
    int *work = malloc((size_t)st->num_r * sizeof(int));
    int *fin  = malloc((size_t)st->num_p * sizeof(int));

    if (is_safe(st, seq, work, fin)) {
        printf("P%d request: granted, safe sequence", p);
        for (int i = 0; i < st->num_p; i++) printf(" P%d", seq[i]);
        printf("\n");
    } else {
        for (int r = 0; r < st->num_r; r++) {
            st->avail[r] += req[r];
            *cell(st->alloc, st->num_r, p, r) -= req[r];
            *cell(st->need, st->num_r, p, r) += req[r];
        }
        printf("P%d request: denied (unsafe)\n", p);
    }

    free(seq);
    free(work);
    free(fin);
}

static void verify_assignment_example(void)
{
    const int P = 5, R = 3;
    BankerState *st = create_state(P, R);
    int init_avail[3] = {3, 3, 2};
    int init_max[5][3] = {{7,5,3}, {3,2,2}, {9,0,2}, {2,2,2}, {4,3,3}};
    int init_alloc[5][3] = {{0,1,0}, {2,0,0}, {3,0,2}, {2,1,1}, {0,0,2}};

    memcpy(st->avail, init_avail, sizeof(init_avail));
    for (int p = 0; p < P; p++) {
        for (int r = 0; r < R; r++) {
            *cell(st->maxm, R, p, r) = init_max[p][r];
            *cell(st->alloc, R, p, r) = init_alloc[p][r];
        }
    }
    compute_need(st);

    int seq[5], work[3], fin[5];
    printf("=== Correctness Verification (Assignment Example: P=5, R=3) ===\n");
    if (is_safe(st, seq, work, fin)) {
        printf("initial state safe:");
        for (int i = 0; i < P; i++) printf(" P%d", seq[i]);
        printf("\n");
    }
    int r1[3] = {1, 0, 2};
    int r4[3] = {3, 3, 0};
    int r0[3] = {0, 2, 0};
    request_resources(st, 1, r1);
    request_resources(st, 4, r4);
    request_resources(st, 0, r0);
    free_state(st);
}

static double elapsed_us(struct timespec s, struct timespec e)
{
    return (double)(e.tv_sec - s.tv_sec) * 1e6 + (double)(e.tv_nsec - s.tv_nsec) / 1e3;
}

static void benchmark_n(int n_proc, int m_res, int repeats)
{
    BankerState *st = create_state(n_proc, m_res);

    /* Construct a valid reverse-order safe state so is_safe() exercises O(m * n^2) */
    for (int r = 0; r < m_res; r++) {
        st->avail[r] = 1;
    }
    for (int p = 0; p < n_proc; p++) {
        for (int r = 0; r < m_res; r++) {
            *cell(st->alloc, m_res, p, r) = 1;
            *cell(st->maxm, m_res, p, r) = 1 + (n_proc - p);
        }
    }
    compute_need(st);

    int *seq  = malloc((size_t)n_proc * sizeof(int));
    int *work = malloc((size_t)m_res * sizeof(int));
    int *fin  = malloc((size_t)n_proc * sizeof(int));

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int safe = 0;
    for (int i = 0; i < repeats; i++) {
        safe = is_safe(st, seq, work, fin);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    double avg_us = elapsed_us(start, end) / (double)repeats;
    printf("%6d    | %8d | %8d | %12.3f us | %10.4f ms | %s\n",
           n_proc, m_res, repeats, avg_us, avg_us / 1000.0, safe ? "SAFE" : "UNSAFE");

    free(seq);
    free(work);
    free(fin);
    free_state(st);
}

int main(void)
{
    verify_assignment_example();

    printf("\n=== Banker's Safety Algorithm Growth Benchmark (m = 4 resource types) ===\n");
    printf("Processes | Res (m)  | Repeats  | Avg Time (us)   | Avg Time (ms) | State\n");
    printf("--------------------------------------------------------------------------\n");
    benchmark_n(10, 4, 1000);
    benchmark_n(100, 4, 200);
    benchmark_n(1000, 4, 20);

    return 0;
}
