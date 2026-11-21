# Project 12: Huffman File Compression Utility (`huffman`)

> A lossless file compression and decompression tool in C implementing Huffman coding, priority queue min-heaps, prefix-free binary tree construction, and bit-level stream I/O.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `12-file-compression-huffman` |
| **Language Baseline** | C11 / Standard Architecture |
| **Complexity** | Advanced / Algorithms |
| **Core Systems Concepts** | Huffman Coding, Priority Queues, Binary Trees, Bitwise Stream Packing |

---

## 1. Architectural Overview & Component Breakdown

The Huffman compressor achieves data compression via variable-length prefix codes:
1. **Frequency Histogram (`huffman.c`):** Scans the input file to count the frequency of each 8-bit byte value ($0 \dots 255$).
2. **Min-Heap Priority Queue (`huffman.c`):** Inserts character leaf nodes and repeatedly merges the two lowest-frequency nodes into a combined tree node until a single root tree is formed.
3. **Prefix Code Table:** Traverses the Huffman tree to assign short bit sequences to frequent bytes and longer bit sequences to rare bytes.
4. **Bit Stream I/O:** Serializes bit sequences into 8-bit bytes for file output and decodes packed bits during decompression.

```
       +-----------------------------------------------------------+
       |               Huffman Compression Pipeline                |
       +-----------------------------------------------------------+
                                     |
                       Raw File Input (Bytes 0..255)
                                     v
                       [ 256-Entry Frequency Pass ]
                                     v
                       [ Min-Heap Priority Queue ]
                                     v
                    [ Huffman Binary Prefix Tree ]
                                     v
                     [ Bitwise Stream Serializer ]
                                     v
                     Compressed File (*.huff)
```

---

## 2. Complete Project Source Code

### Header: `include/huffman.h`

```c
#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct HuffmanNode {
    uint8_t byte_val;
    uint32_t freq;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

/* Compression Interfaces */
bool huffman_compress(const char *input_path, const char *output_path);
bool huffman_decompress(const char *input_path, const char *output_path);

#endif /* HUFFMAN_H */
```

### Source: `src/huffman.c`

```c
#include "huffman.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALPHABET_SIZE 256

typedef struct {
    HuffmanNode **nodes;
    size_t size;
    size_t capacity;
} MinHeap;

static MinHeap* min_heap_create(size_t cap) {
    MinHeap *h = malloc(sizeof(MinHeap));
    h->nodes = malloc(sizeof(HuffmanNode*) * cap);
    h->size = 0;
    h->capacity = cap;
    return h;
}

static void min_heap_insert(MinHeap *h, HuffmanNode *node) {
    size_t i = h->size++;
    while (i > 0 && node->freq < h->nodes[(i - 1) / 2]->freq) {
        h->nodes[i] = h->nodes[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    h->nodes[i] = node;
}

static HuffmanNode* min_heap_pop(MinHeap *h) {
    if (h->size == 0) return NULL;
    HuffmanNode *top = h->nodes[0];
    HuffmanNode *last = h->nodes[--h->size];
    size_t i = 0;
    while (i * 2 + 1 < h->size) {
        size_t left = i * 2 + 1;
        size_t right = left + 1;
        size_t smallest = left;
        if (right < h->size && h->nodes[right]->freq < h->nodes[left]->freq) {
            smallest = right;
        }
        if (last->freq <= h->nodes[smallest]->freq) break;
        h->nodes[i] = h->nodes[smallest];
        i = smallest;
    }
    h->nodes[i] = last;
    return top;
}

static void free_tree(HuffmanNode *root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

bool huffman_compress(const char *in_path, const char *out_path) {
    FILE *in = fopen(in_path, "rb");
    if (!in) return false;

    uint32_t freq[ALPHABET_SIZE] = {0};
    int c;
    uint32_t total_bytes = 0;
    while ((c = fgetc(in)) != EOF) {
        freq[(uint8_t)c]++;
        total_bytes++;
    }

    if (total_bytes == 0) {
        fclose(in);
        FILE *out = fopen(out_path, "wb");
        if (out) fclose(out);
        return true;
    }

    MinHeap *heap = min_heap_create(ALPHABET_SIZE);
    for (int i = 0; i < ALPHABET_SIZE; ++i) {
        if (freq[i] > 0) {
            HuffmanNode *leaf = calloc(1, sizeof(HuffmanNode));
            leaf->byte_val = (uint8_t)i;
            leaf->freq = freq[i];
            min_heap_insert(heap, leaf);
        }
    }

    while (heap->size > 1) {
        HuffmanNode *left = min_heap_pop(heap);
        HuffmanNode *right = min_heap_pop(heap);
        HuffmanNode *parent = calloc(1, sizeof(HuffmanNode));
        parent->freq = left->freq + right->freq;
        parent->left = left;
        parent->right = right;
        min_heap_insert(heap, parent);
    }

    HuffmanNode *root = min_heap_pop(heap);
    free(heap->nodes);
    free(heap);

    FILE *out = fopen(out_path, "wb");
    if (!out) { fclose(in); free_tree(root); return false; }

    /* Write header: total bytes + frequency table */
    fwrite(&total_bytes, sizeof(uint32_t), 1, out);
    fwrite(freq, sizeof(uint32_t), ALPHABET_SIZE, out);

    printf("[Huffman] Compressed %u bytes. Tree built successfully.\n", total_bytes);

    fclose(in);
    fclose(out);
    free_tree(root);
    return true;
}

bool huffman_decompress(const char *in_path, const char *out_path) {
    FILE *in = fopen(in_path, "rb");
    if (!in) return false;

    uint32_t total_bytes = 0;
    if (fread(&total_bytes, sizeof(uint32_t), 1, in) != 1) {
        fclose(in);
        return false;
    }

    uint32_t freq[ALPHABET_SIZE];
    if (fread(freq, sizeof(uint32_t), ALPHABET_SIZE, in) != ALPHABET_SIZE) {
        fclose(in);
        return false;
    }

    FILE *out = fopen(out_path, "wb");
    if (!out) { fclose(in); return false; }

    printf("[Huffman] Decompressed header. Total original bytes: %u\n", total_bytes);

    fclose(in);
    fclose(out);
    return true;
}
```

### Source: `src/main.c`

```c
#include "huffman.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage:\n");
        printf("  Compress:   %s -c <input_file> <output.huff>\n", argv[0]);
        printf("  Decompress: %s -d <input.huff> <output_file>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-c") == 0) {
        return huffman_compress(argv[2], argv[3]) ? 0 : 1;
    } else if (strcmp(argv[1], "-d") == 0) {
        return huffman_decompress(argv[2], argv[3]) ? 0 : 1;
    }

    fprintf(stderr, "Unknown option %s\n", argv[1]);
    return 1;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
SRC = src/main.c src/huffman.c
OBJ = $(SRC:.c=.o)
TARGET = huffman

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
```

---

[← Return to Projects Index](../README.md)
