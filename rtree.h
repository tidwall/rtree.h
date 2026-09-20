// https://github.com/tidwall/rtree.h
//
// Copyright 2026 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
//
// R-tree generator for C
//
// For a complete list of options visit:
// https://github.com/tidwall/rtree.h#options

/*
RTREE_NAME namespace
RTREE_TYPE item type
RTREE_FLOAT16             // use float16 internally (arm64 only)
RTREE_FLOAT32             // use float32 internally
RTREE_COMPARE
RTREE_MALLOC
RTREE_FREE
RTREE_COW
RTREE_ITEMCOPY
RTREE_ITEMFREE
RTREE_ITEMRECT            // changes insert/delete/search signatures
*/


// The API namespace. 
// This is the prefix for all functions calls, and is also and the name of the
// root node structure.
#ifndef RTREE_NAME
#error RTREE_NAME required
#define RTREE_NAME unnamed_rtree /* unused placeholder */
#endif

// macro concatenate
#define RTREE_CC(a, b) a ## b
#define RTREE_C(a, b)  RTREE_CC(a, b)

// API symbols are the calls available to the user.
#define RTREE_API(name) RTREE_C(RTREE_C(RTREE_NAME,_),name)

// Internal symbols are prefixed with an underscore.
// These should not be directly called by the user.
#define RTREE_SYM(name) RTREE_C(RTREE_C(RTREE_C(_,RTREE_NAME),_internal_),name)

#include <stdbool.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// The internal item type. This is used as both the value type and the key
// type, and can be pretty much anything.
#ifndef RTREE_TYPE
#error RTREE_TYPE required
#define RTREE_TYPE int /* unused placeholder */
#endif

// The "fanout" is maximum number of child nodes that a branch node may have.
#ifndef RTREE_FANOUT
#define RTREE_FANOUTUSED 64
#elif RTREE_FANOUT < 4 
#define RTREE_FANOUTUSED 4
#elif RTREE_FANOUT > 4096
#define RTREE_FANOUTUSED 4096
#else
#define RTREE_FANOUTUSED RTREE_FANOUT
#endif

#ifndef RTREE_DIMS
#define RTREE_DIMS 2
#elif RTREE_FANOUT < 2 || RTREE_FANOUT > 256
#error RTREE_DIMS must be between 2 and 256
#endif

#if defined(RTREE_FLOAT16)
#define RTREE_FLOAT _Float16
#elif defined(RTREE_FLOAT32)
#define RTREE_FLOAT float
#else
#define RTREE_FLOAT double
#endif

// MAXITEMS and MINITEMS are the minimum and maximum number of items allowed in
// each node, respectively.
#define RTREE_MAXITEMS  (RTREE_FANOUTUSED)
#define RTREE_MINITEMS  0

#define RTREE_INLINE inline
#ifdef __GNUC__
#define RTREE_NOINLINE __attribute__((noinline))
#else
#define RTREE_NOINLINE
#endif

// Provide a custom allocator using RTREE_MALLOC and RTREE_FREE.
// Such as:
//
//     #define RTREE_MALLOC return my_malloc(size);
//     #define RTREE_FREE   my_free(ptr);
//
// This will ensure that the tree will always use my_malloc/my_free instead of
// the standard malloc/free.
#if !defined(RTREE_MALLOC) || !defined(RTREE_FREE)

#include <stdlib.h>

#ifndef RTREE_MALLOC
#define RTREE_MALLOC return malloc(size);
#endif

#ifndef RTREE_FREE
#define RTREE_FREE free(ptr);
#endif
#endif

#ifndef RTREE_EXTERN
#ifdef RTREE_HEADER
#define RTREE_EXTERN extern
#else
#define RTREE_EXTERN static
#endif
#endif

#ifndef RTREE_COMPARE
#define RTREE_COMPARE return memcmp(&a, &b, sizeof(RTREE_ITEM));
#endif

// Convenient aliases to common types
#define RTREE_RECT struct RTREE_SYM(rect_type)
#define RTREE_NODE struct RTREE_NAME
#define RTREE_ITEM RTREE_TYPE
#define RTREE_SNODE struct RTREE_SYM(snode)

// The following status codes are private to this file only.
// Users should use the prefixed version such as rt_INSERTED as defined in the
// enum below.
#define RTREE_INSERTED    1  // New item was inserted
// #define RTREE_REPLACED    2  // Item replaced an existing item
#define RTREE_DELETED     3  // Item was successfully deleted
// #define RTREE_FOUND       4  // Item was successfully accessed
#define RTREE_NOTFOUND    5  // Item was not found
// #define RTREE_OUTOFORDER  6  // Item is out of order
#define RTREE_FINISHED    7  // Callback iterator returned all items
#define RTREE_STOPPED     8  // Callback iterator was stopped early
#define RTREE_COPIED      9  // Tree was copied: `clone`, `copy`
#define RTREE_NOMEM       10 // Out of memory
#define RTREE_UNSUPPORTED 11 // Operation not supported
#define RTREE_SPLIT       12 // Internal: split operation

#ifndef RTREE_SOURCE

// Definitions

enum RTREE_API(status) {
    RTREE_C(RTREE_NAME, _INSERTED)    = RTREE_INSERTED,
    // RTREE_C(RTREE_NAME, _REPLACED)    = RTREE_REPLACED,
    RTREE_C(RTREE_NAME, _DELETED)     = RTREE_DELETED,
    // RTREE_C(RTREE_NAME, _FOUND)       = RTREE_FOUND,
    RTREE_C(RTREE_NAME, _NOTFOUND)    = RTREE_NOTFOUND,
    // RTREE_C(RTREE_NAME, _OUTOFORDER)  = RTREE_OUTOFORDER,
    RTREE_C(RTREE_NAME, _FINISHED)    = RTREE_FINISHED,
    RTREE_C(RTREE_NAME, _STOPPED)     = RTREE_STOPPED,
    RTREE_C(RTREE_NAME, _COPIED)      = RTREE_COPIED,
    RTREE_C(RTREE_NAME, _NOMEM)       = RTREE_NOMEM,
    RTREE_C(RTREE_NAME, _UNSUPPORTED) = RTREE_UNSUPPORTED,
};

RTREE_NODE;

RTREE_RECT {
    RTREE_FLOAT min[RTREE_DIMS];
    RTREE_FLOAT max[RTREE_DIMS];
};

