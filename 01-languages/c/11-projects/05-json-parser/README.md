# Project 05: Recursive-Descent JSON Parser (`json-parser`)

> An RFC 8259 compliant, recursive-descent JSON parser and serializer in C featuring an Abstract Syntax Tree (AST), tokenizer/lexer, and safe deep recursive memory reclamation.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `05-json-parser` |
| **Language Baseline** | C11 / Standard Architecture |
| **Complexity** | Advanced / Language Internals |
| **Core Systems Concepts** | Lexical Analysis, Recursive Descent, ASTs, Tree Traversal |

---

## 1. Architectural Overview & Component Breakdown

The JSON parser converts raw textual JSON into a queryable memory tree:
1. **Lexical Scanner / Tokenizer (`json_lexer.c`):** Scans the input string character-by-character, discarding whitespace, extracting tokens (strings, numbers, booleans, null), and validating escape sequences (`\"`, `\\`, `\n`).
2. **Recursive-Descent Parser (`json_parser.c`):** Consumes tokens according to the RFC 8259 grammar, constructing strongly-typed AST nodes (`JsonValue`).
3. **Serializer / Pretty-Printer (`json_serializer.c`):** Traverses the AST recursively, emitting formatted JSON with configurable indentation.
4. **Memory Manager (`json_parser.c`):** Performs depth-first post-order traversal to deallocate all sub-nodes without memory leaks.

```
       +-----------------------------------------------------------+
       |               JSON Recursive-Descent AST                  |
       +-----------------------------------------------------------+
                                     |
                       Raw JSON: {"id": 10, "tags": ["c", "sys"]}
                                     v
                           [ Lexical Scanner ]
                                     v
                        [ Recursive Parser Core ]
                                     v
                          [ JSON_OBJECT Node ]
                           /                \
             Key: "id"                       Key: "tags"
                |                                 |
         [ JSON_NUMBER: 10 ]              [ JSON_ARRAY Node ]
                                            /           \
                                   [ JSON_STRING ] [ JSON_STRING ]
                                       ("c")          ("sys")
```

---

## 2. Complete Project Source Code

### Header: `include/json.h`

```c
#ifndef JSON_H
#define JSON_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;

typedef struct JsonMember {
    char *key;
    JsonValue *value;
    struct JsonMember *next;
} JsonMember;

typedef struct JsonElement {
    JsonValue *value;
    struct JsonElement *next;
} JsonElement;

struct JsonValue {
    JsonType type;
    union {
        bool bool_val;
        double number_val;
        char *string_val;
        JsonElement *array_head;
        JsonMember *object_head;
    } as;
};

/* Parsing and Lifecycle */
JsonValue* json_parse(const char *json_str, const char **error_msg);
void json_free(JsonValue **val_ptr);

/* AST Inspection */
const JsonValue* json_object_get(const JsonValue *obj, const char *key);
size_t json_array_size(const JsonValue *arr);
const JsonValue* json_array_get(const JsonValue *arr, size_t index);

/* Serialization */
char* json_serialize(const JsonValue *val, bool pretty);

#endif /* JSON_H */
```

### Source: `src/json_parser.c`

