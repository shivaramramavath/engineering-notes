#include <stdio.h>

static void demonstrate_switch(int command) {
    switch (command) {
        case 1:
            printf("Action: START\n");
            break;
        case 2:
            printf("Action: PAUSE\n");
            break;
        case 3:
            printf("Action: RESUME (falling through to STOP)\n");
            /* Intentional fallthrough */
            /* In C23: [[fallthrough]]; */
        case 4:
            printf("Action: STOP\n");
            break;
        default:
            printf("Action: UNKNOWN\n");
            break;
    }
}

int main(void) {
    printf("=== Control Flow & Switch Demonstration ===\n");
    demonstrate_switch(1);
    demonstrate_switch(3);

    /* Goto cleanup idiom demonstration */
    int error_code = 0;
    int *buffer = NULL;
    
    if (error_code != 0) {
        goto cleanup;
    }
    printf("Processing completed successfully without error.\n");

cleanup:
    /* Centralized resource teardown */
    (void)buffer;
    return 0;
}
