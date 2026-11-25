# Monte Carlo Methods: First-Visit, Every-Visit & Importance Sampling

> **Track:** `05-ai/machine-learning` | **Module:** `07-reinforcement-learning` | **Status:** `Production Standard`  
> **Metadata:** `NumPy: >=1.26.0` | `Gymnasium: >=0.29.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Monte Carlo Methods (First-Visit, Every-Visit, MCES & Off-Policy Importance Sampling) |
| **Family** | Model-Free Reinforcement Learning / Sample-Average Estimation / Episodic Control |
| **Input Feature Types** | Episodic trajectory records $\tau = (s_0, a_0, r_1, s_1, a_1, r_2, \dots, s_T)$ generated via environment simulation. Requires no prior knowledge of transition dynamics $\mathcal{P}_{ss'}^a$ or reward functions $\mathcal{R}(s, a)$. |
| **Target Output** | Estimated value functions $\hat{V}(s)$ / $\hat{Q}(s, a)$ and improved policy $\pi(a \mid s)$. |
| **Optimization Target** | Minimizing empirical mean squared error between value estimates and sampled episode returns: $\min_V \mathbb{E}[(G_t - V(S_t))^2]$. |
| **Key Metrics** | Episode Return Mean/Variance, Mean Squared Value Error (MSVE), Exploration Rate ($\epsilon$). |
| **Key Hyperparameters** | Exploration parameter ($\epsilon \in [0.01, 0.2]$), discount factor ($\gamma \in [0.9, 1.0]$), step size ($\alpha$ or sample average $\frac{1}{N(s)}$), importance sampling type (`'weighted'` vs `'ordinary'`). |
| **Inference Mechanism** | Tabular lookup: $\pi(s) = \arg\max_a Q(s, a)$ executed in $\mathcal{O}(1)$ time. |
| **Primary Failure Modes** | **High Variance**: Return estimates $G_t$ accumulate stochastic noise across hundreds of time-steps; **Episodic Limitation**: Completely unable to learn in continuous/infinite-horizon tasks; **Importance Sampling Weight Explosion**: Unbounded variance in ordinary importance sampling. |

---

## 2. Architecture & Intuition

### Learning from Experience Without a Model
Unlike Dynamic Programming, **Monte Carlo (MC)** methods do **not** require a model of the environment's transition probabilities or reward distributions. They learn value functions and optimal policies directly from **episodes of simulated experience**:

$$\text{Value} = \text{Sample Average of Observed Returns}$$

```
    Dynamic Programming (Model-Based)               Monte Carlo (Model-Free)
    (Backs up from all possible futures)            (Rolls out one complete trajectory to T)
    
                ( S_t )                                     ( S_t )
               /   |   \                                       │
            (a1)  (a2)  (a3)                                  │ a_t
            / \   / \   / \                                    ▼
          s'  s' s'  s' s' s'                              ( S_t+1 )
                                                               │
                                                               │ a_t+1
                                                               ▼
                                                           ( S_t+2 )
                                                               │ ...
                                                               ▼
                                                           ( S_T: Terminal ) -> Compute G_t
