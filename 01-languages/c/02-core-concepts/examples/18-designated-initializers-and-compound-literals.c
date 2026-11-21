#include <stdio.h>

typedef struct Config {
    int port;
    const char *host;
    int timeout_ms;
    int max_clients;
} Config;

static void print_config(const Config *cfg) {
    printf("Config: host=%s, port=%d, timeout=%dms, max_clients=%d\n",
           cfg->host, cfg->port, cfg->timeout_ms, cfg->max_clients);
}

int main(void) {
    printf("=== C Designated Initializers & Compound Literals Demo ===\n");

    /* 1. Designated initializer for struct */
    Config srv = {
        .host = "127.0.0.1",
        .port = 8080,
        .max_clients = 100
        /* timeout_ms omitted -> zero initialized */
    };
    print_config(&srv);

    /* 2. Designated initializer for array */
    int sparse[8] = {[0] = 1, [3] = 42, [7] = 99};
    printf("Sparse array: [0]=%d, [1]=%d, [3]=%d, [7]=%d\n",
           sparse[0], sparse[1], sparse[3], sparse[7]);

    /* 3. Compound literal passed directly to function */
    print_config(&(Config){
        .host = "api.internal",
        .port = 443,
        .timeout_ms = 5000,
        .max_clients = 500
    });

    return 0;
}