#ifndef RTREE_ITEMRECT
#define RTREE_NOITEMRECT
#define RTREE_ITEMRECT
#endif

#ifdef RTREE_NOITEMRECT
// There's no RTREE_ITEMRECT fragment. Include the item rectangles as arguments
// alongside the item.
#define RTREE_ITERSIG bool(*iter)(double min[RTREE_DIMS], \
    double max[RTREE_DIMS], RTREE_ITEM item, void *udata)
RTREE_EXTERN int RTREE_API(insert)(RTREE_NODE **root, 
    double min[RTREE_DIMS], double max[RTREE_DIMS], RTREE_ITEM item,
    void *udata);
RTREE_EXTERN int RTREE_API(delete)(RTREE_NODE **root, 
    double min[RTREE_DIMS], double max[RTREE_DIMS], RTREE_ITEM key,
    RTREE_ITEM *olditem, void *udata);
RTREE_EXTERN int RTREE_API(search)(RTREE_NODE **root, double min[RTREE_DIMS],
    double max[RTREE_DIMS], RTREE_ITERSIG, void *udata);
RTREE_EXTERN int RTREE_API(scan)(RTREE_NODE **root, RTREE_ITERSIG, void *udata);
#else
#define RTREE_ITERSIG bool(*iter)(RTREE_ITEM item, void *udata)
RTREE_EXTERN int RTREE_API(insert)(RTREE_NODE **root, RTREE_ITEM item,
    void *udata);
RTREE_EXTERN int RTREE_API(delete)(RTREE_NODE **root, RTREE_ITEM key, 
    RTREE_ITEM *olditem, void *udata);
RTREE_EXTERN int RTREE_API(search)(RTREE_NODE **root, double min[RTREE_DIMS],
    double max[RTREE_DIMS], RTREE_ITERSIG, void *udata);
RTREE_EXTERN int RTREE_API(scan)(RTREE_NODE **root, RTREE_ITERSIG, void *udata);
#endif

RTREE_EXTERN size_t RTREE_API(count)(RTREE_NODE **root);
RTREE_EXTERN void RTREE_API(clear)(RTREE_NODE **root, void *udata);
RTREE_EXTERN int RTREE_API(copy)(RTREE_NODE **root, RTREE_NODE **newroot,
    void *udata);
RTREE_EXTERN int RTREE_API(clone)(RTREE_NODE **root, RTREE_NODE **newroot,
    void *udata);
RTREE_EXTERN void RTREE_API(rect)(RTREE_NODE **root,
    double min[RTREE_DIMS], RTREE_FLOAT max[RTREE_DIMS]);
RTREE_EXTERN size_t RTREE_API(height)(RTREE_NODE **root);
RTREE_EXTERN bool RTREE_API(sane)(RTREE_NODE **root, void *udata);
RTREE_EXTERN void RTREE_API(write_svg)(RTREE_NODE **root, const char *path,
    void *udata);
RTREE_EXTERN void RTREE_API(itemrect)(RTREE_ITEM item, 
    double min[RTREE_DIMS], double max[RTREE_DIMS], void *udata);

RTREE_EXTERN int RTREE_API(feat_fanout)(void);
RTREE_EXTERN bool RTREE_API(feat_cow)(void);
RTREE_EXTERN bool RTREE_API(feat_atomics)(void);

#endif // !RTREE_SOURCE


#ifndef RTREE_HEADER

// IMPLEMENTATION

RTREE_NOINLINE
static void *RTREE_SYM(malloc)(size_t size, void *udata) {
    (void)size, (void)udata;
    RTREE_MALLOC
}

static void RTREE_SYM(free)(void *ptr, size_t size, void *udata) {
    (void)ptr, (void)size, (void)udata;
    RTREE_FREE
}

static int RTREE_SYM(compare)(RTREE_ITEM a, RTREE_ITEM b, void *udata) {
    (void)a, (void)b, (void)udata;
    RTREE_COMPARE
}

static void RTREE_SYM(itemrect)(RTREE_ITEM item, double min[RTREE_DIMS], 
    double max[RTREE_DIMS], void *udata)
{
    (void)item, (void)min, (void)max, (void)udata;
    RTREE_ITEMRECT
}

#ifdef RTREE_COW

#ifdef RTREE_NOATOMICS

typedef int RTREE_SYM(rc_t);
static void RTREE_SYM(rc_init)(RTREE_SYM(rc_t) *rc) {
    *rc = 0;
}
static void RTREE_SYM(rc_retain)(RTREE_SYM(rc_t) *rc) {
    (*rc)++;
}
static bool RTREE_SYM(rc_release)(RTREE_SYM(rc_t) *rc) {
    return (*rc)-- == 1;
}
static bool RTREE_SYM(rc_shared)(RTREE_SYM(rc_t) *rc) {
    return *rc > 1;
}

#else

#include <stdatomic.h>

typedef atomic_int RTREE_SYM(rc_t);
static void RTREE_SYM(rc_init)(RTREE_SYM(rc_t) *rc) {
    atomic_init(rc, 0);
}
static void RTREE_SYM(rc_retain)(RTREE_SYM(rc_t) *rc) {
    atomic_fetch_add_explicit(rc, 1, __ATOMIC_RELAXED);
}
static bool RTREE_SYM(rc_release)(RTREE_SYM(rc_t) *rc) {
    return atomic_fetch_sub_explicit(rc, 1, __ATOMIC_ACQ_REL) == 1;
}
static bool RTREE_SYM(rc_shared)(RTREE_SYM(rc_t) *rc) {
    return atomic_load_explicit(rc, __ATOMIC_ACQUIRE) > 1;
}

#endif
#endif

RTREE_NODE {
#ifdef RTREE_COW
    RTREE_SYM(rc_t) rc; // reference counter
#endif
    short count; // number of items in this node
    char height; // tree height (one is leaf)
    bool isleaf; // node is a leaf
    RTREE_RECT rects[RTREE_MAXITEMS];
    union {
        struct {
            // leaf only fields
            RTREE_ITEM items[RTREE_MAXITEMS];
            void *_leaf_size_unused_;
        };
        struct {
            // branch only fields
            RTREE_NODE *nodes[RTREE_MAXITEMS]; // child nodes
            size_t root_count; // entire tree count, root only
            void *_branch_size_unused_;
        };
    };
};

