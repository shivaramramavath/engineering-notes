# Module 01: Core Fundamentals

## Syllabus & Topic Overview

Module 01 explores the foundational principles that govern the React programming model. Moving beyond superficial syntax tutorials, this module examines how React transforms declarative JSX into virtual element trees, how reconciliation keys protect against state aliasing, synthetic event delegation, and the mathematical purity contract required for Concurrent Mode and the React Compiler.

### Concept Notes Index

1. [00-thinking-in-react.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/01-fundamentals/00-thinking-in-react.md) - The mental model: decomposing mockups into component hierarchies, identifying minimal state representation, and inverse data flow.
2. [01-jsx.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/01-fundamentals/01-jsx.md) - JavaScript XML syntax rules: single parent constraint, fragment syntax (`<>`), camelCase attributes, and embedding expressions.
3. [02-components-and-props.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/01-fundamentals/02-components-and-props.md) - Functional components, read-only props, default parameters, destructuring, and unidirectional data flow.
4. [03-children-and-composition.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/01-fundamentals/03-children-and-composition.md) - The `children` prop, container components, visual slots, and composition over inheritance.
5. [04-conditional-rendering.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/01-fundamentals/04-conditional-rendering.md) - Ternary operators, short-circuit evaluation (`&&`), early returns, and the `0` falsy number rendering bug.
6. [05-lists-and-keys.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/01-fundamentals/05-lists-and-keys.md) - Rendering collections with `map()`, why index keys cause state corruption, stable ID generation, and reconciliation diffing.
7. [06-events.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/01-fundamentals/06-events.md) - Synthetic events, event delegation to root container (`#root`), synthetic event pooling removal, stopping propagation, and passive listeners.
8. [07-forms-basics.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/01-fundamentals/07-forms-basics.md) - Controlled inputs with `value`/`onChange`, uncontrolled inputs with `useRef`, `defaultValue`, and native form reset behaviors.
9. [08-jsx-transform-and-compilation.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/01-fundamentals/08-jsx-transform-and-compilation.md) - Deep dive into modern `react/jsx-runtime`, `_jsx` vs `_jsxs`, the `$$typeof` XSS security tag, and element objects.
10. [09-component-purity-and-side-effects.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/01-fundamentals/09-component-purity-and-side-effects.md) - The $\text{UI} = f(\text{State}, \text{Props})$ formula, side effect isolation, local mutations, derived state, and React Compiler rules.

---

## Learning Outcomes & Core Takeaways

- **JSX De-mystified**: Understand what JSX actually outputs (`_jsx` element trees) and how the internal `$$typeof` Symbol prevents cross-site scripting (XSS) via injected JSON.
- **Component Purity**: Write deterministic components with zero render side-effects, unlocking automatic memoization in the React Compiler.
- **Reconciliation Keys**: Avoid state-aliasing bugs by always providing unique, stable string/numeric keys rather than array indices.
- **Modern Event Architecture**: Leverage React's root-container synthetic event delegation system without legacy event pooling traps.
