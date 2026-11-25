# Dynamic Programming: Policy Evaluation, Policy Iteration & Value Iteration

> **Track:** `05-ai/machine-learning` | **Module:** `07-reinforcement-learning` | **Status:** `Production Standard`  
> **Metadata:** `NumPy: >=1.26.0` | `SciPy: >=1.12.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Dynamic Programming (Policy Evaluation, Policy Iteration & Value Iteration) |
| **Family** | Model-Based Reinforcement Learning / Exact Tabular Planning / Markov Control |
| **Input Feature Types** | Fully known Markov Decision Process model $\mathcal{M} = \langle \mathcal{S}, \mathcal{A}, \mathcal{P}, \mathcal{R}, \gamma \rangle$ with explicit transition tensors and reward tables. |
| **Target Output** | Globally optimal deterministic policy $\pi^*(s) \in \mathcal{A}$ and optimal value function $V^*(s) \in \mathbb{R}^{|\mathcal{S}|}$. |
| **Optimization Target** | Exact fixed-point convergence under the Bellman Expectation Operator $T^\pi$ (Policy Evaluation) or Bellman Optimality Operator $T^*$ (Value Iteration). |
| **Key Metrics** | Bellman Error $\Delta = \max_s |V_{k+1}(s) - V_k(s)|$, Policy Stability (number of changed actions per sweep), Total State Backup Count. |
| **Key Hyperparameters** | Convergence threshold ($\theta \le 10^{-7}$), discount factor ($\gamma \in [0, 1)$), sweep order (synchronous, in-place Gauss-Seidel, or prioritized). |
| **Inference Mechanism** | Deterministic lookup table: $\pi^*(s)$ executed in $\mathcal{O}(1)$ time. |
| **Primary Failure Modes** | **Curse of Dimensionality**: Requires iterating over $|\mathcal{S}|$ and $|\mathcal{A}|$ (completely intractable for continuous state spaces); **Model Reliance**: Ineffective if true environment dynamics $P(s' \mid s, a)$ are unknown. |

---

## 2. Architecture & Intuition

### The Core Idea: Planning with a Perfect Model
Dynamic Programming (DP) refers to a collection of algorithms that solve Markov Decision Processes given a **perfect model of the environment's dynamics** ($P(s' \mid s, a)$ and $R(s, a)$).

DP resolves the complex problem of finding an optimal long-term policy by breaking it down into two interlocking processes:
1. **Policy Evaluation**: Calculating the true value $V^\pi(s)$ of the current policy $\pi$.
2. **Policy Improvement**: Making the policy greedy with respect to the updated values: $\pi'(s) = \arg\max_a Q^\pi(s, a)$.

```
                      Generalized Policy Iteration (GPI)
                      
                      Policy Evaluation: v -> v^pi
                        (Bellman Expectation)
                           ▲              │
                           │              │
                           │              ▼
                       Value V          Policy pi
                           ▲              │
                           │              │
                           └──────────────┘
                      Policy Improvement: pi -> greedy(v)
