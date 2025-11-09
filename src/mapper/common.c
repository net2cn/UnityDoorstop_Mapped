#include "../crt.h"
#include "../util/logging.h"
#include "mapper.h"

Mapper *mapper = NULL;

void cleanup_mapper() {
#define FREE_NON_NULL(val)                                                     \
    if (val != NULL) {                                                         \
        free(val);                                                             \
        val = NULL;                                                            \
    }

    if (mapper && mapper->entries) {
        for (size_t i = 0; i < mapper->count; ++i) {
            FREE_NON_NULL(mapper->entries[i].original_name);
            FREE_NON_NULL(mapper->entries[i].mapped_name);
        }
        FREE_NON_NULL(mapper->entries);
    }
    if (mapper) {
        FREE_NON_NULL(mapper);
        LOG("Global mappings successfully freed and reset.");
    }

#undef FREE_NON_NULL
}