#include <stdio.h>
#include <pthread.h>
#include "arrayvar.h"


int final_mean;
int final_sqrmean;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct thread_data {
    int startidx;
    int maxidx;
};

void *arraymean(void *threadarg) {
    int i, num;
    struct thread_data *arg = (struct thread_data *)threadarg;
    int startidx = arg->startidx;
    int maxidx = arg->maxidx;
    int temp = 0;

    for (i = startidx; i < maxidx; i++) {
        num = input_array[i];
        temp += num;    
    }
    temp = temp / (maxidx - startidx);
    pthread_mutex_lock(&mutex);
    final_mean += temp/GROUP;
    pthread_mutex_unlock(&mutex);


    pthread_exit(NULL);
}

void *arrayvar(void *threadarg) {
    int i, num;
    struct thread_data *arg = (struct thread_data *)threadarg;
    int startidx = arg->startidx;
    int maxidx = arg->maxidx;
    int temp = 0;

    for (i = startidx; i < maxidx; i++) {
        num = input_array[i];
        temp += num*num;
    }
    temp = temp / (maxidx - startidx);
    pthread_mutex_lock(&mutex);
    final_sqrmean += temp/ GROUP;
    pthread_mutex_unlock(&mutex);    



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
        pthread_create(&threads[i], NULL, arraymean, (void *)&data[i]);
    }
    for (i = GROUP; i < NUM_ACCEL; i++) {
        pthread_create(&threads[i], NULL, arrayvar, (void *)&data[i]);
    }

    // join the threads
    for (i = 0; i < NUM_ACCEL; i++) {
        pthread_join(threads[i], NULL);
    }

    int var;
    var = final_sqrmean - final_mean*final_mean;

    // check final result
    if (final_mean == expected_mean && var == expected_var) {
        printf("RESULT: PASS\n");
    } 
    else {
        printf("RESULT: FAIL\n");
        printf("%d, %d, %d, %d\n", expected_mean, expected_var, final_mean, var);
 
    }


    return 0;
}

