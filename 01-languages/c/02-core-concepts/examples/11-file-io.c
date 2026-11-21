#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Record {
    int id;
    char name[20];
    double score;
} Record;

int main(void) {
    printf("=== C File I/O Demo ===\n");
    const char *text_filename = "batch8_temp_test.txt";
    const char *bin_filename = "batch8_temp_records.bin";

    /* 1. Formatted Text Write */
    FILE *ft = fopen(text_filename, "w");
    if (!ft) {
        perror("Failed to open text file for writing");
        return 1;
    }
    fprintf(ft, "Line 1: ISO C17 Standard\nLine 2: Systems Programming\n");
    fclose(ft);

    /* 2. Text Read via fgets */
    ft = fopen(text_filename, "r");
    if (!ft) {
        perror("Failed to open text file for reading");
        return 1;
    }
    char buffer[128];
    printf("Reading text file:\n");
    while (fgets(buffer, sizeof buffer, ft)) {
        buffer[strcspn(buffer, "\r\n")] = '\0'; /* Strip newline */
        printf("  Read: [%s]\n", buffer);
    }
    fclose(ft);
    remove(text_filename);

    /* 3. Binary Write and Read */
    Record out_rec = {101, "Alice Engineer", 98.5};
    FILE *fb = fopen(bin_filename, "wb");
    if (!fb) {
        perror("Failed to open binary file for writing");
        return 1;
    }
    fwrite(&out_rec, sizeof(Record), 1, fb);
    fclose(fb);

    Record in_rec = {0, "", 0.0};
    fb = fopen(bin_filename, "rb");
    if (!fb) {
        perror("Failed to open binary file for reading");
        return 1;
    }
    size_t items_read = fread(&in_rec, sizeof(Record), 1, fb);
    fclose(fb);
    remove(bin_filename);

    if (items_read == 1) {
        printf("Binary read verified: ID=%d, Name=%s, Score=%.1f\n",
               in_rec.id, in_rec.name, in_rec.score);
    }

    return 0;
}
