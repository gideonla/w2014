#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>

#include "../src/hashtables.h"
#include "../src/lockedtable.h"
#include "../src/lfctable.h"
#include "../src/splittable.h"
#include "../src/lockfree_lists.h"

int *key_helper(int N, int limit);

void addcontain1(char *tabtype, int N, int T);
void addcontain2(char *tabtype, int N, int T);
void alltogether(char *tabtype, int N, int Tc, int Ta, int Tr);
void indistinct_add(char *tabtype, int N, int T, int R);
void resize_contains(int N, int T);
void ind_init (int N, int T);
void testtab(int N, int T);

int main(int argc, char *argv[]) {
    char *test_type = argv[1];
    if (!strcmp(test_type, "addcontain1")) {
        addcontain1(argv[2], atoi(argv[3]), atoi(argv[4]));
        return 0;
    }

    if (!strcmp(test_type, "addcontain2")) {
        addcontain2(argv[2], atoi(argv[3]), atoi(argv[4]));
        return 0;
    }

    if (!strcmp(test_type, "alltogether")) {
        alltogether(argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
        return 0;
    }

    if (!strcmp(test_type, "indistinct_add")) {
        indistinct_add(argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
        return 0;
    }

    if (!strcmp(test_type, "resize_contains")) {
        resize_contains(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }

    if (!strcmp(test_type, "ind_init")) {
        ind_init(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }

    if (!strcmp(test_type, "testtab")) {
        testtab(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }

    printf("Invalid test\n");
    return 1;
}


// Parallel tests start here --------------------------------------
// (they all leak memory like crazy because the OS cleans up after me)

// Makes an array of N distinct random values with upper bound limit. Uses
// the (presumably correct) lockfree list to guarantee distinctness. 
int *key_helper(int N, int limit) {
    int *keys = malloc (N * sizeof(int));
    unsigned x = time(0);
    srand (x);
    struct lockfree_list *l = create_lockfree_lists(1);

    for (int i = 0; i < N; i++) {
        keys[i] = rand() % limit;
        while (!lf_add (l, keys[i], NULL, NULL)) {
            keys[i] = rand() % limit;
        }
    }
    destroy_lockfree_lists (l, 1);
    return keys;
}

// Parallel worker -- takes a struct with a array of keys, 
// a array of operations (0=contains, 1=add, 2=remove), a begin
// and an end (both inclusive), and performs
// the requested operations, then stores the results in the
// preallocated results array 
typedef struct worker_data {
    struct hashtable *tab;
    int *keys;
    int *ops;
    int begin;
    int end;
    bool *results;
} pdata;

void *worker(void *_data) {
    pdata *data = (pdata *) _data;
    struct hashtable *tab = data->tab;
    int *keys = data->keys;
        
    for (int i = data->begin; i <= data->end; i++) {
        switch (data->ops[i]) {
            case 0:
                data->results[i] = tab->contains(tab, keys[i]);
                break;
            case 1:
                data->results[i] = tab->add(tab, keys[i], (Packet_t *) 0x10);
                break;
            default:
                data->results[i] = tab->remove(tab, keys[i]);
                break;
        }
    }
    return NULL;
}

// Adds in N distinct keys to the list using T threads, then uses T threads
// to test that they're in the list.
void addcontain1(char *tabtype, int N, int T) {
    int *keys = key_helper(N, INT_MAX);
    bool *results = malloc(N * sizeof(bool));
    int *ops = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) {
        ops[i] = 1;
    }

    if (T > N)
        T = N;

    struct hashtable *tab = create_ht(tabtype, T);

    pdata *datas = malloc (T * sizeof(pdata));
    pthread_t *threads = malloc(T * sizeof(pthread_t));
    for (int i = 0; i < T; i++) {
        datas[i].tab = tab;
        datas[i].keys = keys;
        datas[i].ops = ops;
        datas[i].begin = i * N / T;
        datas[i].end = (i + 1) * N / T - 1;
        datas[i].results = results;
    }

    for (int i = 0; i < T; i++) 
        pthread_create(&threads[i], NULL, worker, (void *)(datas + i));

    for (int i = 0; i < T; i++) 
        pthread_join(threads[i], NULL);

    for (int i = 0; i < N; i++) {
        if (!results[i]) {
            printf("FAIL: bad return on parallel adds\n");
            return;
        }
        ops[i] = 0; //convert it over to a containment operation
    }

    for (int i = 0; i < T; i++) 
        pthread_create(&threads[i], NULL, worker, (void *)(datas + i));

    for (int i = 0; i < T; i++) 
        pthread_join(threads[i], NULL);

    for (int i = 0; i < N; i++) {
        if (!results[i]) {
            printf("FAIL: bad return on parallel contains\n");
            return;
        }
    }
    return;
}

// Tests concurrent adds and contains -- Adds N random keys, 
// then adds an additional distinct N while testing containment
// on the first N. All results should be successful. Assume T divisible
// by 2.
void addcontain2(char *tabtype, int N, int T) {
    if (T < 2)
        return;

    if (T > N)
        T = N;
    int *keys = key_helper(2*N, INT_MAX);
    bool *results = malloc(N * sizeof(bool));
    int *ops = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) 
        ops[i] = 1;

    struct hashtable *tab = create_ht(tabtype, T);

    pdata *datas = malloc (T * sizeof(pdata));
    pthread_t *threads = malloc(T * sizeof(pthread_t));
    for (int i = 0; i < T; i++) {
        datas[i].tab = tab;
        datas[i].keys = keys;
        datas[i].ops = ops;
        datas[i].begin = i * N / T;
        datas[i].end = (i + 1) * N / T - 1;
        datas[i].results = results;
    }

    for (int i = 0; i < T; i++) 
        pthread_create(&threads[i], NULL, worker, (void *)(datas + i));

    for (int i = 0; i < T; i++) 
        pthread_join(threads[i], NULL);

    // Now separate the threads into two groups
    int half_T = T / 2;    
    bool *results_a = malloc(N * sizeof(bool));
    bool *results_c = malloc(N * sizeof(bool));
    int *adds = ops;
    int *cts = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++)
        cts[i] = 0;

    for (int i = 0; i < T; i++) {
        datas[i].tab = tab;
        datas[i].begin = (i/2) * N / half_T;
        datas[i].end = ((i/2) + 1) * N / half_T - 1;
        if (i % 2 == 0) {
            datas[i].results = results_a;
            datas[i].keys = keys + N;
            datas[i].ops = adds;
        } else {
            datas[i].results = results_c;
            datas[i].keys = keys;
            datas[i].ops = cts;
        }
    }

    for (int i = 0; i < T; i++) 
        pthread_create(&threads[i], NULL, worker, (void *)(datas + i));

    for (int i = 0; i < T; i++) 
        pthread_join(threads[i], NULL);

    for (int i = 0; i < N; i++) {
        if (!results_c[i]) {
            printf("FAIL: bad return on parallel contains\n");
            return;
        }
    }
}

// Tests concurrent adds, contains, and removes. Add in 2N keys, then call
// adds on N more, removes on the second N, and contains on the first N.
// Everything should succeed. Proceed to test containment on all keys. 
// Should succeed on first N, fail on second block, succeed on third block.
void alltogether(char *tabtype, int N, int Tc, int Ta, int Tr) {
    int T = Tc + Ta + Tr;

    int *keys = key_helper(3*N, INT_MAX);

    bool *results = malloc(2*N * sizeof(bool));
    int *ops = malloc(2*N * sizeof(int));
    for (int i = 0; i < 2*N; i++) 
        ops[i] = 1;

    struct hashtable *tab = create_ht(tabtype, T);

    pdata *datas = malloc (T * sizeof(pdata));
    pthread_t *threads = malloc(T * sizeof(pthread_t));
    for (int i = 0; i < T; i++) {
        datas[i].tab = tab;
        datas[i].keys = keys;
        datas[i].ops = ops;
        datas[i].begin = i * 2 * N / T;
        datas[i].end = (i + 1) * 2 * N / T - 1;
        datas[i].results = results;
    }

    for (int i = 0; i < T; i++) 
        pthread_create(&threads[i], NULL, worker, (void *)(datas + i));

    for (int i = 0; i < T; i++) 
        pthread_join(threads[i], NULL);

    bool *results_a = malloc(N * sizeof(bool));
    bool *results_r = malloc(N * sizeof(bool));
    bool *results_c = malloc(N * sizeof(bool));
    int *adds = ops;
    int *cts = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++)
        cts[i] = 0;

    int *rms = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++)
        rms[i] = 2;

    for (int i = 0; i < Tc; i++) {
        datas[i].tab = tab;
        datas[i].keys = keys;
        datas[i].ops = cts;
        datas[i].begin = i * N / Tc;
        datas[i].end = (i + 1) * N / Tc - 1;
        datas[i].results = results_c;
    }

    for (int i = Tc; i < Tc + Tr; i++) {
        datas[i].tab = tab;
        datas[i].keys = keys + N;
        datas[i].ops = rms;
        datas[i].begin = (i-Tc) * N / Tr;
        datas[i].end = (i-Tc + 1) * N / Tr - 1;
        datas[i].results = results_r;
    }

    for (int i = Tc + Tr; i < T; i++) {
        datas[i].tab = tab;
        datas[i].keys = keys + 2*N;
        datas[i].ops = adds;
        datas[i].begin = (i-Tc-Tr) * N / Ta;
        datas[i].end = (i-Tc-Tr + 1) * N / Ta - 1;
        datas[i].results = results_a;
    }

    for (int i = 0; i < T; i++) 
        pthread_create(&threads[i], NULL, worker, (void *)(datas + i));

    for (int i = 0; i < T; i++) 
        pthread_join(threads[i], NULL);

    for (int i = 0; i < N; i++) {
        if (!results_c[i] && Tc) {
            printf("FAIL: bad return on parallel contains\n");
            return;
        }
        if (!results_r[i] && Tr) {
            printf("FAIL: bad return on parallel removes\n");
            return;
        }
        if (!results_a[i] && Ta) {
            printf("FAIL: bad return on parallel adds\n");
            return;
        }
    }

    results = malloc(3*N*sizeof(bool));
    ops = malloc(3*N*sizeof(int));
    for(int i = 0; i < 3*N; i++)
        ops[i] = 0;

    for (int i = 0; i < T; i++) {
        datas[i].tab = tab;
        datas[i].keys = keys;
        datas[i].ops = ops;
        datas[i].begin = i * 3 * N / T;
        datas[i].end = (i + 1) * 3 * N / T - 1;
        datas[i].results = results;
    }

    for (int i = 0; i < T; i++) 
        pthread_create(&threads[i], NULL, worker, (void *)(datas + i));

    for (int i = 0; i < T; i++) 
        pthread_join(threads[i], NULL);

    for (int i = 0; i < 3*N; i++) {
        if (i < N && !results[i]) {
            printf("FAIL: bad return on block1\n");
            return;
        }
        else if (i >= N && i < 2*N && results[i]) {
            printf("FAIL: bad return on block2\n");
            return;
        }
        else if (i >= 2*N && !results[i]) {
            printf("FAIL: bad return on block3\n");
            return;
        }
    }
}

