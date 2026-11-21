# Math and Number Theory

> Foundational algorithms for prime factorization, modular arithmetic, fast exponentiation, Euclidean GCD, and combinatorial calculations.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01_arrays-and-strings.md](01_arrays-and-strings.md) |
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

Mathematical algorithms provide high-efficiency solutions to problems in cryptography, hashing, geometric computing, and competitive programming.
- **Greatest Common Divisor (GCD)**: Computes the largest positive integer that divides two numbers without a remainder in $O(\log(\min(A, B)))$ time.
- **Prime Generation & Testing**: Sieve of Eratosthenes identifies all primes up to $N$ in $O(N \log \log N)$ time.
- **Fast Modular Exponentiation**: Computes $(X^N) \pmod M$ in $O(\log N)$ time, fundamental to RSA and modern asymmetric cryptography.
- **Combinatorics**: Evaluates combinations $\binom{n}{k} \pmod M$ via modular multiplicative inverses.

---

## 2. Core Concepts

### 1. Euclidean Algorithm for GCD
Based on the principle that the greatest common divisor of two integers does not change if the larger number is replaced by its difference with the smaller number:
$$\gcd(a, b) = \gcd(b, a \pmod b)$$
Base case: $\gcd(a, 0) = a$.

Least Common Multiple (LCM):
$$\text{lcm}(a, b) = \frac{|a \times b|}{\gcd(a, b)}$$

### 2. Sieve of Eratosthenes
To find all prime numbers up to $N$:
1. Create a boolean array `isPrime` of size $N + 1$, initialized to `true`.
2. Set `isPrime[0] = isPrime[1] = false`.
3. Loop $p$ from $2$ to $\sqrt{N}$. If `isPrime[p]` is true, mark all multiples of $p$ starting from $p^2$ as `false`: $p^2, p^2 + p, p^2 + 2p, \dots \le N$.
Total time complexity is $O(N \log \log N)$, virtually indistinguishable from linear time.

### 3. Binary Exponentiation ($O(\log N)$)
Instead of multiplying $X$ by itself $N$ times ($O(N)$), express $N$ in binary:
$$X^N = \begin{cases} 1 & \text{if } N = 0 \\ (X^{N/2})^2 & \text{if } N \text{ is even} \\ X \times (X^{(N-1)/2})^2 & \text{if } N \text{ is odd} \end{cases}$$

### 4. Modular Multiplicative Inverse (Fermat's Little Theorem)
If $M$ is a prime number, for any integer $a$ not divisible by $M$:
$$a^{M - 1} \equiv 1 \pmod M \implies a \times a^{M - 2} \equiv 1 \pmod M$$
Therefore, the modular inverse of $a$ modulo prime $M$ is:
$$a^{-1} \equiv a^{M - 2} \pmod M$$

---

## 3. How It Works Internally

### Preventing Arithmetic Overflow in Modular Multiplication
When multiplying two 32-bit integers under modulo $10^9 + 7$:
```java
int a = 1_000_000_000;
int b = 1_000_000_000;
int mod = 1_000_000_007;

// BUG: 32-bit integer overflow before modulo is applied!
int bug = (a * b) % mod; // a * b = 10^18 overflows 32-bit signed int (-1486618624)

// FIX: Cast operand to 64-bit primitive long!
int fix = (int) (((long) a * b) % mod); // Correct!
```

---

## 4. Syntax & Usage

### Core Number Theory Utilities

```java
public class MathUtils {
    // Euclidean GCD in O(log(min(a, b)))
    public static long gcd(long a, long b) {
        while (b != 0) {
            long temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

    public static long lcm(long a, long b) {
        return (a / gcd(a, b)) * b; // Divide first to prevent overflow!
    }

    // Binary Modular Exponentiation: (base^exp) % mod
    public static long powerMod(long base, long exp, long mod) {
        long result = 1;
        base %= mod;

        while (exp > 0) {
            if ((exp & 1) == 1) {
                result = (result * base) % mod;
            }
            base = (base * base) % mod;
            exp >>= 1;
        }

        return result;
    }

    // Modular Multiplicative Inverse for Prime Modulo
    public static long modInverse(long a, long primeMod) {
        return powerMod(a, primeMod - 2, primeMod);
    }
}
```

---

## 5. Practical Examples

### Example 1: Count Primes (LeetCode 204, Sieve of Eratosthenes)

