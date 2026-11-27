# Design System & Specifications: Native Standards Accordion & Tabs Suite

## 1. Interaction & State Specifications

### Exclusive Accordion Mechanics
- **Collapsed State**: Height = `summary` height (~52px); body container has `grid-template-rows: 0fr`; chevron rotation = `0deg`.
- **Opening Animation**: Duration = `220ms`; timing function = `cubic-bezier(0.16, 1, 0.3, 1)`; grid interpolates to `1fr`; chevron rotates `180deg`.
- **Exclusivity**: Native browser engine closes currently open sibling sharing `name="faq"` attribute automatically.

---

## 2. Accessible Focus Flow

```
[User Presses Tab Key]
         │
         ▼
[Focus enters <summary> element: outline: 2px solid var(--accent); outline-offset: -2px]
         │
         ├── Pressing Enter / Space -> Toggles open state
         │
         ▼
[Focus tabs to next interactive link inside expanded accordion body]
```