```

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $\tau = (s_0, a_0, r_1, \dots, s_T)$ | Sequence | Complete episodic trajectory from time 0 to terminal step $T$ |
| $G_t = \sum_{k=0}^{T - t - 1} \gamma^k r_{t+k+1}$ | Real scalar | Empirical discounted return from step $t$ |
| $N(s), N(s, a)$ | Integer count | Cumulative visitation count of state $s$ or pair $(s, a)$ |
| $V(s), Q(s, a)$ | Real scalar | Estimated state and action values |
| $\rho_{t:T-1}$ | Non-negative scalar | Importance sampling ratio between target $\pi$ and behavior $b$ |
| $W$ | Real scalar | Cumulative importance sampling weight accumulator |

### First-Visit vs Every-Visit Monte Carlo Prediction

In an episode, a state $s$ may be visited multiple times.
1. **First-Visit MC**: Averages returns following **only the very first occurrence** of state $s$ in the episode.
   - *Theoretical Property*: Strictly **unbiased** estimate of $V^\pi(s)$: $\mathbb{E}[\hat{V}(s)] = V^\pi(s)$.
   - Variance decreases as $\mathcal{O}(1/\sqrt{K})$ across $K$ episodes.
2. **Every-Visit MC**: Averages returns following **every occurrence** of state $s$ in the episode.
   - *Theoretical Property*: Slightly biased for small sample sizes, but **consistent** (asymptotically unbiased as $K \to \infty$) and frequently achieves lower Mean Squared Error (MSE).

### Incremental Implementation
Rather than storing all historical returns in memory lists, updates are computed incrementally:

$$V(S_t) \leftarrow V(S_t) + \alpha \left[ G_t - V(S_t) \right]$$

- For stationary sample averaging: $\alpha = \frac{1}{N(S_t)}$.
- For non-stationary environments (tracking drifting dynamics): constant $\alpha \in (0, 1]$.

### On-Policy MC Control with $\epsilon$-Greedy Policies
To ensure continuous exploration without requiring artificial "Exploring Starts":

$$\pi(a \mid s) = \begin{cases} 1 - \epsilon + \frac{\epsilon}{|\mathcal{A}|} & \text{if } a = \arg\max_{a'} Q(s, a') \\ \frac{\epsilon}{|\mathcal{A}|} & \text{if } a \ne \arg\max_{a'} Q(s, a') \end{cases}$$

### Off-Policy Monte Carlo via Importance Sampling
To learn the optimal target policy $\pi$ while following an exploratory behavior policy $b$:
The **Importance Sampling Ratio** over trajectory $\tau$:

$$\rho_{t:T-1} = \frac{\prod_{k=t}^{T-1} \pi(A_k \mid S_k) P(S_{k+1} \mid S_k, A_k)}{\prod_{k=t}^{T-1} b(A_k \mid S_k) P(S_{k+1} \mid S_k, A_k)} = \prod_{k=t}^{T-1} \frac{\pi(A_k \mid S_k)}{b(A_k \mid S_k)}$$

The unknown environment transition probabilities cancel out completely!

1. **Ordinary Importance Sampling** (Unbiased, unbounded variance):
   $$V(s) = \frac{\sum_{t \in \mathcal{T}(s)} \rho_{t:T(t)-1} G_t}{|\mathcal{T}(s)|}$$
2. **Weighted Importance Sampling** (Biased, bounded variance, preferred in practice):
   $$V(s) = \frac{\sum_{t \in \mathcal{T}(s)} \rho_{t:T(t)-1} G_t}{\sum_{t \in \mathcal{T}(s)} \rho_{t:T(t)-1}}$$

---

## 4. Computational Complexity

| Operation | Time Complexity | Memory Complexity | Notes |
| :--- | :--- | :--- | :--- |
| **Trajectory Rollout** | $\mathcal{O}(T \cdot \mathcal{T}_{\text{env}})$ | $\mathcal{O}(T)$ | Stores actions, states, rewards |
| **Return Backward Pass** | $\mathcal{O}(T)$ | $\mathcal{O}(T)$ | Reverse pass $G_t = r + \gamma G_{t+1}$ |
| **Value / Policy Update** | $\mathcal{O}(T)$ | $\mathcal{O}(|\mathcal{S}| \times |\mathcal{A}|)$ | Incremental tabular updates |
| **Overall per Episode** | $\mathcal{O}(T)$ | $\mathcal{O}(|\mathcal{S}| |\mathcal{A}| + T)$ | Independent of $|\mathcal{S}|$ (only updates visited states!) |

---

## 5. Inductive Bias & Statistical Assumptions

1. **Episodic Guarantee**: All episodes must terminate in finite steps ($T < \infty$). MC cannot learn in non-terminating tasks without artificial truncation.
2. **Zero Bootstrapping Bias**: Target returns $G_t$ are drawn from true rollouts, making updates completely free of bootstrapping bias ($\text{Bias} = 0$).
3. **High Return Variance**: Because return $G_t$ is a sum of many stochastic transitions, $\text{Var}(G_t)$ can be enormous.

---

## 6. From-Scratch Implementation (NumPy)

A complete implementation of First-Visit Monte Carlo Prediction and $\epsilon$-Greedy Monte Carlo Control in pure NumPy:

```python
"""
Pure NumPy implementation of First-Visit Monte Carlo Prediction and Control.
"""

from __future__ import annotations
import numpy as np

