<!--
================================================================================
Metadata:
  Title: Reference Project 02 - Interactive Rebase & Disaster Recovery Lab
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 03-history-rewriting/02-rebase.md, 03-history-rewriting/04-reflog-and-recovery.md
  Key Terminology: Interactive Rebase, squash, fixup, drop, git reflog, HEAD@{n}, git fsck, Dangling Commits
================================================================================
-->

# Reference Project 02: Interactive Rebase & Disaster Recovery Lab

[<- Back to Projects Overview](./README.md) | [Next Project: 03-ci-pipeline-for-a-node-app.md ->](./03-ci-pipeline-for-a-node-app.md)

---

## 1. Overview & Architecture

One of Git's greatest superpowers is its safety net: **Git almost never permanently deletes committed data**. Even after executing destructive-sounding operations like `git reset --hard`, accidental branch deletions (`git branch -D`), or botched interactive rebases, the underlying commit objects remain intact within the `.git/objects` database until explicit garbage collection (`git gc`) purges them weeks later.

This reference lab simulates a catastrophic developer disaster scenario:
1. You perform an interactive rebase (`git rebase -i`) to clean up sloppy commit messages and squash WIP commits.
2. An errant command (`git reset --hard HEAD~5`) accidentally wipes out 5 days of work.
3. You inadvertently delete the active feature branch with un-pushed work.
4. Using **`git reflog`** and **`git fsck`**, you perform forensic recovery to reconstruct the branch pointer and restore every lost commit cleanly.

### Disaster & Recovery Sequence Diagram

```
Commit History:
(Base) C0 ---> C1 (WIP) ---> C2 (WIP typo) ---> C3 (feat) ---> C4 (docs)
                                                                 |
                                                          [HEAD on branch]
                                                                 |
               +-------------------------------------------------+
               | Accident: git reset --hard C0
               | Result: Branch pointer moves back to C0!
               | Working tree wiped. C1..C4 are unreferenced ("lost").
               v
(Base) C0 <--- [HEAD]
   |
   +-- (Dangling): C1 -> C2 -> C3 -> C4
   |
   | [Forensic Tool: git reflog]
   | Queries .git/logs/HEAD to locate HEAD@{1}: C4 SHA (8f2a1b...)
   v
[Recovery Command: git branch recovered-feature 8f2a1b]
   |
   v
Result: 100% of commits, trees, and files restored!
```

---

## 2. Prerequisites & Setup

### Environment Requirements
- **Git**: Version 2.25+ installed
- **Terminal**: Bash shell (Linux, macOS, WSL, or Git Bash on Windows)

### Lab Directory Initialization
```bash
mkdir -p ~/git-labs/rebase-recovery-lab && cd ~/git-labs/rebase-recovery-lab
git init
```

---

## 3. Step-by-Step Implementation Walkthrough

### Step 1: Populate Baseline and Messy Commit Stream
Create a sequence of realistic, un-curated development commits:

```bash
# Baseline commit
echo "# Order Management Service" > README.md
git add README.md && git commit -m "chore: initial commit"

# Checkout feature branch
git checkout -b feature-order-fulfillment

# Commit 1: Core order model
cat << 'EOF' > order.py
class Order:
    def __init__(self, order_id, total):
        self.order_id = order_id
        self.total = total
        self.status = "PENDING"
EOF
git add order.py && git commit -m "wip order"

# Commit 2: Fix typo in model
cat << 'EOF' >> order.py

    def cancel(self):
        self.status = "CANCELLED"
EOF
git add order.py && git commit -m "fix typo in order"

# Commit 3: Add fulfillment logic
cat << 'EOF' > fulfillment.py
class FulfillmentEngine:
    def process(self, order):
        order.status = "FULFILLED"
        return True
EOF
git add fulfillment.py && git commit -m "add fulfillment logic"

# Commit 4: Add bad debugging print
echo "print('DEBUGGING ORDER PROCESS')" >> fulfillment.py
git add fulfillment.py && git commit -m "temp debug print"

# Commit 5: Add tests
cat << 'EOF' > test_order.py
import unittest
from order import Order
from fulfillment import FulfillmentEngine

class TestOrder(unittest.TestCase):
    def test_fulfillment(self):
        o = Order("101", 50)
        FulfillmentEngine().process(o)
        self.assertEqual(o.status, "FULFILLED")

if __name__ == "__main__":
    unittest.main()
EOF
git add test_order.py && git commit -m "feat: add fulfillment unit test"
```

Inspect the messy log:
```bash
git log --oneline
```

---

### Step 2: Perform Interactive Rebase Surgery
Clean up the 5 commits into a clean, professional two-commit atomic history:
1. Squash `wip order` and `fix typo in order` into one commit.
2. Drop `temp debug print`.
3. Reword `add fulfillment logic`.

