#include <stdio.h>
#include <limits.h>
#include <stdint.h>

int main(void) {
    printf("=== C Toolchain Environment Diagnostic ===\n");
#if defined(__STDC_VERSION__)
    printf("C Standard Version (__STDC_VERSION__): %ldL\n", (long)__STDC_VERSION__);
#if __STDC_VERSION__ >= 202311L
    printf("Target Standard: ISO C23\n");
#elif __STDC_VERSION__ >= 201710L
    printf("Target Standard: ISO C17\n");
#elif __STDC_VERSION__ >= 201112L
    printf("Target Standard: ISO C11\n");
#elif __STDC_VERSION__ >= 199901L
    printf("Target Standard: ISO C99\n");
#else
    printf("Target Standard: ISO C90/C89\n");
#endif
#else
    printf("C Standard Version: Pre-C89 / K&R\n");
#endif

#if defined(__GNUC__)
    printf("Compiler: GNU GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(__clang__)
    printf("Compiler: Clang %s\n", __clang_version__);
#elif defined(_MSC_VER)
    printf("Compiler: MSVC %d\n", _MSC_VER);
#else
    printf("Compiler: Unknown / Conforming C\n");
#endif

    printf("\n--- Platform Data Model & Sizing ---\n");
    printf("sizeof(char):       %zu byte (CHAR_BIT = %d)\n", sizeof(char), CHAR_BIT);
    printf("sizeof(short):      %zu bytes\n", sizeof(short));
    printf("sizeof(int):        %zu bytes\n", sizeof(int));
    printf("sizeof(long):       %zu bytes\n", sizeof(long));
    printf("sizeof(long long):  %zu bytes\n", sizeof(long long));
    printf("sizeof(void *):     %zu bytes (Pointer Width: %zu-bit)\n", sizeof(void *), sizeof(void *) * 8);

    if (sizeof(long) == 4 && sizeof(void *) == 8) {
        printf("Data Model: LLP64 (Standard 64-bit Windows)\n");
    } else if (sizeof(long) == 8 && sizeof(void *) == 8) {
        printf("Data Model: LP64 (Standard 64-bit Linux / macOS)\n");
    } else if (sizeof(int) == 4 && sizeof(long) == 4 && sizeof(void *) == 4) {
        printf("Data Model: ILP32 (Standard 32-bit Architecture)\n");
    } else {
        printf("Data Model: Other / Custom\n");
    }

    return 0;
}