```

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $|\mathcal{S}|, |\mathcal{A}|$ | $\mathbb{N}^+$ | Cardinality of discrete state and action spaces |
| $V_k(s)$ | $\mathbb{R}$ | Value estimate of state $s$ at iteration $k$ |
| $\pi(s)$ | $\mathcal{S} \to \mathcal{A}$ | Deterministic policy mapping |
| $\mathcal{P}_{ss'}^a$ | $[0, 1]$ | Known probability of transitioning from $s$ to $s'$ under action $a$ |
| $\mathcal{R}(s, a)$ | $\mathbb{R}$ | Expected scalar immediate reward |
| $\theta > 0$ | $\mathbb{R}^+$ | Stopping tolerance threshold |

### 1. Iterative Policy Evaluation (Prediction)
To compute $V^\pi(s)$ for an arbitrary policy $\pi$, initialize $V_0(s) = 0$ for all $s$. Apply successive Bellman expectation backups:

$$V_{k+1}(s) = \sum_{a \in \mathcal{A}} \pi(a \mid s) \left[ \mathcal{R}(s, a) + \gamma \sum_{s' \in \mathcal{S}} \mathcal{P}_{ss'}^a V_k(s') \right]$$

Convergence to $V^\pi$ as $k \to \infty$ is guaranteed by the contraction property of $T^\pi$.

### 2. The Policy Improvement Theorem
Let $\pi$ and $\pi'$ be two deterministic policies such that for all $s \in \mathcal{S}$:

$$Q^\pi(s, \pi'(s)) \ge V^\pi(s)$$

**Theorem**: Then the policy $\pi'$ must be globally as good as, or better than, $\pi$:

$$V^{\pi'}(s) \ge V^\pi(s) \quad \forall s \in \mathcal{S}$$

#### Proof:
$$V^\pi(s) \le Q^\pi(s, \pi'(s)) = \mathbb{E}_{\pi'}[R_{t+1} + \gamma V^\pi(S_{t+1}) \mid S_t = s]$$
Expanding recursively:
$$\le \mathbb{E}_{\pi'}[R_{t+1} + \gamma Q^\pi(S_{t+1}, \pi'(S_{t+1})) \mid S_t = s]$$
$$\le \mathbb{E}_{\pi'}[R_{t+1} + \gamma R_{t+2} + \gamma^2 V^\pi(S_{t+2}) \mid S_t = s]$$
$$\dots \le \mathbb{E}_{\pi'}\left[ \sum_{k=0}^\infty \gamma^k R_{t+k+1} \,\Big|\, S_t = s \right] = V^{\pi'}(s)$$

### 3. Policy Iteration (Control)
Policy Iteration alternates full policy evaluation until convergence with a greedy policy improvement step:

$$\pi_{k+1}(s) = \arg\max_{a \in \mathcal{A}} \left[ \mathcal{R}(s, a) + \gamma \sum_{s' \in \mathcal{S}} \mathcal{P}_{ss'}^a V^{\pi_k}(s') \right]$$

Because a finite MDP has only $|\mathcal{A}|^{|\mathcal{S}|}$ distinct policies, Policy Iteration terminates in a finite number of iterations.

### 4. Value Iteration (Control)
Policy Iteration requires waiting for policy evaluation to converge completely before improving the policy. **Value Iteration** truncates policy evaluation to **exactly one sweep**:

$$V_{k+1}(s) = \max_{a \in \mathcal{A}} \left[ \mathcal{R}(s, a) + \gamma \sum_{s' \in \mathcal{S}} \mathcal{P}_{ss'}^a V_k(s') \right]$$

Value Iteration directly applies the Bellman Optimality Operator $T^*$, combining evaluation and improvement into a single unified step.

---

## 4. Computational Complexity

| Algorithm | Per-Iteration Complexity | Number of Iterations to Convergence | Overall Scalability |
| :--- | :--- | :--- | :--- |
| **Iterative Policy Evaluation** | $\mathcal{O}(|\mathcal{S}|^2 \cdot |\mathcal{A}|)$ | $\mathcal{O}\left( \frac{\ln(1/\theta)}{1 - \gamma} \right)$ | Feasible for $|\mathcal{S}| \le 10,000$ |
| **Policy Iteration** | $\mathcal{O}(K_{\text{eval}} \cdot |\mathcal{S}|^2 |\mathcal{A}| + |\mathcal{S}|^2 |\mathcal{A}|)$ | Very small ($5-20$ outer sweeps) | Excellent convergence rate |
| **Value Iteration** | $\mathcal{O}(|\mathcal{S}|^2 \cdot |\mathcal{A}|)$ | Moderate ($50-200$ sweeps) | Standard baseline for tabular DP |
| **Prioritized Sweeping** | $\mathcal{O}(|\mathcal{A}| \cdot \text{degree} \cdot \log |\mathcal{S}|)$ | Lowest total backups | Highly efficient for sparse transitions |

---

## 5. Inductive Bias & Statistical Assumptions

1. **Perfect Model Availability**: Assumes transition probabilities $\mathcal{P}_{ss'}^a$ and rewards $\mathcal{R}(s, a)$ are known with zero error.
2. **Stationarity**: Environment dynamics do not drift over time.
3. **Tabular Discreteness**: State and action spaces are discrete and enumerable.

---

## 6. From-Scratch Implementation (NumPy)

A complete, object-oriented implementation of Policy Iteration, Value Iteration, and Greedy Policy Extraction in pure NumPy:

```python
"""
Pure NumPy implementation of Dynamic Programming algorithms:
Policy Evaluation, Policy Iteration, and Value Iteration.
"""

from __future__ import annotations
import numpy as np

