# Terminals and termios

> Master terminal I/O programming in C: the POSIX TTY subsystem, canonical (cooked) vs non-canonical (raw) mode, `struct termios`, `tcgetattr`/`tcsetattr`, disabling echo, ANSI VT100 escape sequences, and building responsive terminal text interfaces (TUIs).

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [01-file-descriptors-and-posix-io.md](./01-file-descriptors-and-posix-io.md), [03-signals.md](./03-signals.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / ISO C11 (`<termios.h>`, Win32 Console API contrast) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
By default, the operating system kernel terminal driver operates in **canonical mode** (often called "cooked" mode). In canonical mode, input is line-buffered by the kernel: keypresses are collected, lines are edited with backspace, and no input is delivered to `read()` or `getchar()` until the user presses Enter (`\n`). Furthermore, the kernel automatically echoes every typed character back to the screen.

While canonical mode is ideal for basic command-line utilities (`cat`, `grep`), interactive terminal software—such as text editors (`vim`, `nano`), shells (`bash`, `zsh`), password prompts, and status monitors (`htop`)—requires instant, unbuffered, keystroke-by-keystroke control.

The POSIX **termios** API (`<termios.h>`) allows programmers to inspect and configure terminal driver characteristics: switching to **raw mode** (disabling line buffering and echo), configuring minimum read thresholds (`VMIN`/`VTIME`), and rendering colors and cursor movements with ANSI escape sequences.

---

## 2. Core Concepts
- **Canonical vs Raw Mode**:
  - *Canonical Mode (`ICANON`)*: Input is delivered line-by-line after Enter. Special characters (`Ctrl+C`, `Ctrl+Z`, Backspace) are processed by the terminal driver.
  - *Raw Mode*: Input is delivered byte-by-byte immediately as keys are pressed. Special signal generation is disabled, giving the application complete control over all keyboard input.
- **Echo (`ECHO`)**: When set, characters typed by the user are immediately written to the screen by the terminal line discipline. Disabling `ECHO` is essential for secure password entry prompts.
- **Terminal Control Flags**:
  - `c_iflag`: Input modes (software flow control `IXON`, carriage return translation `ICRNL`).
  - `c_oflag`: Output modes (newline translation `OPOST`, `ONLCR`).
  - `c_cflag`: Hardware control modes (baud rate, character parity, data bits).
  - `c_lflag`: Local modes (`ICANON`, `ECHO`, `ISIG` signal generation).
- **Control Characters (`c_cc`)**:
  - `VMIN`: Minimum number of characters to satisfy a `read()`.
  - `VTIME`: Timeout in tenths of a second (100 ms increments) before `read()` returns.

---

## 3. How It Works Internally

### The TTY Line Discipline Layer
The Unix terminal subsystem is structured as a layered protocol stack between the physical device/emulator and user processes:

```
User Application (vim / bash)
        ^
        | read() / write() system calls
        v
Operating System Kernel
+-------------------------------------------------------+
| Character Device Layer (Virtual TTY: /dev/pts/1)      |
+-------------------------------------------------------+
| TTY Line Discipline (N_TTY)                           |
|   - Canonical Line Buffer                             |
|   - Echo Generator (Sends typed chars back to output) |
|   - Signal Generator (Ctrl+C -> SIGINT, Ctrl+Z -> TSTP)
+-------------------------------------------------------+
| TTY Driver / Hardware Emulation (PTY Master)          |
+-------------------------------------------------------+
        ^
        | Inter-process / Network stream
        v
Terminal Emulator (xterm, GNOME Terminal, Windows Terminal)
```

In canonical mode, the Line Discipline intercepts keystrokes, stores them in an internal 4096-byte edit buffer, echoes them back to the terminal screen, and sleeps the reading application until a line terminator (`\n` or `EOF`) is entered.

In raw mode, the Line Discipline disables line buffering, echoes, and signal translations, forwarding raw byte codes directly to the process's `read()` system call.

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
```

### Signature Overview
```c
int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);
int tcflush(int fd, int queue_selector);
int tcsendbreak(int fd, int duration);
```

### Action Flags for `tcsetattr`
- `TCSANOW`: Changes occur immediately.
- `TCSADRAIN`: Changes occur after all queued output has been transmitted.
- `TCSAFLUSH`: Changes occur after queued output is transmitted, and unread input is discarded.

---

## 5. Practical Examples

### Example 1: Terminal Subsystem Raw Mode Simulation
```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t  c_cc[32];
} TermiosSim;

#define ECHO_FLAG 0x0008
#define ICANON_FLAG 0x0002