// Tests to make sure that indistinct adds will fail. Attempts to insert R distinct values
// a total of N times, and asserts that there are only R successes.
void indistinct_add(char *tabtype, int N, int T, int R) {
    int *keys_low = key_helper(R, INT_MAX);
    int *keys = malloc(N * sizeof(int));

    bool *results = malloc(N * sizeof(bool));
    int *ops = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) {
        ops[i] = 1;
        keys[i] = keys_low[i % R];
    }

    struct hashtable *tab = create_ht(tabtype, T);

    pdata *datas = malloc (T * sizeof(pdata));
    pthread_t *threads = malloc(T * sizeof(pthread_t));
    for (int i = 0; i < T; i++) {
        datas[i].tab = tab;
        datas[i].keys = keys;
        datas[i].ops = ops;
        datas[i].begin = i * N / T;
        datas[i].end = (i + 1) * N / T - 1;
        datas[i].results = results;
    }

    for (int i = 0; i < T; i++) 
        pthread_create(&threads[i], NULL, worker, (void *)(datas + i));

    for (int i = 0; i < T; i++) 
        pthread_join(threads[i], NULL);

    int succ_ctr = 0;
    for (int i = 0; i < N; i++) {
        if (results[i])
            succ_ctr++;
    }
    if (succ_ctr != R) {
        printf("FAIL: %d successes instead of %d\n", succ_ctr, R);
    }
    return;
}

