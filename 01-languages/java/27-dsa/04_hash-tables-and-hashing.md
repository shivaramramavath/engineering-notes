# Hash Tables and Hashing Algorithmic Patterns in Java

> Hash table collision resolution mechanics, Robin Hood hashing, Consistent Hashing rings, Rolling Hash (Rabin-Karp), and HotSpot Java HashMap internals.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Advanced |
| **Prerequisites** | [`27-dsa/00_complexity-analysis.md`](00_complexity-analysis.md), [`08-collections/08_hashmap-and-hashset.md`](../08-collections/08_hashmap-and-hashset.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified (Java 8 to 25 LTS) |
| **Standard** | Java SE 21 / 25 LTS |

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

Hash tables provide average $O(1)$ constant-time insertion, deletion, and retrieval by mapping arbitrary keys to array indices using a deterministic mathematical hash function $h(\text{key})$.

In enterprise systems and algorithmic interviews, mastering hashing involves understanding:
- How collision resolution strategies (Separate Chaining, Linear Probing, Robin Hood Hashing) impact CPU cache performance.
- How Java's `HashMap` defends against HashDoS attacks via Red-Black Treeification.
- How **Rolling Hash (Rabin-Karp)** evaluates substring matches in $O(N)$ time.
- How **Consistent Hashing** distributes data evenly across dynamic distributed server nodes.

---

## 2. Core Concepts

### 2.1 Collision Resolution Strategies

1. **Separate Chaining (Java `HashMap`)**:
   - Each bucket references a linked list of entries sharing the same bucket index.
   - When bucket size exceeds 8 (and table capacity $\ge 64$), Java 8+ converts the list into a Red-Black Tree ($O(\log N)$ worst-case).

2. **Open Addressing (Linear & Quadratic Probing)**:
   - All elements are stored directly inside the main array table (no node pointers).
   - On collision at index $i$, probe next slot: $i + 1, i + 2, \dots$ (Linear) or $i + 1^2, i + 2^2, \dots$ (Quadratic).
   - Superior CPU cache locality, but suffers from primary clustering and degrades rapidly when load factor $> 0.7$.

3. **Robin Hood Hashing**:
   - A variant of open addressing where "rich" elements (few probe steps from ideal hash) yield their slots to "poor" elements (many probe steps).
   - Minimizes variance of probe lengths.

### 2.2 Rolling Hash (Rabin-Karp Algorithm)

Allows computing the hash of a sliding window substring of length $M$ in $O(1)$ time by subtracting the leading character and shifting in the trailing character:
$$H_{t+1} = \left((H_t - c_{\text{out}} \cdot B^{M-1}) \cdot B + c_{\text{in}}\right) \pmod P$$
Where $B$ is the base prime (e.g. 31 or 257) and $P$ is a large modulus (e.g. $10^9 + 7$).

### 2.3 Consistent Hashing with Virtual Nodes

In distributed caches (Redis, Memcached, DynamoDB), naive modular hashing $\text{hash}(\text{key}) \pmod N$ causes $100\%$ cache invalidation when servers are added or removed.
- **Consistent Hash Ring**: Hashes keys and server nodes onto a circular $2^{32}$ integer ring.
- A key is stored on the first server encountered clockwise along the ring.
- **Virtual Nodes**: Each physical server maps to 100+ virtual points on the ring, guaranteeing uniform load distribution.

---

## 3. How It Works Internally

### 3.1 HotSpot Java `HashMap` Bitwise Index Masking

Because computing modulo $h \% \text{length}$ requires expensive integer division, Java forces `HashMap` table capacity to be a power of two ($2^K$). Index computation uses bitwise AND:
$$\text{index} = (n - 1) \& \text{hash}$$

To prevent clustering when keys have poor lower-bit distributions, Java applies a high-bit spread function:
```java
static final int hash(Object key) {
    int h;
    return (key == null) ? 0 : (h = key.hashCode()) ^ (h >>> 16);
}
```

---

## 4. Syntax & Usage

### 4.1 Custom Key `equals()` and `hashCode()` Contract

```java
import java.util.Objects;

public final class CoordinateKey {
    private final int x;
    private final int y;

    public CoordinateKey(int x, int y) {
        this.x = x;
        this.y = y;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof CoordinateKey that)) return false;
        return x == that.x && y == that.y;
    }

    @Override
    public int hashCode() {
        return Objects.hash(x, y);
    }
}
```

---

## 5. Practical Examples

### 5.1 Pattern 1: Two Sum with HashMap (LeetCode 1)
```java
import java.util.HashMap;
import java.util.Map;

public class TwoSumHashing {
    public static int[] twoSum(int[] nums, int target) {
        Map<Integer, Integer> complementMap = new HashMap<>();

        for (int i = 0; i < nums.length; i++) {
            int complement = target - nums[i];
            if (complementMap.containsKey(complement)) {
                return new int[]{complementMap.get(complement), i};
            }
            complementMap.put(nums[i], i);
        }
        return new int[0];
    }
}
```

### 5.2 Pattern 2: Group Anagrams using Frequency Array Signatures (LeetCode 49)
```java
import java.util.*;

public class GroupAnagrams {
    public static List<List<String>> groupAnagrams(String[] strs) {
        Map<String, List<String>> groups = new HashMap<>();

        for (String s : strs) {
            // Count frequencies without sorting
            int[] counts = new int[26];
            for (char c : s.toCharArray()) {
                counts[c - 'a']++;
            }

            // Build deterministic signature: "#1#0#2..."
            StringBuilder sb = new StringBuilder();
            for (int count : counts) {
                sb.append('#').append(count);
            }
            String key = sb.toString();

            groups.computeIfAbsent(key, k -> new ArrayList<>()).add(s);
        }

        return new ArrayList<>(groups.values());
    }
}
```

### 5.3 Pattern 3: Longest Consecutive Sequence in $O(N)$ Time (LeetCode 128)
```java
import java.util.HashSet;
import java.util.Set;

public class LongestConsecutiveSequence {
    public static int longestConsecutive(int[] nums) {
        Set<Integer> numSet = new HashSet<>();
        for (int n : nums) numSet.add(n);

        int longestStreak = 0;

        for (int num : numSet) {
            // Only start counting if num is the beginning of a sequence
            if (!numSet.contains(num - 1)) {
                int currentNum = num;
                int currentStreak = 1;

                while (numSet.contains(currentNum + 1)) {
                    currentNum++;
                    currentStreak++;
                }

                longestStreak = Math.max(longestStreak, currentStreak);
            }
        }
        return longestStreak;
    }
}
```

### 5.4 Pattern 4: Rolling Hash — Rabin-Karp Substring Search (LeetCode 28)
```java
public class RabinKarpSearch {
    private static final long BASE = 256;
    private static final long MOD = 1_000_000_007L;

    public static int strStr(String haystack, String needle) {
        int m = needle.length();
        int n = haystack.length();
        if (m == 0) return 0;
        if (m > n) return -1;

        long needleHash = 0;
        long windowHash = 0;
        long highPower = 1;

        for (int i = 0; i < m - 1; i++) {
            highPower = (highPower * BASE) % MOD;
        }

        // Compute initial hashes
        for (int i = 0; i < m; i++) {
            needleHash = (needleHash * BASE + needle.charAt(i)) % MOD;
            windowHash = (windowHash * BASE + haystack.charAt(i)) % MOD;
        }

        for (int i = 0; i <= n - m; i++) {
            if (needleHash == windowHash) {
                // Confirm match to guard against hash collisions
                if (haystack.regionMatches(i, needle, 0, m)) {
                    return i;
                }
            }

            // Roll hash window
            if (i < n - m) {
                windowHash = (windowHash - haystack.charAt(i) * highPower) % MOD;
                if (windowHash < 0) windowHash += MOD;
                windowHash = (windowHash * BASE + haystack.charAt(i + m)) % MOD;
            }
        }

        return -1;
    }
}
```

### 5.5 Pattern 5: Consistent Hashing Ring with Virtual Nodes
```java
import java.security.MessageDigest;
import java.util.*;

public class ConsistentHashRouter<T> {
    private final int numberOfReplicas; // Virtual nodes per physical server
    private final SortedMap<Integer, T> circle = new TreeMap<>();

    public ConsistentHashRouter(int numberOfReplicas, Collection<T> nodes) {
        this.numberOfReplicas = numberOfReplicas;
        for (T node : nodes) {
            addNode(node);
        }
    }

    public synchronized void addNode(T node) {
        for (int i = 0; i < numberOfReplicas; i++) {
            int hash = hash(node.toString() + "-vn-" + i);
            circle.put(hash, node);
        }
    }

    public synchronized void removeNode(T node) {
        for (int i = 0; i < numberOfReplicas; i++) {
            int hash = hash(node.toString() + "-vn-" + i);
            circle.remove(hash);
        }
    }

    public T route(String key) {
        if (circle.isEmpty()) return null;
        int hash = hash(key);

        if (!circle.containsKey(hash)) {
            SortedMap<Integer, T> tailMap = circle.tailMap(hash);
            hash = tailMap.isEmpty() ? circle.firstKey() : tailMap.firstKey();
        }
        return circle.get(hash);
    }

    private int hash(String key) {
        try {
            MessageDigest md = MessageDigest.getInstance("MD5");
            byte[] digest = md.digest(key.getBytes());
            return ((digest[3] & 0xFF) << 24) | ((digest[2] & 0xFF) << 16)
                 | ((digest[1] & 0xFF) << 8)  | (digest[0] & 0xFF);
        } catch (Exception e) {
            return key.hashCode();
        }
    }
}
```

---

## 6. Best Practices

1. **Always Override `equals` and `hashCode` Together**: If two objects are equal according to `equals()`, their `hashCode()` values MUST be strictly identical.
2. **Use Immutable Keys**: If an object used as a `HashMap` key is mutated after insertion, its hash code changes and the entry becomes permanently unretrievable.
3. **Specify Initial Capacity for Large Maps**: `new HashMap<>((int) (expectedEntries / 0.75f) + 1)` prevents multiple rehash doubling passes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| **Mutating Key Fields** | Key's new hash code maps to different bucket; `get()` returns `null` | Use immutable Java `record` for map keys |
| **Overriding `equals` without `hashCode`**| Identical objects hash to different buckets, violating map set contract | Always generate both methods together |
| **Negative Hash Modulo** | `Math.abs(Integer.MIN_VALUE)` returns negative number in Java | Mask sign bit: `(h & 0x7FFFFFFF) % size` |
| **Forgetting to Verify Rabin-Karp Collisions**| Hash match does not guarantee string equality due to hash collisions | Call `regionMatches()` on hash match |

---

## 8. Performance & Security Notes

### 8.1 HashDoS Vulnerability
When web frameworks parse HTTP parameters into hash tables, attackers submit thousands of crafted keys that collide on the same hash bucket. Java 8's Red-Black treeification ensures that worst-case performance remains bounded at $O(\log N)$ rather than $O(N)$.

---

## 9. Comparison / When to Use What

| Data Structure | Lookup Time | Ordered? | Allows Null Keys? | Concurrency |
| :--- | :--- | :--- | :--- | :--- |
| **`HashMap`** | $O(1)$ avg / $O(\log N)$ | No | Yes (1 null key) | Non-thread-safe |
| **`LinkedHashMap`** | $O(1)$ | Insertion / LRU Order | Yes | Non-thread-safe |
| **`TreeMap`** | $O(\log N)$ | Natural / Comparator | No | Non-thread-safe |
| **`ConcurrentHashMap`**| $O(1)$ lock-free read | No | No nulls permitted | Thread-safe |

---

## 10. Interview Questions

<details>
<summary>1. What happens during HashMap treeification in Java 8+?</summary>
When the number of elements in a single bucket reaches the treeification threshold (`TREEIFY_THRESHOLD = 8`) and total table capacity is at least `MIN_TREEIFY_CAPACITY = 64`, the bucket's linked list is converted into a Red-Black balanced binary search tree (`TreeNode`). This improves worst-case lookup from $O(N)$ to $O(\log N)$.
</details>

<details>
<summary>2. Why must HashMap capacity always be a power of two?</summary>
A power-of-two capacity enables fast bitwise index calculation: `index = (capacity - 1) & hash`. This executes in a single CPU instruction, whereas arithmetic modulo (`hash % capacity`) requires hardware division, which takes 20–40 CPU cycles.
</details>

<details>
<summary>3. Why can `ConcurrentHashMap` not accept `null` keys or values?</summary>
In concurrent maps, returning `null` creates ambiguity: does `get(key) == null` mean the key is mapped to `null`, or does the key not exist? In single-threaded maps, `containsKey(key)` resolves this ambiguity. In concurrent maps, another thread can delete or insert the key between the two calls, creating a fatal race condition.
</details>

<details>
<summary>4. Explain how Rolling Hash works in the Rabin-Karp algorithm.</summary>
It calculates the hash of the next substring in $O(1)$ time by subtracting the contribution of the outgoing character, multiplying by the base, and adding the new incoming character modulo a large prime.
</details>

<details>
<summary>5. What is the role of Virtual Nodes in Consistent Hashing?</summary>
Without virtual nodes, a small number of physical nodes causes non-uniform partitioning along the hash ring (hot spots). Assigning multiple virtual nodes per physical machine evenly interleaves points around the ring, achieving balanced load distribution.
</details>

<details>
<summary>6. Why is `Objects.hash()` slower than writing a manual hash function?</summary>
`Objects.hash(Object... values)` allocates an array for the varargs parameter and autoboxes primitive arguments into wrapper objects, generating unnecessary heap garbage during high-throughput hash lookups.
</details>

<details>
<summary>7. What is Robin Hood Hashing?</summary>
An open-addressing collision resolution algorithm that minimizes search variance by stealing slots from elements that required fewer probes from their ideal index to give to elements that required more probes.
</details>

<details>
<summary>8. How do you safely compute `Math.abs(key.hashCode()) % size` in Java?</summary>
If `key.hashCode() == Integer.MIN_VALUE`, `Math.abs()` returns `Integer.MIN_VALUE` (negative) due to 32-bit two's complement overflow. The correct idiom is `(key.hashCode() & 0x7FFFFFFF) % size`.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Find All Anagrams in a String (LeetCode 438)</summary>

```java
import java.util.*;

public class FindAnagrams {
    public static List<Integer> findAnagrams(String s, String p) {
        List<Integer> result = new ArrayList<>();
        if (s.length() < p.length()) return result;

        int[] pCount = new int[26];
        int[] sCount = new int[26];

        for (int i = 0; i < p.length(); i++) {
            pCount[p.charAt(i) - 'a']++;
            sCount[s.charAt(i) - 'a']++;
        }

        if (Arrays.equals(pCount, sCount)) result.add(0);

        for (int i = p.length(); i < s.length(); i++) {
            sCount[s.charAt(i) - 'a']++;
            sCount[s.charAt(i - p.length()) - 'a']--;

            if (Arrays.equals(pCount, sCount)) {
                result.add(i - p.length() + 1);
            }
        }
        return result;
    }
}
```
</details>

<details>
<summary>Exercise 2: Isomorphic Strings (LeetCode 205)</summary>

```java
public class IsomorphicStrings {
    public static boolean isIsomorphic(String s, String t) {
        int[] m1 = new int[256];
        int[] m2 = new int[256];
        int len = s.length();

        for (int i = 0; i < len; i++) {
            char c1 = s.charAt(i);
            char c2 = t.charAt(i);
            if (m1[c1] != m2[c2]) return false;
            m1[c1] = i + 1;
            m2[c2] = i + 1;
        }
        return true;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Bitwise Masking**: `(capacity - 1) & hash` avoids integer division in power-of-two tables.
- **Treeification**: Converts buckets $\ge 8$ to Red-Black Trees ($O(\log N)$) to defend against HashDoS.
- **Contract**: Equal objects MUST produce equal hashCodes; keys must be immutable.
- **Rabin-Karp**: Rolling hash slides window in $O(1)$ per step.
- **Consistent Hashing**: Distributes keys across nodes with minimal reshuffling on node changes.

---

## 13. Further Reading

- [OpenJDK HashMap Source Code](https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/util/HashMap.java)
- [Tom White: Consistent Hashing](https://tom-e-white.com/2007/11/consistent-hashing.html)
"""

write_dsa_note("04_hash-tables-and-hashing.md", c04)
