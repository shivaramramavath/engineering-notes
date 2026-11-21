# String Algorithms

> Advanced string processing: Knuth-Morris-Pratt (KMP), Rabin-Karp rolling hashing, Z-Algorithm, and Manacher's linear-time palindrome finder.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [01_arrays-and-strings.md](01_arrays-and-strings.md), [21_math-and-number-theory.md](21_math-and-number-theory.md) |
| **Time to Read** | 30 minutes |
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

Given a text string of length $N$ and a pattern string of length $M$, finding occurrences of the pattern using naive scanning requires $O(N \times M)$ worst-case comparisons (e.g., searching for `"aaaaab"` inside `"aaaaaaaaaa"`).

Advanced string algorithms exploit internal symmetries and mathematical properties to achieve strictly **linear time $O(N + M)$**:
- **Knuth-Morris-Pratt (KMP)**: Never backtracks in the text. Uses a failure function (LPS table) to shift the pattern forward intelligently.
- **Rabin-Karp**: Computes polynomial rolling hashes, comparing hash values in $O(1)$ and checking full text only on hash matches.
- **Z-Algorithm**: Precomputes the length of the longest substring starting at each index that matches the prefix of the string.
- **Manacher's Algorithm**: Discovers the longest palindromic substring across an entire string in strictly $O(N)$ time.

---

## 2. Core Concepts

### 1. KMP Longest Proper Prefix which is also Suffix (LPS)
The `lps[i]` value stores the length of the longest proper prefix of pattern $P[0 \dots i]$ that is also a suffix of $P[0 \dots i]$.
When a character mismatch occurs at index $j$ of the pattern:
- Instead of rewinding text pointer $i$, keep $i$ stationary.
- Shift pattern pointer $j$ backwards to `lps[j - 1]`.
- Resume comparisons immediately.

### 2. Rabin-Karp Rolling Hash
Represent a string window $S[i \dots i+L-1]$ as a polynomial base-$B$ number modulo prime $M$:
$$H(S) = \left( \sum_{k=0}^{L-1} S[i + k] \cdot B^{L - 1 - k} \right) \pmod M$$
Rolling to the next window takes $O(1)$:
$$H_{new} = \left( (H_{old} - S[i] \cdot B^{L-1}) \times B + S[i + L] \right) \pmod M$$

### 3. Manacher's Algorithm
Standard palindrome expansion around centers takes $O(N^2)$. Manacher's algorithm inserts `#` delimiters (e.g. `"aba"` $\to$ `"^#a#b#a#$"`) and maintains the rightmost palindrome boundary $[C - R, C + R]$. Palindrome radii for new centers inside the boundary are mirrored in $O(1)$ from earlier symmetric centers, running in strictly $O(N)$ time.

---

## 3. How It Works Internally

### KMP Failure Function Construction Trace
Pattern: `"a b a b a c"`
```
Index:    0   1   2   3   4   5
Pattern:  a   b   a   b   a   c
LPS:      0   0   1   2   3   0

Explanation:
- i=0: "a"       -> 0
- i=1: "ab"      -> 0
- i=2: "aba"     -> 1 ("a")
- i=3: "abab"    -> 2 ("ab")
- i=4: "ababa"   -> 3 ("aba")
- i=5: "ababac"  -> 0 (c breaks match with b)
```

```
HotSpot JVM String Intrinsic:
String.indexOf(String) does not use KMP. It uses hardware-accelerated SIMD
vector instructions (AVX-512 / SSE4.2 string comparison instructions: PCMPESTRI).
For short strings (< 64 chars), JVM vector intrinsics beat KMP by a factor of 4x!
KMP is superior when custom pattern streaming or character-by-character automata are required.
```

---

## 4. Syntax & Usage

### Production KMP Implementation