```java
public class CountPrimes {
    public int countPrimes(int n) {
        if (n <= 2) return 0;
        boolean[] composite = new boolean[n];
        int count = 0;

        for (int p = 2; (long) p * p < n; p++) {
            if (!composite[p]) {
                // Start marking at p * p because smaller multiples already marked
                for (int multiple = p * p; multiple < n; multiple += p) {
                    composite[multiple] = true;
                }
            }
        }

        for (int i = 2; i < n; i++) {
            if (!composite[i]) count++;
        }

        return count;
    }
}
```

### Example 2: Pow(x, n) (LeetCode 50, Handling Negative Exponents)

```java
public class PowerFunction {
    public double myPow(double x, int n) {
        long N = n; // Cast to long to handle Integer.MIN_VALUE (-2147483648)
        if (N < 0) {
            x = 1.0 / x;
            N = -N;
        }

        double result = 1.0;
        double currentProduct = x;

        while (N > 0) {
            if ((N & 1) == 1) {
                result *= currentProduct;
            }
            currentProduct *= currentProduct;
            N >>= 1;
        }

        return result;
    }
}
```

### Example 3: Sqrt(x) (LeetCode 69, Binary Search / Newton-Raphson)

```java
public class SquareRoot {
    public int mySqrt(int x) {
        if (x <= 1) return x;
        int low = 1, high = x / 2;
        int ans = 1;

        while (low <= high) {
            int mid = low + (high - low) / 2;
            // Prevent mid * mid overflow by dividing
            if (mid <= x / mid) {
                ans = mid;
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }

        return ans;
    }
}
```

### Example 4: Fraction to Recurring Decimal (LeetCode 166)

```java
import java.util.*;

public class FractionToDecimal {
    public String fractionToDecimal(int numerator, int denominator) {
        if (numerator == 0) return "0";

        StringBuilder sb = new StringBuilder();
        // Handle signs
        if ((numerator < 0) ^ (denominator < 0)) sb.append("-");

        long num = Math.abs((long) numerator);
        long den = Math.abs((long) denominator);

        // Integral part
        sb.append(num / den);
        long rem = num % den;
        if (rem == 0) return sb.toString();

        sb.append(".");
        Map<Long, Integer> seenRemainders = new HashMap<>();

        while (rem != 0) {
            if (seenRemainders.containsKey(rem)) {
                sb.insert(seenRemainders.get(rem), "(");
                sb.append(")");
                break;
            }

            seenRemainders.put(rem, sb.length());
            rem *= 10;
            sb.append(rem / den);
            rem %= den;
        }

        return sb.toString();
    }
}
```

### Example 5: Super Pow (LeetCode 372, Array Exponent with Euler's Modulo)

```java
public class SuperPow {
    private static final int MOD = 1337;

    public int superPow(int a, int[] b) {
        return superPowHelper(a, b, b.length - 1);
    }

    private int superPowHelper(int a, int[] b, int lastIdx) {
        if (lastIdx < 0) return 1;
        int lastDigit = b[lastIdx];

        int part1 = powMod(a, lastDigit);
        int part2 = powMod(superPowHelper(a, b, lastIdx - 1), 10);

        return (part1 * part2) % MOD;
    }

    private int powMod(int base, int exp) {
        base %= MOD;
        int res = 1;
        while (exp > 0) {
            if ((exp & 1) == 1) res = (res * base) % MOD;
            base = (base * base) % MOD;
            exp >>= 1;
        }
        return res;
    }
}
```

---

## 6. Best Practices

1. **Divide Before Multiplying in LCM**: Write `(a / gcd(a, b)) * b` rather than `(a * b) / gcd(a, b)` to prevent intermediate 64-bit integer overflow.
2. **Promote to Long for Integer Modulo**: Always write `(int) (((long) a * b) % MOD)` in combinatorics and DP loops.
3. **Handle `Integer.MIN_VALUE` Negation**: In Java, `-Integer.MIN_VALUE == Integer.MIN_VALUE` due to two's complement asymmetry. Promote to `long` before negation: `long N = -(long) n`.
4. **Use Sieve for Bulk Queries**: If querying primality for multiple queries up to $10^7$, build the Sieve of Eratosthenes once in $O(N \log \log N)$ rather than running trial division $O(\sqrt{N})$ per query.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| `(a * b) % MOD` without casting | 32-bit integer overflows before modulus is computed | Cast to `long`: `(int) (((long) a * b) % MOD)`. |
| Negating `Integer.MIN_VALUE` | `-(-2147483648)` overflows back to `-2147483648` | Cast to `long` first: `long n = -(long) val;`. |
| Negative modulo in Java | Java's `%` is remainder, so `-5 % 3 == -2` | Write: `(x % mod + mod) % mod`. |
| Sieve inner loop starting at $2p$ | Redundant work already handled by smaller prime factors | Start inner loop at $p \times p$. |

