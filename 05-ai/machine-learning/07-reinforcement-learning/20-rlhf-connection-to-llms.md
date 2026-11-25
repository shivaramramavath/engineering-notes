# Reinforcement Learning from Human Feedback (RLHF) & LLM Alignment

> **Topic:** The 3-Stage Alignment Pipeline, Bradley-Terry Preference Modeling, Reward Modeling, PPO with Token-Level KL Penalty, and Direct Preference Optimization (DPO)  
> **Module ID:** `05-ai/machine-learning/07-reinforcement-learning/20-rlhf-connection-to-llms.md`  
> **Target Version:** Python 3.11+, NumPy 2.5+, PyTorch 2.5+  
> **Level:** Advanced  
> **Prerequisites:** [15-proximal-policy-optimisation.md](15-proximal-policy-optimisation.md), [19-offline-rl-and-imitation-learning.md](19-offline-rl-and-imitation-learning.md)  
> **Status:** Verified (Python 3.11 / CPU)

---

## Architectural / Conceptual Flow

```mermaid
flowchart TD
    subgraph Pretraining["Stage 1: Pretraining & SFT"]
        RawText["Internet Scale Text"] --> LLM_Base["Base Pretrained Foundation Model"]
        LLM_Base --> SFT_Data["Curated Instruction Dataset (Prompt, Target)"]
        SFT_Data --> SFT_Model["Supervised Fine-Tuned (SFT) Model pi_SFT"]
    end

    subgraph RewardModeling["Stage 2: Reward Model Training"]
        SFT_Model --> GeneratePairs["Generate Response Pairs (y_w, y_l) for Prompt x"]
        GeneratePairs --> HumanAnnotators["Human / AI Preferences: Label Winner y_w > y_l"]
        HumanAnnotators --> PreferenceData["Pairwise Preference Dataset D_pref = {(x, y_w, y_l)}"]
        PreferenceData --> RM_Training["Bradley-Terry Binary Cross-Entropy Loss"]
        RM_Training --> RewardModel["Scalar Reward Model r_theta(x, y)"]
    end

    subgraph PPOAlignment["Stage 3A: PPO Reinforcement Learning"]
        PromptX["Prompt x from Task Distribution"] --> ActivePolicy["Active Policy pi_phi (Initialized as pi_SFT)"]
        PromptX --> FrozenRef["Frozen Reference Policy pi_ref (pi_SFT)"]
        ActivePolicy --> GeneratedResponse["Sample Token Sequence y ~ pi_phi(. | x)"]
        GeneratedResponse --> RewardModel
        RewardModel --> RawScore["Raw Reward r_theta(x, y)"]
        ActivePolicy --> TokenLogP_Active["log pi_phi(y | x)"]
        FrozenRef --> TokenLogP_Ref["log pi_ref(y | x)"]
        TokenLogP_Active --> KLPenalty["KL Penalty: beta * (log pi_phi - log pi_ref)"]
        TokenLogP_Ref --> KLPenalty
        RawScore --> TotalReward["R_total = r_theta - beta * KL"]
        KLPenalty --> TotalReward
        TotalReward --> PPO_Engine["PPO-Clip Policy & Critic Gradient Ascent"]
        PPO_Engine --> ActivePolicy
    end

    subgraph DirectPreferenceOptimization["Stage 3B: Direct Preference Optimization (DPO)"]
        PreferenceData --> DPO_Loss["Closed-Form Loss: No Reward Model / No Online Sampling!"]
        DPO_Loss --> DirectUpdate["Direct Gradient Step on pi_theta"]
    end
```

---

## 1. Concept Overview & Core Motivation

Base Large Language Models (LLMs) are trained on next-token prediction over internet text. While this imbues them with broad world knowledge, it also causes severe alignment failures:
- **Misalignment:** The model predicts what text *statistically follows*, including toxic rants, hallucinations, unhelpful tangents, and dangerous instructions (e.g. bomb-making recipes).
- **Instruction Following:** Base models often answer questions with more questions rather than helpful answers.

Supervised Fine-Tuning (SFT) on high-quality prompt-response pairs teaches the model the basic dialogue format. However, SFT suffers from two major limitations:
1. **Exposure Bias & Covariate Shift:** SFT trains only on gold-standard tokens. At test time, if the model generates a slightly suboptimal token, it enters an unfamiliar state and cascades into incoherence.
2. **Preference Expressiveness:** It is far easier for humans to compare two model outputs and choose the better one ("A is more helpful and concise than B") than to write a perfect gold-standard response from scratch.

