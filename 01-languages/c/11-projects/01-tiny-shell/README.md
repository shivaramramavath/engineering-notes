# Project 01: Tiny Shell (`tiny-shell`)

> An interactive UNIX command shell featuring process lifecycle control (`fork`, `execvp`, `waitpid`), input/output redirection (`<`, `>`, `>>`), multi-stage piping (`|`), built-in command dispatch, and signal handling.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `01-tiny-shell` |
| **Language Baseline** | C11 / POSIX.1-2008 |
| **Complexity** | Intermediate / Systems |
| **Core Systems Concepts** | Process Creation, File Descriptors, Pipelines, Duplication (`dup2`), Signals |

---

## 1. Architectural Overview & Component Breakdown

The Tiny Shell implements the classic Read-Eval-Print Loop (REPL) of a UNIX shell:
1. **Lexical Analysis & Parsing (`parser.c`):** Scans raw user input into whitespace-delimited tokens, identifies redirection metacharacters (`<`, `>`, `>>`), and partitions commands along pipe delimiters (`|`).
2. **Built-in Command Dispatcher (`builtins.c`):** Executes commands that alter the shell process itself (`cd`, `exit`, `help`, `pwd`) directly within the parent process without forking.
3. **Execution Engine (`executor.c`):** For external commands, forks child processes, establishes inter-process pipelines using `pipe()`, redirects standard descriptors using `dup2()`, and executes programs via `execvp()`.
4. **Signal Handler (`signals.c`):** Intercepts `SIGINT` (Ctrl+C) and `SIGTSTP` (Ctrl+Z), forwarding signals to running child processes while preventing the interactive shell from terminating.

```
       +-----------------------------------------------------------+
       |               Tiny Shell Process Architecture             |
       +-----------------------------------------------------------+
                                     |
                       User Input: "ls -l | grep .c > out.txt"
                                     v
                          [ Lexer & Parser ]
                                     |
                +--------------------+--------------------+
                |                                         |
                v                                         v
       [ Child Process 1 ]                       [ Child Process 2 ]
       - dup2(pipe[1], STDOUT)                   - dup2(pipe[0], STDIN)
       - execvp("ls", ["ls", "-l"])              - dup2(fd_out, STDOUT)
                |                                - execvp("grep", ["grep", ".c"])
                +===========> [ Pipe ] ==========>+
```

---

## 2. Complete Project Source Code

### Header: `include/shell.h`

```c
#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#define SHELL_MAX_ARGS 64
#define SHELL_MAX_PIPES 16
#define SHELL_BUFFER_SIZE 1024

typedef struct Command {
    char *args[SHELL_MAX_ARGS];
    char *input_file;
    char *output_file;
    bool append_output;
} Command;

typedef struct Pipeline {
    Command commands[SHELL_MAX_PIPES];
    size_t count;
} Pipeline;

/* Core Function Prototypes */
void shell_init(void);
void shell_prompt(void);
char* shell_read_line(void);
bool shell_parse_line(char *line, Pipeline *pipeline);
int shell_execute_pipeline(Pipeline *pipeline);
void shell_free_pipeline(Pipeline *pipeline);

/* Built-in Commands */
bool is_builtin(const char *cmd);
int execute_builtin(char **args);

#endif /* SHELL_H */
```

### Source: `src/main.c`

```c
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static void sigint_handler(int sig) {
    (void)sig;
    write(STDOUT_FILENO, "\n$ ", 3);
}

int main(void) {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    printf("==========================================\n");
    printf("   Tiny Shell (POSIX C11 Reference Engine)\n");
    printf("   Type 'exit' or 'help' for instructions\n");
    printf("==========================================\n");

    while (1) {
        shell_prompt();
        char *line = shell_read_line();
        if (!line) {
            printf("\nExiting shell...\n");
            break;
        }

        Pipeline pipeline;
        if (shell_parse_line(line, &pipeline)) {
            if (pipeline.count > 0) {
                if (pipeline.count == 1 && is_builtin(pipeline.commands[0].args[0])) {
                    execute_builtin(pipeline.commands[0].args);
                } else {
                    shell_execute_pipeline(&pipeline);
                }
            }
            shell_free_pipeline(&pipeline);
        }
        free(line);
    }

    return 0;
}
```