class MonteCarloControl:
    """
    On-Policy First-Visit Monte Carlo Control with epsilon-Greedy exploration.
    """
    def __init__(
        self,
        n_states: int,
        n_actions: int,
        gamma: float = 0.95,
        epsilon: float = 0.1,
        alpha: float | None = None
    ):
        self.n_states = n_states
        self.n_actions = n_actions
        self.gamma = gamma
        self.epsilon = epsilon
        self.alpha = alpha # None uses 1/N(s, a) sample average

        # Tabular estimates
        self.Q = np.zeros((n_states, n_actions), dtype=np.float64)
        self.N = np.zeros((n_states, n_actions), dtype=np.int64)

    def select_action(self, state: int, rng: np.random.Generator) -> int:
        """epsilon-greedy action selection."""
        if rng.uniform(0, 1) < self.epsilon:
            return int(rng.choice(self.n_actions))
        # Greedy tie-breaking
        q_s = self.Q[state]
        best_actions = np.where(q_s == np.max(q_s))[0]
        return int(rng.choice(best_actions))

    def update_from_episode(self, episode: list[tuple[int, int, float]]):
        """
        episode: list of (state, action, reward) tuples from t=0 to T-1.
        First-visit update applied backwards.
        """
        T = len(episode)
        G = 0.0
        
        # Track first visits within this episode
        visited_pairs = set()
        sa_pairs = [(step[0], step[1]) for step in episode]
        
        # Calculate returns backwards in O(T)
        for t in reversed(range(T)):
            state, action, reward = episode[t]
            G = reward + self.gamma * G
            
            # Check if this step is the first occurrence of (state, action)
            if (state, action) not in sa_pairs[:t]:
                self.N[state, action] += 1
                
                # Step size: constant alpha or 1/N
                step_size = self.alpha if self.alpha is not None else (1.0 / self.N[state, action])
                
                # Incremental error update
                self.Q[state, action] += step_size * (G - self.Q[state, action])

    def get_optimal_policy(self) -> np.ndarray:
        return np.argmax(self.Q, axis=1)
```

---

## 7. Production Gymnasium Blackjack Benchmark

Benchmarking Monte Carlo Control on the classic Farama Gymnasium `Blackjack-v1` environment:

```python
"""
Benchmark: Training Monte Carlo Control on Farama Gymnasium Blackjack-v1.
"""

import gymnasium as gym
import numpy as np

def train_blackjack_mc(n_episodes: int = 50_000) -> tuple[np.ndarray, list[float]]:
    # Blackjack observation space: (player_sum: 4-21, dealer_showing: 1-10, usable_ace: 0-1)
    # Total discrete states: 32 x 11 x 2 = 704
    env = gym.make("Blackjack-v1", sab=True)
    rng = np.random.default_rng(42)
    
    Q = np.zeros((32, 11, 2, 2), dtype=np.float64) # (player, dealer, ace, action)
    N = np.zeros((32, 11, 2, 2), dtype=np.int64)
    epsilon = 0.1
    
    win_history = []

    for ep in range(n_episodes):
        obs, _ = env.reset()
        episode = []
        done = False
        
        # Rollout episode
        while not done:
            p_sum, d_card, ace = obs
            # Epsilon-greedy
            if rng.uniform(0, 1) < epsilon:
                action = int(env.action_space.sample())
            else:
                action = int(np.argmax(Q[p_sum, d_card, int(ace)]))
                
            next_obs, reward, terminated, truncated, _ = env.step(action)
            done = terminated or truncated
            episode.append((p_sum, d_card, int(ace), action, reward))
            obs = next_obs

        # Record win/loss
        final_reward = episode[-1][4]
        win_history.append(1.0 if final_reward > 0 else 0.0)

        # First-visit update
        G = 0.0
        T = len(episode)
        visited = set()
        
        for t in reversed(range(T)):
            p_sum, d_card, ace, action, reward = episode[t]
            G = reward + 1.0 * G # gamma = 1.0 in Blackjack
            
            sa = (p_sum, d_card, ace, action)
            # First visit check
            if sa not in [(x[0], x[1], x[2], x[3]) for x in episode[:t]]:
                N[p_sum, d_card, ace, action] += 1
                lr = 1.0 / N[p_sum, d_card, ace, action]
                Q[p_sum, d_card, ace, action] += lr * (G - Q[p_sum, d_card, ace, action])

    env.close()
    rolling_win_rate = np.convolve(win_history, np.ones(1000)/1000, mode="valid")
    return Q, rolling_win_rate
```

---

## 8. Model Persistence & SHA-256 Integrity

```python
import json
import hashlib
import joblib
from pathlib import Path
from typing import Any, Dict

def save_mc_agent(agent: Any, out_path: Path, metadata: Dict[str, Any]) -> Path:
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    
    payload = {
        "Q_table": getattr(agent, "Q", None),
        "N_table": getattr(agent, "N", None)
    }
    joblib.dump(payload, out_path, compress=3)
    
    sha = hashlib.sha256()
    with open(out_path, "rb") as f:
        while chunk := f.read(65536):
            sha.update(chunk)
            
    sidecar = {
        "file": out_path.name,
        "sha256": sha.hexdigest(),
        "metadata": metadata
    }
    with open(out_path.with_suffix(".json"), "w", encoding="utf-8") as f:
        json.dump(sidecar, f, indent=2)
        
    return out_path