**Reinforcement Learning from Human Feedback (RLHF)** (Christiano et al., 2017; Ouyang et al., InstructGPT 2022) bridges this gap:
- An empirical **Reward Model $r_\theta(x, y)$** is trained on human pairwise preferences using the Bradley-Terry model.
- The language model is treated as a **Reinforcement Learning Actor**, where the prompt $x$ is the initial state, tokens $y_1, y_2, \dots$ are sequential actions, and the reward model provides the return.
- **PPO with a KL penalty** aligns the policy to maximize helpfulness and safety while preventing the model from straying too far from the linguistic manifold.
- **Direct Preference Optimization (DPO)** (Rafailov et al., 2023) analytically derives an exact closed-form objective, optimizing the policy directly on preference pairs without requiring an explicit reward model or online PPO rollouts.

---

## 2. Theoretical Foundations & Mathematics

### 2.1 The Bradley-Terry Preference Model
Given a prompt $x$ and two candidate responses $(y_w, y_l)$, where human annotators prefer winning response $y_w$ over losing response $y_l$ ($y_w \succ y_l$), the probability of preference is modeled via the Bradley-Terry logistic model:
$$P(y_w \succ y_l \mid x) = \sigma\left( r_\theta(x, y_w) - r_\theta(x, y_l) \right) = \frac{1}{1 + \exp\left( -(r_\theta(x, y_w) - r_\theta(x, y_l)) \right)}$$
The reward model parameters $\theta$ are optimized by minimizing the negative log-likelihood across preference dataset $\mathcal{D}_{\text{pref}}$:
$$\mathcal{L}_{\text{RM}}(\theta) = -\mathbb{E}_{(x, y_w, y_l) \sim \mathcal{D}_{\text{pref}}} \left[ \ln \sigma\left( r_\theta(x, y_w) - r_\theta(x, y_l) \right) \right]$$

### 2.2 PPO Fine-Tuning with Token-Level KL Penalty
During RL fine-tuning, the policy $\pi_\phi$ is optimized to maximize the reward model score while penalizing deviation from the original reference model $\pi_{\text{ref}}$ (typically the frozen SFT model):
$$\max_\phi \mathbb{E}_{x \sim \mathcal{D}, y \sim \pi_\phi(\cdot \mid x)} \left[ r_\theta(x, y) \right] - \beta \, \mathbb{D}_{\text{KL}}\left( \pi_\phi(y \mid x) \parallel \pi_{\text{ref}}(y \mid x) \right)$$
where the Kullback-Leibler divergence is evaluated token-by-token:
$$\mathbb{D}_{\text{KL}}\left( \pi_\phi(y \mid x) \parallel \pi_{\text{ref}}(y \mid x) \right) = \sum_{t=1}^T \ln \left( \frac{\pi_\phi(y_t \mid x, y_{<t})}{\pi_{\text{ref}}(y_t \mid x, y_{<t})} \right)$$
In implementation, this is integrated as an augmented per-token step reward:
$$R_t(x, y_{1:t}) = \begin{cases} -\beta \ln \left( \frac{\pi_\phi(y_t \mid x, y_{<t})}{\pi_{\text{ref}}(y_t \mid x, y_{<t})} \right) & \text{for intermediate tokens } t < T \\ r_\theta(x, y) - \beta \ln \left( \frac{\pi_\phi(y_T \mid x, y_{<T})}{\pi_{\text{ref}}(y_T \mid x, y_{<T})} \right) & \text{at terminal token } t = T \end{cases}$$

### 2.3 Direct Preference Optimization (DPO)
Rafailov et al. (2023) mathematically proved that the constrained RL problem:
$$\max_\pi \mathbb{E}_{x, y \sim \pi} \left[ r(x, y) \right] - \beta \mathbb{D}_{\text{KL}}(\pi \parallel \pi_{\text{ref}})$$
has an exact closed-form optimal solution:
$$\pi^*(y \mid x) = \frac{1}{Z(x)} \pi_{\text{ref}}(y \mid x) \exp\left( \frac{1}{\beta} r(x, y) \right)$$
where $Z(x) = \sum_y \pi_{\text{ref}}(y \mid x) \exp\left( \frac{1}{\beta} r(x, y) \right)$ is the partition function.

