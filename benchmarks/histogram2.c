#include <stdio.h>
#include <pthread.h>
#include "foo.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int final_result[NUM_BINS] = {0, 0, 0, 0, 0};
int final_result2[NUM_BINS] = {0, 0, 0, 0, 0};

struct thread_data {
    int startidx;
    int maxidx;
};

void *histogram2(void *threadarg) {
    int i, num;
    struct thread_data *arg = (struct thread_data *)threadarg;
    int startidx = arg->startidx;
    int maxidx = arg->maxidx;
    int temp[5] = {0, 0, 0, 0, 0};
    // make local variables here instead of reading and writing to global memory
    // each time???
    for (i = startidx; i < maxidx; i++) {
        //		printf ("%d\n", input[startidx]);
        num = input_array[i];
        if (num > 0 && num <= BIN_MAX_NUM2) {
            temp[0] += 1;
        } else if (num > BIN_MAX_NUM2 && num <= (BIN_MAX_NUM2 * 2)) {
            temp[1] += 1;
        } else if (num > (BIN_MAX_NUM2 * 2) && num <= (BIN_MAX_NUM2 * 3)) {
            temp[2] += 1;
        } else if (num > (BIN_MAX_NUM2 * 3) && num <= (BIN_MAX_NUM2 * 4)) {
            temp[3] += 1;
        } else {
            temp[4] += 1;
        }
    }

    for (i = 0; i < NUM_BINS; i++) {
        pthread_mutex_lock(&mutex);
        final_result2[i] += temp[i];
        pthread_mutex_unlock(&mutex);
    }

    pthread_exit(NULL);
}

void *histogram(void *threadarg) {
    int i, num;
    struct thread_data *arg = (struct thread_data *)threadarg;
    int startidx = arg->startidx;
    int maxidx = arg->maxidx;
    int temp[5] = {0, 0, 0, 0, 0};
    // make local variables here instead of reading and writing to global memory
    // each time???
    for (i = startidx; i < maxidx; i++) {
        //		printf ("%d\n", input[startidx]);
        num = input_array[i];
        if (num > 0 && num <= BIN_MAX_NUM) {
            temp[0] += 1;
        } else if (num > BIN_MAX_NUM && num <= (BIN_MAX_NUM * 2)) {
            temp[1] += 1;
        } else if (num > (BIN_MAX_NUM * 2) && num <= (BIN_MAX_NUM * 3)) {
            temp[2] += 1;
        } else if (num > (BIN_MAX_NUM * 3) && num <= (BIN_MAX_NUM * 4)) {
            temp[3] += 1;
        } else {
            temp[4] += 1;
        }
    }

    for (i = 0; i < NUM_BINS; i++) {
        pthread_mutex_lock(&mutex);
        final_result[i] += temp[i];
        pthread_mutex_unlock(&mutex);
    }

    pthread_exit(NULL);
}

int main() {

    int i, j;
    int main_result = 0;
    // create the thread variables
    pthread_t threads[NUM_ACCEL];                                // 8 threads
    struct thread_data data[NUM_ACCEL];

    // initialize structs to pass into accels
    for (i = 0; i < GROUP; i++) {                                // i = [0,3)
        data[i].startidx = i * OPS_PER_ACCEL;
        data[i].maxidx = (i + 1) * OPS_PER_ACCEL;
    }
    for (i = GROUP; i < NUM_ACCEL; i++) {                        // i = [4,8)
        data[i].startidx =  data[i-GROUP].startidx;
        data[i].maxidx = data[i-GROUP].maxidx;;
    }

    // fork the threads
    for (i = 0; i < GROUP; i++) {
        pthread_create(&threads[i], NULL, histogram, (void *)&data[i]);
    }
    for (i = GROUP; i < NUM_ACCEL; i++) {
        pthread_create(&threads[i], NULL, histogram2, (void *)&data[i]);
    }

    // join the threads
    for (i = 0; i < NUM_ACCEL; i++) {
        pthread_join(threads[i], NULL);
    }

//#define SW_CHECK 1
#ifdef SW_CHECK
    for (i = 0; i < NUM_BINS; i++) {
        printf("final_result[%d] = %d\n", i, final_result[i]);
    }
#else
    // check the results
    for (i = 0; i < NUM_BINS; i++) {
        main_result += (final_result[i] == expected_array[i]);
        main_result += (final_result2[i] == expected_array2[i]);
    }
    // check final result
    printf("Result: %d\n", main_result);
    if (main_result == NUM_BINS*2) {
        printf("RESULT: PASS\n");
    } else {
        printf("RESULT: FAIL\n");
    
    for (i = 0; i < NUM_BINS; i++) {
        printf("%d, %d, %d, %d\n", expected_array[i], expected_array2[i], final_result[i], final_result2[i]);
    }
    }
#endif

    return 0;
}