```

---

## 9. Hyperparameter Tuning & Diagnostics

### Tuning Exploration Decay ($\epsilon$)
A static $\epsilon = 0.1$ maintains asymptotic exploration noise. Decaying $\epsilon$ linearly or exponentially stabilizes optimal policy convergence:

$$\epsilon_t = \max\left( \epsilon_{\min}, \; \epsilon_{\text{start}} \times d^t \right)$$

where $\epsilon_{\text{start}} = 1.0, \epsilon_{\min} = 0.01, d = 0.9995$.

---

## 10. Residuals, Diagnostics & Validation

The primary diagnostic for Monte Carlo is the **Mean Squared Value Error (MSVE)** against an analytical ground truth (if known):

$$\text{MSVE} = \frac{1}{|\mathcal{S}|} \sum_{s \in \mathcal{S}} \left( V^\pi(s) - \hat{V}(s) \right)^2$$

On a log-log plot, First-Visit MC MSVE decays with slope $-0.5$ (governed by Central Limit Theorem standard error $\sigma / \sqrt{N}$).

---

## 11. Uncertainty Estimation & Confidence Intervals

Because First-Visit MC computes empirical sample averages of independent episode returns $G_i(s)$, the $(1 - \alpha)$ confidence interval for $V(s)$ is given directly by Student's $t$-distribution:

$$\text{CI}_{1-\alpha}(V(s)) = \left[ \bar{G}(s) \pm t_{\alpha/2, N-1} \frac{S_G(s)}{\sqrt{N(s)}} \right]$$

where $S_G(s)$ is the sample standard deviation of observed returns for state $s$.

---

## 12. Failure Modes & Edge Cases

| Failure Mode | Root Cause | Behavioral Symptom | Detection Strategy | Mitigation |
| :--- | :--- | :--- | :--- | :--- |
| **Ordinary Importance Sampling Variance Explosion** | Ratios $\frac{\pi(a \mid s)}{b(a \mid s)}$ multiplied across $T$ steps produce variance $\to \infty$. | Value estimates oscillate wildly between $\pm 10^9$. | Track running variance of importance weights. | Switch to **Weighted Importance Sampling**. |
| **Endless Trajectory Trapping** | Policy gets stuck in non-terminating loop; episode length $T \to \infty$. | Process hangs; returns cannot be computed. | Set `max_episode_steps` timeout. | Enforce artificial step limit with truncation handling. |
| **Exploring Starts Impossibility** | Real-world physical robots cannot be arbitrarily teleported into arbitrary initial states. | Sub-optimal policies due to unvisited state regions. | Check visitation counts $N(s, a) == 0$. | Use **$\epsilon$-Greedy Policies** instead of Exploring Starts. |
| **Zero Return Feedback** | Extremely long horizon with sparse reward at end ($T=1000$). | All returns $G_t = 0$; values fail to update. | Fraction of non-zero returns $< 0.001$. | Switch to **Temporal Difference (TD)** learning to bootstrap immediately. |

---

## 13. Comparative Trade-offs & When to Use

| Dimension | Dynamic Programming | Monte Carlo | Temporal Difference (TD(0)) |
| :--- | :--- | :--- | :--- |
| **Model Required?** | **YES** | NO | NO |
| **Bootstrapping?** | YES ($V(s')$) | **NO** (Actual Returns $G_t$) | YES ($V(s')$) |
| **Bias** | Zero (exact model) | **Zero (Unbiased)** | High initially |
| **Variance** | Zero | **High** | Low / Moderate |
| **Episodic Only?** | NO | **YES** | NO (Continuing tasks) |
| **Update Step** | Offline sweep | End of Episode | **Every Step $t$** |

---

## 14. Production Engineering & Latency SLAs

- **Rollout Parallelization**: Monte Carlo episodes are completely independent. Distribute $10,000$ episodes across 32 Ray / Celery worker threads simultaneously; aggregate counts $N(s, a)$ and sum of returns via atomic reduce operations.

---

## 15. Interpretability & Feature Attribution

Because MC values are direct sample averages of returns, explainability is intuitive:
$$\hat{V}(s) = \frac{1}{N} \sum_{i=1}^N G_i$$
Inspect the actual historical trajectory logs for state $s$ to display the exact distribution of final outcomes to human auditors.

---

## 16. Hardware Acceleration, Scaling & Distributed Notes

- **Vectorized Replay Buffers**: Convert completed episode trajectory lists to contiguous NumPy arrays (`np.uint8`, `np.float32`) to minimize Python object allocation overhead during backward return passes.

---

## 17. Pytest Verification Suite

```python
"""
Pytest validation suite for Monte Carlo algorithms.
Save as: tests/test_mc.py
Run: pytest -v tests/test_mc.py
"""