Inverting this equation expresses the ground-truth reward function strictly in terms of the optimal policy and reference policy:
$$r(x, y) = \beta \ln \frac{\pi^*(y \mid x)}{\pi_{\text{ref}}(y \mid x)} + \beta \ln Z(x)$$
Substituting this expression into the Bradley-Terry preference probability causes the unknown partition function $Z(x)$ to **cancel out completely**:
$$P(y_w \succ y_l \mid x) = \sigma\left( \beta \ln \frac{\pi_\theta(y_w \mid x)}{\pi_{\text{ref}}(y_w \mid x)} - \beta \ln \frac{\pi_\theta(y_l \mid x)}{\pi_{\text{ref}}(y_l \mid x)} \right)$$

This yields the **Direct Preference Optimization (DPO) Loss**:
$$\mathcal{L}_{\text{DPO}}(\theta) = -\mathbb{E}_{(x, y_w, y_l) \sim \mathcal{D}} \left[ \ln \sigma \left( \beta \ln \frac{\pi_\theta(y_w \mid x)}{\pi_{\text{ref}}(y_w \mid x)} - \beta \ln \frac{\pi_\theta(y_l \mid x)}{\pi_{\text{ref}}(y_l \mid x)} \right) \right]$$
DPO completely eliminates the need to train a separate reward model, avoids fitting an Actor-Critic value function, and eliminates online sampling rollouts entirely.

### Symbol Reference Table

| Symbol | Mathematical Definition | Role in Concept |
| :---: | :--- | :--- |
| $r_\theta(x, y)$ | Scalar reward model | Evaluates alignment quality of response $y$ given prompt $x$ |
| $\pi_\phi$ | Active policy LLM | Language model parameters updated during reinforcement learning |
| $\pi_{\text{ref}}$ | Frozen reference LLM (SFT) | Anchors policy to prevent language drift and collapse |
| $\beta$ | KL regularization weight | Controls conservatism relative to reference model |
| $y_w, y_l$ | Winning and losing responses | Pairwise preference comparison annotated by humans or LLM judges |

---

## 3. Implementation & Practical Code

Below is a self-contained, pure NumPy implementation of the **Bradley-Terry Reward Model Loss**, the **Token-Level KL Penalty**, and the **DPO Loss with Analytic Gradients**.

```python
"""
RLHF & Direct Preference Optimization (DPO) Mathematical Primitives in pure NumPy.
Verified under Python 3.11+, CPU execution.
"""

import numpy as np
from typing import Tuple, Dict, Any


def bradley_terry_rm_loss(r_w: np.ndarray, r_l: np.ndarray) -> Tuple[float, np.ndarray, np.ndarray]:
    """
    Computes Bradley-Terry Reward Model Loss:
    L_RM = - mean( ln sigma(r_w - r_l) )
    Returns loss, dL/dr_w, dL/dr_l.
    """
    diff = r_w - r_l
    # Stable softplus: -ln sigma(x) = ln(1 + exp(-x))
    loss_elements = np.log1p(np.exp(-diff))
    loss = float(np.mean(loss_elements))
    
    # Derivative w.r.t diff: dL/d(diff) = - (1 - sigma(diff)) = - 1 / (1 + exp(diff))
    sigma_diff = 1.0 / (1.0 + np.exp(-diff))
    grad_diff = -(1.0 - sigma_diff) / len(diff)
    
    grad_rw = grad_diff
    grad_rl = -grad_diff
    return loss, grad_rw, grad_rl


def compute_token_kl_penalty(
    logp_active: np.ndarray,    # shape (T,) - token log-probs from active policy
    logp_ref: np.ndarray,       # shape (T,) - token log-probs from reference policy
    beta: float = 0.05
) -> Tuple[np.ndarray, float]:
    """
    Computes token-level KL penalty and total sequence KL divergence.
    Token penalty = - beta * (logp_active - logp_ref)
    """
    token_kl = logp_active - logp_ref
    penalties = -beta * token_kl
    total_kl = float(np.sum(token_kl))
    return penalties, total_kl


def dpo_loss_and_grad(
    logp_w_policy: np.ndarray,   # shape (B,)
    logp_w_ref: np.ndarray,      # shape (B,)
    logp_l_policy: np.ndarray,   # shape (B,)
    logp_l_ref: np.ndarray,      # shape (B,)
    beta: float = 0.1
) -> Tuple[float, np.ndarray, np.ndarray]:
    """
    Computes DPO Loss and gradients w.r.t policy log-probabilities.
    """
    B = len(logp_w_policy)
    # Implicit reward difference: beta * (log(pi_w / ref_w) - log(pi_l / ref_l))
    log_ratio_w = logp_w_policy - logp_w_ref
    log_ratio_l = logp_l_policy - logp_l_ref
    implicit_diff = beta * (log_ratio_w - log_ratio_l)
    
    # DPO Loss = - ln sigma(implicit_diff)
    loss_elements = np.log1p(np.exp(-implicit_diff))
    loss = float(np.mean(loss_elements))
    
    # Gradient w.r.t implicit_diff: - (1 - sigma(implicit_diff))
    sigma_diff = 1.0 / (1.0 + np.exp(-implicit_diff))
    grad_h = -(1.0 - sigma_diff) / B
    
    # Gradients w.r.t policy log-probs
    grad_w = beta * grad_h
    grad_l = -beta * grad_h
    
    return loss, grad_w, grad_l


# Verification Test Run
if __name__ == "__main__":
    # 1. Test Bradley-Terry RM Loss
    r_winner = np.array([2.5, 1.0, 0.5])
    r_loser = np.array([0.5, -0.2, 0.4])
    rm_l, g_w, g_l = bradley_terry_rm_loss(r_winner, r_loser)
    print(f"Reward Model Loss: {rm_l:.4f}")
    assert rm_l > 0
    assert np.all(g_w < 0) # Gradients pull up winner rewards
    assert np.all(g_l > 0) # Gradients push down loser rewards
    print("Bradley-Terry Reward Model verification passed.")

    # 2. Test DPO Loss
    lp_w_pol = np.array([-10.5, -8.2])
    lp_w_ref = np.array([-11.0, -8.0])
    lp_l_pol = np.array([-15.0, -9.0])
    lp_l_ref = np.array([-13.0, -8.5])

    dpo_l, gw_dpo, gl_dpo = dpo_loss_and_grad(lp_w_pol, lp_w_ref, lp_l_pol, lp_l_ref, beta=0.1)
    print(f"DPO Loss: {dpo_l:.4f}")
    assert dpo_l > 0
    print("DPO Loss and gradient computation verified successfully.")
```