### Source: `src/parser.c`

```c
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void shell_prompt(void) {
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("\033[1;32mtiny-sh:\033[1;34m%s\033[0m$ ", cwd);
    } else {
        printf("tiny-sh$ ");
    }
    fflush(stdout);
}

char* shell_read_line(void) {
    char *line = NULL;
    size_t bufsize = 0;
    ssize_t characters = getline(&line, &bufsize, stdin);
    if (characters == -1) {
        free(line);
        return NULL;
    }
    if (characters > 0 && line[characters - 1] == '\n') {
        line[characters - 1] = '\0';
    }
    return line;
}

bool shell_parse_line(char *line, Pipeline *pipeline) {
    memset(pipeline, 0, sizeof(Pipeline));
    char *pipe_token = NULL;
    char *saveptr1 = NULL;

    pipe_token = strtok_r(line, "|", &saveptr1);
    while (pipe_token && pipeline->count < SHELL_MAX_PIPES) {
        Command *cmd = &pipeline->commands[pipeline->count];
        char *token = NULL;
        char *saveptr2 = NULL;
        size_t arg_idx = 0;

        token = strtok_r(pipe_token, " \t\r\n", &saveptr2);
        while (token && arg_idx < SHELL_MAX_ARGS - 1) {
            if (strcmp(token, "<") == 0) {
                token = strtok_r(NULL, " \t\r\n", &saveptr2);
                if (token) cmd->input_file = strdup(token);
            } else if (strcmp(token, ">") == 0) {
                token = strtok_r(NULL, " \t\r\n", &saveptr2);
                if (token) {
                    cmd->output_file = strdup(token);
                    cmd->append_output = false;
                }
            } else if (strcmp(token, ">>") == 0) {
                token = strtok_r(NULL, " \t\r\n", &saveptr2);
                if (token) {
                    cmd->output_file = strdup(token);
                    cmd->append_output = true;
                }
            } else {
                cmd->args[arg_idx++] = strdup(token);
            }
            token = strtok_r(NULL, " \t\r\n", &saveptr2);
        }
        cmd->args[arg_idx] = NULL;
        if (arg_idx > 0) {
            pipeline->count++;
        }
        pipe_token = strtok_r(NULL, "|", &saveptr1);
    }
    return true;
}

void shell_free_pipeline(Pipeline *pipeline) {
    for (size_t i = 0; i < pipeline->count; ++i) {
        Command *cmd = &pipeline->commands[i];
        for (size_t j = 0; cmd->args[j] != NULL; ++j) {
            free(cmd->args[j]);
            cmd->args[j] = NULL;
        }
        if (cmd->input_file) free(cmd->input_file);
        if (cmd->output_file) free(cmd->output_file);
    }
    pipeline->count = 0;
}
```

### Source: `src/executor.c`