class DynamicProgrammingSolver:
    """
    Solves discrete MDPs using exact Dynamic Programming planning.
    
    Parameters
    ----------
    P : np.ndarray of shape (|S|, |A|, |S|)
        Transition probability tensor.
    R : np.ndarray of shape (|S|, |A|)
        Expected reward matrix.
    gamma : float
        Discount factor in [0, 1).
    """
    def __init__(self, P: np.ndarray, R: np.ndarray, gamma: float = 0.95):
        self.P = P
        self.R = R
        self.gamma = gamma
        self.n_states, self.n_actions, _ = P.shape

    def policy_evaluation(
        self,
        policy: np.ndarray,
        theta: float = 1e-8,
        max_iter: int = 5000
    ) -> np.ndarray:
        """
        Evaluates a deterministic policy pi of shape (|S|,) until delta < theta.
        """
        V = np.zeros(self.n_states, dtype=np.float64)
        for _ in range(max_iter):
            delta = 0.0
            # Vectorized evaluation for deterministic policy
            # Extract P and R for the chosen actions
            chosen_actions = policy
            P_pi = self.P[np.arange(self.n_states), chosen_actions, :] # (|S|, |S|)
            R_pi = self.R[np.arange(self.n_states), chosen_actions]    # (|S|,)
            
            V_new = R_pi + self.gamma * (P_pi @ V)
            delta = np.max(np.abs(V_new - V))
            V = V_new
            if delta < theta:
                break
        return V

    def policy_improvement(self, V: np.ndarray) -> tuple[np.ndarray, bool]:
        """
        Greedily improves policy given value function V.
        Returns: (new_policy, is_policy_stable).
        """
        # Q(s, a) = R(s, a) + gamma * sum_s' P(s, a, s') * V(s')
        expected_future = np.einsum("sad,d->sa", self.P, V)
        Q = self.R + self.gamma * expected_future

        # Greedy choice
        new_policy = np.argmax(Q, axis=1)
        return new_policy

    def policy_iteration(self, max_outer_iter: int = 100) -> tuple[np.ndarray, np.ndarray, int]:
        """
        Executes Policy Iteration until policy is completely stable.
        """
        # Initialize with arbitrary policy (all action 0)
        policy = np.zeros(self.n_states, dtype=np.int32)
        
        for iteration in range(max_outer_iter):
            # 1. Policy Evaluation
            V = self.policy_evaluation(policy)
            
            # 2. Policy Improvement
            new_policy = self.policy_improvement(V)
            
            # 3. Check stability
            if np.array_equal(new_policy, policy):
                return policy, V, iteration + 1
                
            policy = new_policy
            
        return policy, V, max_outer_iter

    def value_iteration(
        self,
        theta: float = 1e-8,
        max_iter: int = 1000
    ) -> tuple[np.ndarray, np.ndarray, int]:
        """
        Executes Value Iteration: combining evaluation and maximization into a single sweep.
        """
        V = np.zeros(self.n_states, dtype=np.float64)
        
        for iteration in range(max_iter):
            expected_future = np.einsum("sad,d->sa", self.P, V)
            Q = self.R + self.gamma * expected_future
            V_new = np.max(Q, axis=1)
            
            delta = np.max(np.abs(V_new - V))
            V = V_new
            if delta < theta:
                # Extract optimal policy from converged values
                optimal_policy = np.argmax(Q, axis=1)
                return optimal_policy, V, iteration + 1
                
        optimal_policy = self.policy_improvement(V)
        return optimal_policy, V, max_iter
```

---

## 7. Production Gridworld Benchmark

Demonstrating the solver on a stochastic 4x4 Gridworld with goal and obstacle hazards:

```python
"""
Production Benchmark: Solving a 4x4 Stochastic Gridworld via Policy & Value Iteration.
"""

import numpy as np

def create_4x4_gridworld_mdp(slip_prob: float = 0.15) -> tuple[np.ndarray, np.ndarray]:
    n_states = 16
    n_actions = 4 # 0=Up, 1=Right, 2=Down, 3=Left
    P = np.zeros((16, 4, 16))
    R = np.full((16, 4), -0.1)

    # Goal: State 15 (+10.0, terminal)
    # Pitfall: State 5 (-10.0, terminal)
    terminal_states = {5, 15}
    for t in terminal_states:
        P[t, :, t] = 1.0
        R[t, :] = 0.0

    deltas = [(-1, 0), (0, 1), (1, 0), (0, -1)]

    for s in range(16):
        if s in terminal_states:
            continue
        r, c = divmod(s, 4)
        for a in range(4):
            dr, dc = deltas[a]
            nr, nc = max(0, min(3, r + dr)), max(0, min(3, c + dc))
            target_s = nr * 4 + nc
            
            # Transition with slip
            P[s, a, target_s] += (1.0 - slip_prob)
            P[s, a, s] += slip_prob
            
            if target_s == 15:
                R[s, a] = 10.0
            elif target_s == 5:
                R[s, a] = -10.0

    return P, R

if __name__ == "__main__":
    P, R = create_4x4_gridworld_mdp()
    solver = DynamicProgrammingSolver(P, R, gamma=0.9)

    pi_policy, v_pi, pi_iters = solver.policy_iteration()
    vi_policy, v_vi, vi_iters = solver.value_iteration()

    print(f"Policy Iteration Converged in {pi_iters} outer iterations.")
    print(f"Value Iteration Converged in  {vi_iters} sweeps.")
    print("Policies Identical:", np.array_equal(pi_policy, vi_policy))
    np.testing.assert_allclose(v_pi, v_vi, atol=1e-5)
    print("Values match perfectly across algorithms!")
```

---

## 8. Model Persistence & SHA-256 Integrity

```python
import json
import hashlib
import joblib
from pathlib import Path
from typing import Any, Dict

