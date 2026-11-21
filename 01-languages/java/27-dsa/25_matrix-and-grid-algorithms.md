# Matrix and Grid Algorithms

> Systematic patterns for 2D grids: in-place rotations, spiral orders, boundary flood fills, staircase searches, and cache-aware traversals.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [01_arrays-and-strings.md](01_arrays-and-strings.md), [11_graphs-bfs-dfs.md](11_graphs-bfs-dfs.md) |
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

A **Matrix** or **2D Grid** is an $M \times N$ array of cells indexed by row $r$ and column $c$.

In computer science, matrices represent:
- **Spatial Maps & Robotics**: Game boards (Chess, Go, Minesweeper), terrain elevation maps, and pathfinding navigation.
- **Image Processing**: Pixels in bitmap images undergoing convolution, rotation, and filtering.
- **Dynamic Programming Tables**: 2D state spaces for alignments and path planning.
- **Implicit Graphs**: A grid is an unweighted graph where each cell $(r, c)$ connects to its 4 cardinal (or 8 Chebyshev) adjacent neighbors.

---

## 2. Core Concepts

### 1. Directional Vectors
Avoid repeating boilerplate boundary arithmetic for adjacent neighbors:
```java
// 4-directional cardinal moves (Up, Down, Left, Right)
private static final int[][] DIRS = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

// 8-directional moves (including diagonals)
private static final int[][] DIRS8 = {
    {-1, -1}, {-1, 0}, {-1, 1},
    {0, -1},           {0, 1},
    {1, -1},  {1, 0},  {1, 1}
};
```

### 2. In-Place 90-Degree Rotation
A $90^\circ$ clockwise rotation can be decomposed into two geometric reflections:
1. **Transpose** the matrix: Swap `matrix[i][j]` with `matrix[j][i]`.
2. **Reverse each row horizontally**: Swap `matrix[i][j]` with `matrix[i][n - 1 - j]`.
Zero auxiliary memory required!

### 3. Staircase Search in Sorted Matrices
In a matrix where each row is sorted left-to-right and each column is sorted top-to-bottom:
- Start at the **top-right corner** $(0, N - 1)$.
- If target is smaller than current cell, move **left** (eliminate current column).
- If target is larger than current cell, move **down** (eliminate current row).
- Runs in strictly $O(M + N)$ time without full matrix scanning.

---

## 3. How It Works Internally

### CPU Cache Hierarchy & Row-Major Order
HotSpot JVM arrays are allocated row-wise. Traversing row-major order maximizes hardware cache prefetching:
```
Row-Major (Fast, O(1) Cache Hits):
for (int r = 0; r < M; r++) {
    for (int c = 0; c < N; c++) {
        sum += matrix[r][c]; // Sequential memory access across 64-byte cache line
    }
}

Column-Major (Slow, Cache Thrashing):
for (int c = 0; c < N; c++) {
    for (int r = 0; r < M; r++) {
        sum += matrix[r][c]; // Jumps across row arrays; misses L1 cache every time!
    }
}
```
Row-major traversal is often 5x to 10x faster due to spatial cache locality.

---

## 4. Syntax & Usage

### Grid In-Bounds Verification Helper

```java
public class GridHelpers {
    public static boolean isInBounds(int r, int c, int numRows, int numCols) {
        return r >= 0 && r < numRows && c >= 0 && c < numCols;
    }
}
```

---

## 5. Practical Examples

### Example 1: Rotate Image 90 Degrees Clockwise In-Place (LeetCode 48)

```java
public class RotateImage {
    public void rotate(int[][] matrix) {
        int n = matrix.length;

        // Step 1: Transpose matrix (swap over main diagonal)
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                int temp = matrix[i][j];
                matrix[i][j] = matrix[j][i];
                matrix[j][i] = temp;
            }
        }

        // Step 2: Reverse each row horizontally
        for (int i = 0; i < n; i++) {
            int left = 0, right = n - 1;
            while (left < right) {
                int temp = matrix[i][left];
                matrix[i][left] = matrix[i][right];
                matrix[i][right] = temp;
                left++;
                right--;
            }
        }
    }
}
```

### Example 2: Spiral Matrix (LeetCode 54)