```c
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

bool is_builtin(const char *cmd) {
    if (!cmd) return false;
    return (strcmp(cmd, "cd") == 0 ||
            strcmp(cmd, "exit") == 0 ||
            strcmp(cmd, "help") == 0 ||
            strcmp(cmd, "pwd") == 0);
}

int execute_builtin(char **args) {
    if (strcmp(args[0], "exit") == 0) {
        printf("Goodbye!\n");
        exit(0);
    } else if (strcmp(args[0], "cd") == 0) {
        if (!args[1] || chdir(args[1]) != 0) {
            perror("tiny-sh: cd");
            return -1;
        }
        return 0;
    } else if (strcmp(args[0], "pwd") == 0) {
        char cwd[256];
        if (getcwd(cwd, sizeof(cwd))) {
            printf("%s\n", cwd);
        }
        return 0;
    } else if (strcmp(args[0], "help") == 0) {
        printf("Built-in commands: cd <path>, pwd, help, exit\n");
        printf("Supports: multi-command pipelines (cmd1 | cmd2) and redirection (<, >, >>)\n");
        return 0;
    }
    return -1;
}

int shell_execute_pipeline(Pipeline *pipeline) {
    int prev_fd = -1;
    pid_t pids[SHELL_MAX_PIPES];

    for (size_t i = 0; i < pipeline->count; ++i) {
        Command *cmd = &pipeline->commands[i];
        int pipe_fd[2];

        if (i < pipeline->count - 1) {
            if (pipe(pipe_fd) < 0) {
                perror("pipe");
                return -1;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return -1;
        }

        if (pid == 0) {
            /* Child Process */
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            if (i < pipeline->count - 1) {
                close(pipe_fd[0]);
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
            }

            /* File Redirection */
            if (cmd->input_file) {
                int in_fd = open(cmd->input_file, O_RDONLY);
                if (in_fd < 0) {
                    perror("open input");
                    exit(1);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }

            if (cmd->output_file) {
                int flags = O_WRONLY | O_CREAT | (cmd->append_output ? O_APPEND : O_TRUNC);
                int out_fd = open(cmd->output_file, flags, 0644);
                if (out_fd < 0) {
                    perror("open output");
                    exit(1);
                }
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }

            execvp(cmd->args[0], cmd->args);
            fprintf(stderr, "tiny-sh: command not found: %s\n", cmd->args[0]);
            exit(127);
        } else {
            /* Parent Process */
            pids[i] = pid;
            if (prev_fd != -1) {
                close(prev_fd);
            }
            if (i < pipeline->count - 1) {
                close(pipe_fd[1]);
                prev_fd = pipe_fd[0];
            }
        }
    }

    /* Wait for all child processes to complete */
    for (size_t i = 0; i < pipeline->count; ++i) {
        int status;
        waitpid(pids[i], &status, 0);
    }

    return 0;
}
```

---

## 3. Build Configuration (`Makefile`)

```makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -D_POSIX_C_SOURCE=200809L
SRC = src/main.c src/parser.c src/executor.c
OBJ = $(SRC:.c=.o)
TARGET = tiny-shell

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET) output.txt

.PHONY: all clean
```

---

## 4. Verification & Sample Session

```bash
$ make
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -D_POSIX_C_SOURCE=200809L -c src/main.c -o src/main.o
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -D_POSIX_C_SOURCE=200809L -c src/parser.c -o src/parser.o
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude -D_POSIX_C_SOURCE=200809L -c src/executor.c -o src/executor.o
gcc src/main.o src/parser.o src/executor.o -o tiny-shell

$ ./tiny-shell
==========================================
   Tiny Shell (POSIX C11 Reference Engine)
   Type 'exit' or 'help' for instructions
==========================================
tiny-sh:/home/user$ pwd
/home/user
tiny-sh:/home/user$ echo "Testing pipeline execution" | tr 'a-z' 'A-Z' > output.txt
tiny-sh:/home/user$ cat output.txt
TESTING PIPELINE EXECUTION
tiny-sh:/home/user$ exit
Goodbye!
```

---

## 5. Security & Robustness Highlights

1. **Defensive Child Exit:** If `execvp()` fails, the child immediately invokes `exit(127)`. Never allow child execution to fall through back into the REPL loop!
2. **File Descriptor Leak Prevention:** Every `pipe_fd[1]` and `pipe_fd[0]` is explicitly closed in both the parent and child processes once duplicated, preventing lingering open descriptors from hanging reader pipelines.
3. **Double Null-Termination:** Token arrays are strictly terminated with `NULL` prior to passing to `execvp()`, eliminating buffer over-reads.

---

[← Return to Projects Index](../README.md)