---

## 8. Performance & Security Notes

- **Time Complexity**:
  - Euclidean GCD: $O(\log(\min(A, B)))$
  - Sieve of Eratosthenes: $O(N \log \log N)$
  - Binary Exponentiation: $O(\log N)$
- **Security in Cryptography**: Standard Java `java.util.Random` is a linear congruential generator (LCG) and is completely insecure for cryptographic key generation or prime generation. Always use `java.security.SecureRandom`.

---

## 9. Comparison / When to Use What

| Algorithm | Purpose | Time Complexity | Space Complexity |
|---|---|---|---|
| **Trial Division** | Single prime test | $O(\sqrt{N})$ | $O(1)$ |
| **Sieve of Eratosthenes** | All primes up to $N$ | $O(N \log \log N)$ | $O(N)$ bits |
| **Segmented Sieve** | Primes in range $[L, R]$ ($R - L \le 10^6$) | $O((R - L) \log \log R)$ | $O(R - L)$ |
| **Euclidean Algorithm** | GCD of two numbers | $O(\log(\min(A, B)))$ | $O(1)$ |

---

## 10. Interview Questions

<details>
<summary>1. Why does the Sieve of Eratosthenes start marking multiples from $p^2$ instead of $2p$?</summary>

Because any composite multiple of $p$ smaller than $p^2$ can be written as $k \times p$ where $k < p$. By prime factorization, $k$ must have at least one prime factor $q < p$. Therefore, the multiple $k \times p$ has already been marked as composite when processing the smaller prime $q$.
</details>

<details>
<summary>2. How does the Euclidean algorithm achieve $O(\log(\min(A, B)))$ time complexity?</summary>

In every two iterations of the Euclidean algorithm, the larger number is reduced by at least a factor of 2:
$$a \pmod b < \frac{a}{2}$$
Therefore, the number of steps is bounded by $2 \log_2(\min(A, B))$. Lamé's Theorem states that the worst-case occurs when $A$ and $B$ are consecutive Fibonacci numbers.
</details>

<details>
<summary>3. What is the difference between `%` in Java and the mathematical modulo operator?</summary>

In Java, `%` is the remainder operator, which preserves the sign of the dividend (e.g. `-7 % 3 = -1`). In mathematics, modulo is always non-negative ($0 \le r < m$). To compute true mathematical modulo in Java:
`int mod = (a % m + m) % m;`
or use `Math.floorMod(a, m)`.
</details>

<details>
<summary>4. What is Miller-Rabin Primality Test?</summary>

A probabilistic primality test based on Fermat's Little Theorem and non-trivial square roots of 1 modulo $p$. It determines whether a large integer (up to $10^{18}$ or $2048$ bits) is prime in $O(k \log^3 N)$ time. Using the first 12 prime bases guarantees 100% deterministic correctness for all 64-bit integers.
</details>

<details>
<summary>5. How do you compute combinations $\binom{N}{K} \pmod M$ when $M$ is prime?</summary>

Precompute factorials `fact[i] = (fact[i-1] * i) % M` and their modular inverse `invFact[i] = modInverse(fact[i], M)` in $O(N)$ time. Then:
$$\binom{N}{K} \equiv \text{fact}[N] \times \text{invFact}[K] \times \text{invFact}[N - K] \pmod M$$
Each query can then be answered in $O(1)$ time.
</details>

<details>
<summary>6. How do you find the number of trailing zeroes in $N!$ (Factorial)?</summary>

Trailing zeroes are produced by factors of 10 ($2 \times 5$). Because factors of 2 are always more abundant than factors of 5, the number of trailing zeroes equals Legendre's formula for the multiplicity of 5:
$$\sum_{k=1}^{\infty} \left\lfloor \frac{N}{5^k} \right\rfloor = \left\lfloor \frac{N}{5} \right\rfloor + \left\lfloor \frac{N}{25} \right\rfloor + \left\lfloor \frac{N}{125} \right\rfloor + \dots$$
Runs in $O(\log_5 N)$ time.
</details>