```java
import java.util.*;

public class SpiralMatrix {
    public List<Integer> spiralOrder(int[][] matrix) {
        List<Integer> result = new ArrayList<>();
        if (matrix == null || matrix.length == 0) return result;

        int top = 0, bottom = matrix.length - 1;
        int left = 0, right = matrix[0].length - 1;

        while (top <= bottom && left <= right) {
            // Traverse Right
            for (int c = left; c <= right; c++) result.add(matrix[top][c]);
            top++;

            // Traverse Down
            for (int r = top; r <= bottom; r++) result.add(matrix[r][right]);
            right--;

            // Traverse Left (guard against single row)
            if (top <= bottom) {
                for (int c = right; c >= left; c--) result.add(matrix[bottom][c]);
                bottom--;
            }

            // Traverse Up (guard against single column)
            if (left <= right) {
                for (int r = bottom; r >= top; r--) result.add(matrix[r][left]);
                left++;
            }
        }

        return result;
    }
}
```

### Example 3: Set Matrix Zeroes (LeetCode 73, In-Place $O(1)$ Space)

```java
public class SetMatrixZeroes {
    public void setZeroes(int[][] matrix) {
        int m = matrix.length, n = matrix[0].length;
        boolean firstColZero = false;

        // Mark zeroes using first row and first column
        for (int r = 0; r < m; r++) {
            if (matrix[r][0] == 0) firstColZero = true;
            for (int c = 1; c < n; c++) {
                if (matrix[r][c] == 0) {
                    matrix[r][0] = 0;
                    matrix[0][c] = 0;
                }
            }
        }

        // Apply zeroes from bottom-right up to avoid corrupting markers
        for (int r = m - 1; r >= 0; r--) {
            for (int c = n - 1; c >= 1; c--) {
                if (matrix[r][0] == 0 || matrix[0][c] == 0) {
                    matrix[r][c] = 0;
                }
            }
            if (firstColZero) {
                matrix[r][0] = 0;
            }
        }
    }
}
```

### Example 4: Search a 2D Matrix II (LeetCode 240, Staircase Search)

```java
public class Search2DMatrixII {
    public boolean searchMatrix(int[][] matrix, int target) {
        if (matrix == null || matrix.length == 0) return false;
        int m = matrix.length, n = matrix[0].length;

        // Start at top-right corner
        int r = 0, c = n - 1;

        while (r < m && c >= 0) {
            if (matrix[r][c] == target) {
                return true;
            } else if (matrix[r][c] > target) {
                c--; // Move left
            } else {
                r++; // Move down
            }
        }

        return false;
    }
}
```

### Example 5: Word Search (LeetCode 79, In-Place Backtracking)

```java
public class WordSearch {
    public boolean exist(char[][] board, String word) {
        int m = board.length, n = board[0].length;
        char[] chars = word.toCharArray();

        for (int r = 0; r < m; r++) {
            for (int c = 0; c < n; c++) {
                if (dfs(board, r, c, chars, 0, m, n)) {
                    return true;
                }
            }
        }
        return false;
    }

    private boolean dfs(char[][] b, int r, int c, char[] w, int k, int m, int n) {
        if (k == w.length) return true;
        if (r < 0 || r >= m || c < 0 || c >= n || b[r][c] != w[k]) return false;

        char temp = b[r][c];
        b[r][c] = '#'; // Mark visited in-place

        boolean found = dfs(b, r - 1, c, w, k + 1, m, n)
                     || dfs(b, r + 1, c, w, k + 1, m, n)
                     || dfs(b, r, c - 1, w, k + 1, m, n)
                     || dfs(b, r, c + 1, w, k + 1, m, n);

        b[r][c] = temp; // Backtrack
        return found;
    }
}
```

---

## 6. Best Practices

1. **In-Place Marking During Traversal**: Avoid allocating `boolean[][] visited = new boolean[M][N]`. Mark visited cells directly on the board using a sentinel (e.g. `'#'`, `0`, or XOR bit flip `board[r][c] ^= 256`), then restore during backtracking.
2. **Order of Bounds Checks**: Always verify boundaries before indexing:
   `if (r >= 0 && r < m && c >= 0 && c < n && matrix[r][c] == target)`
   Reversing order causes `ArrayIndexOutOfBoundsException`.
