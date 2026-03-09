#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <stdatomic.h> // The secret weapon: Lock-free atomic operations

// Global Variables
int global_n;
int total_tasks;
double **global_coeff;
double *global_constants;
double *global_results;

// Lock-free atomic task counter (Replaces the slow mutex lock)
_Atomic int current_task = 0;

// --- Fast Inline Memory Management ---
static inline double** allocate_matrix(int n) {
    double **mat = (double **)malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++) mat[i] = (double *)malloc(n * sizeof(double));
    return mat;
}

static inline void free_matrix(double **mat, int n) {
    for (int i = 0; i < n; i++) free(mat[i]);
    free(mat);
}

// O(n^3) Gaussian Elimination
double gaussian_determinant(double **matrix, int n) {
    double det = 1.0;
    for (int i = 0; i < n; i++) {
        int pivot = i;
        for (int j = i + 1; j < n; j++) {
            if (fabs(matrix[j][i]) > fabs(matrix[pivot][i])) pivot = j;
        }
        if (pivot != i) {
            double *temp = matrix[i];
            matrix[i] = matrix[pivot];
            matrix[pivot] = temp;
            det = -det;
        }
        if (matrix[i][i] == 0.0) return 0.0;
        det *= matrix[i][i];
        for (int j = i + 1; j < n; j++) {
            double factor = matrix[j][i] / matrix[i][i];
            for (int k = i + 1; k < n; k++) matrix[j][k] -= factor * matrix[i][k];
        }
    }
    return det;
}

// --- AI Worker 1: Lock-Free Thread Pool (#Threads = #CPUs) ---
void* ai_pool_worker(void* arg) {
    while (1) {
        // Atomic fetch-and-add: Instantaneous, zero locking required!
        int task_id = atomic_fetch_add(&current_task, 1);

        if (task_id >= total_tasks) break;

        double **temp_mat = allocate_matrix(global_n);
        for (int r = 0; r < global_n; r++) 
            for (int c = 0; c < global_n; c++) temp_mat[r][c] = global_coeff[r][c];

        if (task_id > 0) 
            for (int r = 0; r < global_n; r++) temp_mat[r][task_id - 1] = global_constants[r];

        global_results[task_id] = gaussian_determinant(temp_mat, global_n);
        free_matrix(temp_mat, global_n);
    }
    return NULL;
}

// --- AI Worker 2: One Thread Per Row (#Threads = N) ---
void* ai_direct_worker(void* arg) {
    int task_id = *(int*)arg;

    double **temp_mat = allocate_matrix(global_n);
    for (int r = 0; r < global_n; r++) {
        for (int c = 0; c < global_n; c++) temp_mat[r][c] = global_coeff[r][c];
    }

    if (task_id > 0) {
        for (int r = 0; r < global_n; r++) temp_mat[r][task_id - 1] = global_constants[r];
    }

    global_results[task_id] = gaussian_determinant(temp_mat, global_n);
    free_matrix(temp_mat, global_n);
    
    pthread_exit(NULL);
}

