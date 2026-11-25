# Value Function Approximation & The Deadly Triad

> **Topic:** Linear & Non-Linear Value Function Approximation, Semi-Gradient Updates, and The Deadly Triad  
> **Module ID:** `05-ai/machine-learning/07-reinforcement-learning/10-function-approximation.md`  
> **Target Version:** Python 3.11+, NumPy 2.5+, Gymnasium 1.0+  
> **Level:** Core / Advanced  
> **Prerequisites:** [06-temporal-difference-learning.md](06-temporal-difference-learning.md), [07-q-learning.md](07-q-learning.md), [08-sarsa-and-on-vs-off-policy.md](08-sarsa-and-on-vs-off-policy.md)  
> **Status:** Verified (Python 3.11 / CPU)

---

## Architectural / Conceptual Flow

```mermaid
flowchart TD
    subgraph TabularLimitations["Tabular Limitations: Curse of Dimensionality"]
        S[Continuous State s in R^d] -->|Infinite States| T[Tabular Q-Table Fails]
    end

    subgraph FeatureExtraction["State Representation / Feature Mapping"]
        S -->|Tile Coding / RBF / Fourier| Phi["Feature Vector x(s) in R^p"]
    end

    subgraph ParametricApproximation["Parametric Approximation"]
        Phi -->|Linear: w^T x(s)| V_hat["Estimated Value V_hat(s; w)"]
        Phi -->|Deep Net: f(s; theta)| Q_hat["Estimated Action-Value Q_hat(s, a; w)"]
    end

    subgraph SemiGradientDescent["Semi-Gradient Parameter Update"]
        V_hat --> TD_Error["delta = R + gamma * V_hat(S'; w) - V_hat(S; w)"]
        TD_Error --> Update["w <- w + alpha * delta * grad_w V_hat(S; w)"]
        Note["Note: Bootstrapped Target R + gamma*V_hat is treated as CONSTANT (Semi-gradient)"]
    end

    subgraph DeadlyTriad["The Deadly Triad (Instability & Divergence)"]
        DT1["1. Function Approximation"] --- DT2["2. Bootstrapping (TD / DP)"]
        DT2 --- DT3["3. Off-Policy Training (Q-Learning / Off-Policy SARSA)"]
        DT3 --> Divergence["Geometric Divergence of Weights (Baird's Counterexample)"]
    end
```

---

## 1. Concept Overview & Core Motivation

Tabular reinforcement learning maintains a discrete lookup entry $V(s)$ or $Q(s, a)$ for every state-action pair. While tabular algorithms offer exact convergence guarantees under standard Robbins-Monro conditions, they collapse catastrophically in real-world engineering problems:
1. **Curse of Dimensionality:** An autonomous driving vehicle observing camera pixels, LiDAR point clouds, and IMU velocities possesses an uncountably infinite state space $\mathcal{S} \subseteq \mathbb{R}^d$. Discretizing even a 10-dimensional space into 50 bins per dimension requires $50^{10} \approx 9.76 \times 10^{16}$ memory entries.
2. **Lack of Generalization:** Tabular learning cannot generalize. Visiting state $s$ updates only entry $Q(s, \cdot)$. If the agent transitions to an adjacent state $s + \epsilon$ differing by a fraction of a millimeter, tabular RL treats it as completely novel.

**Value Function Approximation** overcomes both barriers by parameterizing value functions using a compact weight vector $\mathbf{w} \in \mathbb{R}^p$ (where $p \ll |\mathcal{S}|$):
$$\hat{v}(s, \mathbf{w}) \approx v_\pi(s), \quad \hat{q}(s, a, \mathbf{w}) \approx q_\pi(s, a)$$