3. **Use Staircase Search Over Binary Search for Dual-Sorted Matrices**: While each row could be binary searched in $O(M \log N)$, the staircase pointer search runs in strictly $O(M + N)$, requiring zero binary search overhead.
4. **Iterate Row-Major for Raw Speed**: Always structure nested loops `for r ... for c ...`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Fix |
|---|---|---|
| Indexing bounds bug: `c <= n` | Causes `ArrayIndexOutOfBoundsException` on boundary | Strictly check `c < n`. |
| Missing horizontal/vertical guard in Spiral Matrix | Prints redundant duplicates on $1 \times N$ or $M \times 1$ grids | Check `if (top <= bottom)` before left sweep, and `if (left <= right)` before up sweep. |
| In-place zeroing prematurely | Overwrites cells before their original values are read | Use first row/col markers and update backwards from $(M-1, N-1)$. |
| Column-major nested loop | Degrades cache hit rates by 90% | Iterate outer row $r$, inner column $c$. |

---

## 8. Performance & Security Notes

- **Time Complexity**:
  - Rotate Image: $O(N^2)$ in-place swaps.
  - Spiral Matrix: $O(M \times N)$ visiting each cell once.
  - Staircase Search: $O(M + N)$.
- **Space Complexity**: In-place algorithms run in strictly $O(1)$ auxiliary space.

---

## 9. Comparison / When to Use What

| Algorithm | Search Space | Complexity | Auxiliary Space |
|---|---|---|---|
| **Full Scan** | Unsorted $M \times N$ | $O(M \times N)$ | $O(1)$ |
| **Row Binary Search** | Rows sorted only | $O(M \log N)$ | $O(1)$ |
| **Staircase Search** | Rows & columns sorted | $O(M + N)$ | $O(1)$ |
| **Global Binary Search** | Fully sorted 1D flattened matrix | $O(\log(M \times N))$ | $O(1)$ |

---

## 10. Interview Questions

<details>
<summary>1. How do you rotate a matrix $90^\circ$ counter-clockwise in-place?</summary>

Two methods:
1. Reverse each row horizontally, then transpose the matrix.
2. Transpose the matrix, then reverse each column vertically.
</details>

<details>
<summary>2. Why does Staircase Search start at the top-right or bottom-left corner?</summary>

Because those two corners provide orthogonal decision directions. At the top-right corner $(0, N-1)$, moving down strictly increases values, while moving left strictly decreases values. At the top-left corner, both directions increase, providing no way to eliminate a row or column.
</details>

<details>
<summary>3. How do you map a 1D index $K$ to 2D coordinates $(r, c)$ in an $M \times N$ matrix?</summary>

$$r = K / N, \quad c = K \pmod N$$
Conversely, mapping $(r, c)$ back to a 1D index:
$$K = r \times N + c$$
</details>

<details>
<summary>4. What is the Game of Life state trick for $O(1)$ space updates?</summary>

Use extra bits of the integer:
- Bit 0: Current state (0 = dead, 1 = live)
- Bit 1: Next state (0 = will die, 1 = will live)
When counting live neighbors, read `cell & 1`. To finalize the next generation, shift right: `cell >>= 1`.
</details>

<details>
<summary>5. How do you find the diagonal order of an $M \times N$ matrix?</summary>

Notice that all cells on the same anti-diagonal share the property: $r + c = k$ where $k \in [0 \dots M + N - 2]$. Iterate $k$ and alternate traversal directions (up-right vs down-left) based on whether $k$ is even or odd.
</details>

<details>
<summary>6. How do you solve the Surrounded Regions problem (LeetCode 130) efficiently?</summary>

Flood fill from the **outer boundaries** only! Any `'O'` connected to the perimeter cannot be captured; mark it as `'S'` (safe). After boundary floods finish, any remaining `'O'` is completely surrounded and can be flipped to `'X'`; finally revert `'S'` back to `'O'`.
</details>

<details>
<summary>7. What is the Flood Fill algorithm and what is its space complexity?</summary>

Flood fill replaces connected components of matching cells with a target color using BFS or DFS. Time complexity is $O(M \times N)$. Space complexity is $O(M \times N)$ in the worst case (e.g. a spiral snake fill filling the call stack or queue).
</details>

<details>
<summary>8. How do you compute the 2D Prefix Sum matrix in $O(M \times N)$ and query submatrices in $O(1)$?</summary>

Construction:
$$P[r][c] = M[r-1][c-1] + P[r-1][c] + P[r][c-1] - P[r-1][c-1]$$
Query submatrix $[r_1, c_1] \to [r_2, c_2]$:
$$\text{Sum} = P[r_2+1][c_2+1] - P[r_1][c_2+1] - P[r_2+1][c_1] + P[r_1][c_1]$$
</details>

---

## 11. Practice Exercises

### Exercise 1: Game of Life (LeetCode 289, 2-Bit State Encoding)

<details>
<summary>Solution</summary>