Launch interactive rebase targeting the baseline commit:
```bash
# In an interactive terminal:
# git rebase -i HEAD~5
```

For scripted lab automation, we simulate the rebase editor instructions:
```bash
# Set up git sequence editor script
GIT_SEQUENCE_EDITOR="sed -i.bak -e '2s/pick/squash/' -e '4s/pick/drop/'" git rebase -i HEAD~5
```
Or manually edit the todo list to:
```text
pick   3a1b2c  wip order
squash 4b2c3d  fix typo in order
pick   5c3d4e  add fulfillment logic
drop   6d4e5f  temp debug print
pick   7e5f6a  feat: add fulfillment unit test
```

Inspect clean history:
```bash
git log --oneline
```
The history is now clean, atomic, and free of debug statements.

---

### Step 3: Simulate Catastrophic Disaster
Now simulate two critical mistakes made by an exhausted engineer at 2 AM:

**Disaster A: Accidental Hard Reset**
```bash
# The developer accidentally resets to the repo baseline:
git reset --hard HEAD~2
```
Look at your directory: `order.py`, `fulfillment.py`, and `test_order.py` are completely gone!
`git status` reports a clean working tree at the initial commit.

**Disaster B: Accidental Branch Deletion**
To make matters worse, the developer switches away and forcefully deletes the feature branch:
```bash
git checkout main
git branch -D feature-order-fulfillment
```
Output:
```text
Deleted branch feature-order-fulfillment (was a1b2c3d).
```
The branch pointer is destroyed. To an inexperienced developer, days of work appear permanently deleted.

---

### Step 4: Forensic Investigation with `git reflog`
Git maintains a local append-only log of every change to `HEAD` in `.git/logs/HEAD`.

Run `git reflog`:
```bash
git reflog
```
Sample reflog output:
```text
a1b2c3d HEAD@{0}: checkout: moving from feature-order-fulfillment to main
e4f5a6b HEAD@{1}: reset: moving to HEAD~2
b7c8d9e HEAD@{2}: rebase -i (finish): returning to refs/heads/feature-order-fulfillment
b7c8d9e HEAD@{3}: rebase -i (pick): feat: add fulfillment unit test
5c3d4e1 HEAD@{4}: rebase -i (pick): add fulfillment logic
...
```

Notice entry **`HEAD@{2}`** (or `HEAD@{3}`): It records the exact commit hash `b7c8d9e` right after the interactive rebase finished, before the catastrophic hard reset!

---

### Step 5: Full Branch and Commit Restoration
Recreate the deleted branch pointing directly to the discovered commit hash:

```bash
# Extract the SHA corresponding to the state right after rebase finished
RECOVERED_SHA=$(git reflog | grep "rebase -i (finish)" | head -n1 | awk '{print $1}')

echo "Recovered SHA: $RECOVERED_SHA"

# Recreate branch from the reflog commit hash
git checkout -b feature-order-fulfillment-restored "$RECOVERED_SHA"
```

Verify your files:
```bash
ls -la
```
`order.py`, `fulfillment.py`, and `test_order.py` are 100% restored!

---

## 4. Verification & Testing

### Step 1: Run Unit Tests on Restored Code
```bash
python3 test_order.py
```
Output:
```text
.
----------------------------------------------------------------------
Ran 1 test in 0.000s

OK
```

### Step 2: Identify Dangling Commits with `git fsck`
Verify Git's internal object store health:
```bash
git fsck --lost-found
```
Git scans the object database and identifies any unreachable blobs or commits, confirming they are preserved safely.

---

## 5. Production Readiness & Troubleshooting

### Emergency Recovery Quick Reference Table

| Disaster Scenario | Recovery Command |
|:---|:---|
| Accidental `git reset --hard` | `git reset --hard HEAD@{1}` (or commit SHA from `git reflog`) |
| Accidental branch deletion (`git branch -D <name>`) | `git checkout -b <name> <SHA_from_reflog>` |
| Botched interactive rebase in progress | `git rebase --abort` |
| Discarded uncommitted working tree changes | Check editor local history (VSCode/IntelliJ timeline) |
| Finding unreferenced commits without reflog | `git fsck --unreachable --no-reflogs` |

### Critical Recovery Rules
1. **Never run `git gc --prune=now` during an active incident**: Garbage collection purges unreachable objects. Leave Git's internal cache alone while recovering.
2. **Reflogs are local only**: The reflog is not pushed to GitHub. Recovery must occur on the workstation where the commits were created.

---

[<- Back to Projects Overview](./README.md) | [Next Project: 03-ci-pipeline-for-a-node-app.md ->](./03-ci-pipeline-for-a-node-app.md)
