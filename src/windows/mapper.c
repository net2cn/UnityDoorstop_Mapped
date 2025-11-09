#include "../mapper/mapper.h"
#include "../crt.h"
#include "../util/logging.h"
#include "../util/util.h"

#define MAPPING_CONFIG_NAME TEXT("mapper.txt")
#define MAPPING_BINARY_NAME TEXT("UnityPlayer.dll")

/**
 * @brief Removes leading and trailing whitespace from a string.
 */
static char_t *trim_whitespace(char_t *str) {
    if (!str)
        return NULL;

    // Trim leading space: using L'' literals is correct for wide chars
    while (*str == L' ' || *str == L'\t')
        str++;

    // Trim trailing space
    size_t length = strlen(str);
    if (length == 0) {
        // Handle empty string after leading trim
        return str;
    }

    // Set 'end' to the last character of the current string
    char_t *end = str + length - 1;

    // Loop backward while pointer is past the start and character is whitespace
    while (end > str &&
           (*end == L' ' || *end == L'\t' || *end == L'\n' || *end == L'\r'))
        end--;

    // Write new null terminator after the last non-whitespace character
    *(end + 1) = (char_t)L'\0';

    return str;
}

static const char *read_mapped_symbol(void *file, unsigned long offset) {
    if (!file) {
        LOG("Error: File pointer is NULL.\n");
        return NULL;
    }

    // Attempt to seek to the offset
    if (fseek(file, (long)offset, SEEK_SET) != 0) {
        LOG("Error: Failed to seek to offset 0x%x.", offset);
        return NULL;
    }

    size_t capacity = 32;
    size_t length = 0;
    char *buffer = (char *)malloc(capacity * sizeof(char));
    if (!buffer) {
        LOG("Error: Failed to allocate initial buffer for symbol string.\n");
        return NULL;
    }

    char c;
    while ((c = fgetc(file)) != -1 && c != '\0') {

        // Check if we need to resize the buffer
        if (length + 1 >= capacity) {
            capacity *= 2;
            char *new_buffer = (char *)realloc(buffer, capacity * sizeof(char));
            if (!new_buffer) {
                LOG("Error: Failed to reallocate buffer while "
                    "reading symbol.\n");
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }
        buffer[length++] = c;
    }

    // Null-terminate the string, resizing the buffer one last time if necessary
    char *final_buffer = (char *)realloc(buffer, (length + 1) * sizeof(char));
    if (!final_buffer) {
        // If final realloc fails, return the existing buffer (it's slightly
        // over-allocated but safe)
        buffer[length] = '\0';
        return buffer;
    }

    final_buffer[length] = '\0';
    return final_buffer;
}

/**
 * @brief Reads data from the config file and binary directly into
 * the global mapper_store.
 *
 * NOTE: This function must only be called when mapper_store is NULL.
 */
static inline void load_mapper_to_global_store(const char *mapper_config_name,
                                               const char *read_binary_name) {
    void *map_file = NULL;
    void *binary_file = NULL;
    char_t line[256];
    size_t initial_capacity = 100;

    // Open the mapper file for reading
    map_file = fopen((char *)mapper_config_name, "r");
    if (map_file == (void *)INVALID_HANDLE_VALUE) { // Use explicit handle check
                                                    // since fopen is wrapper
        LOG("Error: Could not open mapper file '%S'.", mapper_config_name);
        mapper = NULL;
        return;
    }

    // Open the binary file for reading the mapped symbol strings
    binary_file = fopen((char *)read_binary_name, "rb"); // "rb" for binary read
    if (binary_file == (void *)INVALID_HANDLE_VALUE) {
        LOG("Warning: Could not open binary file '%S'. Symbol "
            "strings will not be read.",
            read_binary_name);
        binary_file = NULL; // Ensure it's NULL if handle is invalid
        return;
    }

    // 1. Allocate the global mapper store container
    mapper = (Mapper *)calloc(1, sizeof(Mapper));
    if (mapper == NULL) {
        LOG("Fatal Error: Failed to allocate memory for global mapper "
            "container.");
        return;
    }

    // 2. Allocate the initial mapper array inside the container
    mapper->capacity = initial_capacity;
    mapper->entries =
        (MapperEntry *)malloc(initial_capacity * sizeof(MapperEntry));

    if (mapper->entries == NULL) {
        LOG("Fatal Error: Failed to allocate initial memory for mapper "
            "array.");
        free(mapper);
        mapper = NULL;
        return;
    }

    // Read the file line by line
    while (fgets(line, sizeof(line), map_file) != NULL) {

        // Dynamically increase array size if capacity is reached
        if (mapper->count >= mapper->capacity) {
            mapper->capacity *= 2;
            MapperEntry *new_ptr = (MapperEntry *)realloc(
                mapper->entries, mapper->capacity * sizeof(MapperEntry));

            if (new_ptr == NULL) {
                LOG("Error: Failed to reallocate memory for entries. "
                    "Stopping at %d entries.",
                    mapper->count);
                break;
            }
            mapper->entries = new_ptr;
        }

        char_t *token;
        char_t *line_ptr = line;
        char_t *tokens[5];
        int token_index = 0;

        // Tokenize the line using ',' as the delimiter
        while ((token = strsep(&line_ptr, L",")) != NULL && token_index < 5) {
            tokens[token_index++] = token;
        }

        // Check if we successfully parsed exactly 5 fields
        if (token_index == 5) {
            // Get the current mapper entry pointer
            MapperEntry *current_mapper = &mapper->entries[mapper->count];

            // --- Field 2: original_name (The name, tokens[1]) ---
            char_t *name_token = trim_whitespace(tokens[1]);
            current_mapper->original_name = narrow(strdup(name_token));
            if (current_mapper->original_name == NULL) {
                LOG("Error: Memory allocation failed for name string.");
            }

            // --- Field 5: read_offset (The file offset, tokens[4]) ---
            char_t *offset_token = trim_whitespace(tokens[4]);
            unsigned long offset_long = strtoul(offset_token, NULL, 16);
            current_mapper->read_offset = offset_long;

            // --- Field 3: mapped_name (Read string from binary file) ---
            current_mapper->mapped_name =
                read_mapped_symbol(binary_file, offset_long);

            // Safety: If reading failed, use an empty string to ensure a valid
            // pointer for later freeing.
            if (current_mapper->mapped_name == NULL) {
                current_mapper->mapped_name = "";
            }

            mapper->count++;
        }
    }

    // Close the files
    fclose(map_file);
    fclose(binary_file);

    // Shrink the array to the exact size of the count
    if (mapper->count > 0 && mapper->entries != NULL) {
        MapperEntry *new_ptr = (MapperEntry *)realloc(
            mapper->entries, mapper->count * sizeof(MapperEntry));
        if (new_ptr != NULL) {
            mapper->entries = new_ptr;
            mapper->capacity = mapper->count;
        } else {
            LOG("Warning: Failed to shrink array, memory may be "
                "slightly over-allocated.");
        }
    }
}

// --- Public Function Implementations ---
const char *get_mapped_player_name(const char *name) {
    if (!mapper) {
        LOG("Error: No entries loaded, cannot read from mapper.");
        return name;
    }

    for (size_t i = 0; i < mapper->count; ++i) {
        if (lstrcmpiA(mapper->entries[i].original_name, name) == 0) {
            return mapper->entries[i].mapped_name;
        }
    }

    // Name not found, return the original name
    return name;
}

void load_mapper() {
    if (mapper != NULL) {
        LOG("Warning: Mappers already initialized. Skipping "
            "re-initialization.");
        return;
    }

    char_t *config_path = get_full_path(MAPPING_CONFIG_NAME);
    char_t *binary_path = get_full_path(MAPPING_BINARY_NAME);

    if (!file_exists(config_path)) {
        LOG("Error: Could not find config file '%S'.", config_path);
        return;
    }

    if (!file_exists(binary_path)) {
        LOG("Error: Could not find binary file '%S'.", binary_path);
        return;
    }

    // Call the void function that loads directly into the global store
    load_mapper_to_global_store(MAPPING_CONFIG_NAME, MAPPING_BINARY_NAME);

    if (mapper != NULL && mapper->count > 0) {
        LOG("Mapper initialization successful. Loaded %d entries.",
            mapper->count);
    } else {
        LOG("Mapper initialization failed: No entries loaded or memory "
            "allocation failed.");
    }

    for (size_t i = 0; i < mapper->count; ++i) {
        LOG("Entry %d: %S -> %S", i, mapper->entries[i].original_name,
            mapper->entries[i].mapped_name);
    }

    free(config_path);
    free(binary_path);
}