```java
public class GameOfLife {
    public void gameOfLife(int[][] board) {
        int m = board.length, n = board[0].length;

        for (int r = 0; r < m; r++) {
            for (int c = 0; c < n; c++) {
                int liveNeighbors = countLive(board, r, c, m, n);
                // Rule 1 & 3: under/over-population dies (bit 1 remains 0)
                // Rule 2: live with 2 or 3 neighbors survives (set bit 1 to 1: 0b10 = 2)
                if ((board[r][c] & 1) == 1 && (liveNeighbors == 2 || liveNeighbors == 3)) {
                    board[r][c] |= 2;
                }
                // Rule 4: dead with 3 neighbors becomes live (set bit 1 to 1: 0b10 = 2)
                if ((board[r][c] & 1) == 0 && liveNeighbors == 3) {
                    board[r][c] |= 2;
                }
            }
        }

        // Shift bit 1 into bit 0
        for (int r = 0; r < m; r++) {
            for (int c = 0; c < n; c++) {
                board[r][c] >>= 1;
            }
        }
    }

    private int countLive(int[][] b, int r, int c, int m, int n) {
        int count = 0;
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = r + dr, nc = c + dc;
                if (nr >= 0 && nr < m && nc >= 0 && nc < n) {
                    count += b[nr][nc] & 1; // Read original state from bit 0
                }
            }
        }
        return count;
    }
}
```
</details>

### Exercise 2: Surrounded Regions (LeetCode 130, Boundary Flood Fill)

<details>
<summary>Solution</summary>

```java
public class SurroundedRegions {
    public void solve(char[][] board) {
        if (board.length == 0) return;
        int m = board.length, n = board[0].length;

        // Flood fill from boundary cells
        for (int r = 0; r < m; r++) {
            dfs(board, r, 0, m, n);
            dfs(board, r, n - 1, m, n);
        }
        for (int c = 0; c < n; c++) {
            dfs(board, 0, c, m, n);
            dfs(board, m - 1, c, m, n);
        }

        // Flip remaining 'O' to 'X', and restore 'S' to 'O'
        for (int r = 0; r < m; r++) {
            for (int c = 0; c < n; c++) {
                if (board[r][c] == 'O') board[r][c] = 'X';
                else if (board[r][c] == 'S') board[r][c] = 'O';
            }
        }
    }

    private void dfs(char[][] b, int r, int c, int m, int n) {
        if (r < 0 || r >= m || c < 0 || c >= n || b[r][c] != 'O') return;
        b[r][c] = 'S'; // Safe from capture
        dfs(b, r - 1, c, m, n);
        dfs(b, r + 1, c, m, n);
        dfs(b, r, c - 1, m, n);
        dfs(b, r, c + 1, m, n);
    }
}
```
</details>

### Exercise 3: Diagonal Traverse (LeetCode 498)

<details>
<summary>Solution</summary>

```java
public class DiagonalTraverse {
    public int[] findDiagonalOrder(int[][] mat) {
        if (mat == null || mat.length == 0) return new int[0];
        int m = mat.length, n = mat[0].length;
        int[] result = new int[m * n];
        int r = 0, c = 0;

        for (int i = 0; i < m * n; i++) {
            result[i] = mat[r][c];

            // Up-Right direction
            if ((r + c) % 2 == 0) {
                if (c == n - 1) { r++; }
                else if (r == 0) { c++; }
                else { r--; c++; }
            } else {
                // Down-Left direction
                if (r == m - 1) { c++; }
                else if (c == 0) { r++; }
                else { r++; c--; }
            }
        }

        return result;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Rotation $90^\circ$**: Transpose then horizontal flip.
- **Spiral Order**: 4 dynamic boundaries (`top`, `bottom`, `left`, `right`) with guard checks.
- **Staircase Search**: Top-right corner traversal solves dual-sorted grids in $O(M + N)$.
- **In-Place Marking**: Use sentinels (`'#'`) or extra bits (`board[r][c] |= 2`).
- **Row-Major Rule**: Iterate row $r$ outer, column $c$ inner for CPU L1 cache hits.

---

## 13. Further Reading

- [Introduction to Algorithms (CLRS), Chapter 28: Matrix Operations](https://mitpress.mit.edu/9780262046305/introduction-to-algorithms/)
- [Baeldung: Guide to Matrix Multiplication and Traversal in Java](https://www.baeldung.com/java-matrix-multiplication)
- [LeetCode Matrix Tagged Problems](https://leetcode.com/tag/matrix/)
