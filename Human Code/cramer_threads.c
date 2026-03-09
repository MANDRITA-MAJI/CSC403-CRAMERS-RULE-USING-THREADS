#include <stdlib.h>
#include <pthread.h>

// Forward declarations of functions from other .c files
double** allocate_matrix(int n);
void free_matrix(double **mat, int n);
double gaussian_determinant(double **matrix, int n);

// Global Variables definition
int global_n;
double **global_coeff;
double *global_constants;
double *global_results;

int current_task = 0;
int total_tasks;
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;

void* worker_thread(void* arg) {
    while (1) {
        pthread_mutex_lock(&queue_lock);
        int task_id = current_task++;
        pthread_mutex_unlock(&queue_lock);

        if (task_id >= total_tasks) break;

        double **temp_mat = allocate_matrix(global_n);
        for (int r = 0; r < global_n; r++) {
            for (int c = 0; c < global_n; c++) {
                temp_mat[r][c] = global_coeff[r][c];
            }
        }

        if (task_id > 0) {
            for (int r = 0; r < global_n; r++) {
                temp_mat[r][task_id - 1] = global_constants[r];
            }
        }

        global_results[task_id] = gaussian_determinant(temp_mat, global_n);
        free_matrix(temp_mat, global_n);
    }
    return NULL;
}