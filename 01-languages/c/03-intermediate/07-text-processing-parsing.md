# Text Processing and Parsing

C gives you raw bytes and a handful of string functions. Parsing safely means reading input in bounded pieces, tracking positions explicitly, validating every conversion, and avoiding functions that modify or overflow buffers.

**Prerequisites:** [Strings](../02-core-concepts/02-strings.md), [input/output](../01-fundamentals/09-input-output.md), [file I/O](../02-core-concepts/11-file-io.md).

## The basic pipeline

```text
read line  →  trim / split  →  convert fields  →  validate  →  use
(fgets)       (manual)         (strtol/strtod)     (ranges)
```

Read a line at a time, then parse the line. Reading and parsing in one `scanf` call makes error recovery hard.

## Reading lines

```c
char line[512];
while (fgets(line, sizeof line, f) != NULL) {
    size_t len = strcspn(line, "\r\n");
    if (line[len] == '\0') {
        /* no newline: line was too long (or last line had none); handle it */
    }
    line[len] = '\0';                      /* strip \n and \r\n */
    parse_line(line);
}
```

For lines of any length on POSIX, `getline` allocates for you:

```c
char *buf = NULL; size_t cap = 0; ssize_t n;
while ((n = getline(&buf, &cap, f)) != -1) { /* n includes the newline */ }
free(buf);
```

## Converting numbers

Prefer `strtol`, `strtoul`, `strtod` to `atoi`/`atof`: they report where parsing stopped and whether the value was out of range.

```c
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

bool parse_int(const char *s, int *out) {
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return false;        /* no digits, or trailing junk */
    if (errno == ERANGE || v < INT_MIN || v > INT_MAX) return false;
    *out = (int)v;
    return true;
}
```

Note: `strtol` skips leading whitespace and accepts a sign. Base `0` auto-detects `0x` and leading-zero octal. `strtoul` accepts a leading `-` and wraps, so check for it if negatives must be rejected.

## Splitting without modifying: pointer + length

Instead of copying or writing NULs into the input, represent a piece of text as a **start pointer and a length** (a "string view"):

```c
typedef struct { const char *p; size_t len; } sv_t;

/* Return the next comma-separated field and advance *cursor. Returns false at end. */
bool next_field(const char **cursor, sv_t *out) {
    const char *s = *cursor;
    if (s == NULL) return false;
    const char *comma = strchr(s, ',');
    out->p = s;
    out->len = comma ? (size_t)(comma - s) : strlen(s);
    *cursor = comma ? comma + 1 : NULL;
    return true;
}
```

Print one with `printf("%.*s", (int)v.len, v.p)`. Compare with `strncmp(v.p, "key", v.len) == 0 && "key"[v.len] == '\0'`, or `memcmp` after checking lengths.

## Splitting with `strtok` (and why to be careful)

```c
for (char *tok = strtok(line, " \t"); tok != NULL; tok = strtok(NULL, " \t")) { ... }
```

`strtok` **modifies** the buffer, keeps hidden static state (not thread-safe, can't nest), treats runs of delimiters as one (so empty CSV fields vanish), and fails on string literals. Alternatives: `strtok_r` (POSIX), `strsep` (BSD/glibc, preserves empty fields), or the pointer-and-length approach above.

## `sscanf`: handy but limited

```c
char key[32]; int value;
if (sscanf(line, " %31[^=]=%d", key, &value) == 2) { ... }
```

- **Always check the return value** (number of conversions).
- **Always bound `%s` and `%[...]`** with a width (one less than the buffer size).
- `%[^=]` matches any run of characters except `=`; leading space in the format skips whitespace.
- `%n` records characters consumed; useful to detect trailing garbage.
- Overflow of numeric conversions in `scanf` is undefined, which is another reason to prefer `strtol`.

## Trimming whitespace

```c
#include <ctype.h>

char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;            /* leading */
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';                                       /* trailing: modifies s */
    return s;                                          /* may point past the original start */
}
```

Keep the original pointer if you will `free` it.

## Example: `key=value` config lines

```c
/* Parses lines like:  name = Alice   (comments start with #) */
bool parse_kv(char *line, char **key, char **value) {
    char *hash = strchr(line, '#');
    if (hash) *hash = '\0';

    char *eq = strchr(line, '=');
    if (eq == NULL) return false;
    *eq = '\0';

    *key = trim(line);
    *value = trim(eq + 1);
    return **key != '\0';
}
```

The outputs point into `line`, so they're valid only as long as the buffer is.

## A tiny tokenizer

For expressions and small languages, scan a character at a time with an index:

```c
typedef enum { T_NUM, T_PLUS, T_MINUS, T_STAR, T_SLASH, T_LP, T_RP, T_END, T_ERR } tok_kind;
typedef struct { tok_kind kind; double num; } token_t;

token_t next_token(const char **sp) {
    const char *s = *sp;
    while (isspace((unsigned char)*s)) s++;

    token_t t = { T_ERR, 0 };
    if (*s == '\0')             { t.kind = T_END; }
    else if (isdigit((unsigned char)*s) || *s == '.') {
        char *end;
        t.num = strtod(s, &end);
        t.kind = T_NUM;
        s = end;
        *sp = s;
        return t;
    }
    else {
        switch (*s) {
        case '+': t.kind = T_PLUS;  break;
        case '-': t.kind = T_MINUS; break;
        case '*': t.kind = T_STAR;  break;
        case '/': t.kind = T_SLASH; break;
        case '(': t.kind = T_LP;    break;
        case ')': t.kind = T_RP;    break;
        }
        if (t.kind != T_ERR) s++;
    }
    *sp = s;
    return t;
}
```

This is the lexer half of the [expression interpreter project](../09-projects/08-expression-interpreter/).

## CSV warning

Real CSV allows quoted fields containing commas, quotes (`""`), and newlines. Splitting on commas works only for simple data. For anything else use a proper state machine or a library.

## Other useful functions

| Function | Use |
|---|---|
| `strpbrk(s, set)` | first char from a set |
| `strspn` / `strcspn` | length of a prefix inside / outside a set |
| `memchr(p, c, n)` | find a byte in a buffer, not necessarily NUL-terminated |
| `strncmp`, `strncasecmp` (POSIX) | prefix and case-insensitive comparison |

## Common mistakes

| Mistake | Effect |
|---|---|
| `scanf("%s", buf)` unbounded | Buffer overflow |
| Ignoring `sscanf`'s return value | Using uninitialized variables |
| `atoi` on untrusted input | Silent zero, no error |
| `strtok` on a literal or shared state | Crash, or interleaved parsing |
| Holding pointers into a buffer that gets reused by the next `fgets` | Corrupted tokens |
| Not handling `\r\n` | Stray `\r` at end of fields |
| `isalpha(c)` with a negative `char` | Undefined; cast to `unsigned char` |

## Summary

- Read a bounded line, then parse it; check every conversion.
- Use `strtol`/`strtod` with end-pointer and `errno` checks.
- Pointer-plus-length views avoid copying and `strtok` pitfalls.
- Bound every `%s`; handle `\r\n`, empty fields, and over-long lines.

**Next:** [Unicode and locale](./08-unicode-locale.md)