```java
import java.util.ArrayList;
import java.util.List;

public class KMPAlgorithm {
    public static int[] buildLPS(String pattern) {
        int m = pattern.length();
        int[] lps = new int[m];
        int len = 0; // Length of previous longest prefix suffix
        int i = 1;

        while (i < m) {
            if (pattern.charAt(i) == pattern.charAt(len)) {
                len++;
                lps[i] = len;
                i++;
            } else {
                if (len != 0) {
                    len = lps[len - 1]; // Fallback to smaller prefix
                } else {
                    lps[i] = 0;
                    i++;
                }
            }
        }

        return lps;
    }

    public static List<Integer> search(String text, String pattern) {
        List<Integer> occurrences = new ArrayList<>();
        if (pattern == null || pattern.isEmpty()) return occurrences;

        int[] lps = buildLPS(pattern);
        int n = text.length(), m = pattern.length();
        int i = 0; // Index for text
        int j = 0; // Index for pattern

        while (i < n) {
            if (text.charAt(i) == pattern.charAt(j)) {
                i++;
                j++;
            }

            if (j == m) {
                occurrences.add(i - j); // Match found at index (i - j)
                j = lps[j - 1];         // Look for overlapping matches
            } else if (i < n && text.charAt(i) != pattern.charAt(j)) {
                if (j != 0) {
                    j = lps[j - 1];
                } else {
                    i++;
                }
            }
        }

        return occurrences;
    }
}
```

---

## 5. Practical Examples

### Example 1: Implement strStr() / First Occurrence (LeetCode 28, KMP)

```java
public class StrStrKMP {
    public int strStr(String haystack, String needle) {
        if (needle.isEmpty()) return 0;
        int[] lps = buildLPS(needle);

        int i = 0, j = 0;
        while (i < haystack.length()) {
            if (haystack.charAt(i) == needle.charAt(j)) {
                i++;
                j++;
            }

            if (j == needle.length()) {
                return i - j;
            } else if (i < haystack.length() && haystack.charAt(i) != needle.charAt(j)) {
                if (j != 0) {
                    j = lps[j - 1];
                } else {
                    i++;
                }
            }
        }

        return -1;
    }

    private int[] buildLPS(String pat) {
        int[] lps = new int[pat.length()];
        int len = 0, i = 1;
        while (i < pat.length()) {
            if (pat.charAt(i) == pat.charAt(len)) {
                lps[i++] = ++len;
            } else if (len > 0) {
                len = lps[len - 1];
            } else {
                lps[i++] = 0;
            }
        }
        return lps;
    }
}
```

### Example 2: Repeated Substring Pattern (LeetCode 459, LPS Analysis)

```java
public class RepeatedSubstringPattern {
    public boolean repeatedSubstringPattern(String s) {
        int n = s.length();
        int[] lps = new int[n];
        int len = 0, i = 1;

        while (i < n) {
            if (s.charAt(i) == s.charAt(len)) {
                lps[i++] = ++len;
            } else if (len > 0) {
                len = lps[len - 1];
            } else {
                lps[i++] = 0;
            }
        }

        int longestBorder = lps[n - 1];
        // If s has a repeating unit, the remaining length (n - longestBorder) divides n evenly
        return longestBorder > 0 && (n % (n - longestBorder) == 0);
    }
}
```

### Example 3: Longest Palindromic Substring (LeetCode 5, Manacher's Algorithm $O(N)$)

```java
public class ManacherPalindrome {
    public String longestPalindrome(String s) {
        if (s == null || s.length() <= 1) return s;

        // Transform "aba" -> "^#a#b#a#$"
        StringBuilder sb = new StringBuilder("^");
        for (char c : s.toCharArray()) sb.append('#').append(c);
        sb.append("#$");
        char[] t = sb.toString().toCharArray();

        int[] p = new int[t.length];
        int center = 0, right = 0;
        int maxLen = 0, centerIndex = 0;

        for (int i = 1; i < t.length - 1; i++) {
            int mirror = 2 * center - i;

            if (right > i) {
                p[i] = Math.min(right - i, p[mirror]);
            }

            // Expand around center i
            while (t[i + 1 + p[i]] == t[i - 1 - p[i]]) {
                p[i]++;
            }

            // Update center and right boundary if palindrome extends past right
            if (i + p[i] > right) {
                center = i;
                right = i + p[i];
            }

            if (p[i] > maxLen) {
                maxLen = p[i];
                centerIndex = i;
            }
        }

        int start = (centerIndex - maxLen) / 2;
        return s.substring(start, start + maxLen);
    }
}
```