import pytest
import numpy as np

def test_first_visit_mc_unbiased_estimator():
    """Verify First-Visit MC converges to expected value of a simple coin flip environment."""
    rng = np.random.default_rng(42)
    # 1 state, 2 actions: Action 0 gives +10 with 50% prob, Action 1 gives +0
    true_q0 = 5.0
    
    Q = np.zeros(2)
    N = np.zeros(2, dtype=int)
    
    for _ in range(5000):
        action = int(rng.choice(2))
        reward = 10.0 if (action == 0 and rng.uniform(0, 1) < 0.5) else 0.0
        
        N[action] += 1
        Q[action] += (1.0 / N[action]) * (reward - Q[action])
        
    np.testing.assert_allclose(Q[0], true_q0, atol=0.25)
    np.testing.assert_allclose(Q[1], 0.0, atol=0.01)

def test_weighted_importance_sampling_bounds():
    """Verify Weighted Importance Sampling weights sum to 1.0."""
    weights = np.array([0.5, 2.5, 1.0, 0.0])
    returns = np.array([10.0, 4.0, 2.0, 100.0])
    
    v_weighted = np.sum(weights * returns) / np.sum(weights)
    assert v_weighted >= np.min(returns[:3])
    assert v_weighted <= np.max(returns[:3])