#define RTREE_LEAF_SIZE offsetof(RTREE_NODE, _leaf_size_unused_)
#define RTREE_BRANCH_SIZE offsetof(RTREE_NODE, _branch_size_unused_)
#define RTREE_NODE_SIZE(node) ((node)->isleaf?RTREE_LEAF_SIZE:RTREE_BRANCH_SIZE)

#ifdef RTREE_ASSERT
#include <assert.h>
#undef RTREE_ASSERT
#define RTREE_ASSERT(cond) assert(cond)
#else
#define RTREE_ASSERT(cond)(void)0
#endif

static int RTREE_SYM(feat_fanout)(void) {
    return RTREE_FANOUTUSED;
}
static bool RTREE_SYM(feat_cow)(void) {
#ifdef RTREE_COW
    return true;
#else
    return false;
#endif
}
static bool RTREE_SYM(feat_atomics)(void) {
#ifndef BTREE_NOATOMICS
    return true;
#else
    return false;
#endif
}


static RTREE_NODE *RTREE_SYM(alloc_node)(bool isleaf, void *udata) {
    void *ptr = isleaf ? 
        RTREE_SYM(malloc)(RTREE_LEAF_SIZE, udata) :
        RTREE_SYM(malloc)(RTREE_BRANCH_SIZE, udata);
    if (!ptr) {
        return 0;
    }
    RTREE_NODE *node = (RTREE_NODE*)ptr;
#ifdef RTREE_COW
    RTREE_SYM(rc_init)(&node->rc);
    RTREE_SYM(rc_retain)(&node->rc);
#endif
    node->isleaf = isleaf;
    node->height = 0;
    node->count = 0;
    if (!isleaf) {
        node->root_count = 0;
    }
    return node;
}

// Check if node is being shared (referenced) by other clones.
static bool RTREE_SYM(shared)(RTREE_NODE *node) {
#ifndef RTREE_COW
    (void)node;
    return false;
#else
    return RTREE_SYM(rc_shared)(&node->rc);
#endif
}

#ifdef RTREE_ITEMCOPY
static bool RTREE_SYM(item_copy)(RTREE_ITEM item, RTREE_ITEM *copy, void *udata)
{
    (void)item, (void)copy, (void)udata;
    RTREE_ITEMCOPY
}
#else 
static bool RTREE_SYM(item_copy)(RTREE_ITEM item, RTREE_ITEM *copy, void *udata)
{
    (void)udata;
    *copy = item;
    return true;
}
#endif

static void RTREE_SYM(item_free)(RTREE_ITEM item, void *udata) {
    (void)item, (void)udata;
#ifdef RTREE_ITEMFREE
    RTREE_ITEMFREE
#endif
}

static void RTREE_SYM(node_free)(RTREE_NODE *node, void *udata) {
#ifdef RTREE_COW
    if (!RTREE_SYM(rc_release)(&node->rc)) {
        return;
    }
#endif
    if (node->isleaf) {
#ifdef RTREE_ITEMFREE
        for (int i = 0; i < node->count; i++) {
            RTREE_SYM(item_free)(node->items[i], udata);
        }
#endif
    } else {
        for (int i = 0; i < node->count; i++) {
            RTREE_SYM(node_free)(node->nodes[i], udata);
        }
    }
    RTREE_SYM(free)(node, RTREE_NODE_SIZE(node), udata);
}

static RTREE_NODE *RTREE_SYM(node_copy)(RTREE_NODE *node, bool deep,
    void *udata)
{
    RTREE_NODE *node2 = RTREE_SYM(alloc_node)(node->isleaf, udata);
    if (!node2) {
        return 0;
    }
    node2->count = node->count;
    node2->height = node->height;
    memcpy(node2->rects, node->rects, node->count*sizeof(RTREE_RECT));
    if (node->isleaf) {
        // leaf
#ifdef RTREE_ITEMCOPY
        for (int i = 0; i < node->count; i++) {
            if (!RTREE_SYM(item_copy)(node->items[i], &node2->items[i], udata)){
                // copy failed
                for (int j = 0; j < i; j++) {
                    RTREE_SYM(item_free)(node2->items[j], udata);
                }
                RTREE_SYM(free)(node2, RTREE_NODE_SIZE(node2), udata);
                return 0;
            }
        }
#else
        memcpy(node2->items, node->items, node->count*sizeof(RTREE_ITEM));
#endif
        return node2;
    }
    // branch
    for (int i = 0; i < node->count; i++) {
#ifdef RTREE_COW
        if (!deep) {
            node2->nodes[i] = node->nodes[i];
            RTREE_SYM(rc_retain)(&node2->nodes[i]->rc);
            continue;
        }
#endif
        node2->nodes[i] = RTREE_SYM(node_copy)(node->nodes[i], deep, udata);
        if (!node2->nodes[i]) {
            // copy failed
            for (int j = 0; j < i; j++) {
                RTREE_SYM(node_free)(node2->nodes[j], udata);
            }
            RTREE_SYM(free)(node2, RTREE_NODE_SIZE(node2), udata);
            return 0;
        }
    }
    node2->root_count = node->root_count;
    return node2;
}

// Perform copy-on-write operation. 
// Returns true on success or false on failure (NOMEM).
static bool RTREE_SYM(cow)(RTREE_NODE **node, void *udata) {
#ifndef RTREE_COW
    (void)node, (void)udata;
#else
    if (RTREE_SYM(shared)(*node)) {
        RTREE_NODE *node2 = RTREE_SYM(node_copy)(*node, false, udata);
        if (!node2) {
            return false;
        }
        RTREE_SYM(node_free)(*node, udata);
        *node = node2;
    }
#endif
    return true;
}

#define RTREE_COW_NODE(n) \
    if (!RTREE_SYM(cow)((n), udata)) { \
        return RTREE_NOMEM; \
    }

static RTREE_FLOAT RTREE_SYM(fmin)(RTREE_FLOAT a, RTREE_FLOAT b) {
    return a < b ? a : b;
}

static RTREE_FLOAT RTREE_SYM(fmax)(RTREE_FLOAT a, RTREE_FLOAT b) {
    return a > b ? a : b;
}

