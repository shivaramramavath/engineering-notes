<!--
================================================================================
Metadata:
  Title: Reference Project 01 - Advanced Merge Conflict Resolution Lab
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 45 minutes
  Prerequisites: 02-commands/07-merging.md, 02-commands/08-merge-conflicts-and-resolution.md
  Key Terminology: 3-Way Merge, diff3, merge.conflictStyle, git rerere, OURS vs THEIRS, Cherry-Pick Conflicts
================================================================================
-->

# Reference Project 01: Advanced Merge Conflict Resolution Lab

[<- Back to Projects Overview](./README.md) | [Next Project: 02-rebase-and-recovery-lab.md ->](./02-rebase-and-recovery-lab.md)

---

## 1. Overview & Architecture

Merge conflicts are an inevitable reality of concurrent engineering in distributed teams. When two developers modify the same lines of code or alter shared configuration schemas on divergent branches, Git halts automated merging to protect codebase integrity. Resolving conflicts under pressure without dropping critical changes requires a structured mental model and mastery of Git's conflict analysis tooling.

This hands-on reference project provides an automated, self-contained simulation harness that constructs a complex, realistic three-way merge conflict scenario across code, database migration scripts, and configuration files. You will configure Git's advanced `diff3` conflict inspection format, resolve syntactic and structural collisions, leverage `git rerere` (Reuse Recorded Resolution) to automate repetitive conflict fixes, and verify code execution.

### Conflict Topology Diagram

```
                              [Branch: feature-stripe-v2]
                              Commit C: Update payment gateway logic
                             /          Add idempotency header to API
                            /
... --- Commit A -----------
        (Base Snapshot)     \
        Payment Service v1   \
                              \
                              [Branch: feature-paypal-express]
                              Commit B: Refactor payment interface
                                        Update database schema & config
                                        (Modifies identical lines in core service)
                                            |
                                            v
                                 [Target: Merge Collision]
                                 CONFLICT in payment_service.py
                                 CONFLICT in config/settings.yaml
```

### Learning Outcomes
- Construct an isolated conflict reproduction sandbox via shell scripting.
- Understand the 3-Way merge algorithm: Base (`ancestor`), Ours (`HEAD`), and Theirs (`MERGE_HEAD`).
- Enable and interpret Git's `diff3` conflict style to inspect original base code directly inside conflict markers.
- Resolve multi-file collisions using checkout strategies (`--ours`, `--theirs`, and manual reconciliation).
- Activate and verify `git rerere` to automatically record and re-apply conflict resolutions across rebases and cherry-picks.

---

## 2. Prerequisites & Setup

### Environment Requirements
- **Git**: Version 2.30+ installed (`git --version`)
- **Bash / Terminal**: Linux, macOS, or Windows (Git Bash / WSL / PowerShell with bash)
- **Python**: Version 3.8+ (for executing the simulated payment application)

### Project Layout
The lab generates the following file hierarchy:
```text
conflict-lab/
├── payment_service.py      # Core service file with concurrent logic edits
├── config/
│   └── settings.yaml       # Configuration file with conflicting timeouts
└── tests/
    └── test_payment.py     # Verification unit test suite
```

---

## 3. Step-by-Step Implementation Walkthrough

### Step 1: Initialize Lab Repository and Configure `diff3`
Before generating conflicts, configure Git to use `diff3` style so conflict markers display the common ancestor:

```bash
# Set up lab directory
mkdir -p ~/git-labs/conflict-lab && cd ~/git-labs/conflict-lab
git init

# Configure diff3 conflict style globally or locally
git config merge.conflictStyle diff3

# Enable Reuse Recorded Resolution (rerere)
git config rerere.enabled true
git config rerere.autoUpdate true
```

### Step 2: Establish the Common Ancestor (Base Commit)
Create the initial implementation of the payment service:

```bash
mkdir -p config tests

# 1. Base payment service
cat << 'EOF' > payment_service.py
class PaymentProcessor:
    def __init__(self, timeout=30):
        self.gateway = "StandardGateway_v1"
        self.timeout = timeout
        self.currency = "USD"

    def charge(self, user_id, amount):
        print(f"Connecting to {self.gateway} with {self.timeout}s timeout...")
        # Baseline charge logic
        if amount <= 0:
            raise ValueError("Amount must be positive")
        return {"status": "SUCCESS", "gateway": self.gateway, "amount": amount}
EOF

# 2. Base configuration
cat << 'EOF' > config/settings.yaml
app:
  name: "PaymentGatewayService"
  env: "production"
  timeout_seconds: 30
  retry_attempts: 3
EOF

# 3. Test harness
cat << 'EOF' > tests/test_payment.py
import unittest
from payment_service import PaymentProcessor

class TestPayment(unittest.TestCase):
    def test_charge_success(self):
        processor = PaymentProcessor()
        res = processor.charge("usr_123", 100)
        self.assertEqual(res["status"], "SUCCESS")

if __name__ == "__main__":
    unittest.main()
EOF

git add .
git commit -m "feat(core): initial payment service baseline"
```

### Step 3: Author Branch A (`feature-stripe-v2`)
Branch A upgrades the engine to Stripe v2 with idempotency keys:

```bash
git checkout -b feature-stripe-v2

cat << 'EOF' > payment_service.py
import uuid

class PaymentProcessor:
    def __init__(self, timeout=45):
        self.gateway = "Stripe_API_v2"
        self.timeout = timeout
        self.currency = "USD"

    def charge(self, user_id, amount, idempotency_key=None):
        idem = idempotency_key or str(uuid.uuid4())
        print(f"Connecting to {self.gateway} with {self.timeout}s timeout [Idem: {idem}]...")
        if amount <= 0:
            raise ValueError("Amount must be positive")
        return {"status": "SUCCESS", "gateway": self.gateway, "amount": amount, "idempotency_key": idem}
EOF

cat << 'EOF' > config/settings.yaml
app:
  name: "PaymentGatewayService"
  env: "production"
  timeout_seconds: 45
  retry_attempts: 5
  provider: "stripe"
EOF

git commit -am "feat(stripe): upgrade to Stripe v2 with idempotency"
```

### Step 4: Author Branch B (`feature-paypal-express`)
Switch back to `main` and introduce concurrent changes modifying the identical constructor and configuration:

```bash
git checkout main
git checkout -b feature-paypal-express

cat << 'EOF' > payment_service.py
class PaymentProcessor:
    def __init__(self, timeout=60, region="EU"):
        self.gateway = "PayPal_Express_v3"
        self.timeout = timeout
        self.region = region
        self.currency = "EUR"

    def charge(self, user_id, amount):
        print(f"Connecting to {self.gateway} in {self.region} with {self.timeout}s timeout...")
        if amount <= 0:
            raise ValueError("Amount must be positive")
        return {"status": "SUCCESS", "gateway": self.gateway, "amount": amount, "region": self.region}
EOF

cat << 'EOF' > config/settings.yaml
app:
  name: "PaymentGatewayService"
  env: "production"
  timeout_seconds: 60
  retry_attempts: 2
  provider: "paypal"
EOF

git commit -am "feat(paypal): introduce PayPal Express engine with EU regional routing"
```

### Step 5: Merge and Trigger the Conflict
Merge `feature-stripe-v2` into `feature-paypal-express`:

```bash
git merge feature-stripe-v2
```

Git halts with conflict errors:
```text
Auto-merging config/settings.yaml
CONFLICT (content): Merge conflict in config/settings.yaml
Auto-merging payment_service.py
CONFLICT (content): Merge conflict in payment_service.py
Automatic merge failed; fix conflicts and then commit the result.
```

### Step 6: Inspect and Resolve the Conflicts using `diff3`
Open `payment_service.py`. Notice the three sections:
- `<<<<<<< HEAD` (Our branch: PayPal)
- `||||||| merged common ancestors` (Original baseline)
- `=======`
- `>>>>>>> feature-stripe-v2` (Incoming branch: Stripe)

```text
<<<<<<< HEAD
class PaymentProcessor:
    def __init__(self, timeout=60, region="EU"):
        self.gateway = "PayPal_Express_v3"
        self.timeout = timeout
        self.region = region
        self.currency = "EUR"

    def charge(self, user_id, amount):
        print(f"Connecting to {self.gateway} in {self.region} with {self.timeout}s timeout...")
||||||| merged common ancestors
class PaymentProcessor:
    def __init__(self, timeout=30):
        self.gateway = "StandardGateway_v1"
        self.timeout = timeout
        self.currency = "USD"

    def charge(self, user_id, amount):
        print(f"Connecting to {self.gateway} with {self.timeout}s timeout...")
=======
import uuid

class PaymentProcessor:
    def __init__(self, timeout=45):
        self.gateway = "Stripe_API_v2"
        self.timeout = timeout
        self.currency = "USD"

    def charge(self, user_id, amount, idempotency_key=None):
        idem = idempotency_key or str(uuid.uuid4())
        print(f"Connecting to {self.gateway} with {self.timeout}s timeout [Idem: {idem}]...")
>>>>>>> feature-stripe-v2
```