<details>
<summary>7. What is the Chinese Remainder Theorem (CRT)?</summary>

CRT states that if $m_1, m_2, \dots, m_k$ are pairwise coprime positive integers, then the system of simultaneous congruences $x \equiv a_i \pmod{m_i}$ has a unique solution modulo $M = m_1 m_2 \dots m_k$. It allows breaking down large modular calculations into independent smaller modular calculations.
</details>

<details>
<summary>8. How do you test if a number is a power of 3 without loops or recursion?</summary>

Since 3 is prime, the maximum power of 3 that fits in a signed 32-bit integer is $3^{19} = 1162261467$. A positive integer $n$ is a power of 3 if and only if:
`n > 0 && 1162261467 % n == 0`.
</details>

---

## 11. Practice Exercises

### Exercise 1: Factorial Trailing Zeroes (LeetCode 172)
Given an integer `n`, return the number of trailing zeroes in `n!`.

<details>
<summary>Solution</summary>

```java
public class FactorialTrailingZeroes {
    public int trailingZeroes(int n) {
        int count = 0;
        while (n >= 5) {
            count += n / 5;
            n /= 5;
        }
        return count;
    }
}
```
</details>

### Exercise 2: Ugly Number II (LeetCode 264, Three Pointers)
Find the $n$-th ugly number (numbers whose prime factors are limited to 2, 3, and 5).

<details>
<summary>Solution</summary>

```java
public class UglyNumberII {
    public int nthUglyNumber(int n) {
        int[] ugly = new int[n];
        ugly[0] = 1;
        int i2 = 0, i3 = 0, i5 = 0;

        for (int i = 1; i < n; i++) {
            int next2 = ugly[i2] * 2;
            int next3 = ugly[i3] * 3;
            int next5 = ugly[i5] * 5;

            int nextUgly = Math.min(next2, Math.min(next3, next5));
            ugly[i] = nextUgly;

            if (nextUgly == next2) i2++;
            if (nextUgly == next3) i3++;
            if (nextUgly == next5) i5++;
        }

        return ugly[n - 1];
    }
}
```
</details>

### Exercise 3: Basic Calculator (LeetCode 224, String Math with Parentheses)

<details>
<summary>Solution</summary>

```java
import java.util.ArrayDeque;
import java.util.Deque;

public class BasicCalculator {
    public int calculate(String s) {
        Deque<Integer> stack = new ArrayDeque<>();
        int operand = 0;
        int result = 0;
        int sign = 1; // 1 for +, -1 for -

        for (int i = 0; i < s.length(); i++) {
            char ch = s.charAt(i);

            if (Character.isDigit(ch)) {
                operand = 10 * operand + (ch - '0');
            } else if (ch == '+') {
                result += sign * operand;
                sign = 1;
                operand = 0;
            } else if (ch == '-') {
                result += sign * operand;
                sign = -1;
                operand = 0;
            } else if (ch == '(') {
                // Push current result and sign onto stack
                stack.push(result);
                stack.push(sign);
                result = 0;
                sign = 1;
            } else if (ch == ')') {
                result += sign * operand;
                result *= stack.pop(); // Pop sign
                result += stack.pop(); // Pop operand
                operand = 0;
            }
        }

        return result + (sign * operand);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Euclidean GCD**: `while (b != 0) { long t = b; b = a % b; a = t; }`.
- **Sieve of Eratosthenes**: Outer loop $2 \dots \sqrt{N}$, inner multiples start at $p^2$.
- **Fast Pow**: Binary exponentiation evaluates $X^N$ in $O(\log N)$ steps.
- **Fermat's Inverse**: $a^{-1} \equiv a^{M-2} \pmod M$ when $M$ is prime.
- **True Modulo**: `Math.floorMod(a, m)` or `(a % m + m) % m`.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 31: Number-Theoretic Algorithms](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Hardy & Wright: An Introduction to the Theory of Numbers](https://global.oup.com/academic/product/an-introduction-to-the-theory-of-numbers-9780199219865)
- [Baeldung: Guide to Primality Testing in Java](https://www.baeldung.com/java-primality-testing)
- [LeetCode Math Tagged Problems](https://leetcode.com/tag/math/)