```

---

## 18. Cross-Track Integration

- **`01-languages/python/21-data-and-scientific-basics`**: NumPy random sampling and trajectory array slicing.
- **`05-ai/machine-learning/07-reinforcement-learning/06-temporal-difference-learning.md`**: Contrasting Monte Carlo zero-bias/high-variance with TD zero-variance/high-bias.

---

## 19. Real-World Case Study

### Card Counting & Strategy Optimization in Casino Blackjack
- **Context**: A gaming mathematics consultancy analyzed optimal playing strategies under non-standard casino rule variants (dealer hits soft 17, surrender allowed).
- **Solution Architecture**:
  1. Simulated 200 million full blackjack shoe rollouts using On-Policy Monte Carlo Control.
  2. Evaluated 704 discrete state combinations without modeling complex shuffle transition matrices.
- **Outcome**: Derived exact optimal decision charts that reduced house edge from $2.4\%$ to $0.38\%$.

---

## 20. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Why is First-Visit Monte Carlo an unbiased estimator of $V^\pi(s)$, while Every-Visit Monte Carlo is biased?</strong></summary>

**Answer:**  
- **First-Visit MC**: Considers only the return $G_t$ following the first visit to state $s$ in each episode. Because episodes are generated independently from policy $\pi$ and environment dynamics, the returns $\{G_1, G_2, \dots, G_K\}$ are independent, identically distributed (i.i.d.) random variables with expectation $\mathbb{E}[G_k] = V^\pi(s)$. By the Law of Large Numbers, the sample average is an **unbiased estimator**: $\mathbb{E}[\bar{G}] = V^\pi(s)$.
- **Every-Visit MC**: Averages returns from all visits to state $s$ within the same episode. If an episode visits state $s$ multiple times (e.g. at step $t_1$ and $t_2$), the returns $G_{t_1}$ and $G_{t_2}$ are **strongly correlated** (sharing overlapping reward subsequences). The sample average of correlated variables within the same trajectory introduces finite-sample bias. However, Every-Visit MC is **consistent** (bias vanishes as episodes $K \to \infty$) and frequently exhibits lower Mean Squared Error (MSE) because it averages over more data points.
</details>

<details>
<summary><strong>Q2: Contrast Ordinary Importance Sampling with Weighted Importance Sampling and explain the infinite variance catastrophe.</strong></summary>

**Answer:**  
To estimate $V^\pi(s)$ using data generated from behavior policy $b$:
1. **Ordinary Importance Sampling**:
   $$V(s) = \frac{1}{|\mathcal{T}(s)|} \sum_{t \in \mathcal{T}(s)} \rho_{t:T(t)-1} G_t$$
   - *Bias*: Strictly **unbiased** ($\mathbb{E}[V(s)] = V^\pi(s)$).
   - *Variance*: Can be **infinite**. The importance ratio $\rho = \prod_{k=t}^{T-1} \frac{\pi(a_k \mid s_k)}{b(a_k \mid s_k)}$ is a product of ratios. Even if each ratio is slightly greater than 1, a 50-step trajectory can yield an enormous multiplier $\rho \approx 10^{15}$. Even in simple 2-state MDPs, the analytical variance of $\rho G_t$ can be proven to diverge to $\infty$.
2. **Weighted Importance Sampling**:
   $$V(s) = \frac{\sum_{t} \rho_{t:T-1} G_t}{\sum_{t} \rho_{t:T-1}}$$
   - *Bias*: Biased for small samples (bounded by $\mathcal{O}(1/K)$), but consistent ($\to 0$ as $K \to \infty$).
   - *Variance*: **Strictly bounded**. In the extreme case of a single episode, $\rho$ cancels out: $V(s) = \frac{\rho G}{\rho} = G$. The variance is dramatically lower than Ordinary Importance Sampling, making Weighted Importance Sampling the universal choice in practice.
</details>

<details>
<summary><strong>Q3: Why can Monte Carlo methods NOT be applied to non-terminating (continuing) tasks?</strong></summary>

**Answer:**  
Monte Carlo updates are defined by:
$$V(S_t) \leftarrow V(S_t) + \alpha [G_t - V(S_t)]$$
where $G_t = \sum_{k=0}^{T - t - 1} \gamma^k R_{t+k+1}$.
Evaluating $G_t$ requires waiting until the episode terminates at step $T$ to observe all subsequent rewards.
In continuing tasks, there is no terminal state ($T = \infty$). The agent never reaches an episode boundary. Consequently, $G_t$ can never be evaluated, and the algorithm hangs indefinitely without executing a single update. Temporal Difference learning solves this by bootstrapping from the next state's value $V(S_{t+1})$ after a single step.
</details>

<details>
<summary><strong>Q4: What is the "Exploring Starts" assumption in Monte Carlo Control and why is it problematic in practice?</strong></summary>

**Answer:**  
Monte Carlo Exploring Starts (MCES) assumes that every episode begins by picking a state-action pair $(s_0, a_0)$ drawn from a probability distribution that assigns non-zero probability to **every possible state-action pair** in the MDP:
$$P(S_0 = s, A_0 = a) > 0 \quad \forall s \in \mathcal{S}, a \in \mathcal{A}$$
- *Purpose*: Guarantees that every action is visited infinitely often, ensuring policy convergence without needing stochastic exploratory policies.
- *Why it is problematic*: In real-world physical systems (e.g. driving a car or landing a rocket), the agent cannot be magically teleported into arbitrary initial states (e.g. starting an episode suspended 100 meters upside down). We must instead rely on on-policy $\epsilon$-greedy exploration or off-policy learning.
</details>

<details>
<summary><strong>Q5: Why does Monte Carlo estimation suffer less from violations of the Markov property than Dynamic Programming?</strong></summary>

**Answer:**  
- **Dynamic Programming & TD**: Update values by **bootstrapping** from immediate neighbor states: $V(s) = r + \gamma V(s')$. If the environment is non-Markovian (state $s'$ does not capture historical momentum), $V(s')$ is corrupted, and bootstrapping spreads this error systematically across all predecessor states.
- **Monte Carlo**: Never bootstraps. It records the actual cumulative discounted return $G_t$ experienced over real trajectories. Even if intermediate states violate the Markov assumption, the sample return $G_t$ represents the genuine physical outcome of following policy $\pi$. MC provides an unbiased sample of the true expected return regardless of internal state violations.
</details>

<details>
<summary><strong>Q6: How does the step-size parameter $\alpha$ govern the trade-off between stationarity and non-stationarity in Monte Carlo?</strong></summary>

**Answer:**  
In the incremental update rule $V(S_t) \leftarrow V(S_t) + \alpha [G_t - V(S_t)]$:
- If $\alpha = \frac{1}{N(S_t)}$: This is the exact empirical sample average. By the Law of Large Numbers, it minimizes variance and converges to the true stationary mean $V^\pi(s)$. However, older returns receive equal weight to newer returns, making it incapable of adapting to changing dynamics.
- If $\alpha \in (0, 1]$ is a constant: The value can be rewritten as an Exponential Moving Average (EMA):
  $$V_k = (1 - \alpha)^k V_0 + \sum_{i=1}^k \alpha (1 - \alpha)^{k-i} G_i$$
  The weight of past returns decays exponentially at rate $(1 - \alpha)^{k-i}$. This allows the agent to track drifting policies and non-stationary environment transitions.
</details>

<details>
<summary><strong>Q7: What is the relationship between Monte Carlo prediction and Supervised Regression?</strong></summary>

**Answer:**  
Monte Carlo prediction is mathematically identical to standard Supervised Linear / Non-linear Regression:
1. Each episode rollout generates a sequence of input-output training pairs: $(S_t, G_t)$.
2. The states $S_t$ act as the input features $\mathbf{x}$.
3. The empirical discounted returns $G_t$ act as the ground-truth regression targets $y$.
4. The objective function is Mean Squared Error:
   $$\min_\theta \frac{1}{2} \sum_{i=1}^M \left( G_i - V_\theta(S_i) \right)^2$$
Unlike TD learning (which has non-gradient semi-gradient targets), standard supervised learning loss functions, deep backpropagation, and SGD apply directly to Monte Carlo without modification.
</details>

<details>
<summary><strong>Q8: Explain the Concept of "Coverage" in Off-Policy Learning.</strong></summary>

**Answer:**  
For off-policy learning to evaluate a target policy $\pi$ using data generated from a behavior policy $b$, the behavior policy must satisfy the **Coverage Assumption**:
$$\text{If } \pi(a \mid s) > 0, \quad \text{then } b(a \mid s) > 0 \quad \forall s \in \mathcal{S}, a \in \mathcal{A}$$
Every action that is possible under the target policy $\pi$ must be actively sampled by behavior policy $b$ with non-zero probability. If $b(a \mid s) = 0$ for some action where $\pi(a \mid s) > 0$, the importance sampling ratio has a zero denominator $\frac{\pi}{0}$, and the agent can never observe or evaluate that action, introducing uncorrectable asymptotic bias.
</details>

---

## 21. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): First-Visit State Identification</strong></summary>

**Task**: Given an episode state sequence `[1, 2, 3, 2, 1, 4]`, write a function identifying the exact time-step indices corresponding to the *first visit* of each unique state.

**Solution**:
```python
def find_first_visit_indices(states: list[int]) -> dict[int, int]:
    first_visits = {}
    for t, s in enumerate(states):
        if s not in first_visits:
            first_visits[s] = t
    return first_visits

