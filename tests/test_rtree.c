#define TESTNAME "rtree"
#include "testutils.h"
#include "cities.h"
#include "curve.h"
#include <math.h>

double min0(double a, double b) {
    return a < b ? a : b;
}

double max0(double a, double b) {
    return a > b ? a : b;
}

struct point {
    double x;
    double y;
};

void point_rect(struct point *point, double min[2], double max[2]) {
    min[0] = point->x;
    min[1] = point->y;
    max[0] = point->x; 
    max[1] = point->y;
}

struct point *point_new(double x, double y) {
    struct point *point = malloc0(sizeof(struct point));
    if (!point) {
        return 0;
    }
    point->x = x;
    point->y = y;
    return point;
}


struct point *rand_point(void) {
    struct point *point = malloc0(sizeof(struct point));
    if (!point) {
        return 0;
    }
    point->x = rand_double()*360-180;
    point->y = rand_double()*180-90;
    return point;
}

struct point *point_copy(struct point *point) {
    struct point *point2 = malloc0(sizeof(struct point));
    if (!point2) {
        return 0;
    }
    memcpy(point2, point, sizeof(struct point));
    return point2;
}

// struct point *point_copyR(struct point *point) {
//     if (atomic_load(&mallocRactive) && rand()%atomic_load(&mallocRmod)==0) {
//         return 0;
//     }
//     return point_copy(point);
// }

static struct point **rand_points(int count, double min[2], double max[2]) {
    double pmin[] = { INFINITY, INFINITY };
    double pmax[] = { -INFINITY, -INFINITY };
    struct point **points = malloc0(count*sizeof(struct point));
    for (int i = 0; i < count; i++) {
        points[i] = rand_point();
        pmin[0] = min0(points[i]->x, pmin[0]);
        pmin[1] = min0(points[i]->y, pmin[1]);
        pmax[0] = max0(points[i]->x, pmax[0]);
        pmax[1] = max0(points[i]->y, pmax[1]);
    }
    if (min) {
        memcpy(min, pmin, 16);
    }
    if (max) {
        memcpy(max, pmax, 16);
    }
    return points;
}

void shuffle_points(struct point **array, size_t numels) {
    shuffle0(array, numels, sizeof(struct point*));
}

#define RTREE_NAME points
#define RTREE_TYPE struct point *
#define RTREE_MALLOC return malloc0(size);
#define RTREE_FREE free0(ptr);
#define RTREE_ITEMRECT point_rect(item, min, max);
#define RTREE_COMPARE return memcmp(a, b, sizeof(struct point));
#define RTREE_ITEMCOPY { *copy = point_copy(item); return !!*copy; }
#define RTREE_ITEMFREE free0(item);
#define RTREE_FANOUT 8
#define RTREE_COW
#include "../rtree.h"

struct points_search_iter_ctx {
    int count;
    struct point *point;
    bool all;
};

bool points_search_iter(struct point *item, void *udata) {
    struct points_search_iter_ctx *ctx = udata;
    ctx->point = item;
    ctx->count++;
    return ctx->all;
}

struct scaniterctx {
    struct point *point;
    bool all;
    int count;
};

static bool scaniter(struct point *item, void *udata) {
    struct scaniterctx *ctx = udata;
    if (ctx->all) {
        ctx->count++;
        return true;
    }
    if (memcmp(item, ctx->point, sizeof(struct point)) == 0) {
        ctx->count++;
        return false;
    }
    return true;
}

