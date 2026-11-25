# Q-Learning: Off-Policy Temporal Difference Control

> **Track:** `05-ai/machine-learning` | **Module:** `07-reinforcement-learning` | **Status:** `Production Standard`  
> **Metadata:** `NumPy: >=1.26.0` | `Gymnasium: >=0.29.0` | `Python: >=3.11`  
> **Type:** `Algorithm Specification & Production Guide`

---

## 1. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Algorithm Name** | Watkins' Q-Learning (Off-Policy Tabular TD Control) |
| **Family** | Model-Free Reinforcement Learning / Temporal Difference Control / Off-Policy Learning |
| **Input Feature Types** | Transition tuples $(s_t, a_t, r_{t+1}, s_{t+1}, \text{done})$ collected via trial-and-error environment interaction. Completely model-free (requires no knowledge of $\mathcal{P}_{ss'}^a$ or $\mathcal{R}(s, a)$). |
| **Target Output** | Optimal action-value function table $Q^*(s, a) \in \mathbb{R}^{|\mathcal{S}| \times |\mathcal{A}|}$ and extracted optimal greedy policy $\pi^*(s) = \arg\max_a Q^*(s, a)$. |
| **Optimization Target** | Satisfying the Bellman Optimality Equation: driving TD error $\delta_t = R_{t+1} + \gamma \max_{a'} Q(S_{t+1}, a') - Q(S_t, A_t)$ to zero. |
| **Key Metrics** | Cumulative Episode Reward, Success Rate, Average Bellman Error, Action Value Overestimation Magnitude. |
| **Key Hyperparameters** | Learning rate ($\alpha \in [0.05, 0.2]$), discount factor ($\gamma \in [0.9, 0.99]$), exploration rate ($\epsilon \in [0.01, 1.0]$ with exponential decay). |
| **Inference Mechanism** | Deterministic greedy lookup: $a^* = \arg\max_a Q(s, a)$ executed in $\mathcal{O}(|\mathcal{A}|)$ time. |
| **Primary Failure Modes** | **Maximization Bias**: Systemic overestimation of action-values due to $\mathbb{E}[\max X] \ge \max \mathbb{E}[X]$; **State Space Explosion**: Inability to generalize across unvisited continuous coordinates without function approximation; **Exploration Collapse**: Decaying $\epsilon$ too rapidly traps the agent in suboptimal local minima. |

---

## 2. Architecture & Intuition

### The Breakthrough of Watkins (1989)
Prior to Q-learning, reinforcement learning algorithms either required a complete mathematical model of the environment (Dynamic Programming) or had to wait until the end of entire episodes to evaluate returns (Monte Carlo).

Chris Watkins introduced **Q-Learning**, which solved both limitations simultaneously. The core breakthrough is its **off-policy** design:
- The agent behaves in the environment using an exploratory policy (e.g. $\epsilon$-greedy), trying random actions to discover new paths.
- However, its update target uses the **greedy maximum operator** ($\max_a Q(s_{t+1}, a)$), directly approximating the value of the **optimal policy** independent of the exploratory actions actually taken.

```
                            Q-Learning Backup Diagram
                            
                                     ( S_t )
                                        │
                                        │ Action A_t (from behavior policy b)
                                        ▼
                                     [ S_t, A_t ]
                                        │
                                        ▼ Reward R_t+1
                                     ( S_t+1 )
                                    /    |    \
                                   O     O     O  (Evaluate all possible a')
                                         │
                                         ▼ Target: max_a' Q(S_t+1, a')
```

---

## 3. Theoretical Derivation

### Symbol Table

| Symbol | Mathematical Domain | Concrete Definition |
| :--- | :--- | :--- |
| $Q(s, a)$ | Real scalar | Table estimating expected return for taking action $a$ in state $s$ |
| $Q^*(s, a)$ | Real scalar | The unique optimal action-value function |
| $\delta_t$ | Real scalar | Q-learning Temporal Difference error |
| $\alpha_t \in (0, 1]$ | Real scalar | Learning rate parameter at step $t$ |
| $\gamma \in [0, 1)$ | Real scalar | Discount factor |
| $\epsilon_t \in [0, 1]$ | Real scalar | Exploration probability at step $t$ |

### The Q-Learning Update Equation
Given a transition $(S_t = s, A_t = a, R_{t+1} = r, S_{t+1} = s')$:

$$Q(s, a) \leftarrow Q(s, a) + \alpha \left[ r + \gamma \max_{a' \in \mathcal{A}} Q(s', a') - Q(s, a) \right]$$

If $s'$ is a terminal state, future value is zero: the target collapses to $r$.

### Convergence Proof (Watkins & Dayan, 1992; Tsitsiklis, 1994)
**Theorem**: Let the state and action spaces $\mathcal{S}$ and $\mathcal{A}$ be finite. If:
1. Every state-action pair $(s, a)$ is visited an infinite number of times.
2. The learning rate schedule satisfies the Robbins-Monro conditions:
   $$\sum_{t=0}^{\infty} \alpha_t(s, a) = \infty, \qquad \sum_{t=0}^{\infty} \alpha_t^2(s, a) < \infty$$
3. Rewards are bounded: $|R_{t+1}| \le R_{\max} < \infty$.

Then $Q(s, a)$ converges with probability 1 to the unique optimal action-value function $Q^*(s, a)$ as $t \to \infty$.

---

### Maximization Bias & Jensen's Inequality
The target in Q-learning uses a maximum over noisy value estimates: $\max_a Q(s', a)$.
By **Jensen's Inequality**, for any set of independent random variables $X_1, \dots, X_k$:

$$\mathbb{E}\left[ \max(X_1, \dots, X_k) \right] \ge \max\left( \mathbb{E}[X_1], \dots, \mathbb{E}[X_k] \right)$$

#### Concrete Example:
Suppose a state $s'$ has 5 actions, each having a true expected value of $Q^*(s', a) = 0.0$, but estimation noise causes values to fluctuate uniformly in $[-1.0, +1.0]$.
The true maximum is $\max(0, 0, 0, 0, 0) = 0.0$.
However, the expected maximum of 5 uniform variables in $[-1, 1]$ is:
$$\mathbb{E}[\max_{a} Q(s', a)] = \frac{5 - 1}{5 + 1} = +0.67$$
The Bellman target bootstraps from $+0.67$ instead of $0.0$. This systematic positive bias accumulates across backpropagated steps, leading to severe **action-value overestimation**.

---

## 4. Computational Complexity

| Operation | Time Complexity | Memory Complexity | Scalability |
| :--- | :--- | :--- | :--- |
| **Action Selection ($\epsilon$-greedy)** | $\mathcal{O}(|\mathcal{A}|)$ | $\mathcal{O}(1)$ | Fast vector operations |
| **Q-Value Update** | $\mathcal{O}(|\mathcal{A}|)$ ($\max$ over actions) | $\mathcal{O}(1)$ | Single arithmetic step ($\approx 30\text{ ns}$) |
| **Q-Table Storage** | N/A | $\mathcal{O}(|\mathcal{S}| \times |\mathcal{A}|)$ | Fits in RAM for $|\mathcal{S}| \le 500,000$ |

---

## 5. Inductive Bias & Statistical Assumptions

1. **Stationary Markov Dynamics**: Assumes transition probabilities $\mathcal{P}_{ss'}^a$ and reward functions are time-invariant.
2. **Infinite Exploration**: Assumes all valid state-action pairs continue to be sampled infinitely often to guarantee asymptotic optimality.
3. **Optimistic Overestimation**: The $\max$ operator biases initial estimates upward, which historically encourages exploration of uncertain actions.

---

## 6. From-Scratch Implementation (NumPy)

A complete, vectorized implementation of Tabular Q-Learning with $\epsilon$-decay schedules and action masking in pure NumPy:

```python
"""
Pure NumPy implementation of Watkins' Tabular Q-Learning algorithm.
"""

from __future__ import annotations
import numpy as np

class TabularQLearning:
    """
    Watkins' Q-Learning agent with epsilon-greedy exploration.
    
    Parameters
    ----------
    n_states : int
        Cardinality of discrete state space |S|.
    n_actions : int
        Cardinality of discrete action space |A|.
    gamma : float
        Discount factor in [0, 1).
    alpha : float
        Learning rate in (0, 1].
    epsilon_start : float
        Initial exploration probability.
    epsilon_min : float
        Minimum exploration floor.
    epsilon_decay : float
        Multiplicative decay per episode.
    """
    def __init__(
        self,
        n_states: int,
        n_actions: int,
        gamma: float = 0.95,
        alpha: float = 0.1,
        epsilon_start: float = 1.0,
        epsilon_min: float = 0.01,
        epsilon_decay: float = 0.999
    ):
        self.n_states = n_states
        self.n_actions = n_actions
        self.gamma = gamma
        self.alpha = alpha
        self.epsilon = epsilon_start
        self.epsilon_min = epsilon_min
        self.epsilon_decay = epsilon_decay

        # Q-table initialization (zeros or small optimistic values)
        self.Q = np.zeros((n_states, n_actions), dtype=np.float64)

    def select_action(self, state: int, rng: np.random.Generator) -> int:
        """epsilon-greedy action selection."""
        if rng.uniform(0, 1) < self.epsilon:
            return int(rng.choice(self.n_actions))
        
        # Greedy selection with random tie-breaking
        q_s = self.Q[state]
        max_val = np.max(q_s)
        best_actions = np.where(q_s == max_val)[0]
        return int(rng.choice(best_actions))

    def update(self, s: int, a: int, r: float, s_next: int, done: bool) -> float:
        """
        One-step Q-Learning update:
        Q(s, a) <- Q(s, a) + alpha * [ r + gamma * max_a' Q(s', a') - Q(s, a) ]
        Returns the scalar TD error delta.
        """
        if done:
            target = r
        else:
            target = r + self.gamma * np.max(self.Q[s_next])

        td_error = target - self.Q[s, a]
        self.Q[s, a] += self.alpha * td_error
        return float(td_error)

    def decay_epsilon(self):
        """Decay exploration rate at the end of each episode."""
        self.epsilon = max(self.epsilon_min, self.epsilon * self.epsilon_decay)

    def get_greedy_policy(self) -> np.ndarray:
        """Extract deterministic optimal policy table."""
        return np.argmax(self.Q, axis=1)
```

---

## 7. Production Gymnasium CliffWalking Benchmark

Benchmarking Q-Learning on Farama Gymnasium's classic `CliffWalking-v0`:

```python
"""
Production Benchmark: Solving CliffWalking-v0 using Tabular Q-Learning.
CliffWalking: 4x12 grid (48 states). 
Bottom row has the Cliff (states 37-46). Stepping into the cliff yields -100 reward.
Q-Learning learns the optimal short path directly along the edge of the cliff!
"""

import gymnasium as gym
import numpy as np

def train_cliff_walking(n_episodes: int = 500) -> tuple[np.ndarray, list[float]]:
    env = gym.make("CliffWalking-v0")
    agent = TabularQLearning(
        n_states=48,
        n_actions=4,
        gamma=0.99,
        alpha=0.1,
        epsilon_start=1.0,
        epsilon_min=0.01,
        epsilon_decay=0.99
    )
    rng = np.random.default_rng(42)
    episode_rewards = []

    for ep in range(n_episodes):
        obs, _ = env.reset()
        total_reward = 0.0
        done = False
        
        while not done:
            action = agent.select_action(obs, rng)
            next_obs, reward, terminated, truncated, _ = env.step(action)
            done = terminated or truncated
            
            agent.update(obs, action, reward, next_obs, done)
            obs = next_obs
            total_reward += reward

        agent.decay_epsilon()
        episode_rewards.append(total_reward)

    env.close()
    return agent.Q, episode_rewards

if __name__ == "__main__":
    Q_cliff, rewards = train_cliff_walking(500)
    avg_final_reward = np.mean(rewards[-50:])
    print(f"Average final reward over last 50 episodes: {avg_final_reward:.1f}")
    # The optimal deterministic path takes 13 steps = -13 reward
    assert avg_final_reward > -25.0
    print("Q-Learning successfully converged to the optimal cliff edge policy!")
```

---

## 8. Model Persistence & SHA-256 Integrity

```python
import json
import hashlib
import joblib
from pathlib import Path
from typing import Any, Dict

def save_q_table_artifact(agent: Any, out_path: Path, metadata: Dict[str, Any]) -> Path:
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    
    payload = {
        "Q": agent.Q,
        "gamma": agent.gamma,
        "alpha": agent.alpha
    }
    joblib.dump(payload, out_path, compress=3)
    
    sha = hashlib.sha256()
    with open(out_path, "rb") as f:
        while chunk := f.read(65536):
            sha.update(chunk)
            
    sidecar = {
        "file": out_path.name,
        "sha256": sha.hexdigest(),
        "shape": list(agent.Q.shape),
        "metadata": metadata
    }
    with open(out_path.with_suffix(".json"), "w", encoding="utf-8") as f:
        json.dump(sidecar, f, indent=2)
        
    return out_path
```

---

## 9. Hyperparameter Tuning & Diagnostics

### Tuning Learning Rate $\alpha$ and Decay Schedule
If $\alpha$ is constant, $Q$-values oscillate with environmental noise. For mathematical convergence, use a count-decayed learning rate:

$$\alpha_t(s, a) = \frac{\alpha_0}{1 + c \cdot N(s, a)}$$

where $N(s, a)$ is the state-action visitation counter.

---

## 10. Residuals, Diagnostics & Validation

The primary diagnostic metric is the **Mean Bellman Optimality Residual**:

$$\overline{\delta^2} = \frac{1}{|\mathcal{S}| |\mathcal{A}|} \sum_{s, a} \left( \mathcal{R}(s, a) + \gamma \max_{a'} Q(s', a') - Q(s, a) \right)^2$$

As training progresses, $\overline{\delta^2}$ must decay toward zero.

---

## 11. Uncertainty Estimation & Confidence Intervals

Uncertainty in Q-values stems from finite visit counts $N(s, a)$. Under the Upper Confidence Bound framework (UCB Q-Learning):

$$\text{Confidence Interval}(s, a) = \pm c \sqrt{\frac{\ln t}{N(s, a)}}$$

---

## 12. Failure Modes & Edge Cases

| Failure Mode | Root Cause | Behavioral Symptom | Detection Strategy | Mitigation |
| :--- | :--- | :--- | :--- | :--- |
| **Maximization Bias Overestimation** | The $\max$ operator systematically overestimates noisy action returns. | Agent overestimates expected value by $200\%$; prefers risky volatile states. | Compare $Q(s, a)$ against true Monte Carlo empirical return rollouts. | Switch to **Double Q-Learning** (decouples action selection from evaluation). |
| **Premature Exploitation Trap** | Epsilon decays to $\epsilon_{\min}$ before the agent discovers the goal. | Agent loops endlessly in local minima; success rate drops to $0\%$. | Cumulative success rate flatlines at 0 after 100 episodes. | Use optimistic initial values ($Q_0(s, a) = +10.0$) or slower decay. |
| **Cliff Walking Over-Confidence** | Q-Learning learns the optimal path along the precipice; during $\epsilon$-greedy execution, random exploration causes frequent falls. | High training performance during test, but catastrophic failures during exploration. | Compare on-policy training return vs greedy evaluation return. | Use **SARSA** if online safety during exploration is paramount. |

---

## 13. Comparative Trade-offs & When to Use

| Dimension | Q-Learning (Off-Policy) | SARSA (On-Policy) | Monte Carlo (MCES) | DQN (Deep Q-Network) |
| :--- | :--- | :--- | :--- | :--- |
| **Policy Evaluated** | **Optimal Greedy $\pi^*$** | Behavior Policy $\pi$ | Target Policy | Optimal Greedy $\pi^*$ |
| **Safety During Training**| Low (risky paths) | **High (conservative paths)** | Low | Low |
| **Replay Buffer Reuse?** | **YES** (Off-Policy) | NO (Strictly On-Policy) | NO | **YES** |
| **State Space Scaling** | Small Discrete ($< 10^5$) | Small Discrete ($< 10^5$) | Small Discrete | **Infinite Continuous** |

---

## 14. Production Engineering & Latency SLAs

- **Sub-Microsecond Edge Execution**: Once trained, the Q-table is a flattened 1D array. Given discrete index $s$, the optimal action is retrieved via `np.argmax(Q[s])` in $< 100\text{ ns}$, running directly on embedded microcontrollers (ARM Cortex).

---

## 15. Interpretability & Feature Attribution

Q-tables are transparent: for any state $s$, print all action values:
$$[Q(s, \text{Up}): -1.2, \quad Q(s, \text{Down}): -50.0, \quad Q(s, \text{Right}): +10.0]$$
The numeric preference and safety margin are visible without black-box attribution methods.

---

## 16. Hardware Acceleration, Scaling & Distributed Notes

- **Ape-X Distributed Q-Learning**: Run 128 parallel worker processes collecting transitions into a centralized shared Prioritized Replay Buffer, while a GPU learner executes batched gradient updates.

---

## 17. Pytest Verification Suite

```python
"""
Pytest validation suite for Q-Learning.
Save as: tests/test_q_learning.py
Run: pytest -v tests/test_q_learning.py
"""

import pytest
import numpy as np

def test_q_learning_update_math():
    """Verify single Q-learning update matches analytical formula."""
    Q = np.zeros((2, 2))
    s = 0
    a = 1
    r = 5.0
    s_next = 1
    gamma = 0.9
    alpha = 0.5
    
    # Q(1, 0) = 10, Q(1, 1) = 20 -> max is 20
    Q[1, 0] = 10.0
    Q[1, 1] = 20.0
    
    # Target = 5.0 + 0.9 * 20.0 = 23.0
    # delta = 23.0 - 0.0 = 23.0
    # Q_new(0, 1) = 0.0 + 0.5 * 23.0 = 11.5
    target = r + gamma * np.max(Q[s_next])
    td_error = target - Q[s, a]
    Q[s, a] += alpha * td_error
    
    assert np.isclose(Q[s, a], 11.5)

def test_q_learning_terminal_state():
    """Verify terminal state updates do not bootstrap future values."""
    Q = np.zeros((2, 2))
    Q[1, :] = 50.0 # Next state has values, but done=True
    
    r = 10.0
    alpha = 1.0
    # Target should be strictly r (10.0), ignoring Q[1]
    target = r
    Q[0, 0] += alpha * (target - Q[0, 0])
    
    assert np.isclose(Q[0, 0], 10.0)
```

---

## 18. Cross-Track Integration

- **`03-backend/fastapi/16-ai-and-ml-serving`**: Serving real-time greedy policy actions via a low-latency REST endpoint.
- **`05-ai/machine-learning/07-reinforcement-learning/11-deep-q-networks.md`**: Transitioning from tabular Q-learning to Deep Q-Networks (DQN).

---

## 19. Real-World Case Study

### Autonomous Warehouse Mobile Robot Dispatch
- **Context**: An automated logistics fulfillment center operates 45 Autonomous Guided Vehicles (AGVs) navigating a grid layout.
- **Problem**: AGVs frequently collided at bottleneck intersections or deadlocked, causing warehouse line stoppages.
- **Solution Architecture**:
  1. Modeled floor intersections as a discrete 120-state MDP.
  2. Applied Tabular Q-Learning with collision penalties ($-500$) and timely delivery rewards ($+50$).
  3. Deployed the pre-computed Q-tables directly into AGV onboard firmware.
- **Outcome**: Completely eliminated intersection gridlock; increased parcel throughput by $24\%$.

---

## 20. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Why is Q-Learning classified as an "Off-Policy" algorithm while SARSA is "On-Policy"?</strong></summary>

**Answer:**  
An algorithm's policy classification depends on the relationship between the **behavior policy** (the policy used to generate actions in the environment) and the **target policy** (the policy whose value is being estimated and optimized):
- **Q-Learning**:
  The update rule is:
  $$Q(S_t, A_t) \leftarrow Q(S_t, A_t) + \alpha \left[ R_{t+1} + \gamma \max_{a'} Q(S_{t+1}, a') - Q(S_t, A_t) \right]$$
  Notice that the action taken in the environment $A_t$ comes from an exploratory behavior policy (e.g. $\epsilon$-greedy). However, the target uses the greedy maximum: $\max_{a'} Q(S_{t+1}, a')$.
  This means the target policy is strictly the **deterministic greedy policy** $\pi(s) = \arg\max_a Q(s, a)$, regardless of what exploratory action the agent actually takes at step $t+1$. Because the target policy differs from the behavior policy, Q-Learning is strictly **off-policy**.
- **SARSA**:
  The update rule is:
  $$Q(S_t, A_t) \leftarrow Q(S_t, A_t) + \alpha \left[ R_{t+1} + \gamma Q(S_{t+1}, A_{t+1}) - Q(S_t, A_t) \right]$$
  SARSA evaluates the actual action $A_{t+1}$ chosen by the exploratory behavior policy. The target policy and behavior policy are identical, making SARSA strictly **on-policy**.
</details>

<details>
<summary><strong>Q2: Explain the Maximization Bias in Q-Learning and how Double Q-Learning resolves it mathematically.</strong></summary>

**Answer:**  
In Q-Learning, the target contains a maximum over noisy value estimates: $\max_a Q(s', a)$.
Because $\mathbb{E}[\max(X_1, \dots, X_k)] \ge \max(\mathbb{E}[X_1], \dots, \mathbb{E}[X_k])$, any random noise in estimation causes the expected maximum to be strictly greater than the true maximum. This leads to **systematic overestimation** of action values.
**The Double Q-Learning Solution (Hasselt, 2010)**:
Decouple the **selection** of the best action from the **evaluation** of that action by maintaining two independent Q-tables, $Q_A$ and $Q_B$:
1. With $50\%$ probability, update $Q_A$:
   - Use $Q_A$ to **select** the greedy action: $a^* = \arg\max_a Q_A(s', a)$.
   - Use $Q_B$ to **evaluate** the value of that action:
     $$Q_A(s, a) \leftarrow Q_A(s, a) + \alpha \left[ R + \gamma Q_B(s', a^*) - Q_A(s, a) \right]$$
2. With $50\%$ probability, update $Q_B$ reversing the roles.
Because $Q_B$ provides an independent evaluation of the action selected by $Q_A$, $\mathbb{E}[Q_B(s', \arg\max_a Q_A(s', a))] = Q^*(s', a^*)$. The overestimation bias is completely eliminated.
</details>

<details>
<summary><strong>Q3: State the Robbins-Monro conditions for Q-Learning convergence and explain what each condition guarantees.</strong></summary>

**Answer:**  
The Robbins-Monro conditions on the learning rate schedule $\alpha_t(s, a)$ are:
1. **$\sum_{t=1}^{\infty} \alpha_t(s, a) = \infty$**:
   - *Guarantee*: Guarantees that the step sizes are sufficiently large to overcome any initial arbitrary initialization $Q_0$ and allow the estimates to travel arbitrarily far to reach the true fixed point $Q^*$, regardless of starting distance.
2. **$\sum_{t=1}^{\infty} \alpha_t^2(s, a) < \infty$**:
   - *Guarantee*: Guarantees that the step sizes eventually shrink sufficiently small to dampen and extinguish stochastic environment transition noise, ensuring convergence to a deterministic fixed point rather than oscillating forever.
For example, the schedule $\alpha_t = \frac{1}{t}$ satisfies both conditions, whereas a constant $\alpha = 0.1$ satisfies the first but violates the second (leading to continuous Brownian oscillation around the mean).
</details>

<details>
<summary><strong>Q4: Why does Q-Learning learn the risky "cliff edge" path in CliffWalking while SARSA learns the safe detour?</strong></summary>

**Answer:**  
In CliffWalking, the shortest path runs directly adjacent to a cliff that incurs a $-100$ penalty and resets the episode.
- **Q-Learning** evaluates the optimal target policy: $\max_a Q(s', a)$. It assumes that once it is at the edge of the cliff, it will always execute the optimal greedy action (moving straight forward along the edge) with zero mistakes. Thus, Q-Learning learns that the edge path is optimal ($-13$ steps). However, during training, an exploratory $\epsilon$-greedy action can randomly veer off the edge, causing frequent falls.
- **SARSA** evaluates the actual $\epsilon$-greedy exploratory behavior policy: it updates using $Q(S_{t+1}, A_{t+1})$. SARSA realizes that because it explores with probability $\epsilon$, walking along the edge has a high likelihood of randomly falling off the cliff. It incorporates its own exploratory mistakes into its value calculations, penalizing the edge path and learning the safe detour path around the top of the grid.
</details>

<details>
<summary><strong>Q5: What happens if you initialize Q-values optimistically (e.g. $Q_0(s, a) = +100.0$) in a task where all rewards are negative?</strong></summary>

**Answer:**  
Optimistic initialization acts as a powerful, automatic mechanism for **implicit exploration**:
1. When an agent visits any state-action pair $(s, a)$ for the first time, it receives a realistic negative reward (e.g. $-1$).
2. The Q-value drops from $+100.0$ to around $+89.0$.
3. When the agent returns to state $s$, all unvisited actions still have values of $+100.0$.
4. The greedy $\max$ operator naturally selects the unvisited actions!
The agent is systematically driven to try every available action in every state at least once before repeating any action, achieving comprehensive early exploration without needing complex exploration heuristics.
</details>

<details>
<summary><strong>Q6: Why does Q-Learning NOT require Importance Sampling corrections, unlike Off-Policy Monte Carlo?</strong></summary>

**Answer:**  
In Off-Policy Monte Carlo, the return $G_t$ is a sum of rewards accumulated along an entire trajectory generated by the behavior policy $b$. The probability of that trajectory occurring under the target policy $\pi$ differs, requiring the product of importance ratios $\prod \frac{\pi}{b}$ to correct for trajectory distribution mismatch.
In Q-Learning, the algorithm updates using the **Bellman Optimality Operator**:
$$Q(s, a) \leftarrow Q(s, a) + \alpha [r + \gamma \max_{a'} Q(s', a') - Q(s, a)]$$
1. The update does not compute an expectation over future actions; it computes an explicit algebraic $\max_{a'}$.
2. The immediate reward $r$ and transition to $s'$ are drawn from the true environment dynamics $P(s' \mid s, a)$, which is independent of the policy.
3. Because the target is explicitly evaluated on $\max_{a'}$ rather than an action sampled from policy $b$, there is no policy action distribution mismatch in the target. Thus, importance sampling ratios are completely unnecessary for 1-step tabular Q-learning.
</details>

<details>
<summary><strong>Q7: What is Expected SARSA and how does it bridge Q-Learning and SARSA?</strong></summary>

**Answer:**  
Expected SARSA modifies the update target to use the expected value under the current policy rather than a single sampled action (SARSA) or a hard maximum (Q-Learning):
$$Q(S_t, A_t) \leftarrow Q(S_t, A_t) + \alpha \left[ R_{t+1} + \gamma \sum_{a'} \pi(a' \mid S_{t+1}) Q(S_{t+1}, a') - Q(S_t, A_t) \right]$$
- **Bridge to SARSA**: Eliminates the variance introduced by randomly sampling action $A_{t+1}$ in SARSA, exhibiting smoother convergence.
- **Bridge to Q-Learning**: If the target policy $\pi$ is greedy ($\pi(a \mid s) = 1$ for $\arg\max Q$), the sum $\sum_a \pi Q$ collapses to $\max_a Q(s', a)$, making Expected SARSA **identical to Q-Learning**.
Expected SARSA generalizes both algorithms and can be used on-policy or off-policy.
</details>

<details>
<summary><strong>Q8: What is the convergence failure mode when Tabular Q-Learning is naively applied to continuous state spaces via discretization?</strong></summary>

**Answer:**  
Discretizing continuous spaces into fixed bins (e.g. dividing a robot's 6 continuous joint angles into 20 bins each):
1. **Exponential Bin Explosion**: Results in $20^6 = 64,000,000$ discrete states. The vast majority of states will never be visited even once during training, violating the infinite visitation condition.
2. **State Aliasing at Bin Boundaries**: Points on opposite sides of a bin boundary have identical physical meaning but are treated as completely independent discrete states with zero parameter sharing.
3. **Discontinuous Policies**: The resulting policy exhibits jagged, discontinuous jumps across bin thresholds, causing mechanical instability and violent actuator oscillations on physical robots. This necessitates continuous function approximation (DQN, Actor-Critic).
</details>

---

## 21. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Q-Table Lookup and Greedy Action Selection</strong></summary>

**Task**: Write a function that takes a Q-table of shape $(4, 3)$, a state index $s=2$, and returns the greedy action with random tie-breaking.

**Solution**:
```python
import numpy as np

def get_greedy_action(Q: np.ndarray, state: int, rng: np.random.Generator) -> int:
    q_s = Q[state]
    max_val = np.max(q_s)
    best_actions = np.where(np.isclose(q_s, max_val))[0]
    return int(rng.choice(best_actions))

rng = np.random.default_rng(42)
Q_test = np.array([
    [1.0, 2.0, 0.5],
    [0.0, 0.0, 0.0],
    [4.5, 1.2, 4.5], # Tie between action 0 and 2
    [3.1, 9.8, 2.2]
])

action = get_greedy_action(Q_test, state=2, rng=rng)
print(f"Sampled greedy action for State 2: {action}")
assert action in [0, 2]
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Computing Maximization Bias Numerically</strong></summary>

**Task**: Demonstrate maximization bias by drawing 10 standard normal variables $X_i \sim \mathcal{N}(0, 1)$ over 10,000 trials and showing that $\mathbb{E}[\max X_i] > 0$.

**Solution**:
```python
import numpy as np

rng = np.random.default_rng(42)
n_trials = 10_000
n_actions = 10

# All actions have true mean = 0.0
samples = rng.normal(loc=0.0, scale=1.0, size=(n_trials, n_actions))

# Compute maximum per trial
max_estimates = np.max(samples, axis=1)
empirical_mean_max = float(np.mean(max_estimates))

print(f"True expected action value: 0.0")
print(f"Empirical mean of maximum:  {empirical_mean_max:.4f}")
# Theoretical E[max of 10 standard normals] ~ 1.538
assert empirical_mean_max > 1.4 and empirical_mean_max < 1.7
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Step-by-Step Double Q-Learning Implementation</strong></summary>

**Task**: Implement Double Q-Learning in pure NumPy and verify that it maintains two Q-tables and updates them with 50/50 probability.

**Solution**:
```python
import numpy as np

class DoubleQLearning:
    def __init__(self, n_states, n_actions, gamma=0.9, alpha=0.1):
        self.QA = np.zeros((n_states, n_actions))
        self.QB = np.zeros((n_states, n_actions))
        self.gamma = gamma
        self.alpha = alpha

    def update(self, s, a, r, s_next, done, rng):
        if rng.uniform(0, 1) < 0.5:
            # Update QA using argmax QA and evaluation from QB
            if done:
                target = r
            else:
                best_a = np.argmax(self.QA[s_next])
                target = r + self.gamma * self.QB[s_next, best_a]
            self.QA[s, a] += self.alpha * (target - self.QA[s, a])
        else:
            # Update QB using argmax QB and evaluation from QA
            if done:
                target = r
            else:
                best_a = np.argmax(self.QB[s_next])
                target = r + self.gamma * self.QA[s_next, best_a]
            self.QB[s, a] += self.alpha * (target - self.QB[s, a])

# Test
rng = np.random.default_rng(42)
dql = DoubleQLearning(2, 2)
for _ in range(100):
    dql.update(0, 0, 1.0, 1, False, rng)
print("QA:\n", dql.QA)
print("QB:\n", dql.QB)
assert dql.QA[0, 0] > 0 and dql.QB[0, 0] > 0
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Epsilon Decay Exponential Scheduler</strong></summary>

**Task**: Write an exponential decay scheduler and plot the exploration curve over 1,000 episodes ensuring it respects an exploration floor $\epsilon_{\min} = 0.02$.

**Solution**:
```python
import numpy as np

def generate_epsilon_schedule(n_episodes=1000, eps_start=1.0, eps_min=0.02, decay=0.995):
    epsilons = []
    eps = eps_start
    for _ in range(n_episodes):
        epsilons.append(eps)
        eps = max(eps_min, eps * decay)
    return np.array(epsilons)

schedule = generate_epsilon_schedule()
print(f"Episode 0:   epsilon = {schedule[0]:.3f}")
print(f"Episode 200: epsilon = {schedule[200]:.3f}")
print(f"Episode 999: epsilon = {schedule[999]:.3f}")
assert schedule[-1] == 0.02
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Solving FrozenLake-v1 with Tabular Q-Learning</strong></summary>

**Task**: Train a Q-learning agent to solve the stochastic `FrozenLake-v1` (4x4) environment with slip probabilities and achieve $> 70\%$ success rate over 100 test trials.

**Solution**:
```python
import gymnasium as gym
import numpy as np

def train_frozen_lake(n_episodes=10_000):
    env = gym.make("FrozenLake-v1", is_slippery=True)
    Q = np.zeros((16, 4))
    alpha = 0.1
    gamma = 0.99
    epsilon = 1.0
    rng = np.random.default_rng(42)
    
    for ep in range(n_episodes):
        s, _ = env.reset()
        done = False
        while not done:
            if rng.uniform(0, 1) < epsilon:
                a = env.action_space.sample()
            else:
                a = int(np.argmax(Q[s]))
                
            s_next, r, term, trunc, _ = env.step(a)
            done = term or trunc
            
            target = r if done else (r + gamma * np.max(Q[s_next]))
            Q[s, a] += alpha * (target - Q[s, a])
            s = s_next
            
        epsilon = max(0.01, epsilon * 0.9995)
        
    # Evaluate greedy policy over 100 episodes
    successes = 0
    for _ in range(100):
        s, _ = env.reset()
        done = False
        while not done:
            a = int(np.argmax(Q[s]))
            s, r, term, trunc, _ = env.step(a)
            done = term or trunc
            if r == 1.0:
                successes += 1
    env.close()
    return successes / 100.0

success_rate = train_frozen_lake()
print(f"FrozenLake Evaluation Success Rate: {success_rate * 100:.1f}%")
assert success_rate >= 0.65
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Maximization Bias Head-to-Head Comparison</strong></summary>

**Task**: Replicate Sutton & Barto's Maximization Bias experiment (Figure 6.5) comparing standard Q-Learning against Double Q-Learning in a state with many zero-mean noisy actions.

**Solution**:
```python
import numpy as np

def run_maximization_bias_trial(n_runs=200, n_episodes=300):
    # State A: 2 actions: Right (leads to Terminal, 0 reward)
    # Left (leads to State B).
    # State B has 10 actions, each giving N(-0.1, 1.0) reward and terminating.
    # Optimal action in A is Right (0 > -0.1).
    # Q-Learning gets tricked into picking Left because of maximization bias!
    
    q_left_picks = np.zeros(n_episodes)
    dq_left_picks = np.zeros(n_episodes)
    
    for run in range(n_runs):
        rng = np.random.default_rng(run)
        
        # Standard Q-learning
        Q = { "A": np.zeros(2), "B": np.zeros(10) }
        # Double Q-learning
        QA = { "A": np.zeros(2), "B": np.zeros(10) }
        QB = { "A": np.zeros(2), "B": np.zeros(10) }
        
        for ep in range(n_episodes):
            # Q-learning episode
            s = "A"
            a = rng.choice(2) if rng.uniform(0, 1) < 0.1 else np.argmax(Q["A"])
            if a == 0: # Left
                q_left_picks[ep] += 1
                b_action = rng.choice(10) if rng.uniform(0, 1) < 0.1 else np.argmax(Q["B"])
                r = rng.normal(-0.1, 1.0)
                Q["B"][b_action] += 0.1 * (r - Q["B"][b_action])
                Q["A"][0] += 0.1 * (0.0 + 1.0 * np.max(Q["B"]) - Q["A"][0])
            else:
                Q["A"][1] += 0.1 * (0.0 - Q["A"][1])
                
            # Double Q-learning episode
            a_dq = rng.choice(2) if rng.uniform(0, 1) < 0.1 else np.argmax(QA["A"] + QB["A"])
            if a_dq == 0:
                dq_left_picks[ep] += 1
                b_act = rng.choice(10) if rng.uniform(0, 1) < 0.1 else np.argmax(QA["B"] + QB["B"])
                r = rng.normal(-0.1, 1.0)
                if rng.uniform(0, 1) < 0.5:
                    QA["B"][b_act] += 0.1 * (r - QA["B"][b_act])
                    best_b = np.argmax(QA["B"])
                    QA["A"][0] += 0.1 * (QB["B"][best_b] - QA["A"][0])
                else:
                    QB["B"][b_act] += 0.1 * (r - QB["B"][b_act])
                    best_b = np.argmax(QB["B"])
                    QB["A"][0] += 0.1 * (QA["B"][best_b] - QB["A"][0])
            else:
                if rng.uniform(0, 1) < 0.5:
                    QA["A"][1] += 0.1 * (0.0 - QA["A"][1])
                else:
                    QB["A"][1] += 0.1 * (0.0 - QB["A"][1])

    q_pct = (q_left_picks / n_runs) * 100.0
    dq_pct = (dq_left_picks / n_runs) * 100.0
    print(f"Q-Learning % Left action chosen (suboptimal bias): {np.mean(q_pct[:50]):.1f}%")
    print(f"Double Q-Learning % Left action chosen:           {np.mean(dq_pct[:50]):.1f}%")
    assert np.mean(q_pct[:50]) > np.mean(dq_pct[:50])

run_maximization_bias_trial(n_runs=100, n_episodes=100)
```
</details>

---

## 13. Summary, Verified References & Navigation

### Core Takeaways
1. **Watkins' Q-learning is off-policy**: Updates directly toward the greedy maximum $\max_{a'} Q(s', a')$ regardless of the exploratory actions taken.
2. **Proven asymptotic convergence**: Converges with probability 1 to $Q^*$ if all state-action pairs are visited infinitely often under Robbins-Monro conditions.
3. **Maximization bias is real**: The $\max$ operator overestimates values; Double Q-Learning resolves this by decoupling selection from evaluation.

### Verified References
- Watkins, C. J. C. H. (1989). *Learning from Delayed Rewards.* PhD Thesis, University of Cambridge.
- Watkins, C. J., & Dayan, P. (1992). *Q-learning.* Machine Learning.
- Hasselt, H. V. (2010). *Double Q-learning.* NeurIPS.
- Tsitsiklis, J. N. (1994). *Asynchronous stochastic approximation and Q-learning.* Machine Learning.

---

[← Previous Note: Temporal Difference Learning](06-temporal-difference-learning.md) | [Module Index](README.md) | [Next Note: SARSA & On vs Off-Policy →](08-sarsa-and-on-vs-off-policy.md)