// Following are the table-specific white-box tests. 

// Tests the linearizability of resize with contains for
// the lock-free contains table. Adds in N elements, then launches
// T-1 workers to call contains on these elements repeatedly.
// Meanwhile, main thread calls resize 10 times. Worker threads
// will signal failure if they ever fail a containment call.
struct resize_test_data {
    struct hashtable *tab;
    int *keys;
    int len;
    volatile bool stop;
    volatile bool error;
};

void *resize_test_worker(void *_data) {
    struct resize_test_data *data = (struct resize_test_data *) _data;
    struct hashtable *tab = data->tab;

    while (!data->stop) {
        for (int i = 0; i < data->len; i++) {
            bool results = tab->contains(tab, data->keys[i]);
            if (!results) {
                data->stop = true;
                data->error = true;
                break;
            }
        }
    }
    return NULL;
}
void lf_inc_size(struct lfc_table *);
void resize_contains(int N, int T) {
    // the setup portion of the test
    int *keys = key_helper(N, INT_MAX);
    bool *results = malloc(N * sizeof(bool));
    int *ops = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) {
        ops[i] = 1;
    }

    struct hashtable *tab = create_ht("lfc", T);

    pdata *datas = malloc (T * sizeof(pdata));
    pthread_t *threads = malloc(T * sizeof(pthread_t));
    for (int i = 0; i < T; i++) {
        datas[i].tab = tab;
        datas[i].keys = keys;
        datas[i].ops = ops;
        datas[i].begin = i * N / T;
        datas[i].end = (i + 1) * N / T - 1;
        datas[i].results = results;
    }

    for (int i = 0; i < T; i++) 
        pthread_create(&threads[i], NULL, worker, (void *)(datas + i));

    for (int i = 0; i < T; i++) 
        pthread_join(threads[i], NULL);

    // The real testing part of the test
    volatile struct resize_test_data d1 = {.tab = tab, .keys = keys, .len = N, .stop = false, .error = false};

    for (int i = 0; i < T - 1; i++) 
        pthread_create(&threads[i], NULL, resize_test_worker, (void *) &d1);


    for (int i = 0; i < 10; i++) {
        lf_inc_size((struct lfc_table *) tab);
    }
    d1.stop = true;

    if (d1.error) {
        printf("Fail: some contains failed\n");
    }

    for (int i = 0; i < T - 1; i++) 
        pthread_join(threads[i], NULL);

    return;
}