void test_basic(void) {
    testinit();

    int N = 5000;
    double pmin[2], pmax[2];
    struct point **points = rand_points(N, pmin, pmax);

    int rc;

    struct points *tr = 0;

#if 0
    int fanout = points_feat_fanout();
    assert(fanout > 0);
    assert(points_feat_cow());
    assert(points_feat_atomics());


    // insert complete
    set_mallocR(true, 1);
    struct point *p1 = rand_point();
    int rc = points_insert(&tr, p1, 0);
    assert(rc == points_NOMEM);
    unset_mallocR();
    free0(p1);

    for (int i = 0; i < fanout; i++) {
        set_mallocR(false, 0);
        struct point *p1 = rand_point();
        int rc = points_insert(&tr, p1, 0);
        assert(rc == points_INSERTED);
        unset_mallocR();
    }
    set_mallocR(true, 1);
    p1 = rand_point();
    rc = points_insert(&tr, p1, 0);
    assert(rc == points_NOMEM);
    unset_mallocR();
    points_clear(&tr, 0);
    free0(p1);
#endif

    
    // set_mallocR(true, 1);
    for (int i = 0; i < N; i++) {
        int rc;
        struct point *p1 = point_copy(points[i]);
        do {
            rc = points_insert(&tr, p1, 0);
        } while (rc == points_NOMEM);
        assert(rc == points_INSERTED);
        if (i%13==0) {
            assert(points_sane(&tr, 0));
            assert(points_count(&tr) == (size_t)(i+1));
        }
    }
    assert(points_sane(&tr, 0));
    assert(points_height(&tr) > 0);



    double rmin[2], rmax[2];
    points_rect(&tr, rmin, rmax);
    assert(memcmp(rmin, pmin, 16) == 0);
    assert(memcmp(rmax, pmax, 16) == 0);


    shuffle_points(points, N);

    for (int i = 0; i < N; i++) {
        double min[2], max[2];
        points_itemrect(points[i], min, max, 0);
        struct points_search_iter_ctx ctx = { .all = false };
        int rc = points_search(&tr, min, max, points_search_iter, &ctx);
        assert(rc == points_STOPPED);
        assert(ctx.count == 1);
        assert(memcmp(ctx.point, points[i], sizeof(struct point)) == 0);
    }

    for (int i = 0; i < N; i++) {
        double min[2], max[2];
        struct point *p = rand_point();
        min[0] = p->x;
        min[1] = p->y;
        max[0] = p->x + rand_double()*10;
        max[1] = p->y + rand_double()*10;
        struct points_search_iter_ctx ctx = { .all = true };
        int rc = points_search(&tr, min, max, points_search_iter, &ctx);
        assert(rc == points_FINISHED);
        free0(p);
    }

    points_write_svg(&tr, "out1.svg", 0);
    struct points *tr2;
    rc = points_clone(&tr, &tr2, 0);
    assert(rc == points_COPIED);

    shuffle_points(points, N);

    for (int i = 0; i < N; i++) {
        struct point *old;
        int rc;
        do {
            rc = points_delete(&tr, points[i], &old, 0);
        } while (rc == points_NOMEM);
        assert(rc == points_DELETED);
        if (i%13==0) {
            assert(points_sane(&tr, 0));
        }
        assert(memcmp(old, points[i], sizeof(struct point)) == 0);
        do {
            rc = points_delete(&tr, points[i], &old, 0);
        } while (rc == points_NOMEM);
        assert(rc == points_NOTFOUND);
        assert(memcmp(old, points[i], sizeof(struct point)) == 0);
        free0(old);
    }

    shuffle_points(points, N);



    assert(points_height(&tr) == 0);
    for (int i = 0; i < N; i++) {
        int rc;
        struct point *p1 = point_copy(points[i]);
        do {
            rc = points_insert(&tr, p1, 0);
        } while (rc == points_NOMEM);
        assert(rc == points_INSERTED);
        if (i%13==0) {
            assert(points_sane(&tr, 0));
            assert(points_count(&tr) == (size_t)(i+1));
        }
    }
    assert(points_sane(&tr, 0));
    assert(points_height(&tr) > 0);


    struct points *tr3;
    // for (int i = 0; i < 10000; i++) {
    //     rc = points_copy(&tr2, &tr3, 0);
    //     if (rc == points_NOMEM) {
    //         break;
    //     }
    // }
    // if (rc == points_NOMEM) {
        // atomic_store(&mallocRactive, false);
        rc = points_copy(&tr2, &tr3, 0);
        // atomic_store(&mallocRactive, true);
    // }
    assert(rc == points_COPIED);
    
    // 


    int npoints = points_count(&tr);
    // add a bunch of null islands
    for (int i = 0; i < 1000; i++) {
        struct point *nullisland = malloc0(sizeof(struct point));
        nullisland->x = 0;
        nullisland->y = 0;
        int rc;
        do {
            rc = points_insert(&tr, nullisland, 0);
        } while (rc == points_NOMEM);
        assert(rc == points_INSERTED);
        if (i%13==0) {
            assert(points_sane(&tr, 0));
            assert(points_count(&tr) == (size_t)(npoints+i+1));
        }
    }

    npoints = points_count(&tr);

    shuffle_points(points, N);
    for (int i = 0; i < N; i++) {
        struct scaniterctx ctx = { .point = points[i] };
        int rc = points_scan(&tr, scaniter, &ctx);
        assert(rc == points_STOPPED);
        assert(ctx.count == 1);
    }



    // delete the null islands
    for (int i = 0; i < 1000; i++) {
        struct point *old;
        int rc = points_delete(&tr, &(struct point){ 0, 0 }, &old, 0);
        assert(rc == points_DELETED);
        assert(old->x == 0);
        assert(old->y == 0);
        free0(old);
        if (i%13==0) {
            assert(points_sane(&tr, 0));
            assert(points_count(&tr) == (size_t)(npoints-i-1));
        }
    }
    rc = points_delete(&tr, &(struct point){ 0, 0 }, 0, 0);
    assert(rc == points_NOTFOUND);


    points_write_svg(&tr2, "out2.svg", 0);

    struct points *tr4;
    rc = points_clone(&tr2, &tr4, 0);
    assert(rc == points_COPIED);
    for (int i = 0; i < N; i++) {
        int rc;
        struct point *p1 = point_copy(points[i]);
        do {
            rc = points_insert(&tr2, p1, 0);
        } while (rc == points_NOMEM);
        assert(rc == points_INSERTED);
        if (i%13==0) {
            assert(points_sane(&tr2, 0));
        }
    }
    for (int i = 0; i < N; i++) {
        int rc;
        struct point *p1 = point_copy(points[i]);
        do {
            rc = points_insert(&tr4, p1, 0);
        } while (rc == points_NOMEM);
        assert(rc == points_INSERTED);
        if (i%13==0) {
            assert(points_sane(&tr4, 0));
        }
    }



    points_write_svg(&tr3, "out3.svg", 0);
    assert(points_sane(&tr, 0));
    assert(points_height(&tr) > 0);

    points_clear(&tr, 0);
    points_clear(&tr2, 0);
    points_clear(&tr3, 0);
    points_clear(&tr4, 0);

    for (int i = 0; i < N; i++) {
        free0(points[i]);
    }

    free0(points);

    _points_internal_all_sym_calls();
    _points_internal_all_api_calls();

    FILE *f1 = fopen("out1.svg", "rb");
    FILE *f2 = fopen("out2.svg", "rb");
    FILE *f3 = fopen("out3.svg", "rb");
    assert(f1 && f2 && f3);
    fseek(f1, 0, SEEK_END);
    fseek(f2, 0, SEEK_END);
    fseek(f3, 0, SEEK_END);
    size_t f1size = ftell(f1);
    size_t f2size = ftell(f2);
    size_t f3size = ftell(f3);
    assert(f1size == f2size);
    assert(f2size == f3size);
    char *svg1 = malloc0(f1size);
    char *svg2 = malloc0(f2size);
    char *svg3 = malloc0(f2size);
    assert(svg1 && svg2 && svg3);
    rewind(f1);
    rewind(f2);
    rewind(f3);
    fread(svg1, 1, f1size, f1);
    fread(svg2, 1, f2size, f2);
    fread(svg3, 1, f2size, f3);
    assert(memcmp(svg1, svg2, f1size) == 0);
    assert(memcmp(svg2, svg3, f1size) == 0);

    free0(svg1);
    free0(svg2);
    free0(svg3);
    fclose(f1);
    fclose(f2);
    fclose(f3);


    checkmem();
}