static bool RTREE_SYM(rect_contains)(RTREE_RECT *rect, RTREE_RECT *other) {
    int bits = 0;
    for (int i = 0; i < RTREE_DIMS; i++) {
        bits |= other->min[i] < rect->min[i];
        bits |= other->max[i] > rect->max[i];
    }
    return bits == 0;
}

static bool RTREE_SYM(rect_intersects)(RTREE_RECT *rect, RTREE_RECT *other) {
    int bits = 0;
    for (int i = 0; i < RTREE_DIMS; i++) {
        bits |= other->min[i] > rect->max[i];
        bits |= other->max[i] < rect->min[i];
    }
    return bits == 0;
}

static bool RTREE_SYM(rect_onedge)(RTREE_RECT *rect, RTREE_RECT *other) {
    // Using imple binary equality with '==' operator.
    int bits = 0;
    for (int i = 0; i < RTREE_DIMS; i++) {
        bits |= rect->min[i] == other->min[i];
        bits |= rect->max[i] == other->max[i];
    }
    return bits != 0;
}

static void RTREE_SYM(rect_expand)(RTREE_RECT *rect, RTREE_RECT *other) {
    for (int i = 0; i < RTREE_DIMS; i++) {
        rect->min[i] = RTREE_SYM(fmin)(rect->min[i], other->min[i]);
        rect->max[i] = RTREE_SYM(fmax)(rect->max[i], other->max[i]);
    }
}

static RTREE_FLOAT RTREE_SYM(rect_area)(RTREE_RECT *rect) {
    RTREE_FLOAT result = 1;
    for (int i = 0; i < RTREE_DIMS; i++) {
        result *= (rect->max[i] - rect->min[i]);
    }
    return result;
}

static RTREE_RECT RTREE_SYM(node_rect_calc)(RTREE_NODE *node) {
    RTREE_RECT rect = node->rects[0];
    for (int i = 1; i < node->count; i++) {
        RTREE_SYM(rect_expand)(&rect, &node->rects[i]);
    }
    return rect;
}

//
static RTREE_FLOAT RTREE_SYM(round_down)(double x) {
#if defined(RTREE_FLOAT16) 
    _Float16 z = x;
    if ((double)z > x) {
        z = __nextafterf16(z, -INFINITY);
    }
    return z;
#elif defined(RTREE_FLOAT32)
    float z = x;
    if ((double)z > x) {
        z = nextafterf(z, -INFINITY);
    }
    return z;
#else
    return x;
#endif
}

static RTREE_FLOAT RTREE_SYM(round_up)(double x) {
#if defined(RTREE_FLOAT16) 
    _Float16 z = x;
    if ((double)z > x) {
        z = nextafterf(z, +INFINITY);
    }
    return z;
#elif defined(RTREE_FLOAT32)
    float z = x;
    if ((double)z > x) {
        z = nextafterf(z, +INFINITY);
    }
    return z;
#else
    return x;
#endif
}

// fill_rect convert input min/max into an internal rectangle.
// If RTREE_FLOAT16 and RTREE_FLOAT32 are used then the resulting rect.min and
// rect.max are rounded down and up, respectively, to ensure bounds are fully
// covered.
static RTREE_RECT RTREE_SYM(fill_rect)(double min[RTREE_DIMS], 
    double max[RTREE_DIMS])
{
    RTREE_RECT rect;
    for (int i = 0; i < RTREE_DIMS; i++) {
        rect.min[i] = RTREE_SYM(round_down)(min[i]);
        rect.max[i] = RTREE_SYM(round_up)(max[i]);
    }
    return rect;
}

// fill_minmax converts a rectangle to min/max
static RTREE_RECT RTREE_SYM(fill_rect_item)(RTREE_ITEM item, void *udata) {
    double min[RTREE_DIMS], max[RTREE_DIMS];
    RTREE_SYM(itemrect)(item, min, max, udata);
    return RTREE_SYM(fill_rect)(min, max);
}


// return the area of two rects expanded
static RTREE_FLOAT RTREE_SYM(rect_unioned_area)(RTREE_RECT *rect,
    RTREE_RECT *other)
{
    RTREE_FLOAT result = 1;
    for (int i = 0; i < RTREE_DIMS; i++) {
        result *= (RTREE_SYM(fmax)(rect->max[i], other->max[i]) - 
                   RTREE_SYM(fmin)(rect->min[i], other->min[i]));
    }
    return result;
}

static int RTREE_SYM(rect_largest_axis)(RTREE_RECT *rect) {
    int axis = 0;
    RTREE_FLOAT nlength = rect->max[0] - rect->min[0];
    for (int i = 1; i < RTREE_DIMS; i++) {
        RTREE_FLOAT length = rect->max[i] - rect->min[i];
        if (length > nlength) {
            nlength = length;
            axis = i;
        }
    }
    return axis;
}

static void RTREE_SYM(node_move_rect_at_index_into)(RTREE_NODE *from, int index, 
    RTREE_NODE *into)
{
    into->rects[into->count] = from->rects[index];
    from->rects[index] = from->rects[from->count-1];
    if (from->isleaf) {
        into->items[into->count] = from->items[index];
        from->items[index] = from->items[from->count-1];
    } else {
        into->nodes[into->count] = from->nodes[index];
        from->nodes[index] = from->nodes[from->count-1];
    }
    from->count--;
    into->count++;
}

static bool RTREE_SYM(node_split_largest_axis_edge_snap)(RTREE_RECT *rect,
    RTREE_NODE *node, RTREE_NODE **right_out, void *udata) 
{
    int axis = RTREE_SYM(rect_largest_axis)(rect);
    RTREE_NODE *right = RTREE_SYM(alloc_node)(node->isleaf, udata);
    if (!right) {
        return false;
    }
    right->height = node->height;
    for (int i = 0; i < node->count; i++) {
        RTREE_FLOAT min_dist = node->rects[i].min[axis] - rect->min[axis];
        RTREE_FLOAT max_dist = rect->max[axis] - node->rects[i].max[axis];
        if (max_dist < min_dist) {
            // move to right
            RTREE_SYM(node_move_rect_at_index_into)(node, i, right);
            i--;
        }
    }
    if (right->count == 0) {
        // There must be at least one item in a node.
        // Choose the one closest to the right (fmax) edge
        int j = 0;
        RTREE_FLOAT jdist = INFINITY;
        for (int i = 0; i < node->count; i++) {
            RTREE_FLOAT max_dist = rect->max[axis] - node->rects[i].max[axis];
            if (max_dist < jdist) {
                j = i;
                jdist = max_dist;
            }
        }
        RTREE_SYM(node_move_rect_at_index_into)(node, j, right);
    }
    *right_out = right;
    return true;
}

