<!--
================================================================================
Metadata:
  Title: Automated Bug Hunting: Binary Search with git bisect
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 14-blame-and-log-search.md
  Key Terminology: git bisect, Binary Search (O(log N)), git bisect run, Exit Code 125 (Skip), Automated Regression Testing, Bisect Replay
================================================================================
-->

# Automated Bug Hunting: Binary Search with git bisect

[<- 14-blame-and-log-search.md](./14-blame-and-log-search.md) | [Back to Module Overview](./README.md) | [16-submodules-and-subtrees.md ->](./16-submodules-and-subtrees.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

A customer reports a subtle regression in production:
*"The export PDF button fails with an HTTP 500 error on accounts with more than 1,000 transactions."*

You verify that the feature worked perfectly in release `v2.4.0` three months ago, but is broken in today's `v2.8.0`. In the three months between those releases, your team pushed **1,500 commits** across 20 microservices and dozens of database migrations.

How do you find the exact commit that introduced the bug?
- **Manual linear search**: Testing each commit one by one would require checking out, building, and testing up to 1,500 commits—taking days or weeks.
- **`git bisect` (Binary Search)**: Applies binary search ($O(\log_2 N)$) across the commit Directed Acyclic Graph.

With `git bisect`, finding the culprit among 1,500 commits requires at most **11 test steps** ($\lceil \log_2(1500) \rceil = 11$). Even better: by providing a test script via **`git bisect run`**, Git executes the entire binary search completely unattended while you grab a coffee!

---

## 2. Core Concepts

### The Binary Search Principle in Git
1. You identify a **"bad"** commit (a commit where the bug is present, usually `HEAD`).
2. You identify a **"good"** commit (a historical commit where the bug was definitely absent, e.g. `v2.4.0`).
3. Git calculates the midpoint commit in the DAG and automatically checks it out.
4. You test the midpoint:
   - If the bug is present $\to$ mark it `bad`. Git discards the newer half of history.
   - If the bug is absent $\to$ mark it `good`. Git discards the older half of history.
5. Git checks out the next midpoint. The search space cuts in half at every iteration until Git outputs:
   `[hash] is the first bad commit`.

```
+-----------------------------------------------------------------------------------+
|                            Binary Search with git bisect                          |
|                                                                                   |
|  v2.4.0 (GOOD)                                                    HEAD (BAD)      |
|  [ c1 ] ── [ c2 ] ── [ c3 ] ── [ c4 ] ── [ c5 ] ── [ c6 ] ── [ c7 ] ── [ c8 ]     |
|                                                                                   |
|  Step 1: Git checks out midpoint [ c4 ]                                           |
|          Test: GOOD! -> Discard c1, c2, c3, c4.                                   |
|                                                                                   |
|  Step 2: Remaining space [ c5, c6, c7, c8 ]. Git checks out [ c6 ]                |
|          Test: BAD! -> Culprit is c5 or c6. Discard c7, c8.                       |
|                                                                                   |
|  Step 3: Git checks out [ c5 ]                                                    |
|          Test: BAD!                                                               |
|                                                                                   |
|  RESULT: c5 is the FIRST BAD COMMIT! (Found in 3 steps instead of 8!)             |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

### Bisect State Storage in `.git/`
When you initiate a bisect session, Git creates administrative state files:
- **`.git/BISECT_START`**: Records the commit where you were standing when you ran `git bisect start`, allowing Git to return you home when you run `git bisect reset`.
- **`.git/BISECT_LOG`**: Records every command, hash, and judgment (`good`, `bad`, `skip`) made during the session.
- **`refs/bisect/good-*` and `refs/bisect/bad`**: Special reference pointers in `.git/refs/` tracking the boundary limits.

### Automated Script Execution (`git bisect run`)
When you supply an automated test command (`git bisect run ./test.sh`), Git executes the script at each midpoint and interprets the **process exit code**:
- **Exit Code `0`**: The test passed $\to$ Git marks the commit as **`good`**.
- **Exit Code `1` to `127` (except 125)**: The test failed $\to$ Git marks the commit as **`bad`**.
- **Exit Code `125` (Special Skip Code)**: The commit cannot be tested (e.g. an unrelated syntax error broke the build, or an external dependency was down) $\to$ Git marks the commit as **`skip`** and selects a neighboring commit!

---

## 4. Syntax & Usage

### 1. Manual Bisect Workflow
```bash
# 1. Start bisect session
git bisect start

# 2. Mark current broken commit as bad
git bisect bad

# 3. Mark known historical working commit or tag as good
git bisect good v1.4.0

# 4. Git checks out midpoint. Test your app, then report:
git bisect good
# (Or if broken):
# git bisect bad

# 5. If a commit has an unrelated build break and cannot be tested:
git bisect skip

# 6. Once Git identifies the culprit, end bisect and return to original branch:
git bisect reset
```

---

### 2. Fully Automated Bisect Workflow (`git bisect run`)
```bash
# Initialize and declare boundaries in one command:
git bisect start HEAD v1.4.0

# Hand off execution to automated test script or command:
git bisect run npm test

# Or run a custom test script:
git bisect run ./scripts/test-regression.sh

# End session after culprit identified:
git bisect reset
```

---

### 3. Bisect Logging & Replay
```bash
# View the exact sequence of bisect decisions made so far
git bisect log

# Save the log to a file for teammate review or reproducibility
git bisect log > bisect-run.log

# Replay an entire bisect session on another machine
git bisect replay bisect-run.log
```

---

## 5. Practical Examples

### Example 1: End-to-End Automated Bisect Demonstration
Simulate a regression introduced in a sequence of commits:

```bash
mkdir /tmp/bisect-lab && cd /tmp/bisect-lab
git init -b main

# Commit 1 (GOOD baseline)
echo "HEALTHY" > status.txt
git add status.txt && git commit -m "feat: initial healthy build"
GOOD_COMMIT=$(git rev-parse HEAD)

# Commits 2 through 5 (Clean updates)
for i in {2..5}; do
  echo "update $i" >> log.txt && git add log.txt && git commit -m "feat: feature $i"
done

# Commit 6 (THE BUG INJECTOR!)
echo "BROKEN_PAYLOAD" > status.txt
git commit -am "fix(perf): refactor status polling engine"

# Commits 7 through 10 (Subsequent work unaware of bug)
for i in {7..10}; do
  echo "subsequent work $i" >> log.txt && git add log.txt && git commit -m "feat: feature $i"
done

# ------------------------------------------------------------------------------
# AUTOMATED HUNTING WITH GIT BISECT
# ------------------------------------------------------------------------------
# Start bisect defining BAD (HEAD) and GOOD (GOOD_COMMIT)
git bisect start HEAD $GOOD_COMMIT

# Run automated test: test fails if status.txt contains "BROKEN_PAYLOAD"
git bisect run grep -q "HEALTHY" status.txt
```
Output:
```text
running grep -q HEALTHY status.txt
Bisecting: 4 revisions left to test after this (roughly 2 steps)
...
8f1e2d4a1b... is the first bad commit
commit 8f1e2d4a1b...
Author: Engineering Team <dev@company.com>
Date:   Mon Nov 25 10:00:00 2024

    fix(perf): refactor status polling engine

:100644 100644 4b825d... 7a8b9c... M  status.txt
```
*Git pinpointed the exact commit in seconds without any manual intervention!*
```bash
# Return to original HEAD
git bisect reset
```

### Example 2: Writing a Robust Test Script with Exit Code 125
Create a production test script that skips unbuildable commits:

```bash
cat << 'EOF' > test-runner.sh
#!/usr/bin/env bash

# 1. Verify project compiles. If build fails, return 125 to SKIP!
if ! make build > /dev/null 2>&1; then
    exit 125
fi

# 2. Run specific regression test suite
if ./bin/app --test-export-pdf; then
    exit 0  # Passed -> GOOD
else
    exit 1  # Failed -> BAD
fi
EOF
chmod +x test-runner.sh

# Run bisect
git bisect start HEAD v2.0.0
git bisect run ./test-runner.sh
```

### Example 3: Bisecting Non-Bug Transitions (Finding When a Feature Landed)
`git bisect` isn't just for bugs; you can use custom terms like `old`/`new`:

```bash
git bisect start --term-new=present --term-old=absent
git bisect present HEAD
git bisect absent v1.0.0

# Finds the first commit where a feature was introduced!
```

---

## 6. Best Practices

1. **Write Fast, Targeted Test Scripts**: When using `git bisect run`, do not run the entire 2-hour monorepo end-to-end suite. Write a minimal test script that tests *only* the specific regressed behavior in under 5 seconds.
2. **Always Handle Compile Failures with Exit Code 125**: If a commit in the middle of history has a temporary compile error (unrelated to your bug), returning exit code 1 will falsely mark it as `bad`. Returning `125` tells Git to skip it and test a neighbor.
3. **Always End with `git bisect reset`**: When bisect completes, you are left in a detached HEAD state at the culprit commit. Running `git bisect reset` safely restores you to your original branch.
4. **Enforce Atomic Commits in CI**: `git bisect` relies on every commit being a working, standalone snapshot. If developers push broken intermediate commits that fail compilation, bisecting becomes difficult.
5. **Save the Bisect Log (`git bisect log`)**: Attach the bisect log to the bug report or PR ticket to mathematically prove when and where the regression was introduced.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| Bisect points to completely innocent commit | Test script returned exit code 1 on an unrelated build error. | Use exit code `125` for build failures so Git skips untestable commits. |
| `fatal: A bisect process seems to be already in progress.` | An earlier bisect session was not terminated. | Run `git bisect reset` to clear old state files. |
| Working tree left in detached HEAD after bisect | Forgot to run `git bisect reset`. | Run `git bisect reset` to return to your original branch. |
| `git bisect run` loops infinitely | Test script did not return non-zero on failure (e.g. bash script lacked error checking). | Ensure test script returns `exit 1` explicitly when the bug is detected. |
| Cannot reproduce bug during bisect step | Environment variables, local database, or untracked cache files are polluting the test. | Run `git clean -fdx` or ensure the test script runs in an isolated container. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Logarithmic Complexity ($O(\log N)$)**:
  - 100 commits $\to$ max 7 tests.
  - 1,000 commits $\to$ max 10 tests.
  - 10,000 commits $\to$ max 14 tests.
  - 100,000 commits $\to$ max 17 tests.
  Even across decade-old repositories, bisect finds the culprit in under 15 tests.

### Cost
- **Developer Hours Saved**: Diagnosing complex regressions that would normally take a senior engineer 2 days of manual code reading can be isolated in **under 10 minutes** via `git bisect run`.

---

## 9. Comparison Table

| Approach | Time Complexity | Steps for 1,000 Commits | Developer Effort |
| :--- | :--- | :--- | :--- |
| **Linear Manual Search** | $O(N)$ | Up to 1,000 tests | Days of manual testing |
| **Manual `git bisect`** | $O(\log N)$ | Max 10 tests | 10 minutes of manual verification |
| **Automated `git bisect run`**| $O(\log N)$ | Max 10 tests | Zero effort (unattended script execution) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. What algorithm does `git bisect` use and what is its time complexity?</strong></summary>

`git bisect` uses a **binary search algorithm** across the Directed Acyclic Graph (DAG) of commits. Its time complexity is $O(\log_2 N)$, where $N$ is the number of commits between the known good commit and the known bad commit. For a history of 1,000 commits, Git requires at most 10 test evaluations ($\lceil \log_2(1000) \rceil = 10$) to isolate the exact commit that introduced the regression.
</details>

<details>
<summary><strong>2. What is the special significance of exit code 125 in `git bisect run`?</strong></summary>

When executing an automated test script with `git bisect run`:
- Exit code `0` means the test passed (commit is marked `good`).
- Exit codes `1` to `127` (except 125) mean the test failed (commit is marked `bad`).
- Exit code **`125`** is specifically reserved by Git to indicate **SKIP**. It signals that the commit cannot be evaluated (e.g. an unrelated syntax error broke compilation, or a test asset was missing). Git skips the commit and chooses an adjacent midpoint without failing the bisect session.
</details>

<details>
<summary><strong>3. How does `git bisect` handle non-linear merge histories with multiple branches?</strong></summary>

Git's bisect engine does not treat commit history as a flat array. It evaluates the topological Directed Acyclic Graph. At each step, Git calculates the commit whose removal divides the remaining reachable graph into two roughly equal halves. If the bad commit is on a merged branch, Git cleanly descends into the merge branch DAG to find the first bad commit.
</details>

<details>
<summary><strong>4. What does `git bisect skip` do and what happens if the culprit commit was skipped?</strong></summary>

`git bisect skip` tells Git that the current midpoint cannot be tested. Git chooses a neighboring commit to continue the binary search. If the actual culprit happens to be among the skipped commits, Git will narrow the result down to the smallest possible range of ambiguous commits (e.g. "The first bad commit could be any of: [c1, c2, c3]").
</details>

<details>
<summary><strong>5. What occurs when you execute `git bisect reset`?</strong></summary>

`git bisect reset` terminates the bisect session. Git reads the original starting commit recorded in `.git/BISECT_START`, clears all temporary bisect references (`refs/bisect/*`), deletes bisect log state files, and checks out your original branch, safely returning your working tree and `HEAD` to where you started.
</details>

<details>
<summary><strong>6. How can an engineer share or replay a bisect session with a teammate?</strong></summary>

Using `git bisect log` and `git bisect replay`:
```bash
git bisect log > bisect.log
```
The teammate can then execute:
```bash
git bisect replay bisect.log
```
Git replays every step and judgment automatically, arriving at the exact same conclusion.
</details>

<details>
<summary><strong>7. Why is keeping commits atomic and buildable crucial for the effectiveness of `git bisect`?</strong></summary>

If developers commit broken, non-compiling intermediary code ("wip", "broken build"), `git bisect` will frequently land on commits that cannot run automated test suites, forcing the engine to skip commits or requiring manual intervention. Enforcing atomic, buildable commits at every step guarantees that `git bisect run` can execute smoothly without interruption.
</details>

<details>
<summary><strong>8. Can `git bisect` be used for performance regressions or benchmark hunting?</strong></summary>

Yes. A test script can execute a performance benchmark, parse the execution time or memory consumption, and exit with code `1` if execution time exceeds a threshold (e.g. `>50ms`) and exit with `0` if below the threshold. `git bisect run` will successfully identify the exact commit that introduced the performance degradation.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Basic Manual Bisect Drill</strong></summary>

**Task**: In a test repository with 6 commits, manually bisect to find the commit that added a bug.

**Solution**:
```bash
mkdir /tmp/bisect-manual && cd /tmp/bisect-manual
git init -b main

# Create 5 commits
echo "safe" > file.txt && git add file.txt && git commit -m "c1"
GOOD_HASH=$(git rev-parse HEAD)

echo "c2" >> file.txt && git commit -am "c2"
echo "c3" >> file.txt && git commit -am "c3"
echo "BUG_LINE" >> file.txt && git commit -am "c4 (bug introduced)"
echo "c5" >> file.txt && git commit -am "c5"

# Start manual bisect
git bisect start
git bisect bad HEAD
git bisect good $GOOD_HASH

# Test midpoint: check if BUG_LINE is present
grep "BUG_LINE" file.txt && git bisect bad || git bisect good

# Git pinpoints c4 as the first bad commit!
git bisect reset
```
</details>

<details>
<summary><strong>Exercise 2: Automated Bisect with Inline Command</strong></summary>

**Task**: Automate Exercise 1 using `git bisect run` with a single inline `grep` command.

**Solution**:
```bash
git bisect start HEAD $GOOD_HASH

# Negated grep: passes (0) if BUG_LINE is ABSENT; fails (1) if PRESENT!
git bisect run ! grep -q "BUG_LINE" file.txt

# Outputs: c4 is the first bad commit!
git bisect reset
```
</details>

<details>
<summary><strong>Exercise 3: Handling Skips with Exit Code 125</strong></summary>

**Task**: Write a test script that exits with 125 if an arbitrary flag file exists, proving that Git skips that commit.

**Solution**:
```bash
cat << 'EOF' > test-skip.sh
#!/usr/bin/env bash
if [ -f "skip.flag" ]; then
    exit 125  # Skip this commit
fi
! grep -q "BUG_LINE" file.txt
EOF
chmod +x test-skip.sh

git bisect start HEAD $GOOD_HASH
git bisect run ./test-skip.sh
git bisect reset
```
</details>

<details>
<summary><strong>Exercise 4: Exporting and Inspecting Bisect Logs</strong></summary>

**Task**: Start a bisect session, make one good and one bad declaration, and inspect the raw contents of `.git/BISECT_LOG`.

**Solution**:
```bash
git bisect start HEAD $GOOD_HASH
git bisect log
# Displays: git bisect start, git bisect bad, git bisect good
git bisect reset
```
</details>

<details>
<summary><strong>Exercise 5: Finding Feature Inception with Custom Terms</strong></summary>

**Task**: Use `--term-old` and `--term-new` to find when a feature was introduced.

**Solution**:
```bash
git bisect start --term-new=feature-present --term-old=feature-absent
git bisect feature-present HEAD
git bisect feature-absent $GOOD_HASH
# Follow prompts using custom term commands
git bisect reset
```
</details>

<details>
<summary><strong>Exercise 6: Performance Regression Bisect Script</strong></summary>

**Task**: Write a script that benchmarks execution time and exits 1 if execution exceeds 10 milliseconds.

**Solution**:
```bash
cat << 'EOF' > perf-test.sh
#!/usr/bin/env bash
START=$(date +%s%N)
# Run application operation
grep "safe" file.txt > /dev/null
END=$(date +%s%N)
DIFF=$(( (END - START) / 1000000 ))

# If execution took > 10ms, mark BAD
if [ $DIFF -gt 10 ]; then
    exit 1
else
    exit 0
fi
EOF
chmod +x perf-test.sh
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Command | Usage | Role |
| :--- | :--- | :--- |
| `git bisect start` | CLI | Starts binary search session |
| `git bisect bad [<hash>]` | CLI | Marks commit as exhibiting the bug |
| `git bisect good [<hash>]` | CLI | Marks commit as clean/working |
| `git bisect skip` | CLI | Skips current commit due to untestable build |
| `git bisect reset` | CLI | Clears bisect state and returns to original branch |
| `git bisect run <cmd>` | Automated | Executes `<cmd>` at each step; unattended bisect |
| `Exit Code 0` | Inside Script | Tells Git commit is GOOD |
| `Exit Code 1-127` | Inside Script | Tells Git commit is BAD |
| `Exit Code 125` | Inside Script | Tells Git to SKIP current commit |
| `git bisect log` | CLI | Displays session history |
| `git bisect replay <file>` | CLI | Re-executes recorded bisect session |

---

## 13. Further Reading & Official Documentation

- [Official Git Documentation - git-bisect](https://git-scm.com/docs/git-bisect)
- [Pro Git Book - Debugging with Git - Binary Search](https://git-scm.com/book/en/v2/Git-Tools-Debugging-with-Git#_binary_search)
- [Git Bisect Run: Automated Regression Hunting](https://git-scm.com/docs/git-bisect#_bisect_run)

---

[<- 14-blame-and-log-search.md](./14-blame-and-log-search.md) | [Back to Module Overview](./README.md) | [16-submodules-and-subtrees.md ->](./16-submodules-and-subtrees.md)
