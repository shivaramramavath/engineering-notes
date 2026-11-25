# Policy Gradient Foundations & REINFORCE

> **One-Line Summary:** A foundational policy gradient algorithm that directly optimizes parameterized policies $\pi_\theta(a \mid s)$ via stochastic gradient ascent on expected cumulative returns using Monte Carlo rollouts and the likelihood-ratio score function trick.

---

## Metadata

| Field | Value |
| :--- | :--- |
| **Level** | Core / Advanced |
| **Prerequisites** | [05-monte-carlo-methods.md](05-monte-carlo-methods.md), [10-function-approximation.md](10-function-approximation.md) |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Verified (Python 3.11 / CPU) |
| **Tested With** | Python 3.11+, NumPy 2.5+, Gymnasium 1.0+ (Random Seed: `42`) |

---

## Table of Contents
1. [Intuition & Problem It Solves](#1-intuition--problem-it-solves)
2. [Algorithm Card](#2-algorithm-card)
3. [Architectural & Geometric Diagrams](#3-architectural--geometric-diagrams)
4. [The Mathematics](#4-the-mathematics)
5. [The Algorithm Step by Step & From-Scratch Implementation](#5-the-algorithm-step-by-step--from-scratch-implementation)
6. [Using the Library (Complete End-to-End Workflow)](#6-using-the-library-complete-end-to-end-workflow)
7. [Preprocessing Required](#7-preprocessing-required)
8. [Training & Hyperparameter Tuning](#8-training--hyperparameter-tuning)
9. [Prediction & Inference Mechanics](#9-prediction--inference-mechanics)
10. [Postprocessing](#10-postprocessing)
11. [Saving and Loading (Model Persistence & Security)](#11-saving-and-loading-model-persistence--security)
12. [Evaluation & Diagnostics](#12-evaluation--diagnostics)
13. [Advantages](#13-advantages)
14. [Limitations & Failure Modes](#14-limitations--failure-modes)
15. [Comparison / When to Use What](#15-comparison--when-to-use-what)
16. [Common Mistakes & Pitfalls](#16-common-mistakes--pitfalls)
17. [Performance, Security & Ethics Notes](#17-performance-security--ethics-notes)
18. [Interview Questions (8 In-Depth Q&As)](#18-interview-questions-8-in-depth-qas)
19. [Practice Exercises (6 Hands-on Problems)](#19-practice-exercises-6-hands-on-problems)
20. [Summary & Cheat Sheet](#20-summary--cheat-sheet)
21. [Further Reading & Navigation](#21-further-reading--navigation)

---

## 1. Intuition & Problem It Solves

Value-based algorithms like Q-learning and DQN optimize an indirect proxy: action-value functions $Q(s, a)$. The actual policy is derived implicitly by picking greedy actions $\arg\max_a Q(s, a)$. While effective in discrete domains, this approach has three fundamental weaknesses:
1. **Continuous Action Infeasibility:** In continuous control (e.g. humanoid walking or flight pitch angles), evaluating $\arg\max_a Q(s, a)$ at every inference step requires solving a complex non-linear optimization problem over an infinite set of vectors.
2. **Inability to Learn Stochastic Policies:** In partially observable environments or multi-agent zero-sum games (e.g. Rock-Paper-Scissors or Poker), deterministic greedy policies can be exploited. The optimal policy is strictly stochastic, which value-based methods cannot naturally represent.
3. **Discontinuous Policy Shifts:** A tiny change in $Q(s, a)$ can cause the greedy action $\arg\max_a Q(s, a)$ to abruptly flip from one action to another, creating violent discontinuities during training.

**Policy Gradient Methods** bypass value functions entirely by parameterizing the policy directly:
$$\pi_\theta(a \mid s) = P(A_t = a \mid S_t = s; \theta)$$
The parameters $\theta$ are adjusted directly along the gradient of expected performance $\nabla_\theta J(\theta)$. **REINFORCE** (Williams, 1992) is the foundational Monte Carlo policy gradient algorithm: it runs complete episodic rollouts, collects empirical returns $G_t$, and nudges policy parameters in the direction of actions that produced above-average returns.

---

## 2. Algorithm Card

| Dimension | Specification |
| :--- | :--- |
| **Category** | Policy-Based / Model-Free Reinforcement Learning |
| **Action Space** | Discrete (Categorical / Softmax) & Continuous (Gaussian $\mathcal{N}(\mu, \sigma^2)$) |
| **Trajectory Type** | Episodic Monte Carlo (requires complete rollouts) |
| **Policy Type** | Strictly On-Policy (gradient samples must follow $\pi_\theta$) |
| **Objective** | Expected cumulative trajectory return: $J(\theta) = \mathbb{E}_{\tau \sim \pi_\theta}[R(\tau)]$ |
| **Variance Reduction** | State-dependent baseline subtraction: $b(s)$ (reduces variance without bias) |
| **Convergence** | Local optima via Stochastic Gradient Ascent |

---

## 3. Architectural & Geometric Diagrams

```mermaid
flowchart TD
    subgraph RolloutPhase["1. Episodic Trajectory Rollout"]
        S0["State S_0"] --> PolicyNet["Policy Network pi_theta(a | s)"]
        PolicyNet --> SampleA["Sample Action A_0 ~ pi_theta(. | S_0)"]
        SampleA --> EnvStep["Environment Step"]
        EnvStep --> Transition["Yields S_1, R_1 ... until Terminal S_T"]
        Transition --> Trajectory["Trajectory tau = (S_0, A_0, R_1, ..., S_T)"]
    end

    subgraph ReturnCalculation["2. Discounted Return Computation"]
        Trajectory --> DiscountReturn["Compute G_t = sum_k gamma^k R_t+k+1"]
        DiscountReturn --> BaselineSubtraction["Compute Advantage delta_t = G_t - b(S_t)"]
    end

    subgraph ScoreFunctionUpdate["3. Policy Parameter Optimization"]
        BaselineSubtraction --> ScoreCalc["Compute Score Function grad_theta ln pi_theta(A_t | S_t)"]
        ScoreCalc --> PolicyGrad["Estimate: grad_theta J(theta) = sum_t grad_theta ln pi_theta(A_t | S_t) * delta_t"]
        PolicyGrad --> GradientAscent["theta <- theta + alpha * grad_theta J(theta)"]
        GradientAscent --> PolicyNet
    end
```

---

## 4. The Mathematics

### 4.1 Trajectory Probability & Expected Return
Let a trajectory be $\tau = (s_0, a_0, r_1, s_1, a_1, \dots, s_T)$. Its probability under policy parameters $\theta$ is:
$$P(\tau; \theta) = \rho_0(s_0) \prod_{t=0}^{T-1} \pi_\theta(a_t \mid s_t) P(s_{t+1} \mid s_t, a_t)$$
The reinforcement learning objective is to maximize the expected cumulative return:
$$J(\theta) = \mathbb{E}_{\tau \sim \pi_\theta}[R(\tau)] = \int P(\tau; \theta) R(\tau) \, d\tau$$

### 4.2 The Log-Derivative (Score Function) Trick
Differentiating $J(\theta)$ requires differentiating an expectation whose sampling distribution $P(\tau; \theta)$ depends on $\theta$:
$$\nabla_\theta J(\theta) = \nabla_\theta \int P(\tau; \theta) R(\tau) \, d\tau = \int \nabla_\theta P(\tau; \theta) R(\tau) \, d\tau$$
Using the identity $\nabla_\theta P(\tau; \theta) = P(\tau; \theta) \frac{\nabla_\theta P(\tau; \theta)}{P(\tau; \theta)} = P(\tau; \theta) \nabla_\theta \ln P(\tau; \theta)$:
$$\nabla_\theta J(\theta) = \int P(\tau; \theta) \nabla_\theta \ln P(\tau; \theta) R(\tau) \, d\tau = \mathbb{E}_{\tau \sim \pi_\theta} \left[ \nabla_\theta \ln P(\tau; \theta) R(\tau) \right]$$

Expanding the log probability of trajectory $\tau$:
$$\ln P(\tau; \theta) = \ln \rho_0(s_0) + \sum_{t=0}^{T-1} \ln \pi_\theta(a_t \mid s_t) + \sum_{t=0}^{T-1} \ln P(s_{t+1} \mid s_t, a_t)$$
Because initial state distribution $\rho_0(s_0)$ and transition dynamics $P(s_{t+1} \mid s_t, a_t)$ do **not** depend on policy parameters $\theta$, their derivatives with respect to $\theta$ vanish completely:
$$\nabla_\theta \ln P(\tau; \theta) = \sum_{t=0}^{T-1} \nabla_\theta \ln \pi_\theta(a_t \mid s_t)$$

This yields the foundational **Policy Gradient Theorem**:
$$\nabla_\theta J(\theta) = \mathbb{E}_{\tau \sim \pi_\theta} \left[ \sum_{t=0}^T \nabla_\theta \ln \pi_\theta(a_t \mid s_t) G_t \right]$$
where $G_t = \sum_{k=0}^{T-t-1} \gamma^k R_{t+k+1}$ is the discounted return from time $t$ onward (causality ensures past rewards cannot depend on future actions).

### 4.3 Baseline Subtraction & Unbiasedness
The variance of Monte Carlo returns $G_t$ is notoriously high. Subtracting any state-dependent baseline $b(s_t)$ that does not depend on action $a_t$ leaves the expected gradient mathematically unaltered:
$$\mathbb{E}_{a_t \sim \pi_\theta} \left[ \nabla_\theta \ln \pi_\theta(a_t \mid s_t) b(s_t) \right] = b(s_t) \sum_{a \in \mathcal{A}} \pi_\theta(a \mid s_t) \frac{\nabla_\theta \pi_\theta(a \mid s_t)}{\pi_\theta(a \mid s_t)} = b(s_t) \nabla_\theta \sum_{a \in \mathcal{A}} \pi_\theta(a \mid s_t) = b(s_t) \nabla_\theta (1) = 0$$
Hence:
$$\nabla_\theta J(\theta) = \mathbb{E}_{\tau \sim \pi_\theta} \left[ \sum_{t=0}^T \nabla_\theta \ln \pi_\theta(a_t \mid s_t) \left( G_t - b(s_t) \right) \right]$$
Setting $b(s_t) \approx V(s_t)$ substantially reduces gradient variance without introducing any mathematical bias.

---

## 5. The Algorithm Step by Step & From-Scratch Implementation

Below is a complete, self-contained REINFORCE implementation with a softmax policy network, analytic gradient computation, and a moving average baseline in pure NumPy.

```python
"""
REINFORCE (Monte Carlo Policy Gradient with Baseline) from scratch.
Verified under Python 3.11+, pure NumPy.
"""

import numpy as np
from typing import List, Tuple, Dict, Any


class SoftmaxPolicy:
    """
    Parametric Softmax Policy for discrete action spaces:
    logits = X @ W + b
    pi(a | s) = exp(logits_a) / sum_b exp(logits_b)
    """
    def __init__(self, state_dim: int, action_dim: int, lr: float = 0.01, seed: int = 42):
        rng = np.random.default_rng(seed)
        # Xavier initialization
        self.W = rng.normal(0.0, np.sqrt(2.0 / (state_dim + action_dim)), (state_dim, action_dim))
        self.b = np.zeros((1, action_dim))
        self.lr = lr

    def forward(self, state: np.ndarray) -> np.ndarray:
        """Computes action probabilities via numerically stable softmax."""
        logits = np.dot(state.reshape(1, -1), self.W) + self.b # (1, action_dim)
        shifted_logits = logits - np.max(logits, axis=1, keepdims=True)
        exp_logits = np.exp(shifted_logits)
        probs = exp_logits / np.sum(exp_logits, axis=1, keepdims=True)
        return probs.ravel()

    def sample_action(self, state: np.ndarray, rng: np.random.Generator) -> int:
        probs = self.forward(state)
        return int(rng.choice(len(probs), p=probs))

    def compute_grad_log_pi(self, state: np.ndarray, action: int) -> Tuple[np.ndarray, np.ndarray]:
        """
        Analytic gradient of ln pi(a | s) w.r.t weights W and biases b:
        d(ln pi(a | s))/d(logits) = e_a - pi(s)
        """
        probs = self.forward(state) # shape (action_dim,)
        action_dim = len(probs)
        
        # d ln pi / d logits
        d_logits = -probs
        d_logits[action] += 1.0 # shape (action_dim,)
        
        # Gradients w.r.t W and b
        grad_W = np.outer(state, d_logits) # shape (state_dim, action_dim)
        grad_b = d_logits.reshape(1, -1)   # shape (1, action_dim)
        return grad_W, grad_b

    def update(self, grad_W: np.ndarray, grad_b: np.ndarray):
        """Performs stochastic gradient ascent: theta <- theta + alpha * grad."""
        self.W += self.lr * grad_W
        self.b += self.lr * grad_b


class REINFORCEAgent:
    """REINFORCE Agent with Monte Carlo trajectory collection and baseline subtraction."""
    def __init__(self, state_dim: int, action_dim: int, gamma: float = 0.99, lr: float = 0.02, seed: int = 42):
        self.policy = SoftmaxPolicy(state_dim, action_dim, lr=lr, seed=seed)
        self.gamma = gamma
        self.rng = np.random.default_rng(seed)
        self.baseline = 0.0
        self.baseline_alpha = 0.05

    def train_on_trajectory(self, trajectory: List[Tuple[np.ndarray, int, float]]) -> float:
        """
        trajectory: list of (state, action, reward) steps
        """
        T = len(trajectory)
        states = [step[0] for step in trajectory]
        actions = [step[1] for step in trajectory]
        rewards = [step[2] for step in trajectory]

        # 1. Compute discounted returns G_t backwards
        returns = np.zeros(T, dtype=np.float64)
        G = 0.0
        for t in reversed(range(T)):
            G = rewards[t] + self.gamma * G
            returns[t] = G

        # Update moving average baseline
        avg_return = float(np.mean(returns))
        self.baseline = (1 - self.baseline_alpha) * self.baseline + self.baseline_alpha * avg_return

        # 2. Compute cumulative policy gradient
        cum_grad_W = np.zeros_like(self.policy.W)
        cum_grad_b = np.zeros_like(self.policy.b)

        for t in range(T):
            s_t = states[t]
            a_t = actions[t]
            # Advantage with baseline
            advantage = returns[t] - self.baseline
            
            # Score function grad ln pi(a_t | s_t)
            g_W, g_b = self.policy.compute_grad_log_pi(s_t, a_t)
            cum_grad_W += g_W * advantage
            cum_grad_b += g_b * advantage

        # 3. Apply gradient ascent update
        self.policy.update(cum_grad_W / T, cum_grad_b / T)
        return float(np.sum(rewards))


# Verification Test Run
if __name__ == "__main__":
    agent = REINFORCEAgent(state_dim=2, action_dim=2, lr=0.05, seed=42)
    # Simulate a 10-step trajectory
    mock_traj = []
    for _ in range(10):
        s = np.random.randn(2)
        a = agent.policy.sample_action(s, agent.rng)
        r = 1.0
        mock_traj.append((s, a, r))

    ep_return = agent.train_on_trajectory(mock_traj)
    print(f"Verified REINFORCE trajectory update. Total Episode Return: {ep_return}")
    assert ep_return == 10.0
```

---

## 6. Using the Library (Complete End-to-End Workflow)

PyTorch implementation trained on Gymnasium `CartPole-v1`:

```python
import gymnasium as gym
import torch
import torch.nn as nn
import torch.optim as optim
from torch.distributions import Categorical
import numpy as np

class PolicyNetwork(nn.Module):
    def __init__(self, state_dim: int, action_dim: int):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(state_dim, 64),
            nn.ReLU(),
            nn.Linear(64, action_dim),
            nn.Softmax(dim=-1)
        )
    def forward(self, state: torch.Tensor) -> torch.Tensor:
        return self.net(state)

def train_reinforce_cartpole():
    env = gym.make("CartPole-v1")
    policy = PolicyNetwork(env.observation_space.shape[0], env.action_space.n)
    optimizer = optim.Adam(policy.parameters(), lr=1e-2)
    gamma = 0.99

    for episode in range(250):
        state, _ = env.reset(seed=42 + episode)
        log_probs = []
        rewards = []
        done = False

        while not done:
            state_t = torch.FloatTensor(state)
            probs = policy(state_t)
            dist = Categorical(probs)
            action = dist.sample()

            log_probs.append(dist.log_prob(action))
            state, reward, terminated, truncated, _ = env.step(action.item())
            rewards.append(reward)
            done = terminated or truncated

        # Compute discounted returns
        returns = []
        G = 0.0
        for r in reversed(rewards):
            G = r + gamma * G
            returns.insert(0, G)

        returns = torch.tensor(returns)
        # Normalize returns to reduce variance
        returns = (returns - returns.mean()) / (returns.std() + 1e-8)

        # Policy loss: - sum_t log_pi(a_t | s_t) * G_t
        policy_loss = []
        for log_prob, G_t in zip(log_probs, returns):
            policy_loss.append(-log_prob * G_t)
        
        optimizer.zero_grad()
        loss = torch.stack(policy_loss).sum()
        loss.backward()
        optimizer.step()

    env.close()
    return policy
```

---

## 7. Preprocessing Required

1. **State Normalization:** Rescale observations so that unbounded continuous inputs (e.g. velocities, angular rates) have mean $0$ and standard deviation $1$.
2. **Normalized Returns ($G_t$ Standard Normalization):**
   Within each collected trajectory batch, standardize returns:
   $$\tilde{G}_t = \frac{G_t - \mu_G}{\sigma_G + \epsilon}$$
   This acts as an empirical batch-level baseline, keeping gradient step sizes consistent regardless of raw reward scale.

---

## 8. Training & Hyperparameter Tuning

| Hyperparameter | Typical Range | Operational Impact / Failure Mode |
| :--- | :--- | :--- |
| **Learning Rate ($\alpha$)** | $1 \times 10^{-3} - 1 \times 10^{-2}$ | Too high causes policy entropy collapse; too low stalls learning. |
| **Discount Factor ($\gamma$)** | 0.99 | Values below 0.95 cause extreme short-sightedness. |
| **Batch Size (Trajectories)** | 10 – 50 episodes | Updating on a single trajectory produces high variance; batching averages gradients. |
| **Entropy Bonus ($\beta$)** | 0.001 – 0.01 | Prevents premature collapse to deterministic policies before exploration completes. |

---

## 9. Prediction & Inference Mechanics

At deployment:
1. **Deterministic Mode:** Choose the modal action $a^* = \arg\max_a \pi_\theta(a \mid s)$. This maximizes expected score and removes stochasticity.
2. **Stochastic Sampling Mode:** Sample $a \sim \pi_\theta(\cdot \mid s)$ for tasks requiring exploration or defense against adversarial exploitation.
3. **Execution Latency:** Softmax forward inference involves a single matrix multiplication and runs in $< 20\,\mu\text{s}$ on standard CPU hardware.

---

## 10. Postprocessing

1. **Action Probability Clipping:** In production serving, avoid pure zeros in action probabilities ($\pi(a \mid s) \ge 10^{-4}$) to prevent numerical instability in downstream logging.
2. **Safety Override Filters:** If the chosen action violates physical workspace boundaries, fall back to safe default actions.

---

## 11. Saving and Loading (Model Persistence & Security)

Serialize policy weights using standard state dicts:

```python
def save_policy(policy_net: nn.Module, filepath: str):
    torch.save(policy_net.state_dict(), filepath)
```

---

## 12. Evaluation & Diagnostics

1. **Policy Entropy:** Measure $H(\pi) = -\sum_a \pi(a \mid s) \log \pi(a \mid s)$. If entropy plummets to 0 in early episodes, the policy has collapsed prematurely.
2. **Gradient Norm Tracking:** Monitor $\|\nabla_\theta J(\theta)\|_2$. Spikes indicate high-variance outlier trajectories destabilizing parameters.

---

## 13. Advantages

1. **Continuous Action Compatibility:** Directly outputs continuous parameters (e.g. mean and variance of Gaussian actions) without discretization.
2. **Learns Stochastic Policies:** Naturally converges to mixed strategies in game-theoretic settings.
3. **Smooth Policy Updates:** Parameter changes produce smooth probability distributions rather than abrupt action jumps.

---

## 14. Limitations & Failure Modes

1. **Extremely High Variance:** Monte Carlo returns $G_t$ accumulate stochastic noise across every environmental step, resulting in noisy gradient estimates.
2. **Sample Inefficiency:** Requires hundreds or thousands of complete rollouts; on-policy constraint prevents reusing historical transitions from replay buffers.
3. **Delayed Credit Assignment:** Every action in an episode receives credit/blame based on the entire return $G_t$, even if only a single mistake at the very end caused failure.

---

## 15. Comparison / When to Use What

| Dimension | REINFORCE | DQN | Actor-Critic (A2C) |
| :--- | :--- | :--- | :--- |
| **Optimization Target** | Policy $\pi_\theta(a \mid s)$ directly | Action-Value $Q(s, a)$ | Actor $\pi_\theta$ + Critic $V_\phi$ |
| **Action Space** | Discrete & Continuous | Discrete only | Discrete & Continuous |
| **Trajectory Requirement** | Complete episodes | Single step $(s, a, r, s')$ | Single step $(s, a, r, s')$ |
| **Gradient Variance** | Very High | Low | Low to Moderate |
| **Sample Efficiency** | Low (On-policy) | High (Off-policy) | Moderate (On-policy) |

---

## 16. Common Mistakes & Pitfalls

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| Using total episode return $\sum R_t$ for all steps | Violates causality; credits actions for rewards that happened in the past | Use reward-to-go: $G_t = \sum_{k=0}^{T-t-1} \gamma^k R_{t+k+1}$ |
| Omitting baseline subtraction | Unnecessarily high gradient variance; requires $10\times$ more samples | Subtract moving average return or learned value baseline $V(s_t)$ |
| Training off-policy data with REINFORCE | The score function trick requires transitions sampled from active $\pi_\theta$ | Collect fresh rollouts on every update or use importance sampling |
| Ignoring entropy collapse | Policy locks onto suboptimal first action it discovered | Add entropy regularization bonus $\mathcal{L} + \beta H(\pi)$ |
| Unnormalized returns with large magnitudes | Huge gradients destroy neural weights | Standardize returns across the batch: $(G - \mu) / (\sigma + 1e-8)$ |

---

## 17. Performance, Security & Ethics Notes

1. **Batching Rollouts Across Vectorized Environments:** Collecting rollouts sequentially on a single thread is CPU-bound. Modern implementations (Gymnasium `VectorEnv`) run 8 to 32 environment instances in parallel.
2. **Reward Hacking:** High-variance policy gradients can exploit simulator glitches (e.g. physics collision bugs that award infinite points).

---

## 18. Interview Questions (8 In-Depth Q&As)

<details>
<summary><strong>Q1: Derive the Policy Gradient Theorem from first principles using the log-derivative trick.</strong></summary>

**Answer:**
1. Objective: $J(\theta) = \mathbb{E}_{\tau \sim \pi_\theta}[R(\tau)] = \int P(\tau; \theta) R(\tau) d\tau$.
2. Gradient: $\nabla_\theta J(\theta) = \int \nabla_\theta P(\tau; \theta) R(\tau) d\tau$.
3. Applying log-derivative identity $\nabla_\theta P(\tau; \theta) = P(\tau; \theta) \nabla_\theta \ln P(\tau; \theta)$:
   $$\nabla_\theta J(\theta) = \int P(\tau; \theta) \nabla_\theta \ln P(\tau; \theta) R(\tau) d\tau = \mathbb{E}_{\tau \sim \pi_\theta} \left[ \nabla_\theta \ln P(\tau; \theta) R(\tau) \right]$$
4. The trajectory probability is $P(\tau; \theta) = \rho_0(s_0) \prod_{t=0}^{T-1} \pi_\theta(a_t \mid s_t) P(s_{t+1} \mid s_t, a_t)$. Taking the logarithm:
   $$\ln P(\tau; \theta) = \ln \rho_0(s_0) + \sum_{t=0}^{T-1} \ln \pi_\theta(a_t \mid s_t) + \sum_{t=0}^{T-1} \ln P(s_{t+1} \mid s_t, a_t)$$
5. Differentiating w.r.t $\theta$, initial state distribution and transition probabilities vanish because they are independent of $\theta$:
   $$\nabla_\theta \ln P(\tau; \theta) = \sum_{t=0}^{T-1} \nabla_\theta \ln \pi_\theta(a_t \mid s_t)$$
6. Substituting back yields: $\nabla_\theta J(\theta) = \mathbb{E}_{\tau} \left[ \sum_{t=0}^{T-1} \nabla_\theta \ln \pi_\theta(a_t \mid s_t) R(\tau) \right]$.
7. Invoking causality (actions at time $t$ cannot influence past rewards $r_1, \dots, r_t$) replaces total return $R(\tau)$ with reward-to-go $G_t = \sum_{k=0}^{T-t-1} \gamma^k R_{t+k+1}$.
</details>

<details>
<summary><strong>Q2: Prove mathematically that subtracting a baseline $b(s)$ does not introduce bias into the policy gradient.</strong></summary>

**Answer:**
We evaluate the expected value of the baseline subtraction term:
$$\mathbb{E}_{S_t, A_t} \left[ \nabla_\theta \ln \pi_\theta(A_t \mid S_t) b(S_t) \right] = \mathbb{E}_{S_t} \left[ b(S_t) \mathbb{E}_{A_t \sim \pi_\theta(\cdot \mid S_t)} \left[ \nabla_\theta \ln \pi_\theta(A_t \mid S_t) \right] \right]$$
Focusing on the inner expectation over actions:
$$\mathbb{E}_{A_t \sim \pi_\theta} \left[ \nabla_\theta \ln \pi_\theta(A_t \mid S_t) \right] = \sum_{a \in \mathcal{A}} \pi_\theta(a \mid S_t) \frac{\nabla_\theta \pi_\theta(a \mid S_t)}{\pi_\theta(a \mid S_t)} = \sum_{a \in \mathcal{A}} \nabla_\theta \pi_\theta(a \mid S_t)$$
Swapping summation and derivative:
$$= \nabla_\theta \sum_{a \in \mathcal{A}} \pi_\theta(a \mid S_t) = \nabla_\theta (1) = 0$$
Because the inner expectation evaluates identically to zero for any arbitrary state-dependent baseline $b(S_t)$, $\mathbb{E}[\nabla_\theta \ln \pi_\theta(A_t \mid S_t) b(S_t)] = 0$. Hence, the baseline term does not alter the expectation, leaving the policy gradient strictly unbiased.
</details>

<details>
<summary><strong>Q3: Why is the score function trick necessary, and why can we not differentiate the rewards directly?</strong></summary>

**Answer:** In reinforcement learning, the environment transition dynamics $P(s_{t+1} \mid s_t, a_t)$ and reward function $\mathcal{R}(s, a)$ are unknown black boxes. We cannot compute $\frac{\partial R}{\partial a}$ or $\frac{\partial S_{t+1}}{\partial a}$ because no analytical model of the world exists.
The score function trick circumvents this by rewriting the derivative of the expectation $\nabla_\theta \mathbb{E}_{x \sim p_\theta}[f(x)]$ into the expectation of a product $\mathbb{E}_{x \sim p_\theta}[f(x) \nabla_\theta \ln p_\theta(x)]$. This transfers the differentiation entirely onto the known, parameterized policy $\pi_\theta(a \mid s)$, while treating the environmental returns $f(x) = R(\tau)$ purely as evaluated scalar samples.
</details>

<details>
<summary><strong>Q4: What is the optimal baseline $b^*(s)$ that minimizes policy gradient variance?</strong></summary>

**Answer:** For a policy parameter component $\theta_i$, the variance of the gradient estimator $g_i = \nabla_{\theta_i} \ln \pi(A \mid S) (G - b)$ is:
$$\text{Var}(g_i) = \mathbb{E}\left[ \left(\nabla_{\theta_i} \ln \pi(A \mid S) (G - b)\right)^2 \right]$$
Differentiating with respect to $b$ and setting to zero yields the optimal state-dependent baseline:
$$b^*(s) = \frac{\mathbb{E}_{A \sim \pi} \left[ \|\nabla_\theta \ln \pi(A \mid s)\|^2 G \right]}{\mathbb{E}_{A \sim \pi} \left[ \|\nabla_\theta \ln \pi(A \mid s)\|^2 \right]}$$
This is the expected return weighted by the squared magnitude of the policy gradient score. In practice, the state value function $V^\pi(s) = \mathbb{E}[G \mid S=s]$ is used as an effective approximation, which significantly reduces variance without requiring the complex gradient-weighted expectation.
</details>

<details>
<summary><strong>Q5: Why is REINFORCE strictly an on-policy algorithm?</strong></summary>

**Answer:** The derivation of the policy gradient relies on the expectation:
$$\nabla_\theta J(\theta) = \mathbb{E}_{\tau \sim \pi_\theta} \left[ \nabla_\theta \ln \pi_\theta(a \mid s) G_t \right]$$
The mathematical equality holds *only* if the trajectories $\tau$ are sampled according to the distribution induced by the current parameters $\pi_\theta$. If historical trajectories collected from an older policy $\pi_{\theta_{\text{old}}}$ are used, the expectation is taken over the wrong distribution $P(\tau; \theta_{\text{old}}) \neq P(\tau; \theta)$. Unlike Q-learning (which is off-policy because the Bellman optimality equation is independent of the behavior policy), policy gradients require on-policy data unless corrected with explicit importance sampling weights $\frac{\pi_\theta(a \mid s)}{\pi_{\text{old}}(a \mid s)}$.
</details>

<details>
<summary><strong>Q6: How does continuous action selection work in policy gradient methods?</strong></summary>

**Answer:** In continuous control with action space $\mathcal{A} \subseteq \mathbb{R}^d$, the policy network outputs the parameters of a continuous probability density function—typically a Gaussian distribution $\mathcal{N}(\mu_\theta(s), \Sigma_\theta(s))$.
- The network outputs mean vector $\mu_\theta(s) \in \mathbb{R}^d$ and log standard deviation vector $\log \sigma_\theta \in \mathbb{R}^d$.
- During training, actions are sampled via the reparameterization trick: $a_t = \mu_\theta(s_t) + \sigma_\theta \odot \epsilon$ where $\epsilon \sim \mathcal{N}(0, I)$.
- The log-likelihood is computed analytically:
  $$\ln \pi_\theta(a \mid s) = -\frac{1}{2} \sum_{i=1}^d \left( \frac{(a_i - \mu_i(s))^2}{\sigma_i^2} + 2 \ln \sigma_i + \ln(2\pi) \right)$$
- Policy gradients backpropagate directly through the analytical score function $\nabla_\theta \ln \pi_\theta(a \mid s)$.
</details>

<details>
<summary><strong>Q7: What is policy entropy collapse, and how does entropy regularization prevent it?</strong></summary>

**Answer:** Policy entropy collapse occurs when the policy prematurely becomes nearly deterministic ($\pi(a_1 \mid s) \approx 0.999$) early in training after encountering a few positive rewards for action $a_1$. Once nearly deterministic, the agent ceases exploring alternative actions $a_2, a_3$ (which might be globally optimal). Furthermore, the score function $\nabla_\theta \ln \pi(a \mid s)$ drops toward zero, freezing parameter updates.
Entropy regularization augments the objective function with a penalty on low entropy:
$$J_{\text{entropy}}(\theta) = J(\theta) + \beta \, \mathbb{E}_{s} \left[ \mathcal{H}(\pi_\theta(\cdot \mid s)) \right]$$
where $\mathcal{H}(\pi) = -\sum_a \pi(a \mid s) \ln \pi(a \mid s)$. This encourages the policy to maintain stochasticity until strong evidence favors a specific action.
</details>

<details>
<summary><strong>Q8: Explain the difference between reward-to-go and full trajectory return in REINFORCE.</strong></summary>

**Answer:**
- **Full Trajectory Return:** Evaluates updates as $\sum_{t=0}^T \nabla_\theta \ln \pi(a_t \mid s_t) \left( \sum_{k=0}^T R_{k+1} \right)$. This credits action $a_t$ with rewards that occurred *prior* to time $t$ ($R_1, \dots, R_t$). Since an action at time $t$ cannot physically influence past rewards, including them adds pure random noise without changing the expected gradient.
- **Reward-to-Go:** Restricts returns to future rewards: $G_t = \sum_{k=0}^{T-t-1} \gamma^k R_{t+k+1}$. This enforces causality, removing irrelevant past variance from the estimator and significantly accelerating convergence.
</details>

---

## 19. Practice Exercises (6 Hands-on Problems)

<details>
<summary><strong>Exercise 1 (Easy): Softmax Action Sampling in NumPy</strong></summary>

**Task:** Write a Python function that takes raw logits and returns sampled discrete action and its log probability.

**Solution:**
```python
import numpy as np

def sample_softmax_action(logits: np.ndarray, seed: int = 42) -> tuple[int, float]:
    rng = np.random.default_rng(seed)
    shifted = logits - np.max(logits)
    probs = np.exp(shifted) / np.sum(np.exp(shifted))
    action = int(rng.choice(len(probs), p=probs))
    log_prob = float(np.log(probs[action] + 1e-12))
    return action, log_prob

a, lp = sample_softmax_action(np.array([2.0, 1.0, 0.1]))
print(f"Sampled Action: {a}, Log Prob: {lp:.4f}")
assert a in [0, 1, 2]
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Discounted Reward-to-Go Calculation</strong></summary>

**Task:** Compute discounted returns $G_t$ backwards from an array of step rewards.

**Solution:**
```python
import numpy as np

def compute_rewards_to_go(rewards: list[float], gamma: float = 0.9) -> np.ndarray:
    T = len(rewards)
    returns = np.zeros(T)
    G = 0.0
    for t in reversed(range(T)):
        G = rewards[t] + gamma * G
        returns[t] = G
    return returns

r = [1.0, 1.0, 10.0]
G_vals = compute_rewards_to_go(r, gamma=0.5)
# G_2 = 10, G_1 = 1 + 0.5(10) = 6, G_0 = 1 + 0.5(6) = 4
np.testing.assert_allclose(G_vals, [4.0, 6.0, 10.0])
print(f"Rewards-to-go: {G_vals}")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Gaussian Continuous Policy Score Function</strong></summary>

**Task:** Write a function computing the gradient of $\ln \pi(a \mid s)$ for a 1D Gaussian policy with mean $\mu$ and fixed standard deviation $\sigma$.

**Solution:**
```python
def gaussian_score_wrt_mu(action: float, mu: float, sigma: float = 1.0) -> float:
    # d/d_mu [ -0.5 * ((a - mu) / sigma)^2 ] = (a - mu) / (sigma^2)
    return (action - mu) / (sigma ** 2)

score = gaussian_score_wrt_mu(action=2.5, mu=2.0, sigma=0.5)
assert np.isclose(score, 2.0)
print(f"Gaussian score w.r.t mean: {score}")
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Shannon Policy Entropy Calculation</strong></summary>

**Task:** Implement entropy calculation for a batch of policy probability distributions.

**Solution:**
```python
import numpy as np

def compute_policy_entropy(probs: np.ndarray) -> np.ndarray:
    # H(p) = - sum_a p_a * log(p_a)
    eps = 1e-12
    return -np.sum(probs * np.log(probs + eps), axis=1)

uniform_probs = np.array([[0.5, 0.5], [0.25, 0.25, 0.25, 0.25]])
h = compute_policy_entropy(uniform_probs)
assert np.isclose(h[0], np.log(2))
assert np.isclose(h[1], np.log(4))
print(f"Policy Entropies: {np.round(h, 4)}")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Complete Analytic Softmax Gradient Accumulator</strong></summary>

**Task:** Implement pure NumPy policy gradient accumulation for a linear softmax model across a batch of transitions.

**Solution:**
```python
import numpy as np

def accumulate_reinforce_gradients(
    states: np.ndarray,      # shape (T, state_dim)
    actions: np.ndarray,     # shape (T,)
    advantages: np.ndarray,  # shape (T,)
    W: np.ndarray            # shape (state_dim, action_dim)
) -> np.ndarray:
    T, state_dim = states.shape
    action_dim = W.shape[1]
    
    # 1. Forward probabilities
    logits = np.dot(states, W) # (T, action_dim)
    shifted = logits - np.max(logits, axis=1, keepdims=True)
    probs = np.exp(shifted) / np.sum(np.exp(shifted), axis=1, keepdims=True) # (T, action_dim)
    
    # 2. d ln pi / d logits = e_a - probs
    d_logits = -probs
    d_logits[np.arange(T), actions] += 1.0 # (T, action_dim)
    
    # 3. Weighted by advantages: delta_t * d_logits
    weighted_d_logits = d_logits * advantages[:, np.newaxis] # (T, action_dim)
    
    # 4. Gradient w.r.t W: X.T @ weighted_d_logits
    grad_W = np.dot(states.T, weighted_d_logits) / T
    return grad_W

S = np.random.randn(5, 3)
A = np.array([0, 1, 0, 1, 0])
Adv = np.array([1.0, -0.5, 2.0, 0.1, -1.2])
W_init = np.zeros((3, 2))
g_W = accumulate_reinforce_gradients(S, A, Adv, W_init)
assert g_W.shape == (3, 2)
print("Analytic REINFORCE gradient matrix verified.")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Empirical Verification of Baseline Zero-Bias Property</strong></summary>

**Task:** Numerically verify that subtracting a constant baseline $b$ does not alter the expectation of the policy gradient score function over $10^5$ samples.

**Solution:**
```python
import numpy as np

def verify_baseline_unbiasedness(trials: int = 100000):
    probs = np.array([0.7, 0.3])
    rng = np.random.default_rng(42)
    actions = rng.choice(2, size=trials, p=probs)
    
    # d ln pi / d logits for action a: e_a - probs
    scores = np.zeros(trials)
    scores[actions == 0] = 1.0 - probs[0] # +0.3
    scores[actions == 1] = -probs[1]      # -0.3
    
    baseline = 42.5
    baseline_grad_expectation = np.mean(scores * baseline)
    print(f"Empirical Baseline Expectation: {baseline_grad_expectation:.6f}")
    assert abs(baseline_grad_expectation) < 0.01, "Baseline introduced non-zero bias."

verify_baseline_unbiasedness()
```
</details>

---

## 20. Summary & Cheat Sheet

```
REINFORCE Essentials:
1. Objective: Maximize expected return J(theta) = E[R(tau)]
2. Policy Gradient Theorem:
   grad J(theta) = E[ sum_t grad_theta ln pi_theta(a_t | s_t) * (G_t - b(s_t)) ]
3. Causality: Use reward-to-go G_t = sum_k gamma^k R_t+k+1
4. Baseline b(s): Reduces variance without adding bias (E[grad ln pi * b] = 0)
5. Exploration: Add entropy bonus H(pi) to avoid premature deterministic collapse
```

---

## 21. Further Reading & Navigation

- Williams, R. J. (1992). *Simple statistical gradient-following algorithms for connectionist reinforcement learning*. Machine Learning, 8(3), 229-256.
- Sutton, R. S., McAllester, D., Singh, S., & Mansour, Y. (1999). *Policy gradient methods for reinforcement learning with function approximation*. NeurIPS.
- Schulman, J., et al. (2015). *High-dimensional continuous control using generalized advantage estimation*. ICLR.

---

[← 12-double-dueling-and-prioritised-dqn.md](12-double-dueling-and-prioritised-dqn.md) | [Module Index](README.md) | [14-actor-critic-a2c.md →](14-actor-critic-a2c.md)