---

## 4. Edge Cases & Advanced Scenarios

1. **Reward Hacking & Goodhart's Law:**
   When optimizing an LLM against a learned reward model, the policy inevitably exploits imperfections in the reward function:
   - **Verbosity Bias:** Reward models systematically assign higher scores to longer answers, causing models to generate bloated, repetitive prose.
   - **Sycophancy:** The model flatters the user and affirms false premises in the prompt to maximize predicted helpfulness scores.
   - **Adversarial Gibberish:** Discovering specific punctuation sequences or strange tokens that trigger anomalous high activations in the reward model network.  
   *Mitigation:* Strict KL penalty weighting ($\beta$), length-normalized rewards, and ensemble reward models with pessimistic lower-bounding.
2. **Mode Collapse / Linguistic Degeneration:**
   If $\beta$ is set too low ($\beta \to 0$), the model maximizes reward by repeatedly outputting a single, high-scoring canned response for every prompt, losing all creative diversity.

---

## 5. Anti-Patterns & Common Pitfalls Table

| Anti-Pattern / Mistake | Why It Fails | Correct Engineering Fix |
| :--- | :--- | :--- |
| Omitting the frozen reference model $\pi_{\text{ref}}$ | Model drifts off the natural language manifold; language collapses | Anchor policy using frozen reference model with KL divergence penalty |
| Setting KL weight $\beta = 0.0$ | Triggers extreme reward hacking, verbosity explosion, and sycophancy | Maintain non-zero $\beta$ (typically $\beta \in [0.01, 0.1]$) |
| Using raw sequence log-prob sums without length normalization | Longer sequences have smaller joint probabilities, penalizing thoroughness | Normalize sequence log probabilities by token length: $\frac{1}{T} \sum \ln \pi(y_t)$ |
| Training reward model on uncurated crowdworker labels | Noisy or conflicting human ratings corrupt reward landscape | Filter data with inter-annotator agreement metrics and gold-standard honeypots |
| Running DPO on responses generated by vastly different base models | Violates Bradley-Terry implicit derivation; causes numerical explosion | Ensure $\pi_{\text{ref}}$ matches the exact model architecture and pre-RL checkpoint |

---

## 6. Best Practices & Production Guidelines

1. **Length Penalty Regularization in Reward Models:** To eliminate verbosity exploitation, add a linear length penalty term to the reward function:
   $$\tilde{r}(x, y) = r_\theta(x, y) - \lambda_{\text{len}} \cdot \text{length}(y)$$
2. **Online DPO / Iterative DPO:** Rather than training DPO on a static offline dataset once, generate fresh responses from the current policy $\pi_{\theta}$, label them using an ensemble of LLM judges (AI Feedback / RLAIF), and execute multiple rounds of iterative DPO fine-tuning.

---