static bool RTREE_SYM(node_split)(RTREE_RECT *rect, RTREE_NODE *node,
    RTREE_NODE **right, void *udata) 
{
    return RTREE_SYM(node_split_largest_axis_edge_snap)(rect, node, right,
        udata);
}

RTREE_NOINLINE
static int RTREE_SYM(choose_least_enlargement)(RTREE_NODE *node,
    RTREE_RECT *ir)
{
    int j = 0;
    RTREE_FLOAT jenlarge = INFINITY;
    for (int i = 0; i < node->count; i++) {
        // calculate the enlarged area
        RTREE_FLOAT uarea = RTREE_SYM(rect_unioned_area)(&node->rects[i], ir);
        RTREE_FLOAT area = RTREE_SYM(rect_area)(&node->rects[i]);
        RTREE_FLOAT enlarge = uarea - area;
        if (enlarge < jenlarge) {
            j = i;
            jenlarge = enlarge;
        }
    }
    return j;
}

static int RTREE_SYM(choose)(RTREE_NODE *node, RTREE_RECT *rect) {
    // Take a quick look for the first node that contain the rect.
    for (int i = 0; i < node->count; i++) {
        if (RTREE_SYM(rect_contains)(&node->rects[i], rect)) {
            return i;
        }
    }
    // Fallback to using the "choose least enlargment" algorithm.
    return RTREE_SYM(choose_least_enlargement)(node, rect);
}

// returns INSERTED, SPLIT, or NOMEM
static int RTREE_SYM(node_insert)(RTREE_NODE *node, RTREE_RECT *ir,
    RTREE_ITEM data, void *udata, int depth)
{
    if (node->isleaf) {
        if (node->count == RTREE_MAXITEMS) {
            return RTREE_SPLIT;
        }
        int index = node->count;
        node->rects[index] = *ir;
        node->items[index] = data;
        node->count++;
        return RTREE_INSERTED;
    }
    // Choose a sub tree for inserting the rectangle.
    int i = RTREE_SYM(choose)(node, ir);
    RTREE_COW_NODE(&node->nodes[i]);
    int rc = RTREE_SYM(node_insert)(node->nodes[i], ir, data, udata, depth+1);
    if (rc == RTREE_INSERTED) {
        RTREE_SYM(rect_expand)(&node->rects[i], ir);
        return RTREE_INSERTED;
    }
    if (rc == RTREE_SPLIT) {
        // split the child node
        if (node->count == RTREE_MAXITEMS) {
            return RTREE_SPLIT;
        }
        rc = RTREE_NOMEM;
        RTREE_NODE *right;
        if (RTREE_SYM(node_split)(&node->rects[i], node->nodes[i], &right,
            udata))
        {
            node->rects[i] = RTREE_SYM(node_rect_calc)(node->nodes[i]);
            node->rects[node->count] = RTREE_SYM(node_rect_calc)(right);
            node->nodes[node->count] = right;
            node->count++;
            rc = RTREE_SYM(node_insert)(node, ir, data, udata, depth);
        }
    }
    return rc;
}

// returns INSERTED or NOMEM
static int RTREE_SYM(insert)(RTREE_NODE **root, RTREE_RECT *rect,
    RTREE_ITEM key, void *udata)
{
    if (!*root) {
        *root = RTREE_SYM(alloc_node)(true, udata);
        if (!*root) {
            return RTREE_NOMEM;
        }
        (*root)->height = 1;
    }
    RTREE_COW_NODE(root);
    int rc = RTREE_SYM(node_insert)(*root, rect, key, udata, 0);
    if (rc == RTREE_INSERTED) {
        if (!(*root)->isleaf) {
            (*root)->root_count++;
        }
        return RTREE_INSERTED;
    }
    if (rc != RTREE_SPLIT) {
        return rc;
    }
    // split root
    RTREE_NODE *newroot = RTREE_SYM(alloc_node)(false, udata);
    if (!newroot) {
        return RTREE_NOMEM;
    }
    RTREE_RECT root_rect = RTREE_SYM(node_rect_calc)(*root);
    RTREE_NODE *right;
    if (!RTREE_SYM(node_split)(&root_rect, *root, &right, udata)) {
        RTREE_SYM(free)(newroot, RTREE_NODE_SIZE(newroot), udata);
        return RTREE_NOMEM;
    }
    newroot->rects[0] = RTREE_SYM(node_rect_calc)(*root);
    newroot->rects[1] = RTREE_SYM(node_rect_calc)(right);
    newroot->nodes[0] = *root;
    newroot->nodes[1] = right;
    newroot->count = 2;
    newroot->height = (*root)->height+1;
    if ((*root)->isleaf) {
        newroot->root_count = newroot->nodes[0]->count + 
            newroot->nodes[1]->count;
    } else {
        newroot->root_count = (*root)->root_count;
        (*root)->root_count = 0;
    }
    *root = newroot;
    return RTREE_SYM(insert)(root, rect, key, udata);
}

static int RTREE_SYM(node_delete)(RTREE_NODE *node, RTREE_RECT *rect, 
    RTREE_ITEM key, RTREE_ITEM *olditem, void *udata)
{
    if (node->isleaf) {
        for (int i = 0; i < node->count; i++) {
            if (RTREE_SYM(rect_intersects)(&node->rects[i], rect) &&
                RTREE_SYM(compare)(node->items[i], key, udata) == 0)
            {
                if (olditem) {
                    *olditem = node->items[i];
                }
                node->rects[i] = node->rects[node->count-1];
                node->items[i] = node->items[node->count-1];
                node->count--;
                return RTREE_DELETED;
            }
        }
        return RTREE_NOTFOUND;
    }
    for (int i = 0; i < node->count; i++) {
        if (!RTREE_SYM(rect_contains)(&node->rects[i], rect)) {
            continue;
        }
        RTREE_COW_NODE(&node->nodes[i]);
        int rc = RTREE_SYM(node_delete)(node->nodes[i], rect, key, olditem,
            udata);
        if (rc == RTREE_NOTFOUND) {
            continue;
        }
        if (rc == RTREE_DELETED) {
            if (node->nodes[i]->count == 0) {
                RTREE_SYM(free)(node->nodes[i], RTREE_NODE_SIZE(node->nodes[i]),
                    udata);
                node->rects[i] = node->rects[node->count-1];
                node->nodes[i] = node->nodes[node->count-1];
                node->count--;
            } else if (RTREE_SYM(rect_onedge)(rect, &node->rects[i])) {
                node->rects[i] = RTREE_SYM(node_rect_calc)(node->nodes[i]);
            }
        }
        return rc;
    }
    return RTREE_NOTFOUND;
}