void test_various(void) {
    testinit();
    
    int N = 5000;
    double pmin[2], pmax[2];
    struct point **points = rand_points(N, pmin, pmax);


    int fanout = points_feat_fanout();
    assert(fanout > 0);
    assert(points_feat_cow() > 0);
    assert(points_feat_atomics());

    struct point *p1;
    int rc;

    struct points *tr = 0;
    struct points *tr2 = 0;

    p1 = rand_point();
    rc = points_insert(&tr, p1, 0);
    assert(rc == points_INSERTED);
    points_clear(&tr, 0);

    // fail on root NOMEM
    p1 = rand_point();
    malloc_fail_in(1);
    rc = points_insert(&tr, p1, 0);
    assert(rc == points_NOMEM);
    free0(p1);

    // COW
    p1 = rand_point();
    rc = points_insert(&tr, p1, 0);
    assert(rc == points_INSERTED);
    rc = points_clone(&tr, &tr2, 0);
    assert(rc == points_COPIED);

    // fail on root COW
    p1 = rand_point();
    malloc_fail_in(1);
    rc = points_insert(&tr, p1, 0);
    assert(rc == points_NOMEM);
    free0(p1);
    points_clear(&tr, 0);
    points_clear(&tr2, 0);

    // insert maxitems into root leaf
    for (int i = 0; i < fanout; i++) {
        p1 = rand_point();
        rc = points_insert(&tr, p1, 0);
        assert(rc == points_INSERTED);
    }

    // fail on root split
    p1 = rand_point();
    malloc_fail_in(1);
    rc = points_insert(&tr, p1, 0);
    assert(rc == points_NOMEM);
    free0(p1);

    // fail on root split (right node)
    p1 = rand_point();
    malloc_fail_in(2);
    rc = points_insert(&tr, p1, 0);
    assert(rc == points_NOMEM);
    free0(p1);


    // split root leaf
    p1 = rand_point();
    rc = points_insert(&tr, p1, 0);
    assert(rc == points_INSERTED);


    // insert until root right or left are maxitems
    assert(tr->isleaf == false);
    assert(tr->count == 2);
    assert(tr->nodes[0]->isleaf);
    rc = points_clone(&tr, &tr2, 0);
    assert(rc == points_COPIED);
    while (tr->nodes[0]->count < fanout || tr->nodes[1]->count < fanout) {
        p1 = rand_point();
        rc = points_insert(&tr, p1, 0);
        assert(rc == points_INSERTED);
        if (tr->count == 3) {
            points_clear(&tr, 0);
            rc = points_clone(&tr2, &tr, 0);
            assert(rc == points_COPIED);
        }
    }
    points_clear(&tr2, 0);
    assert(tr->isleaf == false);
    assert(tr->count == 2);
    assert(tr->nodes[0]->isleaf);
    assert(tr->nodes[1]->isleaf);
    assert(tr->nodes[0]->count == fanout);
    assert(tr->nodes[1]->count == fanout);


    rc = points_clone(&tr, &tr2, 0);
    assert(rc == points_COPIED);

    // split child node, but fail
    p1 = rand_point();
    malloc_fail_in(3);
    rc = points_insert(&tr, p1, 0);
    assert(rc == points_NOMEM);
    free0(p1);
    assert(points_sane(&tr, 0));

    // split child node, but fail
    p1 = rand_point();
    malloc_fail_in(2);
    rc = points_insert(&tr, p1, 0);
    assert(rc == points_NOMEM);
    free0(p1);

    assert(tr->height == 2);
    while (tr->height < 3) {
        p1 = rand_point();
        rc = points_insert(&tr, p1, 0);
        assert(rc == points_INSERTED);
    }

    points_clear(&tr2, 0);
    points_clear(&tr2, 0);
    points_clear(&tr, 0);

    rc = points_insert(&tr, rand_point(), 0);
    assert(rc == points_INSERTED);
    rc = points_insert(&tr, rand_point(), 0);
    assert(rc == points_INSERTED);
    rc = points_insert(&tr, rand_point(), 0);
    assert(rc == points_INSERTED);

    malloc_fail_in(3);
    rc = points_copy(&tr, &tr2, 0);
    assert(rc == points_NOMEM);


    struct points *n = _points_internal_alloc_node(false, 0);
    n->count = 3;
    n->nodes[0] = _points_internal_alloc_node(true, 0);
    n->nodes[0]->count = 1;
    n->nodes[0]->items[0] = rand_point();
    n->nodes[1] = _points_internal_alloc_node(true, 0);
    n->nodes[1]->count = 1;
    n->nodes[1]->items[0] = rand_point();
    n->nodes[2] = _points_internal_alloc_node(true, 0);
    n->nodes[2]->count = 1;
    n->nodes[2]->items[0] = rand_point();


    malloc_fail_in(1);
    assert(!_points_internal_node_copy(n, true, 0));
    malloc_fail_in(2);
    assert(!_points_internal_node_copy(n, true, 0));
    malloc_fail_in(3);
    assert(!_points_internal_node_copy(n, true, 0));
    malloc_fail_in(4);
    assert(!_points_internal_node_copy(n, true, 0));
    malloc_fail_in(5);
    assert(!_points_internal_node_copy(n, true, 0));
    malloc_fail_in(6);
    assert(!_points_internal_node_copy(n, true, 0));
    _points_internal_node_free(n, 0);
    malloc_fail_in(0);



    points_clear(&tr2, 0);
    points_clear(&tr, 0);

    rc = points_copy(&tr, &tr2, 0);
    assert(rc == points_COPIED);

    for (int i = 0; i < N; i++) {
        free0(points[i]);
    }
    free0(points);
    checkmem();
}