void term_enable_raw_mode(TermiosSim *t) {
    // Disable canonical mode (line buffering) and local echo
    t->c_lflag &= ~(ICANON_FLAG | ECHO_FLAG);
    t->c_cc[0] = 1; // VMIN = 1 byte
    t->c_cc[1] = 0; // VTIME = 0
}

int main(void) {
    printf("Simulating POSIX Terminal Subsystem (termios)...\n");
    TermiosSim orig_term = { .c_lflag = ICANON_FLAG | ECHO_FLAG };
    printf("Original Terminal State: Canonical Mode = ON, Echo = ON\n");

    TermiosSim raw_term = orig_term;
    term_enable_raw_mode(&raw_term);

    printf("Applied Raw Mode: Canonical = %s, Echo = %s (VMIN=%d, VTIME=%d)\n",
           (raw_term.c_lflag & ICANON_FLAG) ? "ON" : "OFF",
           (raw_term.c_lflag & ECHO_FLAG) ? "ON" : "OFF",
           raw_term.c_cc[0], raw_term.c_cc[1]);
    printf("Emitted ANSI Control Sequence: \x1b[2J\x1b[H (Clear Screen & Cursor Home)\n");
    return 0;
}
```

```text
Simulating POSIX Terminal Subsystem (termios)...
Original Terminal State: Canonical Mode = ON, Echo = ON
Applied Raw Mode: Canonical = OFF, Echo = OFF (VMIN=1, VTIME=0)
Emitted ANSI Control Sequence:  (Clear Screen & Cursor Home)
```

### Example 2: Canonical Raw Mode Enabler with Exit Restoration
```c
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

static struct termios orig_termios;

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

int enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) < 0) {
        return -1;
    }
    
    /* Register restoration at program exit */
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    /* Input modes: disable break signal, CR->NL translation, parity, strip 8th bit, flow control */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* Output modes: disable post-processing of output (NL to CR-NL) */
    raw.c_oflag &= ~(OPOST);
    /* Control modes: set 8-bit chars */
    raw.c_cflag |= (CS8);
    /* Local modes: disable echo, canonical mode, extended input, signal generation (Ctrl+C, Ctrl+Z) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* Read returns as soon as 1 byte is available with no timeout */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    return tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
