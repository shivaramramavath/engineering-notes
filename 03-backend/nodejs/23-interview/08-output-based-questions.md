# Node.js Technical Interview: Output Prediction & Code Tracing

> 10 tricky, high-frequency interview code execution challenges testing deep knowledge of microtask drains, event loop phase transitions, async/await desugaring, and EventEmitter mechanics.

---

### Challenge 1: The Event Loop & Microtask Priority Gauntlet

#### Code:
```javascript
console.log('1');

setTimeout(() => {
  console.log('2');
  Promise.resolve().then(() => console.log('3'));
  process.nextTick(() => console.log('4'));
}, 0);

setImmediate(() => {
  console.log('5');
});

Promise.resolve().then(() => {
  console.log('6');
  process.nextTick(() => console.log('7'));
});

process.nextTick(() => {
  console.log('8');
  queueMicrotask(() => console.log('9'));
});

console.log('10');
```

#### Output:
```text
1
10
8
6
9
7
2
4
3
5
```

#### Detailed Step-by-Step Explanation:
1. **Synchronous Call Stack**:
   - `console.log('1')` $\rightarrow$ prints `1`.
   - `setTimeout(..., 0)` scheduled in Timers phase min-heap.
   - `setImmediate(...)` scheduled in Check phase queue.
   - Promise registered in Promise Microtask queue (`6`).
   - `process.nextTick` registered in nextTick queue (`8`).
   - `console.log('10')` $\rightarrow$ prints `10`.
2. **First Microtask Drain (before leaving initial execution)**:
   - `nextTick` queue runs first: prints `8`. Schedules `queueMicrotask(9)`.
   - Promise microtask runs: prints `6`. Schedules `process.nextTick(7)`.
   - Next microtask pass: `9` runs $\rightarrow$ prints `9`.
   - Microtask pass cleans remaining `nextTick`: prints `7`.
3. **Event Loop Enters Timers Phase**:
   - Executes timer callback: prints `2`.
   - Inside timer: schedules Promise (`3`) and `process.nextTick` (`4`).
   - **Immediately before leaving Timers phase, microtasks drain**:
     - `nextTick` runs first $\rightarrow$ prints `4`.
     - Promise runs $\rightarrow$ prints `3`.
4. **Event Loop Enters Check Phase**:
   - Executes `setImmediate` callback $\rightarrow$ prints `5`.

---

### Challenge 2: EventEmitter Synchronous Execution

#### Code:
```javascript
import { EventEmitter } from 'node:events';

const emitter = new EventEmitter();

emitter.on('event', () => {
  console.log('A');
  setImmediate(() => console.log('B'));
});

emitter.on('event', () => {
  console.log('C');
});

console.log('1');
emitter.emit('event');
console.log('2');
```

#### Output:
```text
1
A
C
2
B
```

#### Detailed Explanation:
Many candidates mistakenly believe `EventEmitter.emit()` is asynchronous.
**`emitter.emit()` is 100% synchronous!**
It iterates through the registered array of listener functions and invokes them one after another on the active call stack:
1. Prints `1`.
2. `emit('event')` runs immediately:
   - First listener runs: prints `A`, schedules `setImmediate('B')`.
   - Second listener runs: prints `C`.
3. `emit()` returns.
4. Call stack continues: prints `2`.
5. Event loop reaches Check phase: prints `B`.

---

### Challenge 3: Async / Await Execution Desugaring

#### Code:
```javascript
async function async1() {
  console.log('async1 start');
  await async2();
  console.log('async1 end');
}

async function async2() {
  console.log('async2');
}

console.log('script start');

setTimeout(() => {
  console.log('setTimeout');
}, 0);

async1();

new Promise((resolve) => {
  console.log('promise1');
  resolve();
}).then(() => {
  console.log('promise2');
});

console.log('script end');
```

#### Output:
```text
script start
async1 start
async2
promise1
script end
async1 end
promise2
setTimeout
```

#### Detailed Explanation:
1. `script start` prints synchronously.
2. `setTimeout` scheduled in Timers.
3. `async1()` is invoked:
   - Prints `async1 start`.
   - Calls `async2()`, which prints `async2` synchronously and returns a resolved Promise.
   - `await` pauses `async1` and registers the remainder (`console.log('async1 end')`) as a **Promise microtask**.
4. `new Promise(executor)` runs synchronously:
   - Prints `promise1`.
   - `resolve()` registers `.then()` callback as a **Promise microtask**.
5. Prints `script end`.
6. Call stack empty $\rightarrow$ Drain microtask queue:
   - First microtask in queue: `async1 end`.
   - Second microtask in queue: `promise2`.
7. Next tick: Timers phase runs `setTimeout`.

---

### Challenge 4: Recursive `process.nextTick` vs `setImmediate`

#### Code:
```javascript
let count = 0;

function tick() {
  count++;
  if (count < 3) {
    process.nextTick(tick);
  }
}

setImmediate(() => console.log('Immediate 1'));
process.nextTick(tick);
setImmediate(() => console.log('Immediate 2'));

Promise.resolve().then(() => console.log('Promise resolved'));
```

#### Output:
```text
Promise resolved
Immediate 1
Immediate 2
```

#### Detailed Explanation:
- `process.nextTick(tick)` executes recursively 3 times before the initial microtask phase ever releases control.
- Because `nextTick` and `Promise` drain completely before libuv moves to the Check phase, `Promise resolved` finishes before any `setImmediate` is allowed to execute!

---

### Challenge 5: Top-Level Await and Unhandled Rejections

#### Code:
```javascript
process.on('unhandledRejection', (reason) => {
  console.log('Handled rejection:', reason.message);
});

Promise.reject(new Error('Boom 1'));

setTimeout(() => {
  Promise.reject(new Error('Boom 2'));
}, 10);
```

#### Output:
```text
Handled rejection: Boom 1
Handled rejection: Boom 2
```

#### Detailed Explanation:
- In Node.js, `unhandledRejection` catches rejected promises that have no `.catch()` handler attached within the current tick.
- In Node.js 16+, unhandled rejections terminate the process with exit code `1` unless an `unhandledRejection` listener is explicitly registered.