def save_dp_policy(policy: np.ndarray, value_func: np.ndarray, out_path: Path, metadata: Dict[str, Any]) -> Path:
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    
    payload = {
        "policy": policy,
        "values": value_func
    }
    joblib.dump(payload, out_path, compress=3)
    
    sha = hashlib.sha256()
    with open(out_path, "rb") as f:
        while chunk := f.read(65536):
            sha.update(chunk)
            
    sidecar = {
        "file": out_path.name,
        "sha256": sha.hexdigest(),
        "n_states": len(policy),
        "metadata": metadata
    }
    with open(out_path.with_suffix(".json"), "w", encoding="utf-8") as f:
        json.dump(sidecar, f, indent=2)
        
    return out_path
```

---

## 9. Hyperparameter Tuning & Diagnostics

### Tuning Stopping Threshold $\theta$
The error bound for Value Iteration guarantees that stopping when $\Delta < \theta$ results in:

$$\|V_k - V^*\|_\infty \le \frac{2 \gamma \theta}{1 - \gamma}$$

To guarantee that final values are accurate to within $\epsilon = 0.01$ when $\gamma = 0.99$:
$$\theta = \frac{\epsilon (1 - \gamma)}{2 \gamma} = \frac{0.01 \times 0.01}{1.98} \approx 5.05 \times 10^{-5}$$

---

## 10. Residuals, Diagnostics & Validation

Track the **Bellman Residual Decay Curve** during Value Iteration: on log scale, $\log \Delta_k$ decays linearly with slope $\log \gamma$.

---

## 11. Uncertainty Estimation & Confidence Intervals

Because Dynamic Programming assumes the MDP model is exact, point value estimates are deterministic. However, if the transition model was empirically estimated from counts ($\hat{P}_{ss'}^a = \frac{N(s, a, s')}{N(s, a)}$), value uncertainty can be quantified using Dirichlet posterior sampling (Posterior Sampling for Reinforcement Learning - PSRL).

---

## 12. Failure Modes & Edge Cases

| Failure Mode | Root Cause | Behavioral Symptom | Detection Strategy | Mitigation |
| :--- | :--- | :--- | :--- | :--- |
| **Curse of Dimensionality** | Large state spaces ($|\mathcal{S}| > 100,000$) exceed memory and compute capacity. | Process runs out of RAM or takes weeks per sweep. | Monitor memory footprint of transition tensor. | Switch to **Sample-Based RL** (Q-Learning) or **Deep Function Approximation** (DQN). |
| **Model Inaccuracy Exploitation** | Empirical transition model has unvisited transitions set to zero. | Agent plans trajectories through non-existent shortcuts. | Compare simulated trajectory returns against real-world environment rollouts. | Robust MDPs or optimistic exploration bonuses. |
| **Oscillating Tied Policies** | Two actions have identically equal $Q$-values ($Q(s, a_1) = Q(s, a_2)$). | Policy iteration alternates endlessly between $a_1$ and $a_2$, never declaring stability. | Check for policy cycles while $\Delta V = 0$. | Break ties deterministically by selecting the lowest action index. |

---

## 13. Comparative Trade-offs & When to Use

| Dimension | Policy Iteration | Value Iteration | Q-Learning (Model-Free) | Monte Carlo Tree Search (MCTS) |
| :--- | :--- | :--- | :--- | :--- |
| **Model Required?** | **YES** (P and R) | **YES** (P and R) | NO (Trial and Error) | **YES** (Forward Simulator) |
| **Iteration Speed** | Slower per outer sweep | Fast per sweep | Ultra-fast sample step | Real-time tree search |
| **Number of Sweeps** | Very low ($< 15$) | Moderate ($50-200$) | Millions of steps | Continuous rollout budget |
| **State Space Limit** | Small ($|\mathcal{S}| \le 5,000$) | Small ($|\mathcal{S}| \le 20,000$) | Infinite (with Deep Nets) | Large (local expansion) |
| **Optimal Use Case** | Small discrete board games, inventory | Tabular robotics, routing | Complex simulators, games | Chess, Go, deterministic planning |

---

## 14. Production Engineering & Latency SLAs

- **Serving SLA**: Once computed offline, the optimal policy table $\pi^*(s)$ is stored in an in-memory hash table (Redis). Production lookup latency is sub-microsecond ($< 50\ \mu\text{s}$).

---

## 15. Interpretability & Feature Attribution

Dynamic programming policies are $100\%$ glass-box interpretable. For any state $s$, inspect the action-value vector $Q^*(s, :)$ to see the exact numeric expected return for every competing choice.

---

## 16. Hardware Acceleration, Scaling & Distributed Notes

- **GPU Acceleration via CuPy / PyTorch**: Tensor contractions $\sum_{s'} P(s, a, s') V(s')$ can be executed as a batched matrix multiplication (`torch.matmul`) across 100,000 states in $< 2\text{ ms}$ on an NVIDIA GPU.

---

## 17. Pytest Verification Suite

```python
"""
Pytest validation suite for Dynamic Programming algorithms.
Save as: tests/test_dp.py
Run: pytest -v tests/test_dp.py
"""