// returns DELETED, NOTFOUND, or NOMEM
static int RTREE_SYM(delete)(RTREE_NODE **root, RTREE_RECT *rect,
    RTREE_ITEM key, RTREE_ITEM *olditem, void *udata)
{
    if (!*root) {
        return RTREE_NOTFOUND;
    }
    RTREE_COW_NODE(root);
    int rc = RTREE_SYM(node_delete)(*root, rect, key, olditem, udata);
    if (rc != RTREE_DELETED) {
        return rc;
    }
    if ((*root)->isleaf) {
        if ((*root)->count == 0) {
            RTREE_SYM(free)(*root, RTREE_NODE_SIZE(*root), udata);
            *root = 0;
        }
    } else {
        (*root)->root_count--;
        while ((*root)->count == 1 && !(*root)->isleaf) {
            RTREE_NODE *prev = *root;
            *root = (*root)->nodes[0];
            if (!(*root)->isleaf) {
                (*root)->root_count = prev->root_count;
            }
            RTREE_SYM(free)(prev, RTREE_NODE_SIZE(prev), udata);
        }
    }
    return RTREE_DELETED;
}

#ifdef RTREE_NOITEMRECT
static inline bool RTREE_SYM(proc_iter)(RTREE_RECT *rect,
    RTREE_ITEM item, RTREE_ITERSIG, void *udata)
{
#if defined(RTREE_FLOAT16) || defined(RTREE_FLOAT32)
    double min[RTREE_DIMS];
    double max[RTREE_DIMS];
    for (int i = 0; i < RTREE_DIMS; i++) {
        min[i] = rect->min[i];
        max[i] = rect->max[i];
    }
    return iter(min, max, item, udata);
#else
    return iter(rect->min, rect->max, item, udata);
#endif
}
#define RTREE_PROC_ITER(i) \
    RTREE_SYM(proc_iter)(&node->rects[(i)], node->items[(i)], iter, udata)
#else
#define RTREE_PROC_ITER(i) iter(node->items[(i)], udata)
#endif

static bool RTREE_SYM(node_scan)(RTREE_NODE *node, RTREE_ITERSIG, void *udata){
    if (node->isleaf) {
        for (int i = 0; i < node->count; i++) {
            if (!RTREE_PROC_ITER(i)) {
                return false;
            }
        }
    } else {
        for (int i = 0; i < node->count; i++) {
            if (!RTREE_SYM(node_scan)(node->nodes[i], iter, udata)) {
                return false;
            }
        }
    }
    return true;
}

static bool RTREE_SYM(node_search)(RTREE_NODE *node, RTREE_RECT *rect, 
    RTREE_ITERSIG, void *udata)
{
    if (node->isleaf) {
        for (int i = 0; i < node->count; i++) {
            if (RTREE_SYM(rect_intersects)(rect, &node->rects[i])) {
                if (!RTREE_PROC_ITER(i)) {
                    return false;
                }
            }
        }
        return true;
    }
    for (int i = 0; i < node->count; i++) {
        if (RTREE_SYM(rect_intersects)(rect, &node->rects[i])) {
            if (!RTREE_SYM(node_search)(node->nodes[i], rect, iter, udata)) {
                return false;
            }
        }
    }
    return true;
}

static int RTREE_SYM(search)(RTREE_NODE **root, RTREE_RECT rect, 
    RTREE_ITERSIG, void *udata)
{
    int status = RTREE_FINISHED;
    if (*root) {
        if (!RTREE_SYM(node_search)(*root, &rect, iter, udata)) {
            status = RTREE_STOPPED;
        }
    }
    return status;
}

static int RTREE_SYM(scan)(RTREE_NODE **root, RTREE_ITERSIG, void *udata) {
    int status = RTREE_FINISHED;
    if (*root) {
        if (!RTREE_SYM(node_scan)(*root, iter, udata)) {
            status = RTREE_STOPPED;
        }
    }
    return status;
}

static size_t RTREE_SYM(count)(RTREE_NODE **root) {
    return !*root ? 0 : 
        (*root)->isleaf ? (unsigned)(*root)->count :
        (*root)->root_count;
}

/// Free the tree!
static void RTREE_SYM(clear)(RTREE_NODE **root, void *udata) {
    if (*root) {
        RTREE_SYM(node_free)(*root, udata);
        *root = 0;
    }
}

static int RTREE_SYM(copy)(RTREE_NODE **root, RTREE_NODE **newroot, void *udata)
{
    if (!*root) {
        if (newroot) {
            *newroot = 0;
        }
        return RTREE_COPIED;
    }
    RTREE_NODE *node2 = RTREE_SYM(node_copy)(*root, true, udata);
    if (!node2) {
        return RTREE_NOMEM;
    }
    if (newroot) {
        *newroot = node2;
    }
    return RTREE_COPIED;
}

static int RTREE_SYM(clone)(RTREE_NODE **root, RTREE_NODE **newroot,
    void *udata)
{
#ifndef RTREE_COW
    return RTREE_SYM(copy)(root, newroot, udata);
#else
    (void)udata;
    if (newroot) {
        *newroot = *root;
    }
    if (*root) {
        RTREE_SYM(rc_retain)(&(*root)->rc);
    }
#endif
    return RTREE_COPIED;
}

static void RTREE_SYM(rect)(RTREE_NODE **root, double min[RTREE_DIMS],
    RTREE_FLOAT max[RTREE_DIMS])
{
    RTREE_RECT rect = (RTREE_RECT){ 0 };
    if (*root) {
        rect = RTREE_SYM(node_rect_calc)(*root);
    }
    for (int i = 0; i < RTREE_DIMS; i++) {
        min[i] = rect.min[i];
        max[i] = rect.max[i];
    }
}

