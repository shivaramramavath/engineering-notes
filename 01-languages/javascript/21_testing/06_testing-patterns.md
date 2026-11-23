# Advanced Testing Patterns in JavaScript

> Test-Driven Development (TDD), Property-Based Testing (fast-check), Mutation Testing (Stryker), table-driven tests, and CI test pipelines.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Testing Fundamentals](file:///e:/Learning/engineering-notes/01-languages/javascript/21_testing/01_testing-fundamentals.md), [Unit Testing](file:///e:/Learning/engineering-notes/01-languages/javascript/21_testing/02_unit-testing.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, Node.js `node:test` / Vitest |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Writing basic assertion tests against known sample inputs (example-based testing) is the entry point to automated verification. However, example-based tests suffer from human bias: developers write tests for the scenarios they anticipated when designing the algorithm. Unanticipated edge cases—Unicode surrogate pairs, negative zeros, array index boundary overflows, or unexpected null inputs—frequently escape into production.

Advanced testing patterns elevate software quality beyond basic happy-path assertions:
- **Test-Driven Development (TDD)** guides architectural modularity and eliminates speculative code.
- **Table-Driven Testing** eliminates assertion boilerplate across dozens of boundary permutations.
- **Property-Based Testing** generates thousands of pseudo-random inputs to discover boundary bugs automatically.
- **Mutation Testing** validates the effectiveness of your test suite by purposefully injecting mutant bugs into your production code to see if tests catch them.

---

## 2. Core Concepts

### The Red-Green-Refactor Cycle (TDD)
1. **Red**: Write a failing automated test that defines a single desired behavior or interface contract before writing any production code.
2. **Green**: Write the minimal amount of code necessary to make the test pass (even if hardcoded or unoptimized).
3. **Refactor**: Clean up the design, remove duplication, and optimize performance while keeping the test suite green.

### Property-Based Testing (Hypothesis / Invariant Verification)
Instead of testing specific examples (`add(2, 3) === 5`), property-based testing defines mathematical or business **invariants** that must hold true for *any* valid input:
- *Commutativity*: `add(a, b) === add(b, a)` for all numbers $a, b$.
- *Idempotence*: `sort(sort(list)) === sort(list)`.
- *Round-Trip Serialization*: `JSON.parse(JSON.stringify(obj)) === obj`.
Libraries like `fast-check` generate thousands of arbitrary inputs (random strings, negative numbers, deeply nested arrays) and automatically **shrink** failing inputs to the minimal reproduction case.

### Mutation Testing (Testing the Tests)
Traditional code coverage (e.g. 95% line coverage) is deceptive: code can be executed by a test without asserting on anything meaningful. Mutation testing tools (like **Stryker**) parse your source code, inject subtle bugs ("mutants"—e.g. changing `>` to `<`, or deleting a function call), and execute your test suite:
- **Killed Mutant**: A test failed when the mutant was injected (Good: the test suite caught the bug!).
- **Survived Mutant**: All tests passed despite the bug (Bad: your test suite has a blind spot!).

---

## 3. How It Works Internally

```
PROPERTY-BASED TESTING PIPELINE (fast-check):
[ Invariant Assertion ] <--- e.g. reverse(reverse(arr)) === arr
           ^
           | Evaluates
+--------------------------------------------------------------------------+
| FAST-CHECK RUNNER                                                        |
| 1. Generates 1,000 pseudo-random inputs:                                  |
|    [], [1], [-42, 999], [0, NaN], ['\u0000', '🔥']                      |
| 2. Executes assertion against every generated case                       |
| 3. On failure: SHRINKS input to minimal failing case (e.g. [NaN])        |
+--------------------------------------------------------------------------+

MUTATION TESTING PIPELINE (Stryker):
Source Code: if (user.age >= 18) { grantAccess(); }
                  |
                  | Stryker Mutator modifies AST
                  v
Mutant #1:   if (user.age > 18)  { grantAccess(); } // Boundary mutation
Mutant #2:   if (false)          { grantAccess(); } // Block elimination
                  |
                  v
[ Run Test Suite against each Mutant ]
  - Did any test fail? -> KILLED (Score +1)
  - Did all tests pass? -> SURVIVED (Coverage Gap Detected!)
```

---

## 4. Syntax & API Reference

### Table-Driven Tests with `test.each`
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

// Parameterized Test Matrix
const cases = [
  { input: 'hello', expected: 'olleh' },
  { input: 'racecar', expected: 'racecar' },
  { input: '', expected: '' },
  { input: 'a', expected: 'a' },
];

describe('reverseString Table-Driven Tests', () => {
  for (const { input, expected } of cases) {
    it(`correctly reverses "${input}" -> "${expected}"`, () => {
      assert.strictEqual(reverseString(input), expected);
    });
  }
});
```

### Property-Based Testing with `fast-check`
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import fc from 'fast-check';

describe('Property-Based String Sorting Invariants', () => {
  it('guarantees sort is idempotent across arbitrary integer arrays', () => {
    fc.assert(
      fc.property(fc.array(fc.integer()), (arr) => {
        const sortedOnce = [...arr].sort((a, b) => a - b);
        const sortedTwice = [...sortedOnce].sort((a, b) => a - b);
        assert.deepStrictEqual(sortedOnce, sortedTwice);
      }),
      { numRuns: 500 } // Execute 500 generated random test iterations
    );
  });
});
```

---

## 5. Practical Examples

### Example 1: Property-Based Testing Round-Trip Compression
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import fc from 'fast-check';
import zlib from 'node:zlib';

function compress(str) {
  return zlib.gzipSync(Buffer.from(str, 'utf8'));
}

function decompress(buffer) {
  return zlib.gunzipSync(buffer).toString('utf8');
}

describe('Gzip Compression Property Tests', () => {
  it('guarantees lossless round-trip compression for arbitrary Unicode strings', () => {
    fc.assert(
      fc.property(fc.fullUnicodeString(), (original) => {
        const compressed = compress(original);
        const restored = decompress(compressed);
        assert.strictEqual(restored, original);
      }),
      { numRuns: 200 }
    );
  });
});
```

### Example 2: Table-Driven Testing for Email Validation Matrix
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

function isValidEmail(email) {
  if (typeof email !== 'string') return false;
  return /^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$/.test(email);
}

const emailTestMatrix = [
  // Valid cases
  { email: 'user@example.com', valid: true, reason: 'standard domain' },
  { email: 'user.name+tag@sub.domain.co', valid: true, reason: 'subdomain and plus tag' },
  { email: '12345@numbers.org', valid: true, reason: 'numeric username' },

  // Invalid cases
  { email: '', valid: false, reason: 'empty string' },
  { email: 'missing-at-sign.com', valid: false, reason: 'no at symbol' },
  { email: '@missing-username.com', valid: false, reason: 'no username' },
  { email: 'user@missing-tld', valid: false, reason: 'no top level domain' },
  { email: 'user@domain..com', valid: false, reason: 'consecutive dots' },
];

describe('isValidEmail Test Matrix', () => {
  for (const { email, valid, reason } of emailTestMatrix) {
    it(`evaluates "${email}" as ${valid} (${reason})`, () => {
      assert.strictEqual(isValidEmail(email), valid);
    });
  }
});
```

### Example 3: Property-Based Math Invariant: Addition Commutativity and Identity
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import fc from 'fast-check';

describe('Arithmetic Property Invariants', () => {
  it('verifies commutative property of addition: a + b === b + a', () => {
    fc.assert(
      fc.property(fc.integer(), fc.integer(), (a, b) => {
        assert.strictEqual(a + b, b + a);
      })
    );
  });

  it('verifies identity property: a + 0 === a', () => {
    fc.assert(
      fc.property(fc.integer(), (a) => {
        assert.strictEqual(a + 0, a);
      })
    );
  });
});
```

### Example 4: Testing State Invariants with Fuzz Testing
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import fc from 'fast-check';

class BoundedQueue {
  constructor(capacity = 5) {
    this.capacity = capacity;
    this.items = [];
  }
  push(item) {
    if (this.items.length >= this.capacity) return false;
    this.items.push(item);
    return true;
  }
  pop() {
    return this.items.shift();
  }
  get size() {
    return this.items.length;
  }
}

describe('BoundedQueue Property Invariants', () => {
  it('guarantees size never exceeds capacity regardless of operation sequence', () => {
    fc.assert(
      fc.property(
        fc.array(fc.record({ op: fc.constantFrom('push', 'pop'), val: fc.integer() })),
        (operations) => {
          const queue = new BoundedQueue(5);

          for (const { op, val } of operations) {
            if (op === 'push') queue.push(val);
            if (op === 'pop') queue.pop();

            // Invariant must hold true after EVERY operation:
            assert.ok(queue.size >= 0 && queue.size <= 5);
          }
        }
      )
    );
  });
});
```

### Example 5: Mutation Testing Setup (Stryker Configuration)
```javascript
// stryker.config.json
{
  "$schema": "https://raw.githubusercontent.com/stryker-mutator/stryker-js/master/packages/api/schema/stryker-core.json",
  "packageManager": "npm",
  "testRunner": "vitest",
  "reporters": ["html", "clear-text", "progress"],
  "coverageAnalysis": "perTest",
  "mutate": ["src/**/*.js", "!src/**/*.test.js"]
}
// Run via: npx stryker run
// Output reports: Mutation Score % (Killed Mutants / Total Mutants)
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Floating Point Round-Off in Property Tests**: In JavaScript, IEEE-754 arithmetic does not obey strict algebraic associativity: `(0.1 + 0.2) + 0.3 !== 0.1 + (0.2 + 0.3)`. Writing property-based tests for floating point numbers without epsilon tolerances (`Math.abs(a - b) < Number.EPSILON`) causes false failures.
2. **Mutation Testing Execution Duration**: Mutation testing runs your entire test suite dozens or hundreds of times (once per generated mutant). On large codebases, this can take hours. Mitigate this by restricting mutation testing to critical business packages or running differential mutation on pull request diffs.
3. **Flaky Input Shrinking**: When `fast-check` discovers a failure, it runs a shrinking heuristic to find the smallest reproduction case. If your function possesses non-deterministic state or external dependencies, shrinking may fail to stabilize.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Writing 50 duplicate test cases with copy-paste code | High maintenance overhead; hard to read | Use Table-Driven parameterized test arrays |
| Relying solely on line coverage metrics | 100% coverage can exist with zero assertions | Use Mutation Testing (Stryker) to verify assertion quality |
| Testing floating point equality with `strictEqual` | Floating point drift causes false assertion failures | Assert with epsilon tolerance: `Math.abs(a - b) < 1e-6` |
| Over-constraining Property Test inputs | Fails to test real boundary edge cases | Let `fast-check` generate full Unicode and extreme integers |
| Practicing TDD without the Refactor step | Results in messy, unmaintainable code | Complete the full Red -> Green -> Refactor cycle |

---

## 8. Performance Characteristics

- **Table-Driven Test Speed**: Executing 100 table-driven test iterations within a single suite runs in $< 2\text{ms}$ in V8.
- **fast-check Generation Speed**: Generating and evaluating 1,000 property-based random cases typically completes in $< 50\text{ms}$.

---

## 9. Engine Internals

1. **Shrinking Algorithms in Property-Based Testing**:
   - When a test fails on an array of 500 items, `fast-check` bisects the array, tests smaller chunks, and simplifies values (e.g. shrinking `987654` $\to$ `0`, or `"hello world"` $\to$ `""`) until it finds the absolute minimal input that triggers the failure.

---

## 10. Best Practices & Design Guidelines

- **Use Table-Driven Tests for Input Mapping**: Whenever a function maps multiple inputs to predictable outputs (parsers, formatters, validators), organize tests into a clean table matrix.
- **Use Property-Based Testing for Parsers and Algorithms**: Property tests excel at finding bugs in data serialization, cryptographic hashing, compression, sorting, and math kernels.
- **Maintain a Mutation Score Target**: In safety-critical financial or medical software, enforce a minimum Stryker mutation score (e.g. $\ge 80\%$) in CI pipelines.

---

## 11. Real-World Case Study

### Uncovering a Critical Tokenizer Crash with Property-Based Testing
```javascript
// THE SCENARIO: A markdown parser passed 100% of its 80 handwritten unit tests.
// Running property-based testing with fast-check generated an unexpected Unicode string:
// Input: "\uD800\uDC00" (Surrogate pair emoji) followed by a null byte "\u0000".

// RESULT: The legacy parser used `str.charCodeAt()` which split the surrogate pair in half,
// causing a fatal infinite loop in the tokenizer.

// THE PROPERTY TEST THAT CAUGHT IT:
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import fc from 'fast-check';

function parseMarkdownTokens(text) {
  // Parser must NEVER throw an unhandled RangeError or enter infinite loop
  const tokens = [];
  let i = 0;
  while (i < text.length) {
    const codePoint = text.codePointAt(i);
    tokens.push(codePoint);
    i += codePoint > 0xffff ? 2 : 1; // Correctly handles surrogate pairs!
  }
  return tokens;
}

describe('Markdown Tokenizer Property Test', () => {
  it('successfully parses any arbitrary Unicode input without hanging or crashing', () => {
    fc.assert(
      fc.property(fc.fullUnicodeString(), (rawText) => {
        const tokens = parseMarkdownTokens(rawText);
        assert.ok(Array.isArray(tokens));
      }),
      { numRuns: 1000 }
    );
  });
});
```

---

## 12. Interview Questions

<details>
<summary>1. What are the three phases of Test-Driven Development (TDD)?</summary>
1. <b>Red</b>: Write a failing unit test that describes the desired behavior before writing the production code.
2. <b>Green</b>: Write the simplest possible code to make the failing test pass.
3. <b>Refactor</b>: Clean up, optimize, and restructure the code while ensuring that all tests continue to pass.
</details>

<details>
<summary>2. How does Property-Based Testing differ from Example-Based Testing?</summary>
Example-based testing asserts on specific predetermined input-output pairs (e.g. <code>add(2, 3) === 5</code>). Property-based testing defines general invariants that must always hold true (e.g. <code>add(a, b) === add(b, a)</code>) and generates hundreds of pseudo-random inputs to discover edge cases automatically.
</details>

<details>
<summary>3. What is "Shrinking" in property-based testing?</summary>
When a property-based test fails on a complex generated input (such as an array of 200 random strings), the runner runs shrinking algorithms to reduce and simplify the input to the smallest possible reproduction case (e.g., an array containing a single empty string), making debugging effortless.
</details>

<details>
<summary>4. What is Mutation Testing and what problem does it solve?</summary>
Mutation testing evaluates the quality of your test suite. It automatically introduces subtle bugs ("mutants") into your production code and runs your tests. If tests fail, the mutant is "killed"; if tests pass, the mutant "survived". This solves the problem of misleading code coverage metrics, proving whether your assertions actually detect bugs.
</details>

<details>
<summary>5. What are Table-Driven Tests and why are they advantageous?</summary>
Table-driven tests store test cases (inputs, expectations, descriptive labels) as an array of objects and iterate over them in a single test block. They eliminate repetitive boilerplate code, make adding new boundary cases trivial, and clearly document all tested variations in a unified table.
</details>

<details>
<summary>6. What is a "Survived Mutant" in mutation testing?</summary>
A survived mutant occurs when a mutation tool introduces a defect into the production code, but all automated tests still pass. This indicates a gap in the test suite: either code was executed without assertions, or the assertions were too loose to detect the behavioral change.
</details>

<details>
<summary>7. How do you test for idempotence?</summary>
An operation is idempotent if applying it multiple times produces the exact same result as applying it once: $f(f(x)) = f(x)$. You test this by asserting <code>assert.deepStrictEqual(fn(fn(input)), fn(input))</code> (e.g. formatting, sorting, or deduplication algorithms).
</details>

<details>
<summary>8. Why is testing floating-point math with exact equality dangerous?</summary>
Due to binary IEEE-754 representation limits, fractional floating point arithmetic suffers from rounding drift ($0.1 + 0.2 = 0.30000000000000004$). Tests must compare numbers using an epsilon tolerance (such as <code>Math.abs(a - b) < 1e-6</code> or <code>expect(a).toBeCloseTo(b)</code>).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Property-Based Base64 Encoder/Decoder Invariant
Write a property-based test using `fast-check` that proves encoding an arbitrary UTF-8 string to Base64 and decoding it back always yields the exact original string without data loss.

<details>
<summary>View Solution</summary>

```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import fc from 'fast-check';

function encodeBase64(str) {
  return Buffer.from(str, 'utf8').toString('base64');
}

function decodeBase64(base64Str) {
  return Buffer.from(base64Str, 'base64').toString('utf8');
}

describe('Base64 Round-Trip Invariant', () => {
  it('guarantees round-trip equality for arbitrary strings', () => {
    fc.assert(
      fc.property(fc.fullUnicodeString(), (original) => {
        const encoded = encodeBase64(original);
        const decoded = decodeBase64(encoded);
        assert.strictEqual(decoded, original);
      }),
      { numRuns: 500 }
    );
  });
});
```
</details>

### Exercise 2: Table-Driven Slug Generator
Write a parameterized table-driven test suite validating a `slugify(text)` function across 6 different edge cases (spaces, punctuation, uppercase characters, multiple consecutive dashes, accents, and empty inputs).

<details>
<summary>View Solution</summary>

```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

function slugify(text) {
  return text
    .normalize('NFD') // Split accents from letters
    .replace(/[\u0300-\u036f]/g, '') // Strip accents
    .toLowerCase()
    .trim()
    .replace(/[^a-z0-9\s-]/g, '') // Strip invalid chars
    .replace(/[\s-]+/g, '-'); // Collapse spaces/dashes
}

const slugTestCases = [
  { input: 'Hello World', expected: 'hello-world' },
  { input: '  Leading & Trailing  ', expected: 'leading-trailing' },
  { input: 'Crème Brûlée', expected: 'creme-brulee' },
  { input: 'Special! @#$ Characters', expected: 'special-characters' },
  { input: 'Multiple---Dashes   Spaces', expected: 'multiple-dashes-spaces' },
  { input: '', expected: '' },
];

describe('slugify Table-Driven Suite', () => {
  for (const { input, expected } of slugTestCases) {
    it(`transforms "${input}" -> "${expected}"`, () => {
      assert.strictEqual(slugify(input), expected);
    });
  }
});
```
</details>