import pytest
import numpy as np

def test_policy_improvement_theorem():
    """Verify that improved policy achieves values >= original policy."""
    P = np.array([
        [[0.8, 0.2], [0.1, 0.9]],
        [[0.4, 0.6], [0.2, 0.8]]
    ])
    R = np.array([
        [1.0, 5.0],
        [2.0, 0.0]
    ])
    gamma = 0.9
    
    # Original suboptimal policy (action 0 everywhere)
    pi_orig = np.array([0, 0])
    
    # Evaluate pi_orig: (I - gamma * P_pi)^-1 * R_pi
    P_orig = P[np.arange(2), pi_orig, :]
    R_orig = R[np.arange(2), pi_orig]
    V_orig = np.linalg.solve(np.eye(2) - gamma * P_orig, R_orig)
    
    # Greedy improvement
    Q = R + gamma * (P @ V_orig)
    pi_improved = np.argmax(Q, axis=1)
    
    # Evaluate improved
    P_imp = P[np.arange(2), pi_improved, :]
    R_imp = R[np.arange(2), pi_improved]
    V_improved = np.linalg.solve(np.eye(2) - gamma * P_imp, R_imp)
    
    assert np.all(V_improved >= V_orig - 1e-9)
```

---

## 18. Cross-Track Integration

- **`09-sql`**: Storing precomputed optimal policy tables $\pi^*(s)$ in PostgreSQL with primary index on `state_id`.
- **`03-backend/fastapi/16-ai-and-ml-serving`**: Ultra-low-latency tabular policy lookup endpoint.

---

## 19. Real-World Enterprise Case Study

### Traffic Signal Timing Optimization
- **Context**: A metropolitan department of transportation models a synchronized corridor of 8 multi-lane intersections.
- **Problem**: Pre-timed green-light sequences cause severe gridlock during morning commute spikes.
- **Solution Architecture**:
  1. Built an MDP where state $s$ represents discretized vehicle queue lengths (binned into 4 tiers per approach).
  2. Estimated transition probabilities from inductive loop sensors.
  3. Solved using Value Iteration with discount $\gamma = 0.95$.
- **Outcome**: Reduced average vehicle delay by $28\%$, saving an estimated 140,000 commuter hours annually.

---

## 20. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Prove the Policy Improvement Theorem from first principles.</strong></summary>

**Answer:**  
Let $\pi$ and $\pi'$ be deterministic policies such that:
$$q_\pi(s, \pi'(s)) \ge v_\pi(s) \quad \forall s \in \mathcal{S}$$
We want to prove that $v_{\pi'}(s) \ge v_\pi(s)$ for all $s$.
Starting from the condition:
$$v_\pi(s) \le q_\pi(s, \pi'(s)) = \mathbb{E}[R_{t+1} + \gamma v_\pi(S_{t+1}) \mid S_t = s, A_t = \pi'(s)]$$
Because $v_\pi(S_{t+1}) \le q_\pi(S_{t+1}, \pi'(S_{t+1}))$, we substitute recursively:
$$\le \mathbb{E}_{\pi'}[R_{t+1} + \gamma q_\pi(S_{t+1}, \pi'(S_{t+1})) \mid S_t = s]$$
$$= \mathbb{E}_{\pi'}[R_{t+1} + \gamma R_{t+2} + \gamma^2 v_\pi(S_{t+2}) \mid S_t = s]$$
Repeating this expansion $k$ times:
$$\le \mathbb{E}_{\pi'}[R_{t+1} + \gamma R_{t+2} + \dots + \gamma^k v_\pi(S_{t+k}) \mid S_t = s]$$
Taking the limit as $k \to \infty$:
Since $\gamma < 1$ and rewards are bounded, $\lim_{k \to \infty} \gamma^k \mathbb{E}[v_\pi(S_{t+k})] = 0$.
The infinite sum is the exact definition of return under policy $\pi'$:
$$v_\pi(s) \le \mathbb{E}_{\pi'}\left[ \sum_{k=0}^{\infty} \gamma^k R_{t+k+1} \,\Big|\, S_t = s \right] = v_{\pi'}(s)$$
Thus, $v_{\pi'}(s) \ge v_\pi(s)$ for all states.
</details>

<details>
<summary><strong>Q2: Why does Value Iteration converge when initialized with arbitrary starting values $V_0$?</strong></summary>

**Answer:**  
Value Iteration is defined as the repeated application of the Bellman Optimality Operator: $\mathbf{v}_{k+1} = T^* \mathbf{v}_k$.
We previously proved that $T^*$ is a $\gamma$-contraction mapping on the complete metric space $(\mathbb{R}^{|\mathcal{S}|}, \|\cdot\|_\infty)$:
$$\|T^* \mathbf{u} - T^* \mathbf{v}\|_\infty \le \gamma \|\mathbf{u} - \mathbf{v}\|_\infty$$
By the Banach Fixed-Point Theorem, any contraction mapping on a complete metric space has a **unique fixed point** $\mathbf{v}^*$ (which satisfies $T^* \mathbf{v}^* = \mathbf{v}^*$), and the sequence $\mathbf{v}_k$ converges to $\mathbf{v}^*$ from any arbitrary starting point $\mathbf{v}_0$:
$$\|\mathbf{v}_k - \mathbf{v}^*\|_\infty \le \gamma^k \|\mathbf{v}_0 - \mathbf{v}^*\|_\infty$$
As $k \to \infty$, $\gamma^k \to 0$, forcing the distance to zero regardless of $\mathbf{v}_0$.
</details>

<details>
<summary><strong>Q3: Compare Policy Iteration and Value Iteration in terms of computational efficiency and convergence behavior.</strong></summary>

**Answer:**  
- **Policy Iteration**:
  - *Outer Loop*: Very few iterations (typically $3-15$ sweeps).
  - *Inner Loop*: Requires solving full policy evaluation (either via $\mathcal{O}(|\mathcal{S}|^3)$ matrix inversion or multiple iterative sweeps) at every step.
  - *Convergence*: Finite and exact. Best when $|\mathcal{S}|$ is small or policy space is small relative to state space.
- **Value Iteration**:
  - *Outer Loop*: More iterations ($50-200$ sweeps depending on $\gamma$).
  - *Inner Loop*: Exactly one backup sweep per iteration ($\mathcal{O}(|\mathcal{S}|^2 |\mathcal{A}|)$). Combines policy evaluation and improvement simultaneously.
  - *Convergence*: Asymptotic geometric convergence ($\mathcal{O}(\gamma^k)$). Faster computationally per iteration than Policy Iteration.
</details>

<details>
<summary><strong>Q4: What is Prioritized Sweeping and how does it optimize Dynamic Programming?</strong></summary>

**Answer:**  
Standard Value Iteration sweeps blindly through every state in arbitrary numerical order ($s = 0, 1, \dots, |\mathcal{S}|-1$), performing backups even on states whose values have barely changed.
**Prioritized Sweeping (Moore & Atkeson, 1993)**:
1. Maintains a priority queue of states ordered by their **Bellman Error**:
   $$p = |v(s) - \max_a Q(s, a)|$$
2. In each step, pops the state $s$ with the highest priority, updates its value: $v(s) \leftarrow \max_a Q(s, a)$.
3. Computes the impact of this change on all predecessor states $\bar{s}$ that can transition into $s$:
   $$p_{\bar{s}} = \max_a \mathcal{P}_{\bar{s} s}^a |v_{\text{new}}(s) - v_{\text{old}}(s)|$$
4. Inserts or updates predecessor states into the priority queue if $p_{\bar{s}} > \theta$.
This propagates value changes along high-impact causal pathways backwards from goals, reducing total state backups by up to $90\%$.
</details>

<details>
<summary><strong>Q5: What is Asynchronous Dynamic Programming and why is it useful?</strong></summary>

**Answer:**  
Standard DP algorithms are **synchronous**: they require storing two complete value vectors ($V_{\text{old}}$ and $V_{\text{new}}$) and computing backups across all $|\mathcal{S}|$ states before updating.
**Asynchronous DP**:
Backups states in arbitrary order, updating a single shared value vector in-place (Gauss-Seidel style):
$$V(s) \leftarrow \max_a \left[ \mathcal{R}(s, a) + \gamma \sum_{s'} \mathcal{P}_{ss'}^a V(s') \right]$$
- Values updated earlier in the sweep are immediately available to be used by subsequent states in the same sweep, speeding up propagation.
- Allows Real-Time Dynamic Programming (RTDP): updating only states that the agent actually visits during physical interaction, focusing computation entirely on relevant subgraphs of the state space.
- As long as every state is visited infinitely often, convergence to $V^*$ is mathematically guaranteed.
</details>

<details>
<summary><strong>Q6: How do you mathematically guarantee that a policy extracted from Value Iteration is within $\epsilon$ of optimal?</strong></summary>

**Answer:**  
Let $V_k$ be the value estimate when Value Iteration terminates. If the maximum Bellman update change across all states is bounded by:
$$\Delta = \|V_{k+1} - V_k\|_\infty < \theta$$
Then the distance between $V_k$ and the true optimal value function $V^*$ is bounded by:
$$\|V_k - V^*\|_\infty \le \frac{\gamma \Delta}{1 - \gamma}$$
Furthermore, if policy $\pi$ is extracted greedily from $V_k$, its true performance $V^\pi$ satisfies:
$$\|V^\pi - V^*\|_\infty \le \frac{2 \gamma \Delta}{1 - \gamma} < \frac{2 \gamma \theta}{1 - \gamma}$$
Setting $\theta = \frac{\epsilon (1 - \gamma)}{2 \gamma}$ guarantees that the extracted policy is $\epsilon$-optimal: $\|V^\pi - V^*\|_\infty < \epsilon$.
</details>

<details>
<summary><strong>Q7: What is the "Deadly Triad" and why does it NOT apply to tabular Dynamic Programming?</strong></summary>

**Answer:**  
The Deadly Triad (Sutton & Barto) states that Reinforcement Learning algorithms can diverge to infinity when combining three elements simultaneously:
1. **Function Approximation** (e.g. Deep Neural Networks, linear models).
2. **Bootstrapping** (updating value estimates using other estimated values, e.g. Bellman targets).
3. **Off-Policy Training** (evaluating a policy different from the behavior policy).
**Why DP is exempt**:
Tabular Dynamic Programming uses **tabular representations** (no function approximation). In tabular space, the Bellman operator is an exact non-expansion in $\ell_\infty$ norm. Contraction mapping theorems guarantee stability and uniqueness without any possibility of divergence.
</details>

<details>
<summary><strong>Q8: Can Dynamic Programming be applied to environments with unknown transition probabilities?</strong></summary>

**Answer:**  
Pure Dynamic Programming **cannot** be applied directly to unknown environments because evaluating the Bellman expectation $\sum_{s'} \mathcal{P}_{ss'}^a V(s')$ requires explicit transition probabilities $\mathcal{P}_{ss'}^a$.
However, it can be combined with learning in two ways:
1. **Model-Based RL (Certainty Equivalence / Dyna)**: The agent interacts with the environment, records transition frequencies $N(s, a, s')$, estimates empirical dynamics $\hat{\mathcal{P}}_{ss'}^a = \frac{N(s, a, s')}{N(s, a)}$, and then runs Dynamic Programming on the learned model.
2. **Sample-Based Approximations**: Replace expected Bellman backups with sample averages drawn from trial-and-error interaction, which leads directly to **Monte Carlo** and **Temporal Difference** learning.
</details>

---

## 21. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): One Iteration of Policy Evaluation in NumPy</strong></summary>

**Task**: Write a pure NumPy function executing a single synchronous Bellman expectation backup for a 3-state Markov chain.

**Solution**:
```python
import numpy as np