int main() {
    printf("Enter number of equations (n): ");
    if (scanf("%d", &global_n) != 1) return 1;

    // --- Dynamic System Info Extraction ---
    char *user_name = getenv("USER");
    if (user_name == NULL) user_name = "Mandrita Maji"; // Personalized fallback

    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cpus < 1) num_cpus = 4; 
    
    printf("\nDetected User: %s\n", user_name);
    printf("Detected Hardware Threads: %d\n\n", num_cpus);
    // --------------------------------------

    total_tasks = global_n + 1;
    global_coeff = allocate_matrix(global_n);
    global_constants = (double *)malloc(global_n * sizeof(double));
    global_results = (double *)malloc(total_tasks * sizeof(double));

    srandom(time(NULL));
    FILE *eq_file = fopen("equation.txt", "a");
    
    for (int i = 0; i < global_n; i++) {
        for (int j = 0; j < global_n; j++) {
            global_coeff[i][j] = (random() % 20) - 10; 
            if (global_n <= 10) fprintf(eq_file, "%5.1fx%d ", global_coeff[i][j], j+1);
        }
        global_constants[i] = (random() % 20) - 10;
        if (global_n <= 10) fprintf(eq_file, "= %5.1f\n", global_constants[i]);
    }
    fclose(eq_file);

    struct timespec start, end;
    double seq_time, cpu_time, n_time;

    // ==========================================
    // TEST 1: SEQUENTIAL (1 Thread)
    // ==========================================
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int task_id = 0; task_id < total_tasks; task_id++) {
        double **temp_mat = allocate_matrix(global_n);
        for (int r = 0; r < global_n; r++) 
            for (int c = 0; c < global_n; c++) temp_mat[r][c] = global_coeff[r][c];

        if (task_id > 0) 
            for (int r = 0; r < global_n; r++) temp_mat[r][task_id - 1] = global_constants[r];

        global_results[task_id] = gaussian_determinant(temp_mat, global_n);
        free_matrix(temp_mat, global_n);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    seq_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // ==========================================
    // TEST 2: THREAD POOL (#Threads = #CPUs) [LOCK-FREE]
    // ==========================================
    current_task = 0; // Reset atomic queue counter
    pthread_t cpu_threads[num_cpus];

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < num_cpus; i++) {
        pthread_create(&cpu_threads[i], NULL, ai_pool_worker, NULL);
    }
    for (int i = 0; i < num_cpus; i++) {
        pthread_join(cpu_threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    cpu_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // ==========================================
    // TEST 3: ONE THREAD PER EQUATION (#Threads = N)
    // ==========================================
    pthread_t *n_threads = malloc(total_tasks * sizeof(pthread_t));
    int *thread_ids = malloc(total_tasks * sizeof(int));

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < total_tasks; i++) {
        thread_ids[i] = i; 
        pthread_create(&n_threads[i], NULL, ai_direct_worker, &thread_ids[i]);
    }
    for (int i = 0; i < total_tasks; i++) {
        pthread_join(n_threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    n_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    free(n_threads);
    free(thread_ids);

    // ==========================================
    // OUTPUT HANDLING (Append Mode)
    // ==========================================
    FILE *sol_file = fopen("solution.txt", "a"); // APPEND MODE
    double D = global_results[0];

    fprintf(sol_file, "\n========================================\n");
    fprintf(sol_file, "[AI Lock-Free Implementation]\n");
    fprintf(sol_file, "Program run by: %s\n", user_name);
    fprintf(sol_file, "Hardware Threads Used: %d\n\n", num_cpus);
    
    printf("\n--- Performance Comparison (N = %d) ---\n", global_n);
    printf("1. Sequential Time:            %.4f seconds\n", seq_time);
    printf("2. Lock-Free Pool (#CPUs=%d):   %.4f seconds\n", num_cpus, cpu_time);
    printf("3. Thread Per Eq (#Threads=%d): %.4f seconds\n", total_tasks, n_time);

    fprintf(sol_file, "--- Performance Comparison (N = %d) ---\n", global_n);
    fprintf(sol_file, "1. Sequential Time:            %.4f seconds\n", seq_time);
    fprintf(sol_file, "2. Lock-Free Pool (#CPUs=%d):   %.4f seconds\n", num_cpus, cpu_time);
    fprintf(sol_file, "3. Thread Per Eq (#Threads=%d): %.4f seconds\n\n", total_tasks, n_time);

    if (D == 0) {
        fprintf(sol_file, "Determinant is 0. No unique solution.\n");
        if (global_n <= 10) printf("Determinant is 0. No unique solution.\n");
    } else {
        if (global_n <= 10) {
            fprintf(sol_file, "--- Solutions ---\n");
            printf("\n--- Solutions ---\n");
            for (int i = 1; i <= global_n; i++) {
                fprintf(sol_file, "x%d = %.4f\n", i, global_results[i] / D);
                printf("x%d = %.4f\n", i, global_results[i] / D);
            }
        } else {
            fprintf(sol_file, "[Solutions calculated successfully - Values omitted for N > 10]\n");
        }
    }

    printf("\n[Success] Calculated %d determinants of size %dx%d.\n", total_tasks, global_n, global_n);
    
    fclose(sol_file);
    free_matrix(global_coeff, global_n);
    free(global_constants);
    free(global_results);

    return 0;
}