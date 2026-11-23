# Trie (Prefix Tree) in JavaScript

> A production-grade algorithmic guide to Tries (Prefix Trees) in JavaScript, covering insertion, prefix searching, wildcard regex matching, autocomplete suggestion engines, and Boggle grid traversals.

| Metadata | Value |
| :--- | :--- |
| **Level** | Core / Comprehensive |
| **Prerequisites** | [01_complexity-and-big-o.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/01_complexity-and-big-o.md), [06_trees-and-binary-trees.md](file:///e:/Learning/engineering-notes/01-languages/javascript/24_dsa-in-javascript/06_trees-and-binary-trees.md) |
| **Standard Baseline** | ECMAScript 2024 / Node.js 20+ LTS / V8 Engine |
| **Status** | Production Ready |

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

In search bars, code editors, and routing tables, applications frequently need to query collections of text by prefix:
- Autocompleting search terms as a user types ("app" $\to$ "apple", "applet", "apply").
- IP network routing (Longest Prefix Match in CIDR routing).
- Spell-checking and predictive typing on mobile virtual keyboards.

Searching an array of $N$ words takes $O(N \times L)$ time using linear scanning. A Hash Map allows $O(L)$ exact lookups, but **cannot efficiently query by prefix** (finding all words starting with "pre" requires scanning the entire map).

A **Trie (Prefix Tree)** is a tree data structure used to locate specific keys within a set. Keys are structured along character edges:
- **Prefix Matching in $O(L)$**: Searching for a prefix depends **only on the length of the query string $L$**, completely independent of the total number of words in the dictionary ($N$).
- **Common Prefix Compression**: Words sharing prefixes ("cat", "car", "cart") share identical root nodes in memory.

---

## 2. Core Concepts

### Anatomical Structure of a TrieNode
Each node in a Trie contains:
1. `children`: A collection of pointers to child TrieNodes, keyed by character (`Map<char, TrieNode>` or fixed-size array).
2. `isEndOfWord`: A boolean flag indicating whether the node represents the terminal character of a valid complete word.

```
Trie Storing: ["app", "apple", "apply", "bat"]

                   [ (root) ]
                   /        \
                'a'          'b'
                 │            │
                'p'          'a'
                 │            │
            [ 'p' (end) ]  [ 't' (end) ]
             /        \
           'l'        'l'
            │          │
        [ 'e' (end) ][ 'y' (end) ]
```

---

## 3. How It Works Internally

- **Insertion (`insert(word)`)**:
  Traverse from the root character-by-character. If a child node for the character does not exist, instantiate a new `TrieNode`. At the final character, set `node.isEndOfWord = true`.
- **Search (`search(word)`)**:
  Traverse characters. If any character pointer is missing, return `false`. At the end of the word, return `node.isEndOfWord`.
- **Prefix Query (`startsWith(prefix)`)**:
  Traverse characters. If all characters exist, return `true` immediately, regardless of `isEndOfWord`.

---

## 4. Syntax & API Reference

### Production-Grade Trie Implementation
```javascript
export class TrieNode {
  constructor() {
    this.children = new Map(); // char -> TrieNode
    this.isEndOfWord = false;
  }
}

export class Trie {
  constructor() {
    this.root = new TrieNode();
  }

  insert(word) {
    let current = this.root;
    for (let i = 0; i < word.length; i++) {
      const char = word[i];
      if (!current.children.has(char)) {
        current.children.set(char, new TrieNode());
      }
      current = current.children.get(char);
    }
    current.isEndOfWord = true;
  }

  search(word) {
    const node = this._findNode(word);
    return node !== null && node.isEndOfWord === true;
  }

  startsWith(prefix) {
    return this._findNode(prefix) !== null;
  }

  _findNode(str) {
    let current = this.root;
    for (let i = 0; i < str.length; i++) {
      const char = str[i];
      if (!current.children.has(char)) {
        return null;
      }
      current = current.children.get(char);
    }
    return current;
  }
}
```

---

## 5. Practical Examples

### Example 1: Autocomplete Suggestions Engine
```javascript
import { Trie, TrieNode } from './Trie.js';

export class AutocompleteTrie extends Trie {
  /**
   * Retrieves all words in the dictionary matching prefix up to maxLimit.
   */
  getSuggestions(prefix, maxLimit = 10) {
    const startNode = this._findNode(prefix);
    if (!startNode) return [];

    const suggestions = [];

    // Run DFS to collect all words in subtree rooted at prefix
    function collect(node, currentPath) {
      if (suggestions.length >= maxLimit) return;

      if (node.isEndOfWord) {
        suggestions.push(currentPath);
      }

      for (const [char, childNode] of node.children.entries()) {
        collect(childNode, currentPath + char);
      }
    }

    collect(startNode, prefix);
    return suggestions;
  }
}

// const ac = new AutocompleteTrie();
// ['apple', 'app', 'application', 'apply', 'banana'].forEach(w => ac.insert(w));
// console.log(ac.getSuggestions('app', 3)); // ['app', 'apple', 'application']
```

### Example 2: Wildcard Dictionary Search (`.` Matches Any Character)
```javascript
export class WordDictionary {
  constructor() {
    this.root = new TrieNode();
  }

  addWord(word) {
    let current = this.root;
    for (const char of word) {
      if (!current.children.has(char)) {
        current.children.set(char, new TrieNode());
      }
      current = current.children.get(char);
    }
    current.isEndOfWord = true;
  }

  search(word) {
    function dfs(node, idx) {
      if (idx === word.length) {
        return node.isEndOfWord;
      }

      const char = word[idx];

      if (char === '.') {
        // Wildcard: explore all existing child branches
        for (const childNode of node.children.values()) {
          if (dfs(childNode, idx + 1)) return true;
        }
        return false;
      }

      if (!node.children.has(char)) return false;
      return dfs(node.children.get(char), idx + 1);
    }

    return dfs(this.root, 0);
  }
}
```

### Example 3: Word Search II (Boggle Grid Solver with Trie Pruning)
```javascript
export function findWords(board, words) {
  const trie = new Trie();
  for (const w of words) trie.insert(w);

  const rows = board.length;
  const cols = board[0].length;
  const results = new Set();

  function backtrack(r, c, node, path) {
    if (r < 0 || c < 0 || r >= rows || c >= cols || board[r][c] === '#') {
      return;
    }

    const char = board[r][c];
    if (!node.children.has(char)) return; // Prune branch if prefix does not exist!

    const nextNode = node.children.get(char);
    const newPath = path + char;

    if (nextNode.isEndOfWord) {
      results.add(newPath);
    }

    // Choose: Mask cell
    board[r][c] = '#';

    // Explore 4 directions
    backtrack(r + 1, c, nextNode, newPath);
    backtrack(r - 1, c, nextNode, newPath);
    backtrack(r, c + 1, nextNode, newPath);
    backtrack(r, c - 1, nextNode, newPath);

    // Unchoose: Restore cell
    board[r][c] = char;
  }

  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      backtrack(r, c, trie.root, '');
    }
  }

  return Array.from(results);
}
```

### Example 4: Longest Common Prefix via Trie
```javascript
export function longestCommonPrefix(strs) {
  if (!strs || strs.length === 0) return '';
  if (strs.length === 1) return strs[0];

  const root = new TrieNode();

  // Insert all words
  for (const s of strs) {
    if (s === '') return '';
    let curr = root;
    for (const char of s) {
      if (!curr.children.has(char)) {
        curr.children.set(char, new TrieNode());
      }
      curr = curr.children.get(char);
    }
    curr.isEndOfWord = true;
  }

  // Traverse while node has exactly one child and is not the end of a word
  let prefix = '';
  let curr = root;

  while (curr.children.size === 1 && !curr.isEndOfWord) {
    const [char, nextNode] = curr.children.entries().next().value;
    prefix += char;
    curr = nextNode;
  }

  return prefix;
}
```

### Example 5: Deleting Words from a Trie with Orphan Pruning
```javascript
export class DeletableTrie extends Trie {
  delete(word) {
    function remove(node, str, depth) {
      if (!node) return false;

      // Base case: reached end of word
      if (depth === str.length) {
        if (!node.isEndOfWord) return false; // Word does not exist
        node.isEndOfWord = false;
        // If node has no other children, it can be deleted
        return node.children.size === 0;
      }

      const char = str[depth];
      const child = node.children.get(char);
      if (!child) return false;

      const shouldDeleteChild = remove(child, str, depth + 1);

      if (shouldDeleteChild) {
        node.children.delete(char);
        // Return true if current node is not a word and has no other children
        return !node.isEndOfWord && node.children.size === 0;
      }

      return false;
    }

    remove(this.root, word, 0);
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Memory Overhead of Fixed-Size Arrays**:
   In C/Java, `children` is often implemented as a fixed array `new Array(26)`. In JavaScript, allocating 26-slot arrays on thousands of empty leaf nodes bloats memory. Using **`new Map()`** stores only existing child characters, cutting memory usage by over 75% for sparse vocabularies.
2. **Deleting Words Without Pruning Nodes**:
   Simply setting `node.isEndOfWord = false` leaves orphaned node branches in memory forever. A production Trie must prune childless nodes back up toward the root during deletion.
3. **Empty String Handling**:
   Inserting `""` must set `root.isEndOfWord = true`. Always verify whether the root node itself can represent an empty string.
4. **Case Sensitivity & Unicode**:
   Ensure keys are normalized (`word.toLowerCase()`) or use standard `Map` keys to handle international character sets without index calculation errors.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Operational Hazard | Production Fix |
| :--- | :--- | :--- |
| Fixed 26-element array child allocations in JS | Excessive heap memory overhead on sparse nodes. | Use `new Map()` to allocate child pointers on demand. |
| Forgetting to mark `isEndOfWord = true` | `search()` returns false for valid inserted words. | Ensure terminal character node sets `isEndOfWord = true`. |
| Unbounded recursive DFS in autocomplete | Call stack overflow if subtree contains deep word chains. | Cap suggestion results with a `maxLimit` counter. |
| Scanning entire Trie on every keystroke | Degrades autocomplete response latency. | Traverse directly to the prefix node in $O(L)$, then collect. |
| Neglecting orphan pruning on deletion | Memory leaks accumulate from unlinked character branches. | Prune childless nodes upward when `isEndOfWord` is cleared. |
| Linear scanning word lists in 2D Boggle games | Causes $O(\text{Words} \times 4^{\text{Len}})$ timeouts. | Insert dictionary into Trie and prune board paths early. |

---

## 8. Performance Characteristics

- **Time Complexity**:
  - `insert(word)`: **$O(L)$**, where $L$ is word length.
  - `search(word)`: **$O(L)$**, completely independent of total words $N$.
  - `startsWith(prefix)`: **$O(L)$**.
  - `getSuggestions(prefix)`: **$O(L + K)$**, where $K$ is the number of collected nodes.
- **Space Complexity**:
  - Worst Case: **$O(N \times L)$** if words share zero common prefixes.
  - Average Case: Substantially lower than $O(N \times L)$ due to prefix node sharing.

---

## 9. Engine Internals

### V8 Map Traversal & Hidden Class Shape
- **Map Hash Lookup**: Accessing `current.children.get(char)` in V8 executes an $O(1)$ hash table lookup. Because the alphabet per node is typically small (2–6 branches), V8 internalizes child lookups with sub-microsecond latency.
- **Node Object Uniformity**: Initializing `TrieNode` instances with identical properties (`children`, `isEndOfWord`) ensures that all nodes share a single V8 Hidden Class, keeping property accesses monomorphic.

---

## 10. Best Practices & Design Guidelines

1. **Use `Map` for Character Children**: Accommodates international Unicode, uppercase, numbers, and symbols without character code math.
2. **Cap Autocomplete Outputs**: Always pass a `limit` parameter (e.g. 5–10 items) to prevent deep traversals when a user types a single letter like "a".
3. **Use Trie for Crossword / Boggle Solvers**: Pruning word search paths at the root of invalid prefixes eliminates billions of redundant permutations.

---

## 11. Real-World Case Study

### High-Speed Search Engine Autocomplete Service
**Scenario**: An e-commerce search bar handled 80,000 keystrokes per second. As users typed, the backend suggested top matching product names.
**Problem**: An early database query `SELECT title FROM products WHERE title LIKE 'app%' LIMIT 5` took $42\text{ms}$ per keystroke, overloading database CPU.

**Solution Architecture**:
- Built an in-memory `AutocompleteTrie` loaded with 150,000 product keywords.
- When a user enters `"iph"`, the service:
  1. Jumps to the `"iph"` node in 3 pointer hops ($O(L) \approx 0.0001\text{ms}$).
  2. Runs a bounded DFS to collect the top 5 suggestions.
- The entire query completes in **$0.04\text{ms}$ ($40\mu\text{s}$)**.

**Outcome**:
- Database load for typeahead dropped to zero.
- P99 search response latency dropped from $42\text{ms}$ to $<1\text{ms}$.

---

## 12. Interview Questions

<details>
<summary>1. How does a Trie compare to a Hash Map for storing string dictionaries?</summary>

- **Hash Map**: Provides average $O(L)$ exact match lookups, but **cannot perform prefix queries** without scanning all $N$ keys ($O(N \times L)$). Also vulnerable to hash collisions.
- **Trie**: Provides guaranteed $O(L)$ exact match AND **$O(L)$ prefix lookups**. It naturally supports lexicographical ordering, autocompletion, and wildcard searches. However, a Trie consumes more memory due to pointer overhead on node objects.
</details>

<details>
<summary>2. Why does Trie lookup time depend on word length $L$ rather than dictionary size $N$?</summary>

In a Trie, each character of the search key represents one edge transition down the tree. To search for a word of length $L$, the algorithm follows exactly $L$ pointers from the root. It never compares the query string against other words in the dictionary. Consequently, whether the Trie holds 100 words or 100,000,000 words, searching for a 5-letter word requires exactly 5 pointer hops ($O(L)$).
</details>

<details>
<summary>3. What is the difference between implementing Trie children with an Array vs a Map in JavaScript?</summary>

- **Fixed Array (`new Array(26)`)**: Useful in languages like C where array indexing by character code (`code - 97`) has zero overhead. In JavaScript, allocating empty arrays on thousands of leaf nodes wastes heap memory.
- **`Map` (`new Map()`)**: Allocates child pointers on-demand only for characters that actually exist. It saves substantial memory on sparse branches and automatically supports uppercase, numbers, and Unicode characters without offset math.
</details>

<details>
<summary>4. How does a Trie enable $O(1)$ pruning in Word Search / Boggle puzzles?</summary>

In Boggle, traversing a 2D board naively explores $4^L$ paths. By storing the dictionary in a Trie, as we construct a candidate word character-by-character on the board, we check `trieNode.children.has(char)`. If the character is missing, it is impossible for *any* word in the dictionary to begin with this sequence. The algorithm prunes the entire search branch immediately, reducing search time by orders of magnitude.
</details>

<details>
<summary>5. How do you implement wildcard search (e.g. <code>'.'</code> matches any character) in a Trie?</summary>

Using recursive DFS:
- If current character is a letter, proceed to `node.children.get(char)` ($O(1)$).
- If current character is `'.'`: iterate through all child nodes in `node.children.values()` and recursively test if any branch satisfies the remainder of the word.
Time complexity is $O(L)$ for standard characters, and worst-case $O(26^L)$ if all characters are wildcards.
</details>

<details>
<summary>6. How can you optimize a Trie's memory usage when branches do not split?</summary>

Using a **Radix Tree (Compressed Trie)**. In a Radix Tree, any node with only one child is merged with its child. Rather than storing a single character per edge, edges store strings (e.g. edge `"apple"` instead of `'a' \to 'p' \to 'p' \to 'l' \to 'e'`). This drastically reduces node allocations for long unique strings.
</details>

<details>
<summary>7. What is Longest Prefix Match, and how is it used in network routing?</summary>

In network IP routing, a router matches an incoming packet's destination IP against routing table CIDR prefixes (e.g. `192.168.0.0/16` vs `192.168.1.0/24`). A binary Trie (where branches are 0 and 1) is traversed bit-by-bit using the destination IP. The deepest node marked as a route endpoint represents the **Longest Prefix Match**, providing the most specific routing rule in $O(32) = O(1)$ time.
</details>

<details>
<summary>8. How do you delete a word from a Trie without corrupting other words sharing the same prefix?</summary>

Use post-order recursive traversal:
1. Traverse to the end of the word.
2. Unset `isEndOfWord = false`.
3. As the recursion unwinds, check if the current node has zero children and `isEndOfWord === false`. If so, delete the child pointer from its parent's `children` map.
4. If a node has other children or represents another complete word, halt deletion.
This guarantees that shared prefixes remain intact while orphaned character nodes are safely pruned.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Implement Prefix Tree with `insert`, `search`, and `startsWith`
**Specification**: Implement the standard `Trie` class with all three operations running in $O(L)$ time.

<details>
<summary>Solution</summary>

```javascript
export class StandardTrie {
  constructor() {
    this.root = { children: new Map(), isEnd: false };
  }

  insert(word) {
    let curr = this.root;
    for (const char of word) {
      if (!curr.children.has(char)) {
        curr.children.set(char, { children: new Map(), isEnd: false });
      }
      curr = curr.children.get(char);
    }
    curr.isEnd = true;
  }

  search(word) {
    let curr = this.root;
    for (const char of word) {
      if (!curr.children.has(char)) return false;
      curr = curr.children.get(char);
    }
    return curr.isEnd;
  }

  startsWith(prefix) {
    let curr = this.root;
    for (const char of prefix) {
      if (!curr.children.has(char)) return false;
      curr = curr.children.get(char);
    }
    return true;
  }
}
```
</details>

### Exercise 2: Map Sum Pairs
**Specification**: Design a map that allows you to insert a key-value pair, and then calculate the sum of all values whose keys have a specific prefix.

<details>
<summary>Solution</summary>

```javascript
class MapSumNode {
  constructor() {
    this.children = new Map();
    this.val = 0;
  }
}

export class MapSum {
  constructor() {
    this.root = new MapSumNode();
    this.inserted = new Map(); // Tracks existing keys to calculate delta
  }

  insert(key, val) {
    const delta = val - (this.inserted.get(key) || 0);
    this.inserted.set(key, val);

    let curr = this.root;
    curr.val += delta;

    for (const char of key) {
      if (!curr.children.has(char)) {
        curr.children.set(char, new MapSumNode());
      }
      curr = curr.children.get(char);
      curr.val += delta; // Update sum on every node along the prefix path
    }
  }

  sum(prefix) {
    let curr = this.root;
    for (const char of prefix) {
      if (!curr.children.has(char)) return 0;
      curr = curr.children.get(char);
    }
    return curr.val; // O(L) prefix sum retrieval!
  }
}
```
</details>