P_pi = np.array([
    [0.7, 0.2, 0.1],
    [0.0, 0.9, 0.1],
    [0.0, 0.0, 1.0]
])
r_pi = np.array([1.0, 2.0, 0.0])
gamma = 0.9

V_old = np.array([0.0, 0.0, 0.0])

# V_new = r_pi + gamma * P_pi @ V_old
V_new = r_pi + gamma * (P_pi @ V_old)
print("Values after 1 backup:", V_new)
assert np.array_equal(V_new, r_pi)
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Greedy Policy Extraction from Q-Table</strong></summary>

**Task**: Given a $4 \times 3$ table of action-values $Q(s, a)$, extract the greedy deterministic policy and break ties by choosing the smallest action index.

**Solution**:
```python
import numpy as np

Q = np.array([
    [1.5, 4.2, 0.8], # Max is A1
    [5.0, 5.0, 2.1], # Tie between A0 and A1 -> picks A0
    [0.1, -1.0, 3.4],# Max is A2
    [2.0, 2.0, 2.0]  # Triple tie -> picks A0
])

# argmax returns the first occurrence of the maximum value
greedy_policy = np.argmax(Q, axis=1)
print("Extracted greedy policy:", greedy_policy)
assert np.array_equal(greedy_policy, [1, 0, 2, 0])
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Stopping Criterion Calculator for Value Iteration</strong></summary>

**Task**: Implement a function that calculates the required stopping threshold $\theta$ given a target sub-optimality bound $\epsilon = 0.05$ and discount $\gamma = 0.95$.

**Solution**:
```python
def compute_dp_stopping_threshold(epsilon: float = 0.05, gamma: float = 0.95) -> float:
    # theta = epsilon * (1 - gamma) / (2 * gamma)
    return float(epsilon * (1.0 - gamma) / (2.0 * gamma))

