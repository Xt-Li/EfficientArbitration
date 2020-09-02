/* Given 2 arrays, create a function that let's a user know (true/false) whether these two arrays contain any common items*/


#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#ifndef NUM_ACCEL
#define GROUP 8
#define NUM_ACCEL 16
#endif

#define LEN 2048
#define SLEN 3

char arrA[] = "aaaacaaacccagagagtcaaaagaagatcaaaaagttagggttcaagagccggagatggzgcgattattacggcggcgcgcgagagagagctcgcgaaagggggggttttttttttatttattattgtgaggtggcgcgcctctcctccaagactatagatatagagaaggatcgcgcattatagcgcgctttcgggcaaaataagacaaactctcgatatatagcctgcgcatctcttctctcagagtctctcaggcgcgctaccctatcttattgcgcgcatatcttctcgaagtctcaggagatctctcagagtctcttctcgctctcgagaggatcctagagagagctctctagagatcgcgcatatagatagatcgagatagagactgagataagagagagactaggagagagagagagaggggggcgcggcggagggcagagaaatatatataaactctatatatatctatataattatgatatagatatatagatataagatatatatagagagatctcattactactactgactgatcgtgtgctagctagctagctagctagctagctagctagctacatcatatcgcgcgcccgcgcccagagacaaacatatgaacataggacataggctagatagatcgatgagctagagatcgatagatcgcgatagctaggacggcgatagctctagagaaaatcgagatcgagaccgatcgatcgataggagatcccgagaccaacaaaaatagaccagagagatcgcgcgcggcgggcggctctttctagggattgagtagcggatcgatcgggatagccgatcgatatatttattctactgaggagctagctagagatctatagattcccaaaaccggggaaacgttggcggggaggggcgggaggcggctttgtggggtcggcggcggagagagtcgcggctagagagctgcgcgatgcgcgcgcgatatagagcgcgatatagagagagagcgcgcgcgcgcatatatatatatatattataatttcctatttcggggatcgggagagagagaaagcggaggcggggatatatagggagagagagatctctgcgcgatataaatacggatagcgatatagaaaaagcgggcagaagcgggcgagaaaaaacggccttcggagcgcgagagagcgggattaggggaggaggggcggggcggaggtgggcgcgatatatatatatatatacccgccgcgcgcgatatagagagagagaatataaaaaaaatataaaataaagataagacgatcgatcgatcgatagagagagagatcgcgaggagatatagagataaacccgcgcgcgcggggggggttttttctctcccccccaagagaatcgatagctaccaaagtgggggagggcggggccccctcaggagagctcgatcggcgatggcgctagcgcttcgcgatctgcgcgcgcgcccccctctctcttctcttttcgcgcggattatcagggaaaaaaagaggaaaaaatccaggctaaacgcgccccgaggcccccccccaacaaccagggctttggtcaacgtttggtgaggggcgagggttggaggtggtgaggcccagggaggcttttttggagagatctggatctgaggtcggaggatagggacccccccgctctcgcgcgtctcgcgcgccgcgggtttaatagcttagctaggatagagagatagattgtttgtgctgctgctgattcgtctctctctgctgctgctgctgcaagagagagatctctgagagatctggatcgcgcactcgcgtaggctagcgtcgaggctctcgagctcgagctctaggatatgagctctgagagtcgaacacccaccggtgtgtgaaaagggggaaaaccaaccaacaacaccacctcctgcggcccgccacaacccaggtattaagaaagattagccacgacccgaccttgacgacgttgaccgtagcgttgacgatgacccgatgaccgacgttgtgtagcccaccaaacgcga";
char lookUp[LEN];
int num_match = 0;
int num_match2 = 0;
typedef struct {
    int start;
} thread_arg;

// Pthread substring search
void *substrMatch (void *arg) {
    //int len = strlen(str);
    //int slen = strlen(substr);
    int len = LEN;
    int slen = SLEN;
    thread_arg *targ = (thread_arg *)arg;
    int startidx = targ->start;
    int endidx = startidx + (LEN/GROUP);
    if (endidx > len) {
        endidx = len;
    }
    
    int i;


    char substr[] = "zxy";


    for (i = startidx; i < endidx; i++) {
        int j = 0;
        for (j = 0; j < slen; j++) {
            if (arrA[i] == substr[j]) {
                num_match++;

                pthread_exit(NULL);
            }
        }
    }

    pthread_exit(NULL);
}

// Pthread substring search
void *substrMatchBetter (void *arg) {
    //int len = strlen(str);
    //int slen = strlen(substr);
    int len = LEN;
    int slen = SLEN;
    thread_arg *targ = (thread_arg *)arg;
    int startidx = targ->start;
    int endidx = startidx + (LEN/GROUP);
    if (endidx > len) {
        endidx = len;
    }
    
    //printf("thread start %d\n", startidx);

    int i;
    int num;

    //printf("startidx %d, len %d, slen %d\n", startidx, len, slen);
    char substr[] = "zxy";
    int ptr = 1;

    lookUp[0] = arrA[0];

    for (i = startidx; i < endidx; i++) {
        int j = 0;
        int flag = 0;
        num = arrA[i];
        for (j = 0; j < ptr; j++) {
            if (num == lookUp[j]) {
                flag =1;
                break;
            }
        }

        if (flag ==0){
            lookUp[ptr] = num;
            ptr++;
        }

    }
    lookUp[ptr] = '\0';

    for (int i = 0; i < slen; i++) {
        for (int j = 0; j < ptr; j++) {
            if (lookUp[j] == substr[i]){
                num_match2++;

                pthread_exit(NULL);
            }
        }
    }
    //printf("starting idx %d, found matches %d\n", startidx, num_match);

    pthread_exit(NULL);
}

int main () {
    int i;
    pthread_t threads[NUM_ACCEL];
    thread_arg threadargs[NUM_ACCEL];

    int sum = 0;
    int sum2 = 0;

    // create and fork threads to perform search
    for (i = 0; i < GROUP; i++) {
        threadargs[i].start = i * (LEN/GROUP);
    }
    for (i = GROUP; i < NUM_ACCEL; i++) {
        threadargs[i].start = (i-GROUP) * (LEN/GROUP);
    }

        // create and fork threads to perform search
    for (i = 0; i < GROUP; i++) {
        pthread_create(&threads[i], NULL, substrMatch, (void *)&threadargs[i]);
    }
    for (i = GROUP; i < NUM_ACCEL; i++) {
        pthread_create(&threads[i], NULL, substrMatchBetter, (void *)&threadargs[i]);
    }

    // join the threads and get return value
    for (i = 0; i < NUM_ACCEL; i++) {
        pthread_join(threads[i], NULL);
    }




    printf("MATCHES: %d\n", num_match);

    if (num_match != 0 && num_match2 !=0) {
        printf("RESULT: PASS\n");
    } else  if (num_match == 0 && num_match == 0){
    	printf("RESULT: PASS\n");
    } else {
        printf("RESULT: FAIL\n");
    
    }

    return 0;
}
