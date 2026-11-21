# Trie (Prefix Tree)

> High-performance retrieval trees for prefix queries, dictionary lookups, autocomplete engines, and bitwise prefix manipulations.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01_arrays-and-strings.md](01_arrays-and-strings.md), [07_trees-binary-and-bst.md](07_trees-binary-and-bst.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Java Version** | Java 8+ (Verified on Java 25 LTS) |

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

A **Trie** (derived from re**trie**val, pronounced like "try" or "tree") is an $m$-ary tree data structure used to locate specific keys within a set. Unlike binary search trees where keys are stored within the nodes themselves, a node's position in the trie defines the key with which it is associated. All the descendants of a node share a common string prefix.

### The Problem Solved
Consider an autocomplete feature across a vocabulary of $N = 1,000,000$ words of average length $L = 10$.
- Searching in an unsorted array: $O(N \times L)$ per lookup.
- Searching in a sorted array via binary search: $O(L \log N)$ to find an exact match, but finding all words with prefix $P$ requires scanning or bounds checking.
- Searching in a Hash Table (`HashSet<String>`): $O(L)$ for exact matches, but prefix searches (`startsWith("pre")`) degrade to full scans $O(N \times L)$.
- **Trie**: Exact match lookup and prefix search take $O(L)$ time, strictly proportional to the query string length, completely independent of total vocabulary size $N$.

---

## 2. Core Concepts

1. **Root Node**: Represents an empty string prefix `""`.
2. **Edges / Links**: Represent characters. In standard English lowercase tries, each node contains up to 26 child pointers.
3. **`isEndOfWord` Flag**: A boolean indicating whether the path from the root down to this node represents a complete registered word, or merely an intermediate prefix.
4. **Compressed Trie (Radix Tree / Patricia Trie)**: Nodes with single children are merged to conserve memory.
5. **Bitwise Trie**: Operates on bits (0 and 1) rather than characters. Essential for Maximum XOR problems and IP routing (longest prefix matching).

---

## 3. How It Works Internally

### Node Memory Layout in Java

In HotSpot 64-bit JVM with Compressed OOPs enabled:
```
TrieNode Object Header: 12 bytes
Array reference children: 4 bytes
boolean isEndOfWord: 1 byte
padding: 7 bytes
Total TrieNode shallow size: 24 bytes

Child Array TrieNode[26]:
Header: 16 bytes
26 references * 4 bytes: 104 bytes
Total array size: 120 bytes

Every allocated TrieNode costs ~144 bytes!
```

```
       Root ("")
      /         \
    'c'         't'
    /             \
  'a'             'o'
  / \              \
't' 'r'            'p'* (top)
 *   *
(cat)(car)
```

Asterisks `*` mark `isEndOfWord = true`.

---

## 4. Syntax & Usage

### Canonical Standard Trie Implementation

```java
public class StandardTrie {
    public static final class TrieNode {
        public final TrieNode[] children = new TrieNode[26];
        public boolean isEndOfWord;
        public int prefixCount; // Number of words passing through this prefix
    }

    private final TrieNode root = new TrieNode();

    public void insert(String word) {
        TrieNode curr = root;
        for (int i = 0; i < word.length(); i++) {
            int idx = word.charAt(i) - 'a';
            if (curr.children[idx] == null) {
                curr.children[idx] = new TrieNode();
            }
            curr = curr.children[idx];
            curr.prefixCount++;
        }
        curr.isEndOfWord = true;
    }

    public boolean search(String word) {
        TrieNode node = findNode(word);
        return node != null && node.isEndOfWord;
    }

    public boolean startsWith(String prefix) {
        return findNode(prefix) != null;
    }

    private TrieNode findNode(String str) {
        TrieNode curr = root;
        for (int i = 0; i < str.length(); i++) {
            int idx = str.charAt(i) - 'a';
            if (curr.children[idx] == null) {
                return null;
            }
            curr = curr.children[idx];
        }
        return curr;
    }
}
```

---

## 5. Practical Examples

### Example 1: Implement Trie (Prefix Tree) — LeetCode 208

```java
public class Trie {
    private static final class Node {
        Node[] next = new Node[26];
        boolean isTerminal;
    }

    private final Node root = new Node();

    public void insert(String word) {
        Node p = root;
        for (char c : word.toCharArray()) {
            int u = c - 'a';
            if (p.next[u] == null) p.next[u] = new Node();
            p = p.next[u];
        }
        p.isTerminal = true;
    }

    public boolean search(String word) {
        Node p = traverse(word);
        return p != null && p.isTerminal;
    }

    public boolean startsWith(String prefix) {
        return traverse(prefix) != null;
    }

    private Node traverse(String s) {
        Node p = root;
        for (char c : s.toCharArray()) {
            int u = c - 'a';
            if (p.next[u] == null) return null;
            p = p.next[u];
        }
        return p;
    }
}
```

### Example 2: Design Add and Search Words Data Structure (Wildcard `.`) — LeetCode 211

```java
public class WordDictionary {
    private static final class Node {
        Node[] children = new Node[26];
        boolean isWord;
    }

    private final Node root = new Node();

    public void addWord(String word) {
        Node curr = root;
        for (char c : word.toCharArray()) {
            int idx = c - 'a';
            if (curr.children[idx] == null) curr.children[idx] = new Node();
            curr = curr.children[idx];
        }
        curr.isWord = true;
    }

    public boolean search(String word) {
        return match(word.toCharArray(), 0, root);
    }

    private boolean match(char[] chs, int k, Node node) {
        if (k == chs.length) return node.isWord;
        char c = chs[k];
        if (c != '.') {
            Node child = node.children[c - 'a'];
            return child != null && match(chs, k + 1, child);
        } else {
            for (Node child : node.children) {
                if (child != null && match(chs, k + 1, child)) {
                    return true;
                }
            }
            return false;
        }
    }
}
```

### Example 3: Word Search II (Trie + 2D Grid Backtracking) — LeetCode 212

```java
import java.util.*;

public class WordSearchII {
    private static final class TrieNode {
        TrieNode[] next = new TrieNode[26];
        String word; // Store full word at leaf for O(1) collection
    }

    public List<String> findWords(char[][] board, String[] words) {
        TrieNode root = buildTrie(words);
        List<String> result = new ArrayList<>();
        int m = board.length, n = board[0].length;
        for (int r = 0; r < m; r++) {
            for (int c = 0; c < n; c++) {
                dfs(board, r, c, root, result);
            }
        }
        return result;
    }

    private void dfs(char[][] board, int r, int c, TrieNode p, List<String> res) {
        char ch = board[r][c];
        if (ch == '#' || p.next[ch - 'a'] == null) return;
        p = p.next[ch - 'a'];
        if (p.word != null) {
            res.add(p.word);
            p.word = null; // De-duplicate results
        }

        board[r][c] = '#'; // Mark visited
        if (r > 0) dfs(board, r - 1, c, p, res);
        if (c > 0) dfs(board, r, c - 1, p, res);
        if (r < board.length - 1) dfs(board, r + 1, c, p, res);
        if (c < board[0].length - 1) dfs(board, r, c + 1, p, res);
        board[r][c] = ch; // Backtrack
    }

    private TrieNode buildTrie(String[] words) {
        TrieNode root = new TrieNode();
        for (String w : words) {
            TrieNode p = root;
            for (char c : w.toCharArray()) {
                int i = c - 'a';
                if (p.next[i] == null) p.next[i] = new TrieNode();
                p = p.next[i];
            }
            p.word = w;
        }
        return root;
    }
}
```

### Example 4: Maximum XOR of Two Numbers in an Array (Bitwise Trie) — LeetCode 421

```java
public class MaximumXOR {
    private static final class BitNode {
        BitNode[] children = new BitNode[2]; // 0 and 1
    }

    public int findMaximumXOR(int[] nums) {
        BitNode root = new BitNode();
        // Insert all numbers bit by bit (from MSB bit 30 down to 0)
        for (int num : nums) {
            BitNode curr = root;
            for (int i = 30; i >= 0; i--) {
                int bit = (num >>> i) & 1;
                if (curr.children[bit] == null) {
                    curr.children[bit] = new BitNode();
                }
                curr = curr.children[bit];
            }
        }

        int maxXor = 0;
        // Query maximum possible XOR partner
        for (int num : nums) {
            BitNode curr = root;
            int currentXor = 0;
            for (int i = 30; i >= 0; i--) {
                int bit = (num >>> i) & 1;
                int desiredBit = 1 - bit; // Invert bit for maximum XOR
                if (curr.children[desiredBit] != null) {
                    currentXor |= (1 << i);
                    curr = curr.children[desiredBit];
                } else {
                    curr = curr.children[bit];
                }
            }
            maxXor = Math.max(maxXor, currentXor);
        }
        return maxXor;
    }
}
```

### Example 5: Autocomplete & Top-K Prefix Suggestions

```java
import java.util.*;

public class AutocompleteSystem {
    private static final class AutoNode {
        Map<Character, AutoNode> children = new HashMap<>();
        boolean isTerm;
        List<String> topWords = new ArrayList<>();
    }

    private final AutoNode root = new AutoNode();

    public void addWord(String word) {
        AutoNode curr = root;
        for (char c : word.toCharArray()) {
            curr.children.putIfAbsent(c, new AutoNode());
            curr = curr.children.get(c);
            // Maintain top suggestions along prefix path
            if (!curr.topWords.contains(word)) {
                curr.topWords.add(word);
                if (curr.topWords.size() > 5) {
                    curr.topWords.remove(curr.topWords.size() - 1);
                }
            }
        }
        curr.isTerm = true;
    }

    public List<String> suggest(String prefix) {
        AutoNode curr = root;
        for (char c : prefix.toCharArray()) {
            curr = curr.children.get(c);
            if (curr == null) return Collections.emptyList();
        }
        return Collections.unmodifiableList(curr.topWords);
    }
}
```

---

## 6. Best Practices

1. **Fixed Alphabet Array vs. Map**: Use `TrieNode[26]` for lowercase ASCII (ultra-fast array indexing). Use `Map<Character, TrieNode>` only for Unicode or arbitrarily large character sets.
2. **Prune Trie During Deletion**: When deleting words, recursively remove child nodes if `isEndOfWord == false` and all children are null to prevent memory leaks.
3. **Store Leaf String Directly**: In backtracking puzzles (like Word Search II), storing the complete `String word` at the terminal node avoids building strings with `StringBuilder` during search.
4. **Pre-allocate Bitwise Tries**: For integers, 31 bits are sufficient for positive signed 32-bit integers; 32 bits for unsigned or full two's complement ranges.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Allocating `new TrieNode[256]` indiscriminately | Consumes massive memory on empty nodes | Restrict alphabet size to 26 (`'a'...'z'`) or use a sparse Map. |
| Forgetting `isEndOfWord = false` on deletion | Node still registers as a completed word | Clear flag and propagate null pruning back to root. |
| Memory exhaustion with large dictionaries | $10^6$ nodes consume hundreds of megabytes on JVM | Use a Compressed Radix Tree (Patricia Trie) or Double-Array Trie. |
| Out of Bounds on non-lowercase input | `char - 'a'` produces negative index on uppercase | Validate inputs or normalize using `Character.toLowerCase()`. |
| Modifying state during concurrent searches | Thread safety violations | Keep the Trie immutable after construction or use read-write locks. |

---

## 8. Performance & Security Notes

- **Time Complexity**:
  - Insert: $O(L)$ where $L$ is word length.
  - Search / Prefix: $O(L)$.
  - Delete: $O(L)$.
- **Space Complexity**: Worst case $O(\sum L \times |\Sigma|)$ where $|\Sigma|$ is the alphabet size.
- **Cache Locality**: Standard node-pointer tries suffer from poor CPU cache locality due to pointer chasing across scattered heap objects. For performance-critical search engines, memory-mapped byte buffers or Flat / Double-Array Tries (DAT) are preferred.
- **Security (Denial of Service)**: Maliciously crafted deep prefixes ($L = 100,000$) can cause `StackOverflowError` in recursive search/delete methods. Always use iterative traversal for production libraries.

---

## 9. Comparison / When to Use What

| Data Structure | Lookup Time | Prefix Search | Memory Usage | Ordered Traversal |
|---|---|---|---|---|
| **Trie** | $O(L)$ | $O(L)$ | High (pointers) | Yes (alphabetical DFS) |
| **HashSet** | $O(L)$ average | $O(N \times L)$ full scan | Moderate | No |
| **TreeMap** | $O(L \log N)$ | $O(L \log N)$ (subMap) | Low | Yes |
| **Radix Tree** | $O(L)$ | $O(L)$ | Low-to-Moderate | Yes |

---

## 10. Interview Questions

<details>
<summary>1. How does a Trie compare to a Hash Table for dictionary lookups?</summary>

A Hash Table provides $O(L)$ average lookup where $L$ is key length, but suffers from hash collision risks ($O(N)$ worst case) and cannot efficiently perform prefix queries. A Trie guarantees strictly $O(L)$ worst-case time, enables autocomplete/prefix scans, and supports lexicographical sorting, at the expense of higher memory usage.
</details>

<details>
<summary>2. How do you delete a word from a Trie without corrupting overlapping words?</summary>

Use a post-order traversal:
1. Traverse to the leaf node representing the word.
2. Set `isEndOfWord = false`.
3. Backtrack up the tree: if the current node has no children and `isEndOfWord == false`, unlink the node from its parent. Stop backtracking as soon as a node has other children or represents another valid word.
</details>

<details>
<summary>3. What is a Radix Tree (Patricia Trie)?</summary>

A Radix Tree is a space-optimized Trie where every internal node with only one child is merged with its child. For example, the path `r -> o -> o -> t` is collapsed into a single edge holding `"root"`, reducing node count from $O(L)$ to $O(K)$ where $K$ is the number of branching keys.
</details>

<details>
<summary>4. Why is Bitwise Trie optimal for Maximum XOR problems?</summary>

Bitwise XOR yields 1 when bits differ ($1 \oplus 0 = 1$). To maximize XOR for a number $X$, we want each bit of $Y$ to be the opposite of $X$ at the most significant bits. A Bitwise Trie allows greedy traversal: at bit $i$, if a branch with bit `1 - bit(X)` exists, follow it; otherwise follow `bit(X)`. This finds the optimal partner in $O(32) = O(1)$ per number.
</details>

<details>
<summary>5. How would you handle Unicode or arbitrary characters in a Trie?</summary>

Replace the fixed `TrieNode[26]` array with a `Map<Character, TrieNode>` or an array-mapped trie (e.g., using `int[]` or sparse tables). Alternatively, encode Unicode strings as UTF-8 byte sequences and build a 256-ary byte trie.
</details>

<details>
<summary>6. How do you find the longest common prefix among an array of strings using a Trie?</summary>

Insert all strings into a Trie. Start at the root and follow the path as long as:
1. The current node has exactly 1 non-null child.
2. `isEndOfWord` is false.
The accumulated characters form the longest common prefix.
</details>

<details>
<summary>7. What is a Ternary Search Tree (TST)?</summary>

A TST combines the time efficiency of a Trie with the memory efficiency of a BST. Each node contains a character, an end-of-word flag, and three child pointers: `low` (smaller characters), `equal` (next character in matching prefix), and `high` (larger characters). It avoids allocating unused array slots.
</details>

<details>
<summary>8. How do you make a Trie thread-safe for high concurrency?</summary>

1. Use `ConcurrentHashMap` for node children.
2. Use copy-on-write persistent data structures.
3. Use `StampedLock` or `ReentrantReadWriteLock` across tree sections or root.
</details>

---

## 11. Practice Exercises

### Exercise 1: Count Words With a Given Prefix
Implement `int countWordsEqualTo(String word)` and `int countWordsStartingWith(String prefix)` in $O(L)$ time.

<details>
<summary>Solution</summary>

```java
public class CountingTrie {
    static class Node {
        Node[] next = new Node[26];
        int countPrefix = 0;
        int countExact = 0;
    }

    private final Node root = new Node();

    public void insert(String word) {
        Node cur = root;
        for (char c : word.toCharArray()) {
            int u = c - 'a';
            if (cur.next[u] == null) cur.next[u] = new Node();
            cur = cur.next[u];
            cur.countPrefix++;
        }
        cur.countExact++;
    }

    public int countWordsEqualTo(String word) {
        Node cur = root;
        for (char c : word.toCharArray()) {
            int u = c - 'a';
            if (cur.next[u] == null) return 0;
            cur = cur.next[u];
        }
        return cur.countExact;
    }

    public int countWordsStartingWith(String prefix) {
        Node cur = root;
        for (char c : prefix.toCharArray()) {
            int u = c - 'a';
            if (cur.next[u] == null) return 0;
            cur = cur.next[u];
        }
        return cur.countPrefix;
    }
}
```
</details>

### Exercise 2: Replace Words (LeetCode 648)
Given a dictionary of roots and a sentence, replace every word in the sentence with the shortest root that matches its prefix.

<details>
<summary>Solution</summary>

```java
public class ReplaceWords {
    static class Node {
        Node[] next = new Node[26];
        String word;
    }

    public String replaceWords(List<String> dictionary, String sentence) {
        Node root = new Node();
        for (String d : dictionary) {
            Node cur = root;
            for (char c : d.toCharArray()) {
                int u = c - 'a';
                if (cur.next[u] == null) cur.next[u] = new Node();
                cur = cur.next[u];
            }
            cur.word = d;
        }

        String[] tokens = sentence.split(" ");
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < tokens.length; i++) {
            if (i > 0) sb.append(' ');
            sb.append(findRoot(root, tokens[i]));
        }
        return sb.toString;
    }

    private String findRoot(Node root, String token) {
        Node cur = root;
        for (char c : token.toCharArray()) {
            if (cur.word != null) return cur.word; // Shortest root encountered
            int u = c - 'a';
            if (cur.next[u] == null) return token;
            cur = cur.next[u];
        }
        return cur.word != null ? cur.word : token;
    }
}
```
</details>

### Exercise 3: Palindrome Pairs (LeetCode 336)
Given a list of unique words, find all pairs `(i, j)` such that `words[i] + words[j]` forms a palindrome.

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class PalindromePairs {
    static class Node {
        int index = -1;
        Node[] next = new Node[26];
        List<Integer> palindromeBelow = new ArrayList<>();
    }

    public List<List<Integer>> palindromePairs(String[] words) {
        Node root = new Node();
        for (int i = 0; i < words.length; i++) {
            insertReversed(root, words[i], i);
        }

        List<List<Integer>> result = new ArrayList<>();
        for (int i = 0; i < words.length; i++) {
            searchPairs(root, words[i], i, result);
        }
        return result;
    }

    private void insertReversed(Node root, String s, int idx) {
        Node cur = root;
        for (int i = s.length() - 1; i >= 0; i--) {
            if (isPalindrome(s, 0, i)) cur.palindromeBelow.add(idx);
            int u = s.charAt(i) - 'a';
            if (cur.next[u] == null) cur.next[u] = new Node();
            cur = cur.next[u];
        }
        cur.index = idx;
        cur.palindromeBelow.add(idx);
    }

    private void searchPairs(Node root, String s, int idx, List<List<Integer>> res) {
        Node cur = root;
        for (int i = 0; i < s.length(); i++) {
            if (cur.index >= 0 && cur.index != idx && isPalindrome(s, i, s.length() - 1)) {
                res.add(List.of(idx, cur.index));
            }
            int u = s.charAt(i) - 'a';
            if (cur.next[u] == null) return;
            cur = cur.next[u];
        }
        for (int other : cur.palindromeBelow) {
            if (other != idx) {
                res.add(List.of(idx, other));
            }
        }
    }

    private boolean isPalindrome(String s, int l, int r) {
        while (l < r) {
            if (s.charAt(l++) != s.charAt(r--)) return false;
        }
        return true;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Core Strengths**: $O(L)$ exact and prefix search, independent of collection size $N$.
- **Alphabet Array**: `TrieNode[26]` offers maximum performance for lowercase English.
- **Bitwise Trie**: Standard technique for Maximum XOR problems ($O(32)$ constant time operations).
- **Pruning**: Always clear terminal flags and prune childless branches on removal.
- **Space Optimization**: Use Radix Tree or Double-Array Trie for large vocabularies.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Section 12 & Radix Trees](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Donald Knuth: The Art of Computer Programming, Vol 3: Sorting and Searching](https://www-cs-faculty.stanford.edu/~knuth/taocp.html)
- [Baeldung: Guide to the Trie Data Structure in Java](https://www.baeldung.com/trie-java)
- [LeetCode Trie Problems Curated List](https://leetcode.com/tag/trie/)
- [Open-Source Double-Array Trie (DAT) Implementation](https://github.com/hankcs/HanLP)
