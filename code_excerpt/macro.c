
#define ERR(...) do { \
    printf("%s: %d (%s) ", __FILE__, __LINE__, __FUNCTION__); \
    printf(__VA_ARGS__); \
} while (0)