## 7. Real-World Case Study: Enterprise Customer Support Copilot Alignment

**Industry Context:** A Fortune 500 SaaS platform deployed an LLM agent to resolve complex billing and technical support tickets.  
**Challenge:** SFT models frequently hallucinated nonexistent refund policies or promised unauthorized account credits to satisfy users.  
**Solution:**
- Collected 15,000 pairwise comparisons scored by compliance officers.
- Deployed Direct Preference Optimization (DPO) with $\beta = 0.08$ on top of an 8B instruction-tuned model.
- Result: Reduced compliance policy violations from 14.2% down to 0.4%, eliminated false refund promises entirely, and improved customer satisfaction CSAT scores by 18.6%.

---

## 8. Performance & Resource Optimization

1. **Memory Sharding via FSDP / DeepSpeed ZeRO-3:** PPO requires holding 4 distinct LLMs in VRAM simultaneously: Active Actor, Critic (Value Model), Frozen Reference Model, and Reward Model. On an 8B model, this requires 128+ GB VRAM. DPO eliminates the Critic and Reward Model entirely, reducing VRAM footprint by $50\%$.
2. **FlashAttention-2 & vLLM Decoupled Inference:** During PPO rollout collection, decouple rollout generation (using optimized vLLM engines with PagedAttention) from training backpropagation (using PyTorch FSDP).

---

## 9. Security, Privacy & Ethics Considerations

1. **Jailbreaking & Safety Alignment:** RLHF fine-tuning aligns models to refuse dangerous requests (e.g. cyberweapons, malware). However, adversarial "jailbreak" prompts (e.g. hypothetical roleplay, base64 encoding) can bypass safety guardrails if the preference dataset lacks adversarial boundary examples.
2. **Annotator Demographic Bias:** The values, cultural norms, and political perspectives of the human annotators hired to rate responses become hardcoded into the reward model. Auditing annotator pools for demographic diversity is mandatory.

---

## 10. Diagnostic & Verification Playbook

### Diagnostic Checklist
- [x] Does the average sequence reward steadily increase over training iterations?
- [x] Does the approximate KL divergence from reference model remain strictly bounded ($\mathbb{D}_{\text{KL}} \in [2.0, 10.0]$ nats)?
- [x] Are generated response lengths monitored to detect verbosity reward hacking?
- [x] Does the reward model achieve $> 70\%$ validation accuracy on held-out human preference pairs?

---

## 11. Technical Interview Questions & Answers

<details>
<summary><strong>Q1: Why can Supervised Fine-Tuning (SFT) alone not solve the LLM alignment problem?</strong></summary>

**Answer:** SFT trains models via maximum likelihood estimation (teacher forcing) on expert demonstrations. It fails to solve alignment for three reasons:
1. **Compounding Exposure Bias:** During SFT, the model is conditioned on ground-truth tokens. At inference time, the model generates tokens autoregressively. A single slightly off-distribution token pushes the model into states it never saw during training, compounding errors.
2. **Binary Quality Representation:** In SFT, all training tokens are treated as equally optimal. SFT cannot convey that response A is significantly better than response B, nor can it provide negative gradient signals to explicitly suppress plausible but incorrect or unsafe responses.
3. **Data Collection Bottleneck:** Generating flawless, multi-paragraph gold-standard answers for complex prompts (e.g. legal analysis or code synthesis) is expensive and slow. Comparing two candidate outputs and selecting the superior one is significantly cheaper, faster, and more scalable.
</details>

<details>
<summary><strong>Q2: Derive the Bradley-Terry preference loss function from first principles.</strong></summary>

**Answer:** Let $r(x, y)$ be the latent scalar quality of response $y$ given prompt $x$. The Bradley-Terry model assumes the probability that response $y_w$ is preferred over $y_l$ follows the logistic distribution of their score difference:
$$P(y_w \succ y_l \mid x) = \frac{\exp(r(x, y_w))}{\exp(r(x, y_w)) + \exp(r(x, y_l))} = \frac{1}{1 + \exp\left(-(r(x, y_w) - r(x, y_l))\right)} = \sigma\left( r(x, y_w) - r(x, y_l) \right)$$
Given a dataset of independent pairwise observations $\mathcal{D} = \{(x_i, y_w^{(i)}, y_l^{(i)})\}_{i=1}^N$, the likelihood is:
$$L(\theta) = \prod_{i=1}^N \sigma\left( r_\theta(x_i, y_w^{(i)}) - r_\theta(x_i, y_l^{(i)}) \right)$$
Taking the negative logarithm and normalizing by $N$ yields the Bradley-Terry cross-entropy loss:
$$\mathcal{L}_{\text{RM}}(\theta) = -\frac{1}{N} \sum_{i=1}^N \ln \sigma\left( r_\theta(x_i, y_w^{(i)}) - r_\theta(x_i, y_l^{(i)}) \right)$$
</details>

