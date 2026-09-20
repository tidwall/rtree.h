#include <stdio.h>
#include <string.h>
#include <math.h>


struct city {
    char *name;
    double lat;
    double lon;
};

void city_rect(struct city city, double min[2], double max[2]) {
    min[0] = max[0] = city.lon;
    min[1] = max[1] = city.lat;
}

#define RTREE_NAME cities
#define RTREE_TYPE struct city
#define RTREE_ITEMRECT city_rect(item, min, max);
#include "../rtree.h"

struct city phx = { .name = "Phoenix", .lat = 33.448, .lon = -112.073 };
struct city enn = { .name = "Ennis", .lat = 52.843, .lon = -8.986 };
struct city pra = { .name = "Prague", .lat = 50.088, .lon = 14.420 };
struct city tai = { .name = "Taipei", .lat = 25.033, .lon = 121.565 };
struct city her = { .name = "Hermosillo", .lat = 29.102, .lon = -110.977 };
struct city him = { .name = "Himeji", .lat = 34.816, .lon = 134.700 };

bool city_iter(struct city city, void *udata) {
    printf("%s\n", city.name);
    return true;
}

int main() {
    // Create a new rtree variable
    struct cities *tr = 0;

    // Load some cities into the rtree.
    cities_insert(&tr, phx, 0);
    cities_insert(&tr, enn, 0);
    cities_insert(&tr, pra, 0);
    cities_insert(&tr, tai, 0);
    cities_insert(&tr, her, 0);
    cities_insert(&tr, him, 0);
    
    // Search for cities
    printf("\n-- Northwestern cities --\n");
    cities_search(&tr, (double[2]){-180, 0}, (double[2]){0, 90}, city_iter, 0);

    printf("\n-- Northeastern cities --\n");
    cities_search(&tr, (double[2]){0, 0}, (double[2]){180, 90}, city_iter, 0);

    // Deleting an item is similar inserting, except that the second arg is a
    // key and the third is a pointer to the deleted data.
    printf("\n-- Delete Phoenix --\n");
    struct city old;
    cities_delete(&tr, phx, &old, 0);
    printf("%s deleted\n", old.name);

    printf("\n-- Northwestern cities --\n");
    cities_search(&tr, (double[2]){-180, 0}, (double[2]){0, 90}, city_iter, 0);

    cities_clear(&tr, 0);
}
// output:
// -- Northwestern cities --
// Phoenix
// Hermosillo
// Ennis
// 
// -- Northeastern cities --
// Prague
// Taipei
// Himeji
// 
// -- Northwestern cities --
// Ennis
// Hermosillo