### Example 4: Shortest Palindrome (LeetCode 214, KMP Prefix-Suffix)

```java
public class ShortestPalindrome {
    public String shortestPalindrome(String s) {
        String rev = new StringBuilder(s).reverse().toString();
        // Combined string with sentinel '#'
        String combined = s + "#" + rev;

        // Compute LPS of combined string
        int[] lps = new int[combined.length()];
        int len = 0, i = 1;
        while (i < combined.length()) {
            if (combined.charAt(i) == combined.charAt(len)) {
                lps[i++] = ++len;
            } else if (len > 0) {
                len = lps[len - 1];
            } else {
                lps[i++] = 0;
            }
        }

        // lps[combined.length() - 1] gives the longest palindrome prefix of s
        int matched = lps[combined.length() - 1];
        String suffixToPrepend = rev.substring(0, s.length() - matched);

        return suffixToPrepend + s;
    }
}
```

### Example 5: Rabin-Karp Rolling Hash Substring Search

```java
public class RabinKarpSearch {
    private static final long MOD = 1_000_000_007L;
    private static final long BASE = 256L;

    public static int search(String text, String pattern) {
        int n = text.length(), m = pattern.length();
        if (m > n) return -1;

        long patHash = 0;
        long textHash = 0;
        long h = 1; // BASE^(m-1) % MOD

        for (int i = 0; i < m - 1; i++) {
            h = (h * BASE) % MOD;
        }

        for (int i = 0; i < m; i++) {
            patHash = (patHash * BASE + pattern.charAt(i)) % MOD;
            textHash = (textHash * BASE + text.charAt(i)) % MOD;
        }

        for (int i = 0; i <= n - m; i++) {
            if (patHash == textHash) {
                // Verify against hash collisions
                if (text.startsWith(pattern, i)) {
                    return i;
                }
            }

            if (i < n - m) {
                textHash = (BASE * (textHash - text.charAt(i) * h) + text.charAt(i + m)) % MOD;
                if (textHash < 0) textHash += MOD; // Keep positive
            }
        }

        return -1;
    }
}
```

---

## 6. Best Practices

