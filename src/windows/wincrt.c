#include "wincrt.h"
#include "../windows/logger.h"
static HANDLE h_heap;

void init_crt() { h_heap = GetProcessHeap(); }

#pragma function(memset)
void *memset(void *dst, int c, size_t n) {
    char *d = dst;
    while (n--)
        *d++ = (char)c;
    return dst;
}

#pragma function(memcpy)
void *memcpy(void *dst, const void *src, size_t n) {
    char *d = dst;
    const char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *dlsym(void *handle, const char *name) {
    return GetProcAddress((HMODULE)handle, name);
}

void *dlopen(const char_t *filename, int flag) { return LoadLibrary(filename); }

void free(void *mem) { HeapFree(h_heap, 0, mem); }

int setenv(const char_t *name, const char_t *value, int overwrite) {
    return !SetEnvironmentVariable(name, value);
}

size_t strlen_wide(char_t const *str) {
    size_t result = 0;
    while (*str++)
        result++;
    return result;
}

void *malloc(size_t size) {
    return HeapAlloc(h_heap, HEAP_GENERATE_EXCEPTIONS, size);
}

// --- Implementation for realloc (New) ---
void *realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    // HEAP_REALLOC_IN_PLACE_ONLY can be removed if in-place is not required
    return HeapReAlloc(h_heap, 0, ptr, size);
}

void *calloc(size_t num, size_t size) {
    return HeapAlloc(h_heap, HEAP_ZERO_MEMORY, size * num);
}

char_t *strcat_wide(char_t *dst, const char_t *src) {
    size_t size = strlen(dst);
    size_t size2 = strlen(src);
    return strncpy(dst + size, src, size2 + 1);
}

char_t *strcpy_wide(char_t *dst, const char_t *src) {
    char_t *d = dst;
    const char_t *s = src;
    while (*s)
        *d++ = *s++;
    *d = *s;
    return dst;
}