Reconcile both features cleanly into a unified multi-provider architecture:

```python
# Write reconciled payment_service.py
cat << 'EOF' > payment_service.py
import uuid

class PaymentProcessor:
    def __init__(self, gateway="Stripe_API_v2", timeout=45, region="US"):
        self.gateway = gateway
        self.timeout = timeout
        self.region = region
        self.currency = "EUR" if region == "EU" else "USD"

    def charge(self, user_id, amount, idempotency_key=None):
        idem = idempotency_key or str(uuid.uuid4())
        print(f"Connecting to {self.gateway} [{self.region}] with {self.timeout}s timeout [Idem: {idem}]...")
        if amount <= 0:
            raise ValueError("Amount must be positive")
        return {
            "status": "SUCCESS",
            "gateway": self.gateway,
            "amount": amount,
            "region": self.region,
            "idempotency_key": idem
        }
EOF
```

For `config/settings.yaml`, choose the `--theirs` strategy for timeouts but preserve both providers:
```bash
cat << 'EOF' > config/settings.yaml
app:
  name: "PaymentGatewayService"
  env: "production"
  timeout_seconds: 45
  retry_attempts: 5
  supported_providers:
    - "stripe"
    - "paypal"
EOF
```

Stage the resolved files:
```bash
git add payment_service.py config/settings.yaml
git commit -m "merge: resolve payment processor collision supporting both Stripe and PayPal"
```

---

## 4. Verification & Testing

### Step 1: Run Test Suite
Verify that the reconciled payment processor passes tests:
```bash
python3 -m unittest tests/test_payment.py
```
Output:
```text
.
----------------------------------------------------------------------
Ran 1 test in 0.001s

OK
```

### Step 2: Verify `git rerere` Recorded Resolution
Inspect Git's internal rerere database:
```bash
# Check recorded resolution cache in .git/rr-cache
ls -la .git/rr-cache/
```

### Step 3: Test `rerere` Automated Playback
To verify `rerere` works, reset the merge and run it again:
```bash
git reset --hard HEAD~1
git merge feature-stripe-v2
```
Output:
```text
Auto-merging config/settings.yaml
CONFLICT (content): Merge conflict in config/settings.yaml
Auto-merging payment_service.py
CONFLICT (content): Merge conflict in payment_service.py
Resolved 'config/settings.yaml' using previous resolution.
Resolved 'payment_service.py' using previous resolution.
```
Notice Git outputs: **`Resolved '...' using previous resolution`**! All conflict markers were resolved automatically by `rerere`.

---

## 5. Production Readiness & Troubleshooting

### Production Readiness Checklist
- [x] Configure `merge.conflictStyle = diff3` or `zdiff3` across all developer machines.
- [x] Enable `rerere.enabled = true` globally to eliminate repetitive rebase conflicts.
- [x] Run full automated test suites immediately after resolving conflicts before committing.
- [x] Verify semantic correctness (code may merge cleanly textually but introduce runtime logic regressions).

### Troubleshooting Common Failures

| Issue | Root Cause | Remediation Command |
|:---|:---|:---|
| Aborting an active merge safely | Merge went wrong or wrong files touched | `git merge --abort` |
| Rerere recorded a faulty resolution | Developer committed broken code during resolution | `git rerere forget <file>` |
| Accidental commit of conflict markers | Developer staged file without editing `<<<<<<<` | Run pre-commit hook scanning for `^[<>=]{7}` |
| Binary file conflict (e.g., `.png`, `.db`) | Git cannot perform 3-way text diff on binary data | Use `git checkout --ours <file>` or `--theirs` |

---

[<- Back to Projects Overview](./README.md) | [Next Project: 02-rebase-and-recovery-lab.md ->](./02-rebase-and-recovery-lab.md)
