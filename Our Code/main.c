#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h> 

// declarations of functions from other .c files
double** allocate_matrix(int n);
void free_matrix(double **mat, int n);
void* worker_thread(void* arg);
double gaussian_determinant(double **matrix, int n);

// declarations to access globals defined in cramer_threads.c
extern int global_n;
extern double **global_coeff;
extern double *global_constants;
extern double *global_results;
extern int total_tasks;
extern int current_task;

// worker function strictly for the #Threads = N+1 test
void* direct_worker(void* arg) {
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
    char *user_name = getenv("USER");

    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cpus < 1) num_cpus = 4;
    
    printf("\nDetected User: %s\n", user_name);
    printf("Detected CPUs: %d\n\n", num_cpus);

    total_tasks = global_n + 1;
    global_coeff = allocate_matrix(global_n);
    global_constants = (double *)malloc(global_n * sizeof(double));
    global_results = (double *)malloc(total_tasks * sizeof(double));

    srandom(time(NULL));
    FILE *eq_file = fopen("equation.txt", "a");
    fprintf(eq_file, "Program run by: %s\n", user_name);
    fprintf(eq_file, "Hardware Threads Used: %d\n\n", num_cpus);
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

  
    // TEST 1: SEQUENTIAL (1 Thread)
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

    // TEST 2: THREAD POOL (#Threads = #CPUs)
    current_task = 0; 
    pthread_t cpu_threads[num_cpus];

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < num_cpus; i++) {
        pthread_create(&cpu_threads[i], NULL, worker_thread, NULL);
    }
    for (int i = 0; i < num_cpus; i++) {
        pthread_join(cpu_threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    cpu_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // TEST 3: ONE THREAD PER EQUATION (#Threads = N)
    pthread_t n_threads[total_tasks];
    int thread_ids[total_tasks];

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < total_tasks; i++) {
        thread_ids[i] = i; // Assign specific ID so it skips the queue
        pthread_create(&n_threads[i], NULL, direct_worker, &thread_ids[i]);
    }
    for (int i = 0; i < total_tasks; i++) {
        pthread_join(n_threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    n_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // OUTPUT HANDLING
    FILE *sol_file = fopen("solution.txt", "a");
    double D = global_results[0];

    fprintf(sol_file, "Program run by: %s\n", user_name);
    fprintf(sol_file, "Hardware Threads Used: %d\n\n", num_cpus);
    
    // comparison summary to the terminal
    printf("\n--- Performance Comparison (N = %d) ---\n", global_n);
    printf("1. Sequential Time:            %.4f seconds\n", seq_time);
    printf("2. Thread Pool (#CPUs = %d):    %.4f seconds\n", num_cpus, cpu_time);
    printf("3. Thread Per Eq (#Threads = %d): %.4f seconds\n", total_tasks, n_time);

    // comparison summary to the file
    fprintf(sol_file, "--- Performance Comparison (N = %d) ---\n", global_n);
    fprintf(sol_file, "1. Sequential Time:            %.4f seconds\n", seq_time);
    fprintf(sol_file, "2. Thread Pool (#CPUs = %d):    %.4f seconds\n", num_cpus, cpu_time);
    fprintf(sol_file, "3. Thread Per Eq (#Threads = %d): %.4f seconds\n\n", total_tasks, n_time);

    if (D == 0) {
        fprintf(sol_file, "Determinant is 0. No unique solution.\n");
        if (global_n < 10) printf("Determinant is 0. No unique solution.\n");
    } else {
        if (global_n < 10) {
            fprintf(sol_file, "\n--- Solutions ---\n");
            printf("\n--- Solutions ---\n");
            fprintf(sol_file, "Determinant : %.4f ",D);
            printf("\nDeterminant : %.4f \n",D);
            for (int i = 1; i <= global_n; i++) {
                fprintf(sol_file, "x%d = %.4f\n", i, global_results[i] / D);
                printf("x%d = %.4f\n", i, global_results[i] / D);
            }
        } else {
            fprintf(sol_file, "[Solutions calculated successfully - Values omitted for N >= 10]\n");
        }
    }
    printf("\n[Success] Calculated %d determinants of size %dx%d.\n", total_tasks, global_n, global_n);
    fprintf(sol_file, "\n================================================\n\n");
    fclose(sol_file);
    free_matrix(global_coeff, global_n);
    free(global_constants);
    free(global_results);

    return 0;
}