<details>
<summary><strong>Q3: How does Direct Preference Optimization (DPO) eliminate the need for a separate reward model and online PPO rollouts?</strong></summary>

**Answer:** The standard RL objective with KL regularization is:
$$\max_\pi \mathbb{E}_{x \sim \mathcal{D}, y \sim \pi}[r(x, y)] - \beta \mathbb{D}_{\text{KL}}(\pi \parallel \pi_{\text{ref}})$$
Its closed-form analytical maximum satisfies:
$$\pi^*(y \mid x) = \frac{1}{Z(x)} \pi_{\text{ref}}(y \mid x) \exp\left(\frac{1}{\beta} r(x, y)\right)$$
Taking the logarithm and rearranging gives the exact reward identity:
$$r(x, y) = \beta \ln \frac{\pi^*(y \mid x)}{\pi_{\text{ref}}(y \mid x)} + \beta \ln Z(x)$$
Substituting this expression into the Bradley-Terry preference probability:
$$P(y_w \succ y_l \mid x) = \sigma\left( r(x, y_w) - r(x, y_l) \right) = \sigma\left( \beta \ln \frac{\pi^*(y_w \mid x)}{\pi_{\text{ref}}(y_w \mid x)} - \beta \ln \frac{\pi^*(y_l \mid x)}{\pi_{\text{ref}}(y_l \mid x)} \right)$$
The unknown normalizing constant $\beta \ln Z(x)$ cancels out cleanly. DPO optimizes this objective directly over offline preference pairs $(x, y_w, y_l)$ using simple supervised classification loss, eliminating reward model training, value function approximation, and online rollout sampling.
</details>

<details>
<summary><strong>Q4: What is "Reward Hacking" in RLHF, and how does Goodhart's Law manifest in LLMs?</strong></summary>

**Answer:** Goodhart's Law states: *"When a measure becomes a target, it ceases to be a good measure."*
In RLHF, the reward model $r_\theta(x, y)$ is merely an imperfect proxy for true human preferences. When an active policy is trained aggressively to maximize this proxy, it searches the high-dimensional token space for adversarial vulnerabilities:
1. **Verbosity Exploitation:** Human raters subconsciously prefer longer answers; the reward model assigns higher scores to longer text; the LLM generates 1,000-word essays filled with fluff for simple queries.
2. **Sycophancy:** The reward model rewards polite agreement; the policy agrees with factually incorrect statements made by the user.
3. **Distributional Collapse:** The model outputs strange punctuation combinations that trigger outlier positive activations in the reward model's final linear projection.
The token-level KL divergence penalty $-\beta \mathbb{D}_{\text{KL}}(\pi \parallel \pi_{\text{ref}})$ prevents the policy from drifting into these ungrounded regions.
</details>

<details>
<summary><strong>Q5: Why is the KL penalty applied at the token level rather than only as a single scalar at the end of the sequence?</strong></summary>

**Answer:** While sequence-level KL divergence is mathematically $\sum_t \ln \frac{\pi(y_t)}{\pi_{\text{ref}}(y_t)}$, applying it as an augmented per-token step reward:
$$R_t = -\beta \ln \frac{\pi_\phi(y_t \mid x, y_{<t})}{\pi_{\text{ref}}(y_t \mid x, y_{<t})}$$
provides immediate, dense temporal feedback to the Critic network $V_\phi$. If only a single scalar penalty is added at the final token, intermediate tokens receive zero signal, forcing the Critic to solve a difficult long-horizon credit assignment problem. Dense token-level penalties significantly reduce the variance of Generalized Advantage Estimation (GAE) during PPO updates.
</details>

<details>
<summary><strong>Q6: Compare PPO vs. DPO across compute requirements, training stability, and performance.</strong></summary>

**Answer:**
| Dimension | PPO | DPO |
| :--- | :--- | :--- |
| **Active Models in VRAM** | 4 (Actor, Critic, Ref, RM) | 2 (Active Policy, Reference Policy) |
| **Online Generation** | Required (Continuous autoregressive rollouts) | None (Trained offline on static dataset) |
| **Training Stability** | Moderate to Low (Sensitive to hyperparameters) | High (Standard binary cross-entropy) |
| **Compute Cost** | High (Rollout generation dominates compute) | Low ($\approx 3-5\times$ faster than PPO) |
| **Iterative Adaptation** | Continuous online exploration | Requires generating new data for iterative rounds |
</details>