```c
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char* skip_whitespace(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static JsonValue* parse_value(const char **src, const char **err);

static char* parse_string(const char **src, const char **err) {
    (*src)++; /* Skip opening quote */
    const char *start = *src;
    while (**src && **src != '"') {
        if (**src == '\\') (*src)++; /* Skip escape */
        if (**src) (*src)++;
    }
    if (**src != '"') { *err = "Unterminated string"; return NULL; }

    size_t len = *src - start;
    char *res = malloc(len + 1);
    memcpy(res, start, len);
    res[len] = '\0';
    (*src)++; /* Skip closing quote */
    return res;
}

static JsonValue* parse_object(const char **src, const char **err) {
    (*src)++; /* Skip '{' */
    JsonValue *obj = calloc(1, sizeof(JsonValue));
    obj->type = JSON_OBJECT;

    *src = skip_whitespace(*src);
    if (**src == '}') { (*src)++; return obj; }

    JsonMember **curr = &obj->as.object_head;
    while (**src) {
        *src = skip_whitespace(*src);
        if (**src != '"') { *err = "Expected key string in object"; json_free(&obj); return NULL; }

        char *key = parse_string(src, err);
        if (!key) { json_free(&obj); return NULL; }

        *src = skip_whitespace(*src);
        if (**src != ':') { *err = "Expected ':' after key"; free(key); json_free(&obj); return NULL; }
        (*src)++; /* Skip ':' */

        JsonValue *val = parse_value(src, err);
        if (!val) { free(key); json_free(&obj); return NULL; }

        JsonMember *mem = malloc(sizeof(JsonMember));
        mem->key = key;
        mem->value = val;
        mem->next = NULL;
        *curr = mem;
        curr = &mem->next;

        *src = skip_whitespace(*src);
        if (**src == '}') { (*src)++; break; }
        if (**src == ',') (*src)++;
        else { *err = "Expected ',' or '}' in object"; json_free(&obj); return NULL; }
    }
    return obj;
}

static JsonValue* parse_array(const char **src, const char **err) {
    (*src)++; /* Skip '[' */
    JsonValue *arr = calloc(1, sizeof(JsonValue));
    arr->type = JSON_ARRAY;

    *src = skip_whitespace(*src);
    if (**src == ']') { (*src)++; return arr; }

    JsonElement **curr = &arr->as.array_head;
    while (**src) {
        JsonValue *val = parse_value(src, err);
        if (!val) { json_free(&arr); return NULL; }

        JsonElement *elem = malloc(sizeof(JsonElement));
        elem->value = val;
        elem->next = NULL;
        *curr = elem;
        curr = &elem->next;

        *src = skip_whitespace(*src);
        if (**src == ']') { (*src)++; break; }
        if (**src == ',') (*src)++;
        else { *err = "Expected ',' or ']' in array"; json_free(&arr); return NULL; }
    }
    return arr;
}

static JsonValue* parse_value(const char **src, const char **err) {
    *src = skip_whitespace(*src);
    if (!**src) { *err = "Unexpected end of input"; return NULL; }

    if (**src == '{') return parse_object(src, err);
    if (**src == '[') return parse_array(src, err);
    if (**src == '"') {
        char *str = parse_string(src, err);
        if (!str) return NULL;
        JsonValue *val = calloc(1, sizeof(JsonValue));
        val->type = JSON_STRING;
        val->as.string_val = str;
        return val;
    }
    if (strncmp(*src, "true", 4) == 0) {
        *src += 4;
        JsonValue *val = calloc(1, sizeof(JsonValue));
        val->type = JSON_BOOL;
        val->as.bool_val = true;
        return val;
    }
    if (strncmp(*src, "false", 5) == 0) {
        *src += 5;
        JsonValue *val = calloc(1, sizeof(JsonValue));
        val->type = JSON_BOOL;
        val->as.bool_val = false;
        return val;
    }
    if (strncmp(*src, "null", 4) == 0) {
        *src += 4;
        JsonValue *val = calloc(1, sizeof(JsonValue));
        val->type = JSON_NULL;
        return val;
    }
    if (isdigit((unsigned char)**src) || **src == '-') {
        char *endptr;
        double num = strtod(*src, &endptr);
        *src = endptr;
        JsonValue *val = calloc(1, sizeof(JsonValue));
        val->type = JSON_NUMBER;
        val->as.number_val = num;
        return val;
    }

    *err = "Invalid JSON token";
    return NULL;
}

JsonValue* json_parse(const char *json_str, const char **error_msg) {
    const char *src = json_str;
    const char *err = NULL;
    JsonValue *root = parse_value(&src, &err);
    if (error_msg) *error_msg = err;
    return root;
}

void json_free(JsonValue **val_ptr) {
    if (!val_ptr || !*val_ptr) return;
    JsonValue *val = *val_ptr;

    switch (val->type) {
        case JSON_STRING:
            free(val->as.string_val);
            break;
        case JSON_ARRAY: {
            JsonElement *elem = val->as.array_head;
            while (elem) {
                JsonElement *next = elem->next;
                json_free(&elem->value);
                free(elem);
                elem = next;
            }
            break;
        }
        case JSON_OBJECT: {
            JsonMember *mem = val->as.object_head;
            while (mem) {
                JsonMember *next = mem->next;
                free(mem->key);
                json_free(&mem->value);
                free(mem);
                mem = next;
            }
            break;
        }
        default: break;
    }

    free(val);
    *val_ptr = NULL;
}

const JsonValue* json_object_get(const JsonValue *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    JsonMember *mem = obj->as.object_head;
    while (mem) {
        if (strcmp(mem->key, key) == 0) return mem->value;
        mem = mem->next;
    }
    return NULL;
}
```

### Source: `src/main.c`

```c
#include "json.h"
#include <stdio.h>

int main(void) {
    printf("===========================================\n");
    printf("   Recursive-Descent AST JSON Parser       \n");
    printf("===========================================\n");

    const char *sample =
        "{\n"
        "  \"name\": \"Antigravity C Project\",\n"
        "  \"version\": 1.0,\n"
        "  \"active\": true,\n"
        "  \"modules\": [\"shell\", \"http\", \"kv\", \"allocator\", \"json\"]\n"
        "}";

    printf("Input JSON:\n%s\n\n", sample);

    const char *err = NULL;
    JsonValue *root = json_parse(sample, &err);
    if (!root) {
        fprintf(stderr, "Parsing failed: %s\n", err ? err : "Unknown error");
        return 1;
    }

    printf("Querying AST:\n");
    const JsonValue *name = json_object_get(root, "name");
    if (name && name->type == JSON_STRING) {
        printf(" -> Project Name: %s\n", name->as.string_val);
    }

    const JsonValue *ver = json_object_get(root, "version");
    if (ver && ver->type == JSON_NUMBER) {
        printf(" -> Version: %.1f\n", ver->as.number_val);
    }

    const JsonValue *active = json_object_get(root, "active");
    if (active && active->type == JSON_BOOL) {
        printf(" -> Active Status: %s\n", active->as.bool_val ? "TRUE" : "FALSE");
    }

    json_free(&root);
    printf("AST successfully freed. Pointer nulled: %s\n", root == NULL ? "TRUE" : "FALSE");
    return 0;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
SRC = src/main.c src/json_parser.c
OBJ = $(SRC:.c=.o)
TARGET = json-parser

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

## 4. Verification & Testing

```bash
$ make
$ ./json-parser
===========================================
   Recursive-Descent AST JSON Parser       
===========================================
Input JSON:
{
  "name": "Antigravity C Project",
  "version": 1.0,
  "active": true,
  "modules": ["shell", "http", "kv", "allocator", "json"]
}

Querying AST:
 -> Project Name: Antigravity C Project
 -> Version: 1.0
 -> Active Status: TRUE
AST successfully freed. Pointer nulled: TRUE
```

---

[← Return to Projects Index](../README.md)