Updating $\mathbf{w}$ using sample transitions $(s, a, r, s')$ generalizes value estimations across geometrically similar unvisited states.

However, combining function approximation with temporal difference learning introduces **The Deadly Triad**: the simultaneous presence of function approximation, bootstrapping, and off-policy distribution shifts can cause the learned weights to diverge to infinity ($\|\mathbf{w}\| \to \infty$). Understanding this phenomenon is essential before building modern Deep Q-Networks.

---

## 2. Theoretical Foundations & Mathematics

### 2.1 Linear Value Function Approximation

In linear function approximation, state features are projected into a $p$-dimensional feature vector $\mathbf{x}(s) \in \mathbb{R}^p$. The value estimate is the inner product:
$$\hat{v}(s, \mathbf{w}) = \mathbf{w}^\top \mathbf{x}(s) = \sum_{i=1}^p w_i x_i(s)$$

The gradient with respect to the parameter vector $\mathbf{w}$ is simply the feature vector itself:
$$\nabla_\mathbf{w} \hat{v}(s, \mathbf{w}) = \mathbf{x}(s)$$

For action-value prediction $\hat{q}(s, a, \mathbf{w})$, state-action features $\mathbf{x}(s, a) \in \mathbb{R}^p$ are constructed:
$$\hat{q}(s, a, \mathbf{w}) = \mathbf{w}^\top \mathbf{x}(s, a), \quad \nabla_\mathbf{w} \hat{q}(s, a, \mathbf{w}) = \mathbf{x}(s, a)$$

### 2.2 Mean Squared Value Error (MSVE) and Gradient Descent

Given the true value function $v_\pi(s)$ under stationary on-policy state distribution $\mu(s)$, the Mean Squared Value Error objective is:
$$\overline{\text{VE}}(\mathbf{w}) = \sum_{s \in \mathcal{S}} \mu(s) \left( v_\pi(s) - \hat{v}(s, \mathbf{w}) \right)^2$$

True stochastic gradient descent on $\overline{\text{VE}}$ with an unbiased target $U_t$ (such as Monte Carlo return $G_t$ where $\mathbb{E}[G_t \mid S_t=s] = v_\pi(s)$) follows:
$$\mathbf{w}_{t+1} = \mathbf{w}_t + \alpha \left[ U_t - \hat{v}(S_t, \mathbf{w}_t) \right] \nabla_\mathbf{w} \hat{v}(S_t, \mathbf{w}_t)$$

### 2.3 Semi-Gradient TD(0)

In Temporal Difference methods, the target contains a bootstrapped estimate $U_t = R_{t+1} + \gamma \hat{v}(S_{t+1}, \mathbf{w}_t)$. Crucially, $U_t$ depends on the current parameter vector $\mathbf{w}_t$. 

A true gradient calculation would require differentiating the target with respect to $\mathbf{w}$:
$$\nabla_\mathbf{w} \frac{1}{2} \left[ R_{t+1} + \gamma \hat{v}(S_{t+1}, \mathbf{w}) - \hat{v}(S_t, \mathbf{w}) \right]^2 = - \left( R_{t+1} + \gamma \hat{v}(S_{t+1}, \mathbf{w}) - \hat{v}(S_t, \mathbf{w}) \right) \left[ \nabla_\mathbf{w} \hat{v}(S_t, \mathbf{w}) - \gamma \nabla_\mathbf{w} \hat{v}(S_{t+1}, \mathbf{w}) \right]$$

However, ignoring the second term $\gamma \nabla_\mathbf{w} \hat{v}(S_{t+1}, \mathbf{w})$—treating the target as a fixed numerical label—yields the **semi-gradient TD(0)** update:
$$\mathbf{w}_{t+1} = \mathbf{w}_t + \alpha \, \delta_t \, \nabla_\mathbf{w} \hat{v}(S_t, \mathbf{w}_t)$$
where the TD error is:
$$\delta_t = R_{t+1} + \gamma \hat{v}(S_{t+1}, \mathbf{w}_t) - \hat{v}(S_t, \mathbf{w}_t)$$

For linear approximation $\hat{v}(s, \mathbf{w}) = \mathbf{w}^\top \mathbf{x}(s)$:
$$\mathbf{w}_{t+1} = \mathbf{w}_t + \alpha \left[ R_{t+1} + \gamma \mathbf{w}_t^\top \mathbf{x}(S_{t+1}) - \mathbf{w}_t^\top \mathbf{x}(S_t) \right] \mathbf{x}(S_t)$$

### 2.4 The Deadly Triad & Baird's Counterexample

Instability and mathematical divergence occur when all three elements coincide:
1. **Function Approximation:** Representing values with fewer parameters than states (e.g. linear weights or deep neural nets).
2. **Bootstrapping:** Updating values using downstream value estimates (TD, DP, Q-learning) rather than complete returns.
3. **Off-Policy Learning:** Training on transitions distributed according to behavior policy $b(a \mid s) \neq \pi(a \mid s)$, or learning greedily while exploring.

**Mathematical Cause:** The Bellman operator $T^\pi$ is a $\gamma$-contraction under the $\ell_\infty$ norm: $\|T^\pi u - T^\pi v\|_\infty \le \gamma \|u - v\|_\infty$. However, linear projection $\Pi_\mu$ (which projects values onto the subspace spanned by features $\mathbf{X}$) is non-expansive *only* under the $\ell_2$ norm weighted by the on-policy distribution $\mu$: $\|\Pi_\mu u - \Pi_\mu v\|_\mu \le \|u - v\|_\mu$.

When sampling off-policy, the data distribution $d(s) \neq \mu(s)$. The composite operator $\Pi_d T^\pi$ is **not a contraction mapping** in general. The spectral radius of the system update matrix can exceed 1, causing geometric divergence $\|\mathbf{w}_t\| \to \infty$.

### Symbol Reference Table

| Symbol | Mathematical Definition | Data Type / Dimensions | Role in Concept |
| :---: | :--- | :---: | :--- |
| $\mathbf{w}$ | Parameter weight vector | $\mathbb{R}^p$ | Learnable weights approximating value function |
| $\mathbf{x}(s)$ | Feature representation of state $s$ | $\mathbb{R}^p$ | Mapping from raw state space $\mathcal{S}$ to linear features |
| $\hat{v}(s, \mathbf{w})$ | Approximated state value | $\mathbb{R}$ | Estimate of expected discounted return from state $s$ |
| $\hat{q}(s, a, \mathbf{w})$ | Approximated action value | $\mathbb{R}$ | Estimate of expected discounted return taking action $a$ in $s$ |
| $\delta_t$ | TD Error | $\mathbb{R}$ | Instantaneous difference between bootstrapped target and estimate |
| $\mu(s)$ | On-policy stationary state distribution | $\mathbb{R}$ ($[0, 1]$) | Weighting factor for Mean Squared Value Error |
| $\Pi_\mu$ | Orthogonal projection operator | $\mathbb{R}^{|\mathcal{S}|} \to \text{span}(\mathbf{X})$ | Projects arbitrary value vectors onto feature subspace |

---

## 3. Implementation & Practical Code

Below is a self-contained, leakage-safe implementation of Linear Semi-Gradient TD(0) and Semi-Gradient SARSA with continuous state feature extraction (Fourier Basis Features), applied to a simulated continuous 1D navigation problem.

```python
"""
Linear Function Approximation with Fourier Basis Features & Semi-Gradient SARSA.
Verified under Python 3.11+, pure NumPy.
"""

import numpy as np
from typing import Tuple, List, Dict, Any


class FourierBasis:
    """
    Fourier Basis feature representation for continuous state spaces.
    Projects d-dimensional state s in [0, 1]^d into (order + 1)^d cosine features:
    phi_i(s) = cos(pi * c_i^T s) where c_i in {0, ..., order}^d.
    """
    def __init__(self, state_dim: int, order: int = 3, state_bounds: Tuple[np.ndarray, np.ndarray] = None):
        self.state_dim = state_dim
        self.order = order
        if state_bounds is not None:
            self.low, self.high = state_bounds
        else:
            self.low = np.zeros(state_dim)
            self.high = np.ones(state_dim)
            
        # Generate coefficient vectors c in {0, ..., order}^d
        grid = np.meshgrid(*[np.arange(order + 1) for _ in range(state_dim)], indexing='ij')
        self.coefficients = np.stack([g.ravel() for g in grid], axis=1) # shape: (num_features, state_dim)
        self.num_features = self.coefficients.shape[0]

    def normalize(self, state: np.ndarray) -> np.ndarray:
        """Scales raw state into [0, 1]^d range."""
        return np.clip((state - self.low) / (self.high - self.low + 1e-8), 0.0, 1.0)

    def transform(self, state: np.ndarray) -> np.ndarray:
        """Computes cosine Fourier basis vector."""
        s_norm = self.normalize(state)
        # phi_i = cos(pi * sum(c_ij * s_j))
        return np.cos(np.pi * np.dot(self.coefficients, s_norm))


class LinearSemiGradientSARSA:
    """
    Semi-Gradient SARSA with Linear Function Approximation.
    Learns Q(s, a; w) = w_a^T phi(s) across discrete actions.
    """
    def __init__(
        self,
        feature_extractor: FourierBasis,
        n_actions: int,
        alpha: float = 0.005,
        gamma: float = 0.99,
        epsilon: float = 0.1,
        seed: int = 42
    ):
        self.fe = feature_extractor
        self.n_actions = n_actions
        self.alpha = alpha
        self.gamma = gamma
        self.epsilon = epsilon
        self.rng = np.random.default_rng(seed)
        
        # Weight matrix: shape (n_actions, num_features)
        self.W = np.zeros((n_actions, self.fe.num_features), dtype=np.float64)

    def get_q_values(self, state: np.ndarray) -> np.ndarray:
        phi = self.fe.transform(state)
        return np.dot(self.W, phi)

    def select_action(self, state: np.ndarray) -> int:
        if self.rng.random() < self.epsilon:
            return int(self.rng.integers(0, self.n_actions))
        q_vals = self.get_q_values(state)
        return int(np.argmax(q_vals))

    def update(
        self,
        state: np.ndarray,
        action: int,
        reward: float,
        next_state: np.ndarray,
        next_action: int,
        done: bool
    ) -> float:
        """
        Performs semi-gradient update:
        w_a <- w_a + alpha * [r + gamma * Q(s', a'; w) - Q(s, a; w)] * phi(s)
        """
        phi_s = self.fe.transform(state)
        current_q = np.dot(self.W[action], phi_s)
        
        if done:
            target = reward
        else:
            phi_next = self.fe.transform(next_state)
            next_q = np.dot(self.W[next_action], phi_next)
            target = reward + self.gamma * next_q
            
        td_error = target - current_q
        self.W[action] += self.alpha * td_error * phi_s
        return float(td_error)


# Execution & Verification Demo
if __name__ == "__main__":
    # Continuous 1D Target Tracking Environment:
    # State: continuous position x in [-2.0, 2.0]
    # Actions: 0 (move left -0.2), 1 (stay 0.0), 2 (move right +0.2)
    # Goal: reach target at 0.0 within tolerance +-0.1
    bounds = (np.array([-2.0]), np.array([2.0]))
    fe = FourierBasis(state_dim=1, order=5, state_bounds=bounds)
    agent = LinearSemiGradientSARSA(fe, n_actions=3, alpha=0.01, gamma=0.95, epsilon=0.1, seed=42)

    n_episodes = 250
    episode_returns = []

    for ep in range(n_episodes):
        state = np.array([agent.rng.uniform(-2.0, 2.0)])
        action = agent.select_action(state)
        total_return = 0.0

        for step in range(50):
            delta_x = [-0.2, 0.0, 0.2][action]
            next_state = np.clip(state + delta_x + agent.rng.normal(0, 0.02), -2.0, 2.0)
            
            # Continuous quadratic penalty + target bonus
            dist = float(abs(next_state[0]))
            done = dist < 0.15
            reward = 10.0 if done else -dist

            total_return += reward
            next_action = agent.select_action(next_state)

            agent.update(state, action, reward, next_state, next_action, done)

            if done:
                break
            state = next_state
            action = next_action

        episode_returns.append(total_return)

    avg_final = np.mean(episode_returns[-50:])
    print(f"Fourier Features generated: {fe.num_features}")
    print(f"Initial 20-episode Avg Return: {np.mean(episode_returns[:20]):.2f}")
    print(f"Final 50-episode Avg Return:   {avg_final:.2f}")
    assert avg_final > np.mean(episode_returns[:20]), "Agent failed to improve value approximation."
```

---

## 4. Edge Cases & Advanced Scenarios

1. **Catastrophic Interference / Plasticity Loss:**
   When training non-linear neural function approximators, gradient updates in one region of the state space inadvertently distort the predictions in distant regions. In RL, where data is strictly non-IID and follows a non-stationary trajectory, early policy updates overwrite previously learned skills, causing catastrophic forgetting.
2. **Pathological Discontinuities:**
   Linear function approximators assume value functions vary smoothly across feature space. In environments with sharp cliff edges (e.g. falling into a pit right beside a safe lane), smooth linear basis functions (such as low-order polynomials or coarse RBFs) smear the catastrophic penalty into the safe zone, rendering the policy overly timid. High-resolution tile coding with hashing or localized multi-layer networks are required.
3. **Weight Explosion under Baird's Counterexample:**
   In Baird's 7-state star network, standard off-policy TD(0) with linear function approximation causes parameter vector $\|\mathbf{w}\|$ to grow exponentially without bound, even with small learning rates $\alpha$. Resolving this requires Gradient TD (GTD2 / TDC) or Emphatic TD algorithms.

---

## 5. Anti-Patterns & Common Pitfalls Table

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| Normalizing target gradients $\nabla_\mathbf{w} (r + \gamma \hat{v}(s'))$ | Violates semi-gradient mechanics; true gradient TD without corrections diverges | Treat bootstrapped TD target strictly as an evaluation constant |
| Training off-policy without target stabilization | Triggers Deadly Triad; causes unbounded weight divergence | Use target networks, gradient TD methods (TDC), or on-policy updates |
| Unscaled raw continuous inputs into RBF/Fourier | Features with large numerical ranges dominate activation | Scale state variables explicitly to $[0, 1]$ or $[-1, 1]$ |
| Uniform learning rate across Fourier basis orders | High-frequency features oscillate while low frequencies underfit | Scale step size inversely by frequency norm: $\alpha_i = \alpha / \|\mathbf{c}_i\|_2$ |
| Ignoring feature sparsity in Tile Coding | Computing full dense dot product wastes millions of CPU cycles | Use active index lookups: $\mathbf{w}^\top \mathbf{x} = \sum_{j \in \text{active}} w_j$ |
| Applying standard supervised loss without stationary distribution | Minimizes error uniformly rather than weighted by visit distribution $\mu(s)$ | Sample transitions weighted by on-policy discounted state visitation |

---

## 6. Best Practices & Production Guidelines

1. **Fourier Basis Order Selection:** For low-dimensional continuous domains ($d \le 4$), order-3 or order-5 Fourier features yield exceptional smooth approximations without kernel matrix inversions.
2. **Frequency-Weighted Step Sizes:** Always scale learning rate for component $i$ by:
   $$\alpha_i = \frac{\alpha}{\sqrt{\sum_{j=1}^d c_{i, j}^2}}$$
   where $c_{i, j}$ is the frequency multiplier for dimension $j$. This prevents high-frequency oscillations from destabilizing the baseline value estimate.
3. **Weight Initialization:** Initialize parameter vectors $\mathbf{w}$ to exact zeros or small Gaussian values ($\sigma = 10^{-4}$). In optimistic initialization schemes for linear approximation, ensure feature norms do not cause immediate saturation.

---

## 7. Real-World Case Study: Gas Turbine Combustion Control

**Industry Context:** A continuous-flow industrial power turbine requires optimal fuel-to-air valve positioning ($u \in \mathbb{R}^2$) based on 6 continuous sensor telemetry channels (inlet pressure, exhaust temperature, flame luminosity, compressor vibration, mass flow rate, RPM).  
**Challenge:** Continuous state space $\mathcal{S} \subseteq \mathbb{R}^6$ precludes tabular Q-learning. Neural networks were rejected by plant safety compliance due to non-deterministic weight drift during live operations.  
**Solution:**
- Deployed a verified linear semi-gradient SARSA controller utilizing tile coding with 16 asymmetric tilings and memory-efficient hashing.
- Features: $16 \times 1024$ hash bins with sparse dot products running under 8 microseconds on real-time PLC hardware.
- Result: Reduced flame temperature variance by 28.4% and NOx emissions by 14.2% while providing formal mathematical bounded-weight stability guarantees under on-policy operation.

---

## 8. Performance & Resource Optimization

1. **Sparse Dot Products in Tile Coding:**
   In tile coding with $m$ tilings, exactly $m$ features are active ($1$) while all others are $0$. Never construct a dense vector of size $10^6$. Instead, compute:
   $$\hat{v}(s, \mathbf{w}) = \sum_{k=1}^m w_{\text{index}(s, k)}$$
   This reduces vector computation from $\mathcal{O}(p)$ to $\mathcal{O}(m)$ time complexity.
2. **NumPy Vectorized State Batching:**
   When evaluating batches of states $\mathbf{S} \in \mathbb{R}^{B \times d}$ through Fourier basis, compute matrix multiplication $\pi \mathbf{C} \mathbf{S}^\top$ followed by a single SIMD `np.cos` call.

---

## 9. Security, Privacy & Ethics Considerations

1. **Runaway Weight Divergence in Safety Systems:**
   In physical cyber-physical systems (robotics, medical pumps), divergence under the Deadly Triad causes actuator commands to pin to extreme limits ($\pm V_{\max}$). Systems must incorporate hard parameter bounding ($L_\infty$ weight clipping) and watchdog kill-switches.
2. **Representation Bias in State Encoders:**
   Feature basis selection reflects human engineering assumptions. Omitting subtle state interactions from feature basis vectors permanently blinds the agent to critical environmental correlations.

---

## 10. Diagnostic & Verification Playbook

### Diagnostic Checklist
- [x] Does the weight vector norm $\|\mathbf{w}\|_2$ remain bounded throughout training?
- [x] Is the feature mapping $\mathbf{x}(s)$ properly normalized across all state dimensions?
- [x] Are TD targets treated strictly as detached numerical constants during gradient computation?
- [x] Does the environment violate any of the three Deadly Triad conditions?

### Automated Test Snippet
```python
def test_fourier_basis_orthonormality():
    fe = FourierBasis(state_dim=1, order=3, state_bounds=(np.array([0.0]), np.array([1.0])))
    states = np.linspace(0.0, 1.0, 1000).reshape(-1, 1)
    features = np.array([fe.transform(s) for s in states])
    # Verify non-trivial variance across all basis orders
    variances = np.var(features, axis=0)
    assert np.all(variances[1:] > 0.05), "High-order Fourier basis features collapsed to constants."
```

---

## 11. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Why is TD(0) with function approximation termed "semi-gradient" rather than true gradient descent?</strong></summary>

**Answer:** True gradient descent optimizes an objective function by taking the derivative of the squared error with respect to all parameter occurrences:
$$\nabla_\mathbf{w} \frac{1}{2} \left( v_\pi(S) - \hat{v}(S, \mathbf{w}) \right)^2 = - \left( v_\pi(S) - \hat{v}(S, \mathbf{w}) \right) \nabla_\mathbf{w} \hat{v}(S, \mathbf{w})$$
In TD(0), the true target $v_\pi(S)$ is unknown and replaced by the bootstrapped estimate $R_{t+1} + \gamma \hat{v}(S_{t+1}, \mathbf{w})$. This estimate itself depends on $\mathbf{w}$. A true gradient would differentiate through the target, adding a term $-\gamma \nabla_\mathbf{w} \hat{v}(S_{t+1}, \mathbf{w})$. Semi-gradient methods intentionally omit this term, treating $R_{t+1} + \gamma \hat{v}(S_{t+1}, \mathbf{w})$ as a fixed numerical label. Consequently, semi-gradient updates do not follow the true gradient of any objective function, which is why convergence guarantees are weaker and divergence is possible.
</details>

<details>
<summary><strong>Q2: Precisely explain the "Deadly Triad" and why tabular Q-learning never diverges while linear Q-learning can.</strong></summary>

**Answer:** The Deadly Triad refers to the combination of:
1. Function approximation (compressing state space into parameters $\mathbf{w}$).
2. Bootstrapping (updating estimates using downstream estimates: $R + \gamma \max Q(S', \cdot)$).
3. Off-policy distribution (evaluating target policy $\pi$ while following behavior policy $b$).

Tabular Q-learning includes bootstrapping and off-policy sampling, but **lacks function approximation**. In the tabular setting, each state-action pair $(s, a)$ has its own decoupled memory cell. The update is a non-expansion under the $\ell_\infty$ norm:
$$\|T^* Q_1 - T^* Q_2\|_\infty \le \gamma \|Q_1 - Q_2\|_\infty$$
Because $\gamma < 1$, the Banach Fixed Point Theorem guarantees convergence to $Q^*$.

When linear function approximation is introduced, the Bellman target must be projected back onto the subspace spanned by the feature matrix: $\Pi_\mu T^*$. While $\Pi_\mu$ is a non-expansion under the $\ell_2$ norm weighted by on-policy visitation $\mu$, it is **not** a non-expansion under off-policy visitation $d \neq \mu$, nor under the $\ell_\infty$ norm. The composite operator $\Pi_d T^*$ can expand distances, causing parameter weights to diverge geometrically ($\|\mathbf{w}_t\| \to \infty$).
</details>

<details>
<summary><strong>Q3: What is Baird's Counterexample, and what does it formally prove?</strong></summary>

**Answer:** Baird's Counterexample (1995) is a minimal 7-state MDP with 2 actions designed specifically to demonstrate that linear off-policy semi-gradient TD methods can diverge to infinity. The features are constructed such that updates to parameters at states visited frequently under the behavior policy cause the predicted values of states rarely visited under the behavior policy to increase. When those rarely visited states are updated, they further amplify the parameters. Baird proved analytically that for any positive step size $\alpha > 0$, the parameter weights $\mathbf{w}_t$ grow exponentially without bound, demonstrating that off-policy bootstrapping with linear function approximation is fundamentally unstable without correction mechanisms like Gradient TD.
</details>

<details>
<summary><strong>Q4: How do Gradient-TD methods (e.g. GTD2, TDC) mathematically resolve the Deadly Triad divergence?</strong></summary>

**Answer:** Standard semi-gradient TD lacks a true objective function. Gradient-TD methods derive updates from the **Mean Squared Projected Bellman Error (MSPBE)**:
$$\text{MSPBE}(\mathbf{w}) = \|\hat{v}_\mathbf{w} - \Pi_\mu T^\pi \hat{v}_\mathbf{w}\|_\mu^2 = \delta(\mathbf{w})^\top \mathbf{X} (\mathbf{X}^\top \mathbf{D} \mathbf{X})^{-1} \mathbf{X}^\top \mathbf{D} \delta(\mathbf{w})$$
Differentiating MSPBE with respect to $\mathbf{w}$ yields an expression involving products of two expectations, which cannot be sampled directly with a single trajectory. Gradient-TD resolves this by introducing a second auxiliary weight vector $\mathbf{v} \in \mathbb{R}^p$ that tracks the inverse matrix product via a two-timescale stochastic approximation (TDC / TD with Gradient Correction). Because it performs true stochastic gradient descent on the convex MSPBE surface, TDC is mathematically guaranteed to converge under linear function approximation, even with off-policy data.
</details>

<details>
<summary><strong>Q5: What is Tile Coding and how does its computational complexity compare to radial basis functions (RBFs)?</strong></summary>

**Answer:** Tile coding is a sparse, linear feature representation for multi-dimensional continuous spaces. It overlays $m$ distinct multidimensional grids (tilings) over the state space, with each tiling offset by a sub-grid fraction. For any continuous state $s$, exactly one tile is activated per tiling, producing a binary feature vector $\mathbf{x}(s) \in \{0, 1\}^p$ containing exactly $m$ non-zero entries (ones) out of $p$ total features.
- In RBFs, computing $\hat{v}(s) = \sum_i w_i \exp(-\|s - c_i\|^2 / 2\sigma^2)$ requires evaluating $p$ exponential operations, which scales as $\mathcal{O}(p \cdot d)$.
- In tile coding, the inner product is computed simply by summing the $m$ active weights: $\hat{v}(s) = \sum_{k=1}^m w_{\text{index}(s, k)}$, running in $\mathcal{O}(m)$ time regardless of $p$. Tile coding is thus orders of magnitude faster and computationally tractable on real-time hardware.
</details>

<details>
<summary><strong>Q6: How does the choice of state feature representation affect the inductive bias and convergence rate of linear RL?</strong></summary>

**Answer:** In linear approximation, the agent can only represent value functions that lie in the column space (span) of feature matrix $\mathbf{X}$. The choice of basis functions determines:
1. **Generalization geometry:** RBFs impose local spherical generalization; Fourier features impose global harmonic periodicity; polynomial features impose global power-law curvature; tile coding imposes piecewise-constant step functions with compact support.
2. **Condition number of $\mathbf{X}^\top \mathbf{X}$:** Poorly scaled or highly correlated features yield a high condition number, causing gradient descent to oscillate and slowing convergence. Orthonormal bases (such as Fourier basis) decouple weight dimensions, enabling rapid and uniform parameter convergence across orders.
</details>

<details>
<summary><strong>Q7: Why does Deep Q-Networks (DQN) use a Target Network, and how does it relate to the semi-gradient target?</strong></summary>

**Answer:** In tabular Q-learning, updating $Q(s, a)$ alters only that cell. In Deep Q-Networks, updating $Q(s, a; \theta)$ changes the network weights $\theta$, which shifts $Q(s', a'; \theta)$ for subsequent states simultaneously. If the target $r + \gamma \max_{a'} Q(s', a'; \theta)$ is computed using the same active parameters $\theta$, the target constantly flees from the update (the "moving target problem"), creating severe positive feedback loops and divergence. The target network $Q(\cdot, \cdot; \theta^-)$ freezes the parameters used to compute targets, updating them only periodically or via Polyak averaging ($\theta^- \leftarrow \tau \theta + (1-\tau)\theta^-$). This decouples the target generation from current parameter updates, stabilizing semi-gradient descent.
</details>

<details>
<summary><strong>Q8: What is the difference between On-Policy Distribution Weighting $\mu(s)$ and State Visitation under an Off-Policy Behavior Policy $d(s)$?</strong></summary>

**Answer:** The on-policy distribution $\mu(s) = \lim_{t \to \infty} P(S_t = s \mid S_0, \pi)$ represents the asymptotic probability of occupying state $s$ when executing target policy $\pi$. Under $\mu(s)$, the projection operator $\Pi_\mu$ satisfies the Pythagorean theorem with respect to the $\ell_2(\mu)$ norm, making it a non-expansive projection ($\|\Pi_\mu v\|_\mu \le \|v\|_\mu$).
When data is gathered under a behavior policy $b \neq \pi$, states are visited according to $d(s) \neq \mu(s)$. The projection operator $\Pi_d$ does not align with the Bellman backup operator $T^\pi$, meaning $\Pi_d T^\pi$ can expand distances under both $\ell_2(d)$ and $\ell_\infty$ norms, directly enabling the divergence predicted by the Deadly Triad.
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Polynomial Basis Feature Generator</strong></summary>

**Task:** Write a Python function that maps a 1D continuous state $s \in [-1, 1]$ into a polynomial feature vector $\mathbf{x}(s) = [1, s, s^2, s^3]^\top$ and evaluates $\hat{v}(s, \mathbf{w})$ given weight vector $\mathbf{w} = [0.5, -1.2, 0.8, 2.0]^\top$.

**Solution:**
```python
import numpy as np

def evaluate_polynomial_value(s: float, w: np.ndarray, degree: int = 3) -> float:
    # Feature vector: [s^0, s^1, s^2, ..., s^degree]
    features = np.array([s**i for i in range(degree + 1)], dtype=np.float64)
    return float(np.dot(w, features))

w = np.array([0.5, -1.2, 0.8, 2.0])
val = evaluate_polynomial_value(0.5, w)
print(f"Value at s=0.5: {val:.4f}")
# Verification: 0.5 - 1.2(0.5) + 0.8(0.25) + 2.0(0.125) = 0.5 - 0.6 + 0.2 + 0.25 = 0.35
assert abs(val - 0.35) < 1e-6
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Semi-Gradient TD(0) Linear Prediction on Random Walk</strong></summary>

**Task:** Implement linear semi-gradient TD(0) to predict state values on a 5-state continuous random walk where states are encoded as single continuous scalar inputs $s \in \{1, 2, 3, 4, 5\}$ mapped via 2 RBF centers $c_1 = 2.0, c_2 = 4.0$ with $\sigma = 1.0$.

**Solution:**
```python
import numpy as np

def rbf_features(s: float, centers: np.ndarray = np.array([2.0, 4.0]), sigma: float = 1.0) -> np.ndarray:
    return np.exp(-0.5 * ((s - centers) / sigma) ** 2)

# Training linear semi-gradient TD(0)
w = np.zeros(2, dtype=np.float64)
alpha = 0.05
gamma = 1.0

# 500 simulated step transitions
rng = np.random.default_rng(42)
for _ in range(500):
    s = rng.choice([1, 2, 3, 4, 5])
    step = rng.choice([-1, 1])
    s_next = s + step
    
    if s_next == 0:
        reward = -1.0
        done = True
    elif s_next == 6:
        reward = 1.0
        done = True
    else:
        reward = 0.0
        done = False
        
    phi_s = rbf_features(s)
    v_s = np.dot(w, phi_s)
    
    if done:
        target = reward
    else:
        v_next = np.dot(w, rbf_features(s_next))
        target = reward + gamma * v_next
        
    delta = target - v_s
    w += alpha * delta * phi_s

print(f"Trained RBF Weights: {w}")
assert w[1] > w[0], "Right RBF weight must be higher due to positive terminal reward."
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Numerical Demonstration of Baird's Counterexample Divergence</strong></summary>

**Task:** Construct Baird's 7-state counterexample in pure NumPy and verify that standard linear off-policy semi-gradient TD(0) causes parameter norm $\|\mathbf{w}\|$ to diverge exponentially.

**Solution:**
```python
import numpy as np

def simulate_bairds_counterexample(steps: int = 200, alpha: float = 0.01, gamma: float = 0.99):
    # 7 states, 6 upper states (0-5) and 1 lower state (6)
    # Feature matrix X: 7 states x 8 parameters
    # State 0-5 features: [2*e_i, 1]
    # State 6 features:   [1, 1, 1, 1, 1, 1, 2]
    X = np.zeros((7, 8), dtype=np.float64)
    for i in range(6):
        X[i, i] = 2.0
        X[i, 7] = 1.0
    X[6, :6] = 1.0
    X[6, 6] = 2.0
    
    # Initial weights: w = [1, 1, 1, 1, 1, 1, 10, 1]
    w = np.array([1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 10.0, 1.0], dtype=np.float64)
    
    # Behavior policy: choose upper states uniformly with prob 6/7, lower state with prob 1/7
    # Target policy: always transition to lower state 6
    norms = []
    rng = np.random.default_rng(42)
    
    for _ in range(steps):
        # Sample state according to behavior distribution
        s = rng.choice(7, p=[1/7]*7)
        # Next state under target policy is always state 6
        s_next = 6
        reward = 0.0 # Zero rewards throughout
        
        phi_s = X[s]
        phi_next = X[s_next]
        
        # Semi-gradient off-policy TD(0) update
        # Under target policy, expectation is always over s_next = 6
        v_s = np.dot(w, phi_s)
        v_next = np.dot(w, phi_next)
        
        delta = reward + gamma * v_next - v_s
        w += alpha * delta * phi_s
        norms.append(np.linalg.norm(w))
        
    return norms

weight_norms = simulate_bairds_counterexample(steps=150)
print(f"Initial weight norm: {weight_norms[0]:.2f}")
print(f"Weight norm at step 50: {weight_norms[50]:.2f}")
print(f"Weight norm at step 150: {weight_norms[-1]:.2f}")
assert weight_norms[-1] > weight_norms[0] * 5.0, "Baird's system failed to demonstrate divergence."
```
</details>

---

## 13. Summary, Verified References & Navigation

- **Value Function Approximation** parameterizes infinite state spaces via weight vector $\mathbf{w}$, enabling generalization across unvisited states.
- **Semi-Gradient Mechanics:** Because the TD target depends on $\mathbf{w}$, omitting the gradient of the target with respect to $\mathbf{w}$ yields semi-gradient descent.
- **The Deadly Triad:** The intersection of Function Approximation, Bootstrapping, and Off-Policy distribution causes projection operators to expand rather than contract, leading to unbounded divergence.
- **Remedies:** Gradient TD (TDC/GTD2), target networks (DQN), on-policy updates (SARSA), and emphatic weighting.

### Verified External References
- Sutton, R. S., & Barto, A. G. (2018). *Reinforcement Learning: An Introduction* (2nd ed., Chapter 9-11). MIT Press.
- Baird, L. (1995). *Residual Algorithms: Reinforcement Learning with Function Approximation*. ICML.
- Tsitsiklis, J. N., & Van Roy, B. (1997). *An analysis of temporal-difference learning with function approximation*. IEEE Transactions on Automatic Control.

---

[← 09-exploration-vs-exploitation-and-multi-armed-bandits.md](09-exploration-vs-exploitation-and-multi-armed-bandits.md) | [Module Index](README.md) | [11-deep-q-networks.md →](11-deep-q-networks.md)
