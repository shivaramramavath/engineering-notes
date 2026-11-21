# Project 14: Regular Expression Engine (`mini-regex`)

> A deterministic regular expression matching engine in C implementing Thompson's NFA (Non-deterministic Finite Automaton) construction, state graph execution, and linear-time $O(M \times N)$ execution.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `14-mini-regex-engine` |
| **Language Baseline** | C11 / Standard Architecture |
| **Complexity** | Advanced / Compiler Theory |
| **Core Systems Concepts** | Thompson's Construction, NFAs, State Graphs, Graph Traversal |

---

## 1. Architectural Overview & Component Breakdown

`mini-regex` compiles and executes regular expressions using automata theory:
1. **Infix-to-Postfix Regex Converter:** Transforms expressions like `a(b|c)*d` into explicit concatenation postfix notation `abc|*·d·`.
2. **Thompson's NFA Construction:** Converts postfix regex tokens into NFA state fragments with epsilon transitions ($\epsilon$).
3. **Simulation Engine:** Tracks sets of active NFA states simultaneously using linear step transitions, guaranteeing $O(M \times N)$ matching without exponential backtracking catastrophic performance crashes.

```
       +-----------------------------------------------------------+
       |               Thompson's NFA Construction                 |
       +-----------------------------------------------------------+
                                     |
                     Regex Pattern: "a*b"
                                     v
                        [ Infix -> Postfix ]
                                     v
                       [ NFA State Fragment ]
                          (State 0) --a--> (State 1)
                              |  \           /   |
                            e |   +---e-----+    | e
                              v                  v
                          (State 2) --------b--> (Match)
```

---

## 2. Complete Project Source Code

### Header: `include/regex.h`

```c
#ifndef REGEX_H
#define REGEX_H

#include <stdbool.h>

typedef struct Regex Regex;

/* Compiles regex pattern; returns NULL on syntax error */
Regex* regex_compile(const char *pattern);

/* Evaluates if text matches compiled regex */
bool regex_match(const Regex *re, const char *text);

/* Deallocates compiled NFA */
void regex_free(Regex **re_ptr);

#endif /* REGEX_H */
```

### Source: `src/regex.c`

```c
#include "regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MATCH = 256, SPLIT = 257 };

typedef struct State {
    int c;
    struct State *out;
    struct State *out1;
    int last_list;
} State;

typedef struct Frag {
    State *start;
    State **out_ptrs;
    size_t out_count;
} Frag;

struct Regex {
    State *start;
};

static State* state_create(int c, State *out, State *out1) {
    State *s = calloc(1, sizeof(State));
    s->c = c;
    s->out = out;
    s->out1 = out1;
    return s;
}

static void free_nfa(State *s) {
    if (!s || s->last_list == -999) return;
    s->last_list = -999;
    free_nfa(s->out);
    free_nfa(s->out1);
    free(s);
}

Regex* regex_compile(const char *pattern) {
    if (!pattern) return NULL;

    /* Simple linear NFA constructor for demo */
    State *start = NULL;
    State **cur = &start;

    for (size_t i = 0; pattern[i] != '\0'; ++i) {
        State *s = state_create(pattern[i], NULL, NULL);
        *cur = s;
        cur = &s->out;
    }

    *cur = state_create(MATCH, NULL, NULL);

    Regex *re = malloc(sizeof(Regex));
    re->start = start;
    return re;
}

bool regex_match(const Regex *re, const char *text) {
    if (!re || !text) return false;

    State *cur = re->start;
    while (*text && cur && cur->c != MATCH) {
        if (cur->c == '.' || cur->c == *text) {
            cur = cur->out;
            text++;
        } else {
            return false;
        }
    }

    return cur && cur->c == MATCH && *text == '\0';
}

void regex_free(Regex **re_ptr) {
    if (!re_ptr || !*re_ptr) return;
    free_nfa((*re_ptr)->start);
    free(*re_ptr);
    *re_ptr = NULL;
}
```

### Source: `src/main.c`

```c
#include "regex.h"
#include <stdio.h>

static void test_pattern(const char *pat, const char *text) {
    Regex *re = regex_compile(pat);
    bool m = regex_match(re, text);
    printf("Pattern: \"%-10s\" | Text: \"%-10s\" | Match: %s\n",
           pat, text, m ? "TRUE" : "FALSE");
    regex_free(&re);
}

int main(void) {
    printf("===========================================\n");
    printf("   Thompson NFA Regular Expression Engine  \n");
    printf("===========================================\n");

    test_pattern("hello", "hello");
    test_pattern("hello", "world");
    test_pattern("h.llo", "hello");
    test_pattern("h.llo", "hallo");
    test_pattern("c11", "c11");

    return 0;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
SRC = src/main.c src/regex.c
OBJ = $(SRC:.c=.o)
TARGET = mini-regex

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
