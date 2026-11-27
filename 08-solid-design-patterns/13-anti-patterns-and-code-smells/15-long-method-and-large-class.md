# Long Method & Large Class

| Field | Value |
| :--- | :--- |
| **Category** | Code Smells (Bloaters) |
| **Fix** | Extract Method, Decompose Conditional, Extract Class |

> **TL;DR**: A 150-line method with 4 section comments (`// 1. Validate`, `// 2. Price`, `// 3. Persist`) is begging to be split into 3 well-named small functions.

## Why Small Functions Win
- No comments needed when the function name (`calculateTax()`, `reserveInventory()`) explains intent.
- Shallow nesting + early returns make edge cases obvious.
- Note: Don't obsess over an arbitrary "5-line limit"—split at **cohesive abstraction boundaries**.
