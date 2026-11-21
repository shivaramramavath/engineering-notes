#include <stdio.h>
#include <stdbool.h>

/* Bitmask flags enumeration */
typedef enum FilePermission {
    PERM_READ    = 1 << 0, /* 0x01 */
    PERM_WRITE   = 1 << 1, /* 0x02 */
    PERM_EXECUTE = 1 << 2, /* 0x04 */
    PERM_ADMIN   = 1 << 3  /* 0x08 */
} FilePermission;

static bool has_permission(int user_perms, FilePermission req) {
    return (user_perms & req) == req;
}

int main(void) {
    printf("=== C Enums and Flags Demo ===\n");

    /* Combine permissions with bitwise OR */
    int current_perms = PERM_READ | PERM_WRITE;
    printf("Permissions bitmask: 0x%02X\n", current_perms);

    printf("Has READ: %s\n", has_permission(current_perms, PERM_READ) ? "YES" : "NO");
    printf("Has WRITE: %s\n", has_permission(current_perms, PERM_WRITE) ? "YES" : "NO");
    printf("Has EXECUTE: %s\n", has_permission(current_perms, PERM_EXECUTE) ? "YES" : "NO");

    /* Grant EXECUTE */
    current_perms |= PERM_EXECUTE;
    printf("After granting EXECUTE: 0x%02X (Has EXECUTE: %s)\n",
           current_perms, has_permission(current_perms, PERM_EXECUTE) ? "YES" : "NO");

    /* Revoke WRITE */
    current_perms &= ~PERM_WRITE;
    printf("After revoking WRITE: 0x%02X (Has WRITE: %s)\n",
           current_perms, has_permission(current_perms, PERM_WRITE) ? "YES" : "NO");

    return 0;
}
