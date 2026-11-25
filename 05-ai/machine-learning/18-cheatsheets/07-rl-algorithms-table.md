# Cheatsheet 07: Reinforcement Learning Algorithms Master Table

> **Cheatsheet ID:** `05-ai/machine-learning/18-cheatsheets/07-rl-algorithms-table`  
> **Topic:** Master Classification Matrix of Classical & Deep Reinforcement Learning Algorithms  
> **Status:** Production Verified

---

## 1. Master RL Algorithms Taxonomy Table

| Algorithm | Paradigm (Model Free/Based) | Policy Type (On/Off Policy) | Action Space | Method Class | Core Update Mechanism / Loss Formula | Primary Application |
| :--- | :---: | :---: | :---: | :---: | :--- | :--- |
| **Value Iteration** | Model-Based | Off-Policy | Discrete | Value-Based | $V_{k+1}(s) = \max_a \sum_{s'} \mathcal{P}(s' \mid s, a)[R + \gamma V_k(s')]$ | Exact planning on small known MDPs. |
| **Policy Iteration**| Model-Based | On-Policy | Discrete | Policy-Based | Alternates full policy evaluation $(I - \gamma P)^{-1} R$ and greedy step $\arg\max Q$. | Exact planning when $|S| \le 10^4$. |
| **SARSA** | Model-Free | **On-Policy** | Discrete | Value-Based | $Q(s, a) \leftarrow Q(s, a) + \alpha [r + \gamma Q(s', a') - Q(s, a)]$ | Conservative navigation; avoids edge traps. |
| **Q-Learning** | Model-Free | **Off-Policy**| Discrete | Value-Based | $Q(s, a) \leftarrow Q(s, a) + \alpha [r + \gamma \max_{a'} Q(s', a') - Q(s, a)]$ | Classic tabular benchmark; off-policy exploration. |
| **DQN (Mnih 2015)** | Model-Free | **Off-Policy**| Discrete | Value-Based | $\mathcal{L}(\theta) = \mathbb{E}\left[ \left( r + \gamma \max_{a'} Q_{\theta^-}(s', a') - Q_\theta(s, a) \right)^2 \right]$ | Discrete video games (Atari), discrete routing. |
| **Double DQN** | Model-Free | **Off-Policy**| Discrete | Value-Based | $y = r + \gamma Q_{\theta^-}(s', \arg\max_a Q_\theta(s', a))$ (Decouples action from evaluation) | Eliminates maximization bias in Q-learning. |
| **Dueling DQN** | Model-Free | **Off-Policy**| Discrete | Value-Based | $Q(s, a) = V(s) + (A(s, a) - \frac{1}{|A|}\sum A(s, a'))$ | Accelerates learning in states where actions matter little. |
| **REINFORCE** | Model-Free | **On-Policy** | Both | Policy Gradient | $\nabla_\theta J(\theta) = \mathbb{E}_{\tau} \left[ \sum_{t=0}^T \nabla_\theta \ln \pi_\theta(a_t \mid s_t) (G_t - b(s_t)) \right]$ | Baseline Monte Carlo policy gradient. |
| **A2C / A3C** | Model-Free | **On-Policy** | Both | Actor-Critic | Synchronous / Asynchronous Advantage Actor-Critic: $A(s, a) = r + \gamma V(s') - V(s)$ | Fast parallel CPU multi-environment training. |
| **TRPO (2015)** | Model-Free | **On-Policy** | Both | Policy Optimization | Maximize surrogate objective s.t. average KL constraint: $\mathbb{E}[\text{KL}(\pi_{\text{old}} \parallel \pi)] \le \delta$ | Mathematically guaranteed monotonic improvement. |
| **PPO (2017)** | Model-Free | **On-Policy** | Both | Actor-Critic | $\mathcal{L}^{\text{CLIP}}(\theta) = \hat{\mathbb{E}}\left[ \min(r_t(\theta)\hat{A}_t, \, \text{clip}(r_t(\theta), 1\pm\epsilon)\hat{A}_t) \right]$ | **The Modern Industry Workhorse**: Robotics, LLM RLHF. |
| **DDPG (2015)** | Model-Free | **Off-Policy**| **Continuous**| Actor-Critic | Deterministic policy gradient: $\nabla_\theta J = \mathbb{E}[\nabla_a Q(s, a) \nabla_\theta \mu_\theta(s)]$ | Continuous control baseline; sensitive to tuning. |
| **TD3 (2018)** | Model-Free | **Off-Policy**| **Continuous**| Actor-Critic | Clipped double-Q + delayed policy updates + target policy smoothing noise | Robust continuous locomotion control (MuJoCo). |
| **SAC (2018)** | Model-Free | **Off-Policy**| **Continuous**| Maximum Entropy Actor-Critic | $J(\pi) = \mathbb{E}\left[ \sum \gamma^t (R_t + \alpha \mathcal{H}(\pi(\cdot \mid s_t))) \right]$ via reparameterized squashed Gaussian | **SOTA Sample-Efficient Continuous Control**. |
| **AlphaZero / MCTS**| Model-Based | Hybrid | Discrete | Search + Policy/Value | Monte Carlo Tree Search guided by Deep ResNet policy $p(s)$ and value $v(s)$ heads | Perfect information board games (Chess, Go). |

---

## 2. Choosing Between Deep RL Architectures

```mermaid
flowchart TD
    Start(["Action Space & Problem Formulation"]) --> ActionType{"Action Space Nature"}
    
    ActionType -->|Discrete Actions: Left, Right, Buy, Sell| DiscGoal{"Is an accurate world transition simulator available?"}
    DiscGoal -->|YES: Fast Simulator| MCTS["AlphaZero / MuZero / MCTS"]
    DiscGoal -->|NO: Sample-based Environment| DiscData{"Data Collection Speed & Latency"}
    DiscData -->|Real-time Parallel Simulation| PPO_Disc["PPO (Discrete Action Head)"]
    DiscData -->|Logged Off-Policy Data / Replay Buffer| Rainbow["Rainbow / Double Dueling DQN"]
    
    ActionType -->|Continuous Actions: Torques, Angles, Voltages| ContGoal{"Primary Constraint: Stability or Sample Efficiency?"}
    ContGoal -->|Maximum Stability & Monotonic Tuning| PPO_Cont["PPO (Continuous Gaussian Policy)"]
    ContGoal -->|Extreme Sample Efficiency (Expensive Real Robots)| SAC_Cont["Soft Actor-Critic (SAC) with Off-Policy Replay Buffer"]
```