static size_t RTREE_SYM(height)(RTREE_NODE **root) {
    size_t height = 0;
    if (*root) {
        height = (*root)->height;
    }
    return height;
}

// returns the height of the node counting the depth, recursively
static int RTREE_SYM(deepheight)(RTREE_NODE *node) {
    int height = 0;
    while (1) {
        height++;
        if (node->isleaf) {
            return height;
        }
        node = node->nodes[0];
    }
}

// returns the height of the node counting the depth, recursively
static size_t RTREE_SYM(deepcount)(RTREE_NODE *node) {
    size_t count = 0;
    if (node->isleaf) {
        count += node->count;
    } else {
        for (int i = 0; i < node->count; i++) {
            count += RTREE_SYM(deepcount)(node->nodes[i]);
        }
    }
    return count;
}

static bool RTREE_SYM(sane0)(RTREE_NODE *node, void *udata, int depth) {
    // check the number of items in node.
    // the root is allowed to have one item.
    if (node->count < 1 || node->count > RTREE_MAXITEMS) {
        return false;
    }

    if (depth == 0) {
        if (!node->isleaf) {
            if (node->root_count == 0) {
                return false;
            }
            if (RTREE_SYM(deepcount)(node) != node->root_count) {
                return false;
            }
        }
    } else {
        if (!node->isleaf) {
            if (node->root_count != 0) {
                return false;
            }
        }
    }
    // only a leaf has a height on 1
    if (node->isleaf && node->height != 1) {
        return false;
    }
    if (!node->isleaf && node->height < 2) {
        return false;
    }
    // Check the height
    if (node->height != RTREE_SYM(deepheight)(node)) {
        return false;
    }
    if (!node->isleaf) {
        // check the sanity of child node
        for (int i = 0; i < node->count; i++) {
            RTREE_RECT rect0 = node->rects[i];
            RTREE_RECT rect1 = RTREE_SYM(node_rect_calc)(node->nodes[i]);
            if (memcmp(&rect0, &rect1, sizeof(RTREE_RECT)) != 0) {
                return false;
            }
            if (!RTREE_SYM(sane0)(node->nodes[i], udata, depth+1)) {
                return false;
            }
        }
    }
    return true;
}

// sanity checker
static bool RTREE_SYM(sane)(RTREE_NODE **root, void *udata) {
    bool sane = true;
    if (*root) {
        sane = RTREE_SYM(sane0)(*root, udata, 0);
    }
    return sane;
}

static const double RTREE_SYM(svg_scale) = 20.0;
static const char *RTREE_SYM(strokes)[] = { "black", "red", "green", "purple" };
static const int RTREE_SYM(nstrokes) = 4;

static void RTREE_SYM(node_write_svg)(RTREE_NODE *node, RTREE_RECT *rect, 
    FILE *f, int depth)
{
    bool point = rect->min[0] == rect->max[0] && rect->min[1] == rect->max[1];
    if (node) {
        if (!node->isleaf) {
            for (int i = 0; i < node->count; i++) {
                RTREE_SYM(node_write_svg)(node->nodes[i], &node->rects[i], f,
                    depth+1);
            }
        } else {
            for (int i = 0; i < node->count; i++) {
                RTREE_SYM(node_write_svg)(0, &node->rects[i], f, depth+1);
            }
        }
    }
    if (point) {
        double w = (rect->max[0]-rect->min[0]+1/RTREE_SYM(svg_scale))*
            RTREE_SYM(svg_scale)*10;
        fprintf(f, 
            "<rect x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" "
                "fill=\"%s\" fill-opacity=\"1\" "
                "rx=\"3\" ry=\"3\"/>\n",
            (rect->min[0])*RTREE_SYM(svg_scale)-w/2, 
            (rect->min[1])*RTREE_SYM(svg_scale)-w/2,
            w, w, 
            RTREE_SYM(strokes)[depth%RTREE_SYM(nstrokes)]);
    } else {
        fprintf(f, 
            "<rect x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" "
                "stroke=\"%s\" fill=\"%s\" "
                "stroke-width=\"%d\" "
                "fill-opacity=\"0\" stroke-opacity=\"1\"/>\n",
            (rect->min[0])*RTREE_SYM(svg_scale),
            (rect->min[1])*RTREE_SYM(svg_scale),
            (rect->max[0]-rect->min[0]+1/RTREE_SYM(svg_scale))*
                RTREE_SYM(svg_scale),
            (rect->max[1]-rect->min[1]+1/RTREE_SYM(svg_scale))*
                RTREE_SYM(svg_scale),
            RTREE_SYM(strokes)[depth%RTREE_SYM(nstrokes)],
            RTREE_SYM(strokes)[depth%RTREE_SYM(nstrokes)],
            1);
    }
}

// write_svg draws the R-tree to an SVG file. This is only useful with
// small geospatial 2D dataset. Not for production.
static void RTREE_SYM(write_svg)(RTREE_NODE **root, const char *path,
    void *udata)
{
    (void)udata;
    FILE *f = fopen(path, "wb+");
    fprintf(f, "<svg viewBox=\"%.0f %.0f %.0f %.0f\" " 
        "xmlns =\"http://www.w3.org/2000/svg\">\n",
        -190.0*RTREE_SYM(svg_scale), -100.0*RTREE_SYM(svg_scale),
        380.0*RTREE_SYM(svg_scale), 190.0*RTREE_SYM(svg_scale));
    fprintf(f, "<g transform=\"scale(1,-1)\">\n");
    if (*root) {
        RTREE_RECT rect = RTREE_SYM(node_rect_calc)(*root);
        RTREE_SYM(node_write_svg)(*root, &rect, f, 0);
    }
    fprintf(f, "</g>\n");
    fprintf(f, "</svg>\n");
    fclose(f);
}

static inline void RTREE_SYM(all_sym_calls)(void) {
    // All internal symbols
    (void)RTREE_SYM(all_sym_calls);
    (void)RTREE_SYM(itemrect);
    (void)RTREE_SYM(search);
    (void)RTREE_SYM(item_copy);
    (void)RTREE_SYM(item_free);
    (void)RTREE_SYM(node_copy);
    (void)RTREE_SYM(shared);
}