theta = compute_dp_stopping_threshold(0.05, 0.95)
print(f"Required stopping threshold theta: {theta:.6f}")
assert theta > 0 and theta < 0.002
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Truncated Policy Iteration (Modified Policy Iteration)</strong></summary>

**Task**: Implement Modified Policy Iteration, which executes only $M=3$ policy evaluation sweeps before performing policy improvement.

**Solution**:
```python
import numpy as np

def modified_policy_iteration(P, R, gamma=0.9, m_eval_steps=3, max_iter=50):
    n_states, n_actions, _ = P.shape
    V = np.zeros(n_states)
    policy = np.zeros(n_states, dtype=int)
    
    for it in range(max_iter):
        # 1. Truncated evaluation (M steps)
        for _ in range(m_eval_steps):
            P_pi = P[np.arange(n_states), policy, :]
            R_pi = R[np.arange(n_states), policy]
            V = R_pi + gamma * (P_pi @ V)
            
        # 2. Greedy improvement
        Q = R + gamma * np.einsum("sad,d->sa", P, V)
        new_policy = np.argmax(Q, axis=1)
        
        if np.array_equal(new_policy, policy):
            print(f"Modified Policy Iteration converged in {it+1} rounds.")
            return new_policy, V
        policy = new_policy
        
    return policy, V

# Verification
P_sim = np.array([[[1.0, 0.0], [0.0, 1.0]], [[0.5, 0.5], [0.1, 0.9]]])
R_sim = np.array([[1.0, 5.0], [0.0, 2.0]])
pol, val = modified_policy_iteration(P_sim, R_sim)
print("Optimal policy:", pol)
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Prioritized Sweeping Implementation</strong></summary>

**Task**: Implement a priority-queue driven Value Iteration solver that updates states in descending order of Bellman error.

**Solution**:
```python
import numpy as np
import heapq