1. **Double Hashing in Production Rabin-Karp**: Use two distinct prime moduli (e.g. $10^9 + 7$ and $10^9 + 9$) to render probability of false-positive hash collisions practically zero ($10^{-18}$).
2. **Sentinel Characters in String Algorithms**: When combining strings (e.g., $S + \# + \text{rev}(S)$), always use a delimiter like `'#'` that cannot appear in the input alphabet.
3. **Handle Negative Modulo in Rolling Hash**: Subtracting the outgoing character can make the hash negative: `if (hash < 0) hash += MOD;`.
4. **Use Z-Algorithm for Prefix Queries**: The Z-Algorithm is simpler to implement than KMP while delivering identical $O(N + M)$ performance.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Fallback bug in LPS build: `len = 0` on mismatch | Misses intermediate smaller proper prefix matches | Fallback iteratively: `len = lps[len - 1];`. |
| Rolling hash without verification | Hash collision returns false positive index | Verify characters with `equals()` or `startsWith()` upon hash match. |
| Negative hash values after subtraction | Java `%` preserves negative signs | Add `+ MOD` whenever hash difference is negative. |
| Missing boundary guards in Manacher's | Array out of bounds during expansion | Pad transformed string with distinct start `'^'` and end `'$'` sentinels. |

---

## 8. Performance & Security Notes

- **Time Complexity**:
  - KMP: $O(N + M)$ guaranteed worst-case.
  - Rabin-Karp: $O(N + M)$ average; $O(N \times M)$ worst-case under deliberate hash collisions.
  - Manacher's: Strictly $O(N)$.
  - Z-Algorithm: Strictly $O(N + M)$.
- **Hash DoS Vulnerability**: If an external adversary knows your hash base and modulus, they can generate strings with identical hash values, causing Rabin-Karp to degrade to $O(N \times M)$ denial-of-service. Use randomized hash bases or KMP.

---

## 9. Comparison / When to Use What

| Algorithm | Preprocessing Time | Search Time | Memory Overhead | Strengths |
|---|---|---|---|---|
| **KMP** | $O(M)$ | $O(N)$ | $O(M)$ | Zero text backtracking, streaming data |
| **Rabin-Karp** | $O(M)$ | $O(N)$ avg | $O(1)$ | Multi-pattern search, 2D matrix matching |
| **Z-Algorithm** | $O(N + M)$ | $O(N + M)$ | $O(N + M)$ | Pattern matching, prefix palindromes |
| **Manacher's** | $O(N)$ | $O(N)$ | $O(N)$ | All palindromic substrings in $O(N)$ |

---

## 10. Interview Questions

<details>
<summary>1. Why does KMP never need to backtrack the pointer in the main text?</summary>

Because the LPS (failure) table encodes all information about recurring prefixes in the pattern. When a mismatch occurs, the LPS table reveals the longest prefix that matches the text suffix immediately preceding the mismatch. The pattern can be shifted forward to align this prefix without rewinding the text pointer.
</details>

<details>
<summary>2. How does Manacher's algorithm achieve strictly $O(N)$ time complexity?</summary>

The rightmost boundary $R$ of the current palindrome only moves forward, from 0 to $2N + 1$. Whenever the character comparison loop executes, it either increments $R$ or terminates immediately. Because $R$ advances at most $2N$ times, the total number of character comparisons across the entire algorithm is bounded by $2N = O(N)$.
</details>

<details>
<summary>3. Why do we insert `#` sentinels in Manacher's algorithm?</summary>

Inserting `#` between every character transforms all palindromes (both odd-length and even-length) into odd-length palindromes centered at a specific character or delimiter. For example, even palindrome `"abba"` becomes `#a#b#b#a#` centered at the middle `'#'`.
</details>

<details>
<summary>4. What is the Z-Array and how is it defined?</summary>

For a string $S$, $Z[i]$ is the length of the longest substring starting from $S[i]$ that is also a prefix of $S$. For string `"aabzaa"`, $Z = [0, 1, 0, 0, 2, 1]$. To search pattern $P$ in text $T$, compute the Z-array of $P + \# + T$. Any index with $Z[i] == |P|$ represents an occurrence.
</details>

<details>
<summary>5. How can Rabin-Karp search for multiple patterns simultaneously?</summary>

Compute the hash of all $K$ patterns (assuming identical length $M$) and store them in a `HashSet<Long>`. Slide a window of length $M$ over the text, computing its rolling hash in $O(1)$, and check if `set.contains(windowHash)`. Total search time is $O(N + K \times M)$.
</details>

<details>
<summary>6. How does KMP determine if a string is made of repeated substrings?</summary>

Compute the LPS array of string $S$ of length $N$. Let $L = lps[N - 1]$. If $L > 0$ and $N \pmod{N - L} == 0$, the string is formed by repeating the prefix of length $N - L$ exactly $N / (N - L)$ times.
</details>

<details>
<summary>7. What is the Aho-Corasick Algorithm?</summary>

Aho-Corasick combines a Trie with KMP failure links to search for multiple dictionary patterns in a text simultaneously in $O(N + \sum M_i + \text{matches})$ time. It powers `fgrep`, intrusion detection systems (Snort), and DNA search engines.
</details>

<details>
<summary>8. How do you find the Longest Common Prefix (LCP) of two substrings in $O(1)$ time?</summary>

Construct a Suffix Array and an LCP Array. The LCP of two suffixes is the minimum value in the LCP array between their suffix ranks. By precomputing a Range Minimum Query (RMQ / Sparse Table) data structure in $O(N \log N)$, any LCP query can be answered in $O(1)$ time.
</details>

---

## 11. Practice Exercises

### Exercise 1: Longest Happy Prefix (LeetCode 1392)
A happy prefix is a prefix that is also a suffix (excluding the full string). Find the longest happy prefix.

<details>
<summary>Solution</summary>

```java
public class LongestHappyPrefix {
    public String longestPrefix(String s) {
        int n = s.length();
        int[] lps = new int[n];
        int len = 0, i = 1;

        while (i < n) {
            if (s.charAt(i) == s.charAt(len)) {
                lps[i++] = ++len;
            } else if (len > 0) {
                len = lps[len - 1];
            } else {
                lps[i++] = 0;
            }
        }

        return s.substring(0, lps[n - 1]);
    }
}
```
</details>

### Exercise 2: Palindrome Partitioning II (LeetCode 132, Min Cuts)

<details>
<summary>Solution</summary>

```java
public class PalindromePartitioningII {
    public int minCut(String s) {
        int n = s.length();
        boolean[][] isPal = new boolean[n][n];
        int[] cuts = new int[n];

        for (int i = 0; i < n; i++) {
            int minCuts = i; // Max cuts is i (all single letters)
            for (int j = 0; j <= i; j++) {
                if (s.charAt(i) == s.charAt(j) && (i - j <= 2 || isPal[j + 1][i - 1])) {
                    isPal[j][i] = true;
                    minCuts = (j == 0) ? 0 : Math.min(minCuts, cuts[j - 1] + 1);
                }
            }
            cuts[i] = minCuts;
        }

        return cuts[n - 1];
    }
}
```
</details>

### Exercise 3: Text Justification (LeetCode 68, Greedy String Formatting)

<details>
<summary>Solution</summary>

```java
import java.util.*;

public class TextJustification {
    public List<String> fullJustify(String[] words, int maxWidth) {
        List<String> result = new ArrayList<>();
        int i = 0, n = words.length;

        while (i < n) {
            int j = i + 1;
            int lineLen = words[i].length();
            while (j < n && lineLen + 1 + words[j].length() <= maxWidth) {
                lineLen += 1 + words[j].length();
                j++;
            }

            StringBuilder sb = new StringBuilder();
            int diff = j - i - 1;

            // If last line or single word in line, left-justify
            if (j == n || diff == 0) {
                for (int k = i; k < j; k++) {
                    sb.append(words[k]);
                    if (k < j - 1) sb.append(" ");
                }
                while (sb.length() < maxWidth) sb.append(" ");
            } else {
                // Fully justify
                int spaces = (maxWidth - (lineLen - diff)) / diff;
                int extra = (maxWidth - (lineLen - diff)) % diff;

                for (int k = i; k < j; k++) {
                    sb.append(words[k]);
                    if (k < j - 1) {
                        int numSpaces = spaces + (k - i < extra ? 1 : 0);
                        sb.append(" ".repeat(numSpaces));
                    }
                }
            }

            result.add(sb.toString());
            i = j;
        }

        return result;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **KMP**: $O(N + M)$ single-pass text search; LPS table guides pattern shift without text rewinding.
- **Rabin-Karp**: Rolling hash $H_{new} = (H_{old} - S[i] \cdot B^{L-1}) \cdot B + S[i+L]$.
- **Manacher's**: Expands `#`-padded string; mirrors palindrome radii in $O(N)$ total time.
- **Repeated Substring**: $lps[n-1] > 0$ and $n \pmod{n - lps[n-1]} == 0$.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 32: String Matching](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Knuth, Morris, Pratt (1977). Fast pattern matching in strings](https://epubs.siam.org/doi/10.1137/0206024)
- [Baeldung: Guide to the Knuth-Morris-Pratt Algorithm in Java](https://www.baeldung.com/cs/kmp-algorithm)
- [LeetCode String Matching Tag](https://leetcode.com/tag/string-matching/)