void test_sane_units(void) {
    testinit();
    
    malloc_fail_in(0);
    struct points *tr = 0;
    while (tr == 0 || tr->height < 3) {
        int rc = points_insert(&tr, rand_point(), 0);
        assert(rc == points_INSERTED);
        assert(points_sane(&tr, 0));
    }

    tr->isleaf = true;
    assert(!points_sane(&tr, 0));
    tr->isleaf = false;

    size_t root_count = tr->root_count;
    tr->root_count = 0;
    assert(!points_sane(&tr, 0));
    tr->root_count = 1;
    assert(!points_sane(&tr, 0));
    tr->root_count = root_count;

    assert(!tr->nodes[0]->isleaf);
    tr->nodes[0]->root_count = 1;
    assert(!points_sane(&tr, 0));
    tr->nodes[0]->root_count = 0;

    short count = tr->count;
    tr->count = 999;
    assert(!points_sane(&tr, 0));
    tr->count = count;

    size_t height = tr->height;
    tr->height = 0;
    assert(!points_sane(&tr, 0));
    tr->height = 2;
    assert(!points_sane(&tr, 0));
    tr->height = height;

    double x = tr->rects[0].min[0];
    tr->rects[0].min[0] = 120398.0;
    assert(!points_sane(&tr, 0));
    tr->rects[0].min[0] = x;


    assert(points_sane(&tr, 0));
    points_clear(&tr, 0);

    checkmem();
}

int hilbertcmpcity(const void *ap, const void *bp) {
    const struct city_entry *a = ap;
    const struct city_entry *b = bp;
    uint32_t ac = curve_hilbert(a->lon, a->lat, (double[]){-180,-90,180,90});
    uint32_t bc = curve_hilbert(b->lon, b->lat, (double[]){-180,-90,180,90});
    return ac < bc ? -1 : ac > bc;
}


void test_cities(void) {
    testinit();

    struct city_entry *cities = malloc0(sizeof(all_cities));
    memcpy(cities, all_cities, sizeof(all_cities));

    qsort(cities, NCITIES, sizeof(struct city_entry), hilbertcmpcity);
    struct points *tr = 0;

    for (int i = 0; i < NCITIES; i++) {
        struct point *point = point_new(cities[i].lon, cities[i].lat);
        points_insert(&tr, point, 0);
    }


    points_write_svg(&tr, "cities.svg", 0);

    points_clear(&tr, 0);
    free0(cities);
    checkmem();
}

int main(void) {
    initrand();

    test_basic();
    test_various();
    test_sane_units();
    test_cities();

    return 0;
}