char_t *strncpy_wide(char_t *dst, const char_t *src, size_t n) {
    char_t *d = dst;
    const char_t *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

char_t *strsep_wide(char_t **stringp, const char_t *delim) {
    char_t *rv = *stringp;

    if (rv) {
        size_t len = strcspn(*stringp, delim);
        *stringp += len;

        if (**stringp) {
            *(*stringp)++ = (char_t)L'\0';
        } else {
            *stringp = NULL;
        }
    }
    return rv;
}

// --- Implementation for strdup (New) ---
char_t *strdup_wide(const char_t *str) {
    if (str == NULL) {
        return NULL;
    }

    // 1. Get the number of characters
    size_t len = strlen(str);

    // 2. Allocate memory: (length + 1 for null terminator) * sizeof(char_t)
    // bytes
    char_t *new_str = (char_t *)malloc((len + 1) * sizeof(char_t));

    if (new_str) {
        // 3. Copy content: copy (length + 1) characters, multiplied by
        // sizeof(char_t) to get the total number of bytes.
        memcpy(new_str, str, (len + 1) * sizeof(char_t));
    }
    return new_str;
}

// --- Implementation for strcspn (New) ---
size_t strcspn_wide(const char_t *str1, const char_t *str2) {
    const char_t *p;
    const char_t *s;

    // Iterate through every character in str1
    for (p = str1; *p; p++) {
        // For each character in str1, check against every delimiter in str2
        for (s = str2; *s; s++) {
            if (*p == *s) {
                // First match found: return the distance from the start
                size_t len = p - str1;
                return len;
            }
        }
    }
    // No match found: return the length of str1
    return p - str1;
}

// --- Implementation for strtoul (New) ---
// Using RtlUlonglongToUnicodeString or similar is complex.
// For simplicity, we use a basic hex/base conversion, assuming the input format
// is reliable.
unsigned long strtoul_wide(const char_t *nptr, char_t **endptr, int base) {
    // Only support base 16 for IL2CPP offsets, as requested.
    if (base != 16) {
        if (endptr) {
            *endptr = (char_t *)nptr;
        }
        return 0;
    }

    unsigned long result = 0;
    const char_t *p = nptr;

    // --- 1. Skip optional "0x" prefix ---
    // Note the use of wide character literals (L'0', L'x')
    if (*p == L'0' && (*(p + 1) == L'x' || *(p + 1) == L'X')) {
        p += 2;
    }

    // --- 2. Main parsing loop ---
    while (*p) {
        char_t c = *p;
        // LOG("Processing character: %s", c);
        int digit;

        // Note the use of wide character literals (L'0', L'a', L'F', etc.)
        if (c >= L'0' && c <= L'9') {
            digit = c - L'0';
        } else if (c >= L'a' && c <= L'f') {
            digit = c - L'a' + 10;
        } else if (c >= L'A' && c <= L'F') {
            digit = c - L'A' + 10;
        } else {
            break; // Non-hex character, stop parsing
        }

        // Check for potential overflow (simple check for now)
        if (result > (ULONG_MAX >> 4)) {
            // In a full implementation, you'd set errno and return ULONG_MAX.
            // For this embedded context, we just break or cap.
            break;
        }

        result = (result << 4) | digit; // Multiply by 16 and add digit
        p++;
    }

    // --- 3. Set the end pointer ---
    if (endptr) {
        *endptr = (char_t *)p;
    }

    return result;
}

char_t *getenv_wide(const char_t *name) {
    DWORD size = GetEnvironmentVariable(name, NULL, 0);
    if (size == 0)
        return NULL;
    char_t *buf = calloc(size + 1, sizeof(char_t));
    GetEnvironmentVariable(name, buf, size + 1);
    return buf;
}

void shutenv(char_t *buf) {
    if (buf) {
        free(buf);
    }
}

void *fopen(char_t *filename, const char_t *mode) {
    return CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

size_t fread(void *ptr, size_t size, size_t count, void *stream) {
    size_t read_size = 0;
    ReadFile(stream, ptr, size * count, &read_size, NULL);
    return read_size;
}

int fclose(void *stream) { CloseHandle(stream); }

// --- Implementation for fseek (New) ---
int fseek(void *stream, long offset, int origin) {
    if (stream == (void *)INVALID_HANDLE_VALUE)
        return -1;

    DWORD dwMoveMethod;
    if (origin == SEEK_SET) {
        dwMoveMethod = FILE_BEGIN;
    } else if (origin == SEEK_CUR) {
        dwMoveMethod = FILE_CURRENT;
    } else if (origin == SEEK_END) {
        dwMoveMethod = FILE_END;
    } else {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }

    // SetFilePointer moves the file pointer. High part is NULL since we use
    // long offset.
    if (SetFilePointer((HANDLE)stream, offset, NULL, dwMoveMethod) ==
        INVALID_SET_FILE_POINTER) {
        if (GetLastError() != NO_ERROR) {
            return -1;
        }
    }
    return 0; // Success
}

// --- Implementation for fgetc (New) ---
int fgetc(void *stream) {
    char c;
    DWORD bytes_read = 0;

    if (stream == (void *)INVALID_HANDLE_VALUE)
        return EOF;

    if (!ReadFile((HANDLE)stream, &c, 1, &bytes_read, NULL)) {
        return EOF;
    }

    if (bytes_read == 0) {
        return EOF;
    }

    return (int)(unsigned char)c;
}

// --- Implementation for fgets (New) ---
char_t *fgetws(char_t *s, int n, void *stream) {
    if (stream == (void *)INVALID_HANDLE_VALUE || n <= 0)
        return NULL;

    int i = 0;
    char_t *p = s;

    int c = -1;
    while (i < n - 1) {
        c = fgetc(stream); // Use our custom fgetc

        if (c == EOF) {
            break;
        }

        *p++ = (char)c;
        i++;

        if (c == L'\n') {
            break;
        }
    }

    if (i == 0 && c == EOF) {
        return NULL; // Nothing read and end of file reached immediately
    }

    *p = L'\0';
    return s;
}

#ifndef UNICODE
LPSTR *CommandLineToArgvA(LPCSTR cmd_line, int *argc) {
    ULONG len = strlen(cmd_line);
    ULONG i = ((len + 2) / 2) * sizeof(LPVOID) + sizeof(LPVOID);

    LPSTR *argv =
        (LPSTR *)GlobalAlloc(GMEM_FIXED, i + (len + 2) * sizeof(CHAR));

    LPSTR _argv = (LPSTR)(((PUCHAR)argv) + i);

    ULONG _argc = 0;
    argv[_argc] = _argv;
    BOOL in_qm = FALSE;
    BOOL in_text = FALSE;
    BOOL in_space = TRUE;
    ULONG j = 0;
    i = 0;

    CHAR a;
    while ((a = cmd_line[i])) {
        if (in_qm) {
            if (a == '\"') {
                in_qm = FALSE;
            } else {
                _argv[j] = a;
                j++;
            }
        } else {
            switch (a) {
            case '\"':
                in_qm = TRUE;
                in_text = TRUE;
                if (in_space) {
                    argv[_argc] = _argv + j;
                    _argc++;
                }
                in_space = FALSE;
                break;
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                if (in_text) {
                    _argv[j] = '\0';
                    j++;
                }
                in_text = FALSE;
                in_space = TRUE;
                break;
            default:
                in_text = TRUE;
                if (in_space) {
                    argv[_argc] = _argv + j;
                    _argc++;
                }
                _argv[j] = a;
                j++;
                in_space = FALSE;
                break;
            }
        }
        i++;
    }
    _argv[j] = '\0';
    argv[_argc] = NULL;

    (*argc) = _argc;
    return argv;
}
#endif