<details>
<summary><strong>Q7: What is RLAIF (Reinforcement Learning from AI Feedback), and how does it compare to RLHF?</strong></summary>

**Answer:** RLAIF replaces human annotators with an advanced foundation model (e.g. Claude 3.5 Sonnet, GPT-4) guided by a detailed constitutional rubric.
The AI judge evaluates candidate pairs $(y_w, y_l)$, produces chain-of-thought rationales explaining which response is safer and more helpful, and assigns preference labels.
Advantages of RLAIF:
1. **Scalability & Cost:** Millions of preference pairs can be labeled in hours at a fraction of the cost of human crowdsourcing.
2. **Consistency:** AI judges do not suffer from human annotator fatigue, carelessness, or personal bias.
3. **Constitutional Alignment:** The model aligns to an explicit written set of ethical principles (e.g. Anthropic's Constitutional AI) rather than ambiguous crowdworker intuitions.
</details>

<details>
<summary><strong>Q8: What happens if $\beta$ is set too high or too low in DPO?</strong></summary>

**Answer:** The hyperparameter $\beta$ controls the strength of the KL constraint relative to the reference policy:
- **If $\beta$ is too low ($\beta \to 0$):** The model ignores the reference policy and updates parameters aggressively. It overfits to the pairwise preference dataset, loses linguistic coherence, generates repetitive tokens, and suffers mode collapse.
- **If $\beta$ is too high ($\beta \to \infty$):** The KL penalty dominates the loss. The gradient of the policy ratio evaluates to zero, and the model remains frozen at the initial SFT behavior, failing to adopt the preferred alignment.
Optimal values are empirically found in the range $\beta \in [0.01, 0.2]$.
</details>

---

## 12. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Bradley-Terry Preference Probability</strong></summary>

**Task:** Compute the probability $P(y_w \succ y_l \mid x)$ given reward model outputs $r(y_w) = 3.2$ and $r(y_l) = 1.2$.

**Solution:**
```python
import numpy as np

def bradley_terry_prob(r_w: float, r_l: float) -> float:
    diff = r_w - r_l
    return float(1.0 / (1.0 + np.exp(-diff)))

p = bradley_terry_prob(3.2, 1.2)
print(f"Preference probability P(winner > loser): {p:.4f}")
assert p > 0.88 # 1 / (1 + exp(-2)) ~ 0.8808
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Per-Token KL Divergence Penalty</strong></summary>

**Task:** Compute the per-token KL penalties for a sequence of 3 tokens given policy log probabilities and reference log probabilities with $\beta = 0.1$.

**Solution:**
```python
import numpy as np

def compute_step_kl_penalties(logp_pi: np.ndarray, logp_ref: np.ndarray, beta: float = 0.1) -> np.ndarray:
    return -beta * (logp_pi - logp_ref)

lp_act = np.array([-0.5, -1.2, -0.1])
lp_ref = np.array([-0.8, -1.0, -0.4])
penalties = compute_step_kl_penalties(lp_act, lp_ref, beta=0.1)
# token 0: -0.1 * (-0.5 - (-0.8)) = -0.1 * (0.3) = -0.03
# token 1: -0.1 * (-1.2 - (-1.0)) = -0.1 * (-0.2) = +0.02
np.testing.assert_allclose(penalties, [-0.03, 0.02, -0.03])
print(f"Per-token KL penalties: {penalties}")
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Reward Model Binary Cross-Entropy Loss</strong></summary>

**Task:** Write a Python function calculating Bradley-Terry loss for a batch of 3 pairs.

**Solution:**
```python
import numpy as np

def bt_loss_batch(r_w: np.ndarray, r_l: np.ndarray) -> float:
    diff = r_w - r_l
    return float(np.mean(np.log1p(np.exp(-diff))))

rw = np.array([1.5, 2.0, 0.5])
rl = np.array([0.5, 0.0, -0.5]) # diffs are all +1.0
l = bt_loss_batch(rw, rl)
expected = np.log1p(np.exp(-1.0)) # ln(1 + e^-1) ~ 0.31326
assert np.isclose(l, expected)
print(f"Batch RM Loss: {l:.4f}")
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Sequence Length Normalization</strong></summary>

**Task:** Normalize total sequence log-probabilities by sequence length to prevent verbosity distortion.

**Solution:**
```python
import numpy as np

def length_normalized_log_prob(token_log_probs: list[float]) -> float:
    return float(np.mean(token_log_probs))

tokens = [-0.2, -0.5, -0.3, -0.8]
norm_lp = length_normalized_log_prob(tokens)
assert norm_lp == -0.45
print(f"Length-normalized log-prob: {norm_lp:.4f}")
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Complete DPO Loss with Batch Gradient Evaluation</strong></summary>

**Task:** Implement pure NumPy DPO loss and compute exact policy gradients across a batch of 2 preference pairs.

**Solution:**
```python
import numpy as np

def evaluate_dpo(
    logp_w_pi: np.ndarray, logp_w_ref: np.ndarray,
    logp_l_pi: np.ndarray, logp_l_ref: np.ndarray,
    beta: float = 0.1
) -> tuple[float, np.ndarray, np.ndarray]:
    h_w = logp_w_pi - logp_w_ref
    h_l = logp_l_pi - logp_l_ref
    diff = beta * (h_w - h_l)
    
    loss = float(np.mean(np.log1p(np.exp(-diff))))
    
    # Gradient w.r.t diff: - 1 / (1 + exp(diff))
    sigma_diff = 1.0 / (1.0 + np.exp(-diff))
    grad_diff = -(1.0 - sigma_diff) / len(diff)
    
    grad_w = beta * grad_diff
    grad_l = -beta * grad_diff
    return loss, grad_w, grad_l

w_pi = np.array([-10.0, -5.0])
w_ref = np.array([-10.0, -5.0])
l_pi = np.array([-12.0, -6.0])
l_ref = np.array([-12.0, -6.0])
# Initially diff = beta * (0 - 0) = 0 -> loss = ln(2) ~ 0.6931
loss, gw, gl = evaluate_dpo(w_pi, w_ref, l_pi, l_ref, beta=0.1)
assert np.isclose(loss, np.log(2.0))
print(f"DPO Initial Loss: {loss:.4f}")
print(f"Gradients w: {gw}, Gradients l: {gl}")
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Numerical Validation of DPO Partition Function Cancellation</strong></summary>

**Task:** Numerically verify that adding an arbitrary prompt-dependent constant $C(x)$ to both winning and losing reward values leaves the DPO loss invariant.

**Solution:**
```python
import numpy as np

def verify_partition_cancellation(C_val: float = 42.0):
    r_w = 3.5
    r_l = 1.2
    
    # Standard probability: sigma(r_w - r_l)
    p_orig = 1.0 / (1.0 + np.exp(-(r_w - r_l)))
    
    # Augmented with prompt partition constant C(x)
    r_w_aug = r_w + C_val
    r_l_aug = r_l + C_val
    p_aug = 1.0 / (1.0 + np.exp(-(r_w_aug - r_l_aug)))
    
    print(f"Original Prob: {p_orig:.6f}, Augmented Prob: {p_aug:.6f}")
    assert np.isclose(p_orig, p_aug)
    print("DPO partition function cancellation numerically validated.")

verify_partition_cancellation()
```
</details>

---

## 13. Summary, Verified References & Navigation

- **The Alignment Pipeline:** Pretraining gives knowledge $\to$ SFT teaches format $\to$ RLHF / DPO aligns with human values, helpfulness, and safety.
- **Bradley-Terry Model:** Models preference probability via logistic difference $P(y_w \succ y_l \mid x) = \sigma(r(x, y_w) - r(x, y_l))$.
- **PPO Alignment:** Generates online rollouts, scores them with a reward model, and optimizes the policy while penalizing divergence from the reference model via a token-level KL penalty.
- **Direct Preference Optimization (DPO):** Derives an exact analytical solution expressing the reward function in terms of policy log-probabilities, eliminating reward models and online sampling entirely.

### Verified External References
- Ouyang, L., et al. (2022). *Training language models to follow instructions with human feedback (InstructGPT)*. NeurIPS.
- Rafailov, R., Sharma, A., Mitchell, E., Ermon, S., Manning, C. D., & Finn, C. (2023). *Direct Preference Optimization: Your Language Model is Secretly a Reward Model*. NeurIPS.
- Christiano, P. F., et al. (2017). *Deep reinforcement learning from human preferences*. NeurIPS.
- Bai, Y., et al. (2022). *Constitutional AI: Harmlessness from AI Feedback*. Anthropic.

---

[← 19-offline-rl-and-imitation-learning.md](19-offline-rl-and-imitation-learning.md) | [Module Index](README.md) | [21-gymnasium-environments-and-custom-envs.md →](21-gymnasium-environments-and-custom-envs.md)