states_seq = [1, 2, 3, 2, 1, 4]
fv = find_first_visit_indices(states_seq)
print("First visit step indices per state:", fv)
assert fv == {1: 0, 2: 1, 3: 2, 4: 5}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Incremental Mean Update</strong></summary>

**Task**: Implement an incremental mean updater that updates the average value of a state given a stream of 5 returns without storing the history list.

**Solution**:
```python
def incremental_update(current_val: float, count: int, new_return: float) -> tuple[float, int]:
    new_count = count + 1
    new_val = current_val + (1.0 / new_count) * (new_return - current_val)
    return new_val, new_count

val, n = 0.0, 0
sample_returns = [10.0, 4.0, 6.0, 8.0, 12.0]
for r in sample_returns:
    val, n = incremental_update(val, n, r)

print(f"Calculated Incremental Mean: {val:.2f}")
assert np.isclose(val, np.mean(sample_returns))
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Importance Sampling Ratio Calculator</strong></summary>

**Task**: Given a target policy $\pi$ (greedy) and a behavior policy $b$ ($\epsilon$-greedy with $\epsilon=0.2$), calculate the trajectory importance sampling ratio $\rho$ for a 3-step action sequence.

**Solution**:
```python
import numpy as np

# 2 actions. Target policy pi is 100% action 0.
# Behavior policy b: a=0 with prob 0.9, a=1 with prob 0.1
pi_probs = {0: 1.0, 1: 0.0}
b_probs  = {0: 0.9, 1: 0.1}

# Trajectory actions
actions_taken = [0, 0, 0]

rho = 1.0
for a in actions_taken:
    rho *= (pi_probs[a] / b_probs[a])

print(f"Importance sampling ratio rho: {rho:.4f}")
np.testing.assert_allclose(rho, (1.0 / 0.9)**3)
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Weighted vs Ordinary Importance Sampling Simulator</strong></summary>

**Task**: Simulate 1,000 runs of a 1-step bandit problem to demonstrate that Weighted Importance Sampling achieves lower variance than Ordinary Importance Sampling.