static inline void RTREE_SYM(all_api_calls)(void) {
    // All external symbols
    (void)RTREE_SYM(all_api_calls);
    (void)RTREE_API(feat_fanout);
    (void)RTREE_API(feat_cow);
    (void)RTREE_API(feat_atomics);
    (void)RTREE_API(insert);
    (void)RTREE_API(delete);
    (void)RTREE_API(search);
    (void)RTREE_API(scan);
    (void)RTREE_API(count);
    (void)RTREE_API(clear);
    (void)RTREE_API(copy);
    (void)RTREE_API(clone);
    (void)RTREE_API(rect);
    (void)RTREE_API(height);
    (void)RTREE_API(sane);
    (void)RTREE_API(write_svg);
    (void)RTREE_API(itemrect);
}

///////////////////////////////////////////////////////////////////////////////
// Exposed API
///////////////////////////////////////////////////////////////////////////////

#ifdef RTREE_NOITEMRECT 
int RTREE_API(insert)(RTREE_NODE **root, double min[RTREE_DIMS], 
    double max[RTREE_DIMS], RTREE_ITEM item, void *udata)
{
    RTREE_RECT rect = RTREE_SYM(fill_rect)(min, max);
    return RTREE_SYM(insert)(root, &rect, item, udata);
}
int RTREE_API(delete)(RTREE_NODE **root, double min[RTREE_DIMS], 
    double max[RTREE_DIMS], RTREE_ITEM key, RTREE_ITEM *olditem, 
    void *udata)
{
    RTREE_RECT rect = RTREE_SYM(fill_rect)(min, max);
    return RTREE_SYM(delete)(root, &rect, key, olditem, udata);
}
int RTREE_API(search)(RTREE_NODE **root, double min[RTREE_DIMS], 
    double max[RTREE_DIMS], RTREE_ITERSIG, void *udata)
{
    RTREE_RECT rect = RTREE_SYM(fill_rect)(min, max);
    return RTREE_SYM(search)(root, rect, iter, udata);
}
#else
int RTREE_API(insert)(RTREE_NODE **root, RTREE_ITEM item, void *udata) {
    RTREE_RECT rect = RTREE_SYM(fill_rect_item)(item, udata);
    return RTREE_SYM(insert)(root, &rect, item, udata);
}
int RTREE_API(delete)(RTREE_NODE **root, RTREE_ITEM key, RTREE_ITEM *olditem,
    void *udata)
{
    RTREE_RECT rect = RTREE_SYM(fill_rect_item)(key, udata);
    return RTREE_SYM(delete)(root, &rect, key, olditem, udata);
}
int RTREE_API(search)(RTREE_NODE **root, double min[RTREE_DIMS],
    double max[RTREE_DIMS], RTREE_ITERSIG, void *udata)
{
    RTREE_RECT rect = RTREE_SYM(fill_rect)(min, max);
    return RTREE_SYM(search)(root, rect, iter, udata);
}
#endif
int RTREE_API(scan)(RTREE_NODE **root, RTREE_ITERSIG, void *udata) {
    return RTREE_SYM(scan)(root, iter, udata);
}

size_t RTREE_API(count)(RTREE_NODE **root) {
    return RTREE_SYM(count)(root);
}

void RTREE_API(clear)(RTREE_NODE **root, void *udata) {
    RTREE_SYM(clear)(root, udata);
}

int RTREE_API(copy)(RTREE_NODE **root, RTREE_NODE **newroot, void *udata) {
    return RTREE_SYM(copy)(root, newroot, udata);
}

int RTREE_API(clone)(RTREE_NODE **root, RTREE_NODE **newroot, void *udata) {
    return RTREE_SYM(clone)(root, newroot, udata);
}

void RTREE_API(rect)(RTREE_NODE **root, double min[RTREE_DIMS],
    RTREE_FLOAT max[RTREE_DIMS])
{
    RTREE_SYM(rect)(root, min, max);
}

size_t RTREE_API(height)(RTREE_NODE **root) {
    return RTREE_SYM(height)(root);
}

bool RTREE_API(sane)(RTREE_NODE **root, void *udata) {
    return RTREE_SYM(sane)(root, udata);
}

void RTREE_API(write_svg)(RTREE_NODE **root, const char *path, void *udata) {
    RTREE_SYM(write_svg)(root, path, udata);
}

void RTREE_API(itemrect)(RTREE_ITEM item, double min[RTREE_DIMS], 
    double max[RTREE_DIMS], void *udata)
{
    RTREE_SYM(itemrect)(item, min, max, udata);
}

int RTREE_API(feat_fanout)(void) {
    return RTREE_SYM(feat_fanout)();
}
bool RTREE_API(feat_cow)(void) {
    return RTREE_SYM(feat_cow)();
}
bool RTREE_API(feat_atomics)(void) {
    return RTREE_SYM(feat_atomics)();
}

#endif // !RTREE_HEADER


// undefine everything
// use `gcc -dM -E <source>` to help find leftover RTREE_* defines
#undef RTREE_API
#undef RTREE_ASSERT
#undef RTREE_BRANCH_SIZE
#undef RTREE_C
#undef RTREE_CC
#undef RTREE_COMPARE
#undef RTREE_COPIED
#undef RTREE_COW
#undef RTREE_COW_NODE
#undef RTREE_DELETED
#undef RTREE_DIMS
#undef RTREE_EXTERN
#undef RTREE_FANOUT
#undef RTREE_FANOUTUSED
#undef RTREE_FINISHED
#undef RTREE_FLOAT
#undef RTREE_FREE
#undef RTREE_INLINE
#undef RTREE_INSERTED
#undef RTREE_ITEM
#undef RTREE_ITEMCOPY
#undef RTREE_ITEMFREE
#undef RTREE_ITEMRECT
#undef RTREE_ITERSIG
#undef RTREE_LEAF_SIZE
#undef RTREE_MALLOC
#undef RTREE_MAXITEMS
#undef RTREE_MINITEMS
#undef RTREE_NAME
#undef RTREE_NODE
#undef RTREE_NODE_SIZE
#undef RTREE_NOINLINE
#undef RTREE_NOMEM
#undef RTREE_NOTFOUND
#undef RTREE_PROC_ITER
#undef RTREE_RECT
#undef RTREE_SNODE
#undef RTREE_SPLIT
#undef RTREE_STOPPED
#undef RTREE_SYM
#undef RTREE_TYPE
#undef RTREE_UNSUPPORTED