```

---

## 6. Best Practices
1. **Always Restore Terminal Attributes on Exit**: If a program exits without restoring `orig_termios`, the user's terminal remains in raw mode: typing will not echo, Enter will not advance lines, and `Ctrl+C` will fail to break. Always register an `atexit()` cleanup handler.
2. **Handle Signals That Terminate or Suspend**: Register handlers for `SIGINT`, `SIGTERM`, and `SIGTSTP`. Restore cooked mode before suspending (`SIGTSTP`) or exiting to leave the user's shell in a clean state.
3. **Verify File Descriptor is a TTY**: Before calling `tcgetattr()`, verify that the input descriptor is connected to a terminal using `isatty(STDIN_FILENO)`. If input is redirected from a pipe or file (`./app < input.txt`), `tcgetattr()` will fail with `ENOTTY`.
4. **Flush Queues When Changing Modes**: Use `TCSAFLUSH` when applying raw or cooked mode to ensure unread input buffered in canonical mode does not spill into raw mode.
5. **Use Standard ANSI Escape Codes**: When controlling colors, cursor position, or screen clearing, use standard ANSI/VT100 escape sequences (`\x1b[...m`) for cross-platform terminal compatibility.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Exiting without restoring original termios | Leaves shell in raw mode; keyboard input won't echo and newline spacing is broken | Use `atexit(restore_terminal)` and signal handlers |
| Calling `tcgetattr` on non-TTY descriptor | Fails with `ENOTTY` if input was redirected from a file or pipe | Guard with `if (isatty(STDIN_FILENO))` |
| Disabling `OPOST` without writing `\r\n` | In raw output mode, `\n` only moves down 1 line; output staircases across the terminal screen | Write explicit carriage return and newline: `\r\n` |
| Setting `VMIN = 0` and `VTIME = 0` | Causes `read()` to return immediately with 0 bytes if no key is pressed, burning 100% CPU in a busy loop | Set `VMIN = 1` or use `select()`/`poll()` before reading |
| Forgetting to disable `IXON` | Pressing `Ctrl+S` freezes terminal output via software flow control (XOFF), confusing users | Clear `IXON` flag in `c_iflag`: `raw.c_iflag &= ~IXON;` |
| Ignoring terminal window resize | If terminal window is resized, layout corrupts | Handle `SIGWINCH` and query new dimensions via `ioctl(fd, TIOCGWINSZ, &ws)` |

---

## 8. Performance & Security Notes
- **Secure Password Prompting**: When prompting users for passwords or encryption keys, the standard library `getpass()` function disables `ECHO` on `STDIN_FILENO` before reading. In custom security software, clear `ECHO` via termios, read input, and immediately zero-wipe memory buffers with `explicit_bzero()` to prevent passwords from lingering in RAM.
- **Window Size Discovery (`TIOCGWINSZ`)**: TUI applications query terminal column and row dimensions using `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)`. When the terminal window is resized by the user, the kernel delivers the `SIGWINCH` signal to the foreground process group.

---

## 9. Comparison / When to Use What

| Mode / Library | Buffer Granularity | Echo | Best Use Case |
| :--- | :--- | :--- | :--- |
| **Canonical Mode (Cooked)**| Line-buffered (`\n`) | Kernel handles echo | CLI tools, batch scripts, filters |
| **Raw Mode (Termios)** | Byte-buffered (Instant) | Application handles echo | Password prompts, simple text TUIs |
| **Curses / Ncurses** | High-level windowing | Library managed | Full-screen interactive terminal UIs |
| **Win32 Console API** | Event-based (`KEY_EVENT`) | Console handles echo | Windows native terminal development |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between canonical mode and raw mode?</summary>
In canonical (cooked) mode, terminal input is processed and line-buffered by the kernel's TTY line discipline: characters are echoed back to the screen, editing keys (backspace) modify an internal line buffer, and the input is only made available to `read()` when the user presses Enter (`\n`). In raw mode, line buffering and local echo are disabled; input bytes are forwarded to the application's `read()` call immediately as each key is pressed on the keyboard.
</details>

<details>
<summary>2. Why does output "staircase" across the screen when output post-processing (`OPOST`) is disabled?</summary>
In standard canonical mode, the kernel's output post-processing translates every `\n` (Line Feed) into `\r\n` (Carriage Return + Line Feed). When `OPOST` is cleared, `\n` moves the cursor down one row, but keeps the same horizontal column. Without an explicit carriage return (`\r`), each subsequent line starts directly below where the previous line ended, producing a diagonal "staircase" effect.
</details>

<details>
<summary>3. What do `VMIN` and `VTIME` control in raw mode?</summary>
They control the blocking and timeout behavior of `read()` in non-canonical mode:
- `VMIN > 0, VTIME == 0`: `read()` blocks indefinitely until at least `VMIN` bytes are available.
- `VMIN == 0, VTIME > 0`: `read()` acts as a pure timer; returns after `VTIME` tenths of a second if no data arrives.
- `VMIN > 0, VTIME > 0`: Inter-byte timer; `read()` blocks until the first byte arrives, then waits up to `VTIME` for subsequent bytes.
- `VMIN == 0, VTIME == 0`: Completely non-blocking; returns immediately with whatever data is ready (or 0).
</details>

<details>
<summary>4. Why is `atexit()` essential when configuring raw mode?</summary>
If an application running in raw mode crashes, receives an unhandled exception, or exits normally without restoring the original terminal configuration, the parent shell terminal will remain in raw mode. Keystrokes typed by the user will not echo, Enter will not advance lines cleanly, and terminal signals like `Ctrl+C` may be ignored. An `atexit()` handler ensures `orig_termios` is restored regardless of how the program exits.
</details>

<details>
<summary>5. How does a program detect if input is coming from an interactive terminal versus a redirected file?</summary>
Using `isatty(STDIN_FILENO)`. It returns 1 if the descriptor is associated with an interactive terminal device, and 0 if it is redirected from a regular disk file, pipe, or socket (setting `errno = ENOTTY`).
</details>

<details>
<summary>6. What signal is generated when a user resizes their terminal window, and how is the new size obtained?</summary>
The kernel sends the `SIGWINCH` (Window Change) signal to the foreground process group. The application handles this signal and queries the new dimensions using `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)`, reading `ws.ws_col` (columns) and `ws.ws_row` (rows).
</details>

<details>
<summary>7. What does the `ISIG` flag in `c_lflag` control?</summary>
When `ISIG` is set, the terminal line discipline intercepts special keyboard combinations and translates them into POSIX signals: `Ctrl+C` generates `SIGINT`, `Ctrl+\` generates `SIGQUIT`, and `Ctrl+Z` generates `SIGTSTP`. Clearing `ISIG` allows the application to read `Ctrl+C` as a literal byte (`0x03`) without triggering a signal.
</details>

<details>
<summary>8. How do ANSI escape sequences work for cursor control?</summary>
ANSI escape sequences begin with the Escape character (`0x1B` or `\x1b`) followed by `[` (the Control Sequence Introducer, CSI), followed by parameters and a command letter. For example, `\x1b[2J` clears the entire screen, `\x1b[H` moves the cursor to row 1, column 1, and `\x1b[31m` sets the text color to red.
</details>

---

## 11. Practice Exercises

### Easy
1. **TTY Detector**: Write a program that checks whether `STDIN_FILENO`, `STDOUT_FILENO`, and `STDERR_FILENO` are connected to interactive terminals using `isatty()`.
2. **ANSI Color Echo**: Write a function that prints a message in bold green using ANSI escape sequences (`\x1b[1;32m...\x1b[0m`).
3. **Password Masker Outline**: Write a function that uses termios to disable `ECHO`, reads a password from standard input, and restores original termios.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 1 */
#include <stdio.h>
#include <unistd.h>
void check_ttys(void) {
    printf("stdin is TTY: %d\n", isatty(STDIN_FILENO));
    printf("stdout is TTY: %d\n", isatty(STDOUT_FILENO));
    printf("stderr is TTY: %d\n", isatty(STDERR_FILENO));
}

/* Exercise 2 */
#include <stdio.h>
void print_green(const char *msg) {
    printf("\x1b[1;32m%s\x1b[0m\n", msg);
}
```
</details>

### Medium
4. **Single Keystroke Reader**: Implement `char getch(void)` that temporarily switches the terminal to raw mode, reads a single keypress without waiting for Enter, restores original settings, and returns the character.
5. **Arrow Key Parser**: Write a function that decodes 3-byte escape sequences generated by arrow keys (Up: `\x1b[A`, Down: `\x1b[B`, Right: `\x1b[C`, Left: `\x1b[D`).
6. **Window Dimensions Reporter**: Use `ioctl(TIOCGWINSZ)` to retrieve and print the terminal's current row and column count.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 4: getch implementation */
#include <termios.h>
#include <unistd.h>
#include <stdio.h>

char getch(void) {
    struct termios oldt, newt;
    char c = 0;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    read(STDIN_FILENO, &c, 1);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
}
```
</details>

### Hard
7. **Full-Screen Terminal Text Viewer**: Build a minimal raw-mode file pager (like `less`) that clears the screen, displays 20 lines of text, and allows scrolling up and down using the arrow keys or `j`/`k`.
8. **Interactive Terminal Progress Bar**: Implement an in-place updating progress bar using ANSI cursor movement escape sequences (`\r` and `\x1b[K`) that updates smoothly from 0% to 100% without scrolling.
9. **SIGWINCH-Aware Layout Manager**: Write a TUI that centers a dialog box on the screen and automatically recalibrates and re-renders its position whenever the terminal window is resized.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 8: In-place Progress Bar */
#include <stdio.h>
#include <unistd.h>

void show_progress(int percent) {
    const int width = 40;
    int filled = (percent * width) / 100;
    printf("\r[");
    for (int i = 0; i < width; i++) {
        if (i < filled) putchar('=');
        else if (i == filled) putchar('>');
        else putchar(' ');
    }
    printf("] %3d%%", percent);
    fflush(stdout);
}

void run_progress_demo(void) {
    for (int p = 0; p <= 100; p += 5) {
        show_progress(p);
        usleep(100000); /* 100 ms */
    }
    printf("\nDone!\n");
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Canonical**: Line-buffered, local echo enabled by kernel.
- **Raw**: Byte-by-byte instant input, echo disabled.
- **Header**: `#include <termios.h>`.
- **Query/Set**: `tcgetattr(fd, &t)` / `tcsetattr(fd, TCSAFLUSH, &t)`.
- **Restore Rule**: Always restore original attributes via `atexit()`.
- **ANSI Sequences**:
  - `\x1b[2J`: Clear screen.
  - `\x1b[H`: Move cursor to home (1,1).
  - `\x1b[1;31m`: Bold Red text.
  - `\x1b[0m`: Reset attributes.

---

## 13. Further Reading
- W. Richard Stevens, *Advanced Programming in the UNIX Environment*, Chapter 18 (Terminal I/O).
- Michael Kerrisk, *The Linux Programming Interface*, Chapter 62 (Terminals).
- VT100 User Guide: ANSI Escape Sequences Reference.

---

[← Previous Note: Daemons & Direct System Calls](./12-daemons-and-system-calls.md) | [Module Index](README.md) | [Next Note: Filesystem APIs: stat & dirent](./14-filesystem-apis-stat-dirent.md)
