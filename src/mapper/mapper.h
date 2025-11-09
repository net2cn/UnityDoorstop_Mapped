#ifndef MAPPING_H
#define MAPPING_H

#include "../util/util.h"

/**
 * @brief Holds the mapping data for an IL2CPP symbol.
 */
typedef struct {
    // e.g., "il2cpp_init"
    char_t *original_name;
    // e.g., 0x1759e60 (File offset where the mapped name is stored)
    unsigned long read_offset;
    // e.g., "_RQluJpGVqK" (String read from the binary file)
    char_t *mapped_name;
} MapperEntry;

typedef struct {
    MapperEntry *entries;
    size_t count;
    size_t capacity; // Added capacity tracking for dynamic growth
} Mapper;

// --- Global Data (Internal) ---
// The global pointer is declared here but defined in mapping.h as an opaque
// type if needed. For simplicity, we define the full global struct pointer
// here, but users only access it through the get_mapped_il2cpp_name function.
extern Mapper *mapper;

/**
 * @brief Initializes the global mapping data by reading from mapping.txt and
 * UnityPlayer.dll. This should be called once at the start of the program.
 */
extern void load_mapper(void);

/**
 * @brief Frees all dynamically allocated memory used by the mapping system.
 * This should be called once at the end of the program.
 */
extern void cleanup_mapper(void);

/**
 * @brief Looks up the mapped symbol string (e.g., "_RQluJpGVqK") given the
 * original function name (e.g., "il2cpp_init").
 * * @param name The original function name to search for.
 * @return The mapped symbol string if found. Returns the original name if
 * mapping is not initialized or the name is not found.
 */
extern const char *get_mapped_player_name(const char *name);
#endif