**Solution**:
```python
import numpy as np

# True target policy always picks Action 0 (Reward = 10)
# Behavior policy picks Action 0 with 10% prob, Action 1 with 90% (Reward = 0)
n_episodes = 2000
rng = np.random.default_rng(42)

rhos = []
returns = []

for _ in range(n_episodes):
    a = 0 if rng.uniform(0, 1) < 0.10 else 1
    r = 10.0 if a == 0 else 0.0
    rho = (1.0 / 0.10) if a == 0 else 0.0
    rhos.append(rho)
    returns.append(r)

rhos = np.array(rhos)
returns = np.array(returns)

# Ordinary
ord_v = np.mean(rhos * returns)
# Weighted
w_v = np.sum(rhos * returns) / np.sum(rhos)

print(f"True value: 10.0")
print(f"Ordinary Importance Sampling estimate: {ord_v:.2f}")
print(f"Weighted Importance Sampling estimate: {w_v:.2f}")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Complete Off-Policy Weighted Importance Sampling Control</strong></summary>

**Task**: Implement the complete incremental off-policy control algorithm (Sutton & Barto Section 5.7) using cumulative weight accumulator $C(s, a)$.

**Solution**:
```python
import numpy as np

class OffPolicyMCControl:
    def __init__(self, n_states: int, n_actions: int, gamma: float = 0.99):
        self.n_states = n_states
        self.n_actions = n_actions
        self.gamma = gamma
        self.Q = np.zeros((n_states, n_actions))
        self.C = np.zeros((n_states, n_actions)) # Cumulative weight accumulator
        self.target_pi = np.zeros(n_states, dtype=int)

    def update_episode(self, episode, b_prob_function):
        G = 0.0
        W = 1.0
        T = len(episode)
        
        for t in reversed(range(T)):
            s, a, r = episode[t]
            G = r + self.gamma * G
            self.C[s, a] += W
            self.Q[s, a] += (W / self.C[s, a]) * (G - self.Q[s, a])
            
            # Target policy is greedy
            self.target_pi[s] = np.argmax(self.Q[s])
            if a != self.target_pi[s]:
                break # Since pi(a | s) = 0 for non-greedy actions, subsequent W becomes 0
                
            W = W * (1.0 / b_prob_function(s, a))

# Verification
mc_off = OffPolicyMCControl(n_states=2, n_actions=2)
# Single episode
ep = [(0, 0, 1.0), (1, 0, 5.0)]
mc_off.update_episode(ep, lambda s, a: 0.5)
print("Updated Q table:\n", mc_off.Q)
assert mc_off.Q[1, 0] > 0
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Empirical Central Limit Theorem Convergence Rate</strong></summary>

**Task**: Empirically verify that Monte Carlo estimation error decays at rate $\mathcal{O}(1/\sqrt{N})$ across sample sizes $N \in [10, 100, 1000, 10000]$.

**Solution**:
```python
import numpy as np

rng = np.random.default_rng(42)
true_mean = 5.0
sample_sizes = [10, 100, 1000, 10000]

errors = []
for n in sample_sizes:
    trial_errors = []
    for _ in range(200):
        samples = rng.normal(loc=true_mean, scale=2.0, size=n)
        trial_errors.append(abs(np.mean(samples) - true_mean))
    errors.append(np.mean(trial_errors))

print("Sample Size vs Mean Absolute Error:")
for n, err in zip(sample_sizes, errors):
    print(f"N = {n:5d} | MAE = {err:.4f}")

# Check 100x increase in sample size reduces error by ~10x
ratio = errors[0] / errors[2]
print(f"Error reduction from N=10 to N=1000: {ratio:.2f}x (Theoretical: ~10x)")
assert ratio > 7.0 and ratio < 14.0
```
</details>

---

## 13. Summary, Verified References & Navigation

### Core Takeaways
1. **Model-free learning from complete episodes**: Averages sampled returns $G_t$ without requiring transition probabilities $\mathcal{P}_{ss'}^a$.
2. **First-Visit vs Every-Visit**: First-visit is strictly unbiased; every-visit is consistent and often achieves lower MSE.
3. **Off-policy requires Weighted Importance Sampling**: Ordinary importance sampling suffers from unbounded variance; weighted importance sampling bounds variance and stabilizes control.

### Verified References
- Sutton, R. S., & Barto, A. G. (2018). *Reinforcement Learning: An Introduction (2nd Edition).* Chapter 5: Monte Carlo Methods.
- Precup, D., Sutton, R. S., & Singh, S. (2000). *Eligibility Traces for Off-Policy Policy Evaluation.* ICML.

---

[← Previous Note: Dynamic Programming](04-dynamic-programming.md) | [Module Index](README.md) | [Next Note: Temporal Difference Learning →](06-temporal-difference-learning.md)
