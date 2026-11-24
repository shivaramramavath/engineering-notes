# Performance — Senior Interview Questions & Deep Dives

> Profiling render lifecycles, INP/LCP optimization, DOM virtualization, code-splitting with dynamic imports, Web Workers offloading, and memory leak mitigation.

---

## Question 1: How do you systematically diagnose and fix an Interaction to Next Paint (INP) regression caused by heavy React component rendering?

### Diagnostic Process
1. **Field Telemetry & Chrome DevTools**:
   - Record user interactions in the **Performance Panel** with CPU throttling enabled (4x or 6x slowdown).
   - Locate long task bars (>50ms) following a `click` or `keydown` event. Inspect the "Interaction to Next Paint" breakdown:
     - **Input Delay**: Event listener wait time.
     - **Processing Duration**: Time spent executing event handlers and React reconciliation.
     - **Presentation Delay**: Browser style recalculation, layout, and compositing paint.
2. **React Profiler**:
   - Check flame graphs to see which component subtrees rendered and why (e.g., "Parent re-rendered", "Hooks changed").

### Targeted Fixes
1. **Transition De-prioritization**:
   Wrap non-urgent UI updates (e.g., tab switching, filtering 1,000 items) in `startTransition`:
   ```tsx
   startTransition(() => {
     setActiveTab(newTab);
   });
   ```
   This splits the render into interruptible slices, yielding to incoming user clicks.
2. **DOM Virtualization**:
   If the INP bottleneck is caused by mounting 500+ DOM nodes, integrate `@tanstack/react-virtual` to cap rendered nodes to only those visible in the viewport.
3. **Offloading Computation**:
   Move heavy sorting, regex parsing, or cryptography off the main thread into a **Web Worker** via Comlink.

---

## Question 2: What are the primary causes of Memory Leaks in single-page React applications, and how do you profile them?

### Top Memory Leak Culprits
1. **Uncleaned Event Listeners & Observers**:
   ```tsx
   useEffect(() => {
     window.addEventListener('resize', handleResize);
     // Missing return () => window.removeEventListener('resize', handleResize);
   }, []);
   ```
2. **Dangling WebSocket / Timers**:
   `setInterval` holding a closure reference to a component's scope keeps the entire Fiber node and DOM elements alive even after unmounting.
3. **Unbounded Global Caches**:
   Storing search queries or API responses in a global `Map` or array without an LRU eviction policy or WeakMap referencing.
4. **Uncleared Canvas / WebGL Contexts**:
   Failing to call `.dispose()` on Three.js geometries or Chart.js instances during component cleanup.

### Profiling Technique
1. Open Chrome DevTools ➔ **Memory Tab**.
2. Take a **Heap Snapshot** (Baseline).
3. Perform the user action (e.g., open modal, navigate to route, close modal) 5-10 times.
4. Force Garbage Collection (trash can icon) and take a second Heap Snapshot.
5. In the snapshot view, select **Objects allocated between Snapshot 1 and Snapshot 2** or inspect the **Comparison** view for detached `HTMLDivElement` or `FiberNode` retaining trees.
