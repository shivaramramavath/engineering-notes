# Chrome DevTools Shortcuts & Console API Cheatsheet

> Quick-reference guide covering Chrome DevTools keyboard shortcuts, Console API debugging methods, DOM inspection shortcuts, and memory profiling utilities.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | Chrome DevTools / Chromium 120+ |
| **Level** | Reference / Quick Lookup |

---

## 1. Global Navigation Shortcuts

| Action | Windows / Linux | macOS |
| :--- | :--- | :--- |
| **Open DevTools** | `F12` or `Ctrl + Shift + I` | `Cmd + Option + I` |
| **Open Console Panel** | `Ctrl + Shift + J` | `Cmd + Option + J` |
| **Open Command Menu** | `Ctrl + Shift + P` | `Cmd + Shift + P` |
| **Open File by Name** | `Ctrl + P` | `Cmd + P` |
| **Search Across All Sources** | `Ctrl + Shift + F` | `Cmd + Option + F` |
| **Toggle Drawer Pane** | `Esc` | `Esc` |
| **Switch to Next / Prev Panel** | `Ctrl + ]` / `Ctrl + [` | `Cmd + ]` / `Cmd + [` |

---

## 2. Console Utility Expressions (`$`)

| Expression | Meaning & Return Value |
| :--- | :--- |
| `$0` | The currently selected DOM node in the Elements panel |
| `$1`, `$2`, `$3` | Previously selected DOM nodes (history stack) |
| `$('selector')` | Shortcut for `document.querySelector('selector')` |
| `$$('selector')` | Shortcut for `Array.from(document.querySelectorAll('selector'))` |
| `$_` | The evaluated result of the most recently executed console expression |
| `$x('//xpath')` | Evaluates XPath query and returns matching nodes array |
| `copy(value)` | Copies string or serialized object to system clipboard |
| `inspect(node)` | Jumps to Elements panel and selects the specified node |

---

## 3. Advanced Console API Debugging Methods

```javascript
// Tabular Visualization:
console.table([{ name: 'Alice', role: 'Admin' }, { name: 'Bob', role: 'User' }]);

// Timing Invocations:
console.time('fetchTimer');
// ... long-running operation ...
console.timeEnd('fetchTimer'); // Logs: fetchTimer: 42.15ms

// Call-Stack Inspection:
function traceMe() {
  console.trace('Execution reached trace point');
}

// In-Memory Object Profiling:
console.dir(document.body); // Inspect properties without DOM formatting
console.count('loopIteration'); // Monotonically counts calls with that label

// Conditional Assertions (Logs only when condition is false):
console.assert(user.age >= 18, 'User must be 18 or older');
```

---

## 4. Source Code Debugger & Breakpoints

| Shortcut | Action | Description |
| :--- | :--- | :--- |
| `F8` or `Ctrl + \` | **Pause / Resume** | Resumes script until next breakpoint |
| `F10` or `Ctrl + '` | **Step Over** | Advances to next line without entering function calls |
| `F11` or `Ctrl + ;` | **Step Into** | Enters inside the function being invoked on this line |
| `Shift + F11` | **Step Out** | Finishes current function and pauses at caller |
| `debugger;` | **Programmatic Breakpoint** | Pauses execution if DevTools is open |