// This tests the index initialization function in the split table. The procedure is to
// create a table, initialize the first N indices in parallel with T threads, then make
// sure that the resulting linked list (starting at index 0) has exactly N sentinal nodes.
void *ind_init_worker (void *_data) {
    struct worker_data *data = (struct worker_data *) _data;
    struct split_table *tab = (struct split_table *) data->tab;
    for (int i = data->begin; i <= data->end; i++)
        initialize_index (tab, i);
    return NULL;
}

void ind_init (int N, int T) {
    struct hashtable *_tab = create_ht("split", N);
    struct split_table *tab = (struct split_table *) _tab;

    // Spawn off the workers
    pdata *datas = malloc (T * sizeof(pdata));
    pthread_t *threads = malloc(T * sizeof(pthread_t));
    for (int i = 0; i < T; i++) {
        datas[i].tab = _tab;
        datas[i].begin = i * N / T;
        datas[i].end = (i + 1) * N / T - 1;
    }

    for (int i = 0; i < T; i++) 
        pthread_create(&threads[i], NULL, ind_init_worker, (void *)(datas + i));

    for (int i = 0; i < T; i++) 
        pthread_join(threads[i], NULL);

    // Check that all the sentinals are in there and that there are
    // exactly N nodes in there.
    int *index_hits = calloc (N, sizeof(int));
    int total_size = 0;
    struct lf_elem *e = tab->buckets[0].head;
    while (e) {
        index_hits[e->key] = 1;
        total_size++;
        e = e->next;
    }
    if (total_size != N) {
        printf("Uhoh -- total size is %d instead of %d\n", total_size, N);
    }
    for (int i = 0; i < N; i++) {
        if (!index_hits[i]) {
            printf("Uhoh -- sentinal node with key %d not found\n", i);
            break;
        }
    }

    // Check that all the buckets point to the right place
    for (int i = 0; i < N; i++) {
        if (tab->buckets[i].head->key != i) {
            printf("Uhoh -- bucket %d points to %d instead\n", i, tab->buckets[i].head->key);
            break;
        }
    }
    return;
}

void testtab(int N, int T) {
    int *keys = malloc(N * sizeof(int));
    bool *results = malloc(N * sizeof(bool));
    int *ops = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) {
        ops[i] = 1;
        keys[i] = i;
    }

    if (T > N)
        T = N;

    struct hashtable *tab = create_ht("split", T);

    pdata *datas = malloc (T * sizeof(pdata));
    pthread_t *threads = malloc(T * sizeof(pthread_t));
    for (int i = 0; i < T; i++) {
        datas[i].tab = tab;
        datas[i].keys = keys;
        datas[i].ops = ops;
        datas[i].begin = i * N / T;
        datas[i].end = (i + 1) * N / T - 1;
        datas[i].results = results;
    }

    for (int i = 0; i < T; i++) 
        pthread_create(&threads[i], NULL, worker, (void *)(datas + i));

    for (int i = 0; i < T; i++) 
        pthread_join(threads[i], NULL);
    
    printf("poop\n");
}
