#include <stdio.h>
#include "curve.h"
#include "testutils.h"


int N = 1000000;
int G = 50;
int C = 0;        // -1 = worse-case, 0 = average, +1 = best-case

struct point {
    double x;
    double y;
};

struct point *rand_point(void) {
    struct point *point = malloc(sizeof(struct point));
    assert(point);
    point->x = rand_double()*360-180;
    point->y = rand_double()*180-90;
    return point;
}

void point_rect(struct point *point, double min[2], double max[2]) {
    min[0] = point->x;
    min[1] = point->y;
    max[0] = point->x; 
    max[1] = point->y;
}

void shuffle_points(struct point *array, size_t numels) {
    shuffle0(array, numels, sizeof(struct point));
}

int hilbertcmp(const void *ap, const void *bp) {
    const struct point *a = ap;
    const struct point *b = bp;
    uint32_t ac = curve_hilbert(a->x, a->y, (double[]){-180,-90,180,90});
    uint32_t bc = curve_hilbert(b->x, b->y, (double[]){-180,-90,180,90});
    return ac < bc ? -1 : ac > bc;
}

void sort_points(struct point *array, size_t numels) {
    qsort(array, numels, sizeof(struct point), hilbertcmp);
}


#define RTREE_NAME      points
#define RTREE_TYPE      struct point
#define RTREE_ITEMRECT  point_rect(&item, min, max);
// #define RTREE_FLOAT32
#define RTREE_FANOUT    64
#include "../rtree.h"

// static bool iter_scan(int item, void *udata) {
//     double *sum = udata;
//     (*sum) += item;
//     return true;
// }

// #define reset_tree() { \
//     kv_clear(&tree, 0); \
//     shuffle(keys, N); \
//     for (int i = 0; i < N; i++) { \
//         kv_insert(&tree, keys[i], &val, 0); \
//     } \
// }(void)0

#define run_op(label, nruns, preop, op) {\
    double gelapsed = C < 1 ? 0 : 9999; \
    double gstart = now(); \
    double gmstart = mtotal; \
    double gmtotal = 0; \
    for (int g = 0; g < (nruns); g++) { \
        printf("\r%-20s", label); \
        printf("%d/%d ", g+1, (nruns)); \
        fflush(stdout); \
        preop \
        double start = now(); \
        size_t mstart = mtotal; \
        op \
        double elapsed = now()-start; \
        if (C == -1) { \
            if (elapsed > gelapsed) { \
                gelapsed = elapsed; \
            } \
        } else if (C == 0) { \
            gelapsed += elapsed; \
        } else { \
            if (elapsed < gelapsed) { \
                gelapsed = elapsed; \
            } \
        } \
        if (mtotal > mstart) { \
            gmtotal += mtotal-mstart; \
        } \
    } \
    printf("\r"); \
    printf("%-19s", label); \
    if (C == 0) { \
        gelapsed /= nruns; \
    } \
    bench_print_mem(N, gstart, gstart+gelapsed, \
        gmstart, gmstart+(gmtotal/(nruns))); \
}(void)0

static bool siter(struct point item, void *udata) {
    return false;
}

int main(void) {
    if (getenv("N")) {
        N = atoi(getenv("N"));
    }
    if (getenv("G")) {
        G = atoi(getenv("G"));
    }
    if (getenv("C")) {
        C = atoi(getenv("C"));
    }
    printf("Benchmarking %d items, %d times, taking the %s result\n", 
        N, G, C == -1 ? "worst": C == 0 ? "average" : "best");
    seedrand();


    struct point *points = malloc(sizeof(struct point)*N);
    assert(points);

    for (int i = 0; i < N; i++) {
        points[i] = *rand_point();
    }

    struct points *tr = 0;
    run_op("insert(rand)", G, {
        points_clear(&tr, 0);
        shuffle_points(points, N);
    },{
        for (int i = 0; i < N; i++) {
            int rc = points_insert(&tr, points[i], 0);
            assert(rc == points_INSERTED);
        }
    });

    run_op("search(rand)", G, {
        shuffle_points(points, N);
    },{
        double min[2];
        double max[2];
        for (int i = 0; i < N; i++) {
            points_itemrect(points[i], min, max, 0);
            points_search(&tr, min, max, siter, 0);
        }
    });


    struct points *tr2;
    points_copy(&tr, &tr2, 0);
    run_op("delete(rand)", G, {
        shuffle_points(points, N);
        points_copy(&tr2, &tr, 0);
    },{
        for (int i = 0; i < N; i++) {
            int rc = points_delete(&tr, points[i], 0, 0);
            assert(rc == points_DELETED);
        }
    });
    points_clear(&tr2, 0);


    points_clear(&tr, 0);

    sort_points(points, N);
    run_op("insert(hilbert)", G, {
        points_clear(&tr, 0);
    },{
        for (int i = 0; i < N; i++) {
            int rc = points_insert(&tr, points[i], 0);
            assert(rc == points_INSERTED);
        }
    });

    run_op("search(hilbert)", G, {
        // shuffle_points(points, N);
    },{
        double min[2];
        double max[2];
        for (int i = 0; i < N; i++) {
            points_itemrect(points[i], min, max, 0);
            points_search(&tr, min, max, siter, 0);
        }
    });


    points_copy(&tr, &tr2, 0);
    run_op("delete(hilbert)", G, {
        // shuffle_points(points, N);
        points_copy(&tr2, &tr, 0);
    },{
        for (int i = 0; i < N; i++) {
            int rc = points_delete(&tr, points[i], 0, 0);
            assert(rc == points_DELETED);
        }
    });
    points_clear(&tr2, 0);



    return 0;
}