def prioritized_value_iteration(P, R, gamma=0.9, theta=1e-5):
    n_states, n_actions, _ = P.shape
    V = np.zeros(n_states)
    
    # Priority queue stores (-priority, state) for max-heap behavior
    pq = []
    
    # 1. Initialize queue with initial Bellman errors
    for s in range(n_states):
        Q_s = R[s] + gamma * np.einsum("ad,d->a", P[s], V)
        p = abs(np.max(Q_s) - V[s])
        if p > theta:
            heapq.heappush(pq, (-p, s))
            
    updates = 0
    while pq:
        neg_p, s = heapq.heappop(pq)
        
        # Update state value
        Q_s = R[s] + gamma * np.einsum("ad,d->a", P[s], V)
        V[s] = np.max(Q_s)
        updates += 1
        
        # Check all predecessor states that can transition into s
        for s_pred in range(n_states):
            for a_pred in range(n_actions):
                if P[s_pred, a_pred, s] > 0:
                    Q_pred = R[s_pred] + gamma * np.einsum("ad,d->a", P[s_pred], V)
                    err = abs(np.max(Q_pred) - V[s_pred])
                    if err > theta:
                        heapq.heappush(pq, (-err, s_pred))
                        
    return V, updates

# Test on 4-state chain
P_chain = np.zeros((4, 2, 4))
R_chain = np.zeros((4, 2))
for i in range(3):
    P_chain[i, 0, i+1] = 1.0 # Move right
    P_chain[i, 1, max(0, i-1)] = 1.0 # Move left
P_chain[3, :, 3] = 1.0 # Terminal
R_chain[2, 0] = 10.0 # Reward on entering 3

V_prio, num_backups = prioritized_value_iteration(P_chain, R_chain)
print("Prioritized Sweeping Values:", np.round(V_prio, 3))
print(f"Total priority backups: {num_backups}")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Gridworld Policy Iteration vs Value Iteration Profiler</strong></summary>

**Task**: Write an automated benchmark comparing execution wall-clock time and total operations between Policy Iteration and Value Iteration across grid dimensions $N \in \{3, 5, 8\}$.

**Solution**:
```python
import numpy as np
import time

def benchmark_dp_solvers(grid_size=4):
    n_states = grid_size * grid_size
    n_actions = 4
    rng = np.random.default_rng(42)
    
    P = rng.uniform(0, 1, (n_states, n_actions, n_states))
    P /= np.sum(P, axis=2, keepdims=True)
    R = rng.uniform(-1, 1, (n_states, n_actions))
    
    # 1. Benchmark Value Iteration
    t0 = time.perf_counter()
    V_vi = np.zeros(n_states)
    vi_sweeps = 0
    while True:
        Q = R + 0.9 * np.einsum("sad,d->sa", P, V_vi)
        V_new = np.max(Q, axis=1)
        vi_sweeps += 1
        if np.max(np.abs(V_new - V_vi)) < 1e-6 or vi_sweeps > 500:
            break
        V_vi = V_new
    t_vi = (time.perf_counter() - t0) * 1000
    
    print(f"Grid {grid_size}x{grid_size} ({n_states} states):")
    print(f"  Value Iteration: {t_vi:.2f} ms ({vi_sweeps} sweeps)")

benchmark_dp_solvers(grid_size=4)
benchmark_dp_solvers(grid_size=6)
```
</details>
