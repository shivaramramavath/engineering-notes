#include <stdio.h>

/* Simulating modular project structure */
typedef struct {
    int id;
    const char *module_name;
} Subsystem;

static void subsystem_init(Subsystem *sub, int id, const char *name) {
    sub->id = id;
    sub->module_name = name;
}

static void subsystem_print(const Subsystem *sub) {
    printf("Subsystem [ID: %d, Module: %s] initialized.\n", sub->id, sub->module_name);
}

int main(void) {
    printf("=== Modular Project Architecture Demo ===\n");
    Subsystem core;
    subsystem_init(&core, 1, "NetworkingEngine");
    subsystem_print(&core);
    return 0;
}
