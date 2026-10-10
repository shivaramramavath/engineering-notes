# Connection Validation

Out of the box, React Flow lets users connect **any handle to any other handle**. That's rarely what a real editor wants. A workflow can't feed an output into a trigger, a node's "number" input can't accept "text", a branch can't loop back into its own ancestors, and a single-input node shouldn't receive five connections.

**Connection validation** is where your domain rules enter the canvas. Done well, invalid connections are **refused as the user drags** (they can see what's allowed), and the saved graph is **always valid**.

## Where validation happens

Validate at **three levels**, because each catches different problems:

| Level | When | Catches |
|---|---|---|
| **1. During the drag** | While the user drags a connection (`isValidConnection`) | Impossible connections, so they can't even be completed (instant feedback) |
| **2. On connect** | In `onConnect`, before adding the edge | Anything that slipped past (defense in depth), plus rules you can't check mid-drag |
| **3. Whole graph** | Before saving/running, and on load | Missing required inputs, cycles in imported data, orphan nodes, and dangling edges |

And **the server must validate again** if the graph is executed or trusted there. Client-side validation is user experience, not security ([security](../../19-production/05-security.md)).

## `isValidConnection`

The main hook. Pass a function to `<ReactFlow>` that receives the **proposed connection** and returns `true` or `false`:

```tsx
import { ReactFlow, useReactFlow, type Connection, type Edge, type IsValidConnection } from "@xyflow/react"

function Canvas() {
  const { getNodes, getEdges } = useReactFlow()

  const isValidConnection: IsValidConnection = useCallback(
    (connection: Connection | Edge) => {
      const { source, target, sourceHandle, targetHandle } = connection

      if (source === target) return false                                   // no self-connections

      const edges = getEdges()
      const duplicate = edges.some(
        (e) => e.source === source && e.target === target && e.sourceHandle === sourceHandle && e.targetHandle === targetHandle
      )
      if (duplicate) return false                                           // no duplicate edges

      return true
    },
    [getEdges]
  )

  return <ReactFlow isValidConnection={isValidConnection} … />
}
```

- While dragging, React Flow calls it as the pointer moves over candidate handles. **Invalid targets don't snap or complete**, and the connection line can indicate it.
- **Read the latest graph with `getNodes()`/`getEdges()`** inside the function, not from captured state. This avoids stale closures and keeps the function stable (so it doesn't churn re-renders).
- Keep it **fast and pure**. It runs frequently during drags. Don't do network calls, or heavy graph traversal on huge graphs without caching.
- The handler can also be set **per handle** (`<Handle isValidConnection={…} />`), useful when one node type has specific rules.

## Common rules

### Port types (typed handles)

Give each handle a **type** (stored in its `id` or node `data`), and only allow matching ones:

```ts
type PortType = "number" | "text" | "boolean" | "any"

// handle ids encode the type: "out:number", "in:text"
const portType = (handleId?: string | null): PortType => (handleId?.split(":")[1] as PortType) ?? "any"

function typesCompatible(from: PortType, to: PortType) {
  return from === to || from === "any" || to === "any"
}

// inside isValidConnection
if (!typesCompatible(portType(connection.sourceHandle), portType(connection.targetHandle))) return false
```

A `number` output can feed a `number` input but not a `text` input. For richer systems (automatic coercion, generics), encode that in the compatibility function. Store port definitions in a **registry** keyed by node type, rather than scattering strings:

```ts
const nodeSpecs = {
  number: { outputs: [{ id: "out", type: "number" }], inputs: [] },
  add:    { outputs: [{ id: "out", type: "number" }], inputs: [{ id: "a", type: "number", max: 1 }, { id: "b", type: "number", max: 1 }] },
  text:   { outputs: [{ id: "out", type: "text" }],   inputs: [] },
}
```

The registry drives the node UI (handles), validation, and the execution engine from one source of truth ([layered architecture: domain](../../20-frontend-architecture/01-layered-architecture.md)).

### Direction and role

- Only **source → target** connections make sense. Handles of the same kind (source → source) are rejected by default in `strict` mode (`connectionMode`, below).
- Some nodes shouldn't accept inputs (**triggers**) or produce outputs (**terminals**). Express it in the node spec, or simply by **not rendering** that handle.

### Connection limits

```ts
const incomingToTarget = getEdges().filter((e) => e.target === target && e.targetHandle === targetHandle)
if (spec.inputs.find((i) => i.id === targetHandleName)?.max === 1 && incomingToTarget.length >= 1) return false
```

"Each input accepts at most one connection" is a very common rule (outputs can usually fan out to many inputs). Alternatives: **replace** the existing edge on connect (drop the old one, add the new), which is friendlier than refusing.

### Handle-level rules

Handles have their own switches:

```tsx
<Handle type="target" position={Position.Left} isConnectable={inputCount < 1} />           // full: no more connections
<Handle type="source" position={Position.Right} isConnectableStart={true} isConnectableEnd={false} />
```

- **`isConnectable`**: whether the handle can be connected at all.
- **`isConnectableStart` / `isConnectableEnd`**: whether a connection can start from or end at it.
- Prefer these for simple, **static** per-node rules, and `isValidConnection` for rules depending on the **other** end or the graph.

### `connectionMode`

```tsx
<ReactFlow connectionMode={ConnectionMode.Strict} />   // default: source handles connect only to target handles
<ReactFlow connectionMode={ConnectionMode.Loose} />    // any handle to any handle
```

Use **strict** for directed flows, and loose for undirected diagrams (mind maps, networks), where normalizing source/target direction yourself keeps data consistent.

## Preventing cycles

Most workflow and dataflow editors need a **directed acyclic graph (DAG)**: no loops, or execution never finishes. Check whether the new edge would create a cycle by seeing if the **source is reachable from the target**:

```tsx
import { getOutgoers, type Connection, type Edge, type Node } from "@xyflow/react"

function createsCycle(connection: Connection | Edge, nodes: Node[], edges: Edge[]): boolean {
  const target = nodes.find((n) => n.id === connection.target)
  if (!target) return false

  const visited = new Set<string>()
  function reaches(node: Node): boolean {
    if (node.id === connection.source) return true              // came back to the source: a cycle
    if (visited.has(node.id)) return false
    visited.add(node.id)
    return getOutgoers(node, nodes, edges).some(reaches)
  }
  return reaches(target)
}

const isValidConnection: IsValidConnection = useCallback((c) => {
  if (c.source === c.target) return false
  return !createsCycle(c, getNodes(), getEdges())
}, [getNodes, getEdges])
```

- `getOutgoers(node, nodes, edges)` returns the nodes a node connects *to*. (`getIncomers` and `getConnectedEdges` are the related helpers.)
- The traversal is **O(nodes + edges)** per check. That's fine for typical editors, but for graphs with thousands of nodes consider maintaining an adjacency map.
- If **loops are legitimate** (retry loops, state machines), allow them but mark them explicitly (a special "loop back" edge type) so execution can handle them intentionally.

## Feedback while connecting

Users should *see* what's allowed:

- **Handle styling**: React Flow adds state classes to handles during a connection drag (for example for "connecting from," "valid target," etc., and the exact class names and hooks, such as `useConnection`, depend on your v12 minor version, so check the docs). Style valid targets to glow and invalid ones to dim.

```css
.react-flow__handle.connectingto:not(.valid) { background: #ef4444; }   /* hovering an invalid target */
.react-flow__handle.valid { background: #22c55e; }                       /* hovering a valid one */
```

- **Connection line**: color the preview line, or use a custom `connectionLineComponent`, based on validity.
- **Reasons**, not just refusals. A silent "nothing happens" feels broken. Use a tooltip or status message ("Can't connect text output to number input") from `onConnectEnd` when a drag ends without connecting.
- **Dim incompatible handles** on all nodes when a drag begins (`onConnectStart` stores the source type in state, and handles style themselves against it).

## `onConnect`: validate again, then add

```tsx
const onConnect: OnConnect = useCallback(
  (connection) => {
    if (!isValidConnection(connection)) return                       // defense in depth
    setEdges((eds) => {
      const withoutOld = replaceExistingInput(eds, connection)       // e.g. single-input rule: replace
      return addEdge({ ...connection, type: "deletable", data: { createdAt: Date.now() } }, withoutOld)
    })
  },
  [isValidConnection, setEdges]
)
```

- `addEdge` also prevents exact duplicates by default.
- Edges can be added by **other routes** than dragging: loaded data, paste, programmatic APIs, imports. `onConnect` doesn't cover those. That's why [whole-graph validation](#validating-the-whole-graph) matters.
- **Reconnecting** existing edges (dragging an edge end to a new handle) goes through `onReconnect` (v12; `onEdgeUpdate` in v11). Apply the same checks, using the `reconnectEdge` helper:

```tsx
const onReconnect = useCallback((oldEdge: Edge, newConnection: Connection) => {
  if (!isValidConnection(newConnection)) return
  setEdges((eds) => reconnectEdge(oldEdge, newConnection, eds))
}, [isValidConnection, setEdges])
```

### Creating nodes by dropping a connection on empty space

`onConnectEnd` fires when a drag ends; if it ended on the pane (not on a handle), you can open a "which node?" menu and create the node plus the edge together. Use `screenToFlowPosition` for placement ([04](./04-drag-and-drop.md#dropping-onto-the-canvas)), and run the new connection through the same validation.

## Validating the whole graph

Connection-time checks can't catch everything. Write a **graph validator** as a pure function that returns a list of issues:

```ts
type Issue = { level: "error" | "warning"; nodeId?: string; edgeId?: string; message: string }

export function validateGraph(nodes: AppNode[], edges: AppEdge[]): Issue[] {
  const issues: Issue[] = []
  const ids = new Set(nodes.map((n) => n.id))

  // 1. Dangling edges
  for (const e of edges) {
    if (!ids.has(e.source) || !ids.has(e.target)) issues.push({ level: "error", edgeId: e.id, message: "Connection refers to a missing node" })
  }

  // 2. Exactly one trigger
  const triggers = nodes.filter((n) => n.type === "trigger")
  if (triggers.length !== 1) issues.push({ level: "error", message: `A workflow needs exactly one trigger (found ${triggers.length})` })

  // 3. Required inputs connected
  for (const n of nodes) {
    for (const input of nodeSpecs[n.type].inputs.filter((i) => i.required)) {
      if (!edges.some((e) => e.target === n.id && e.targetHandle === input.id))
        issues.push({ level: "error", nodeId: n.id, message: `"${input.label}" is not connected` })
    }
  }

  // 4. Cycles, 5. unreachable/orphan nodes (not reachable from the trigger), 6. node config completeness…
  if (hasCycle(nodes, edges)) issues.push({ level: "error", message: "The workflow contains a loop" })

  return issues
}
```

Run it:

- **On load**, because saved or imported data may be invalid (old versions, hand edits, bugs).
- **Before saving or running**, blocking on errors and allowing warnings.
- **Continuously (debounced)** to show live status, with an error badge on offending nodes and a problems panel listing issues (click to focus the node with `fitView({ nodes: [{ id }] })`).
- **On the server**, as a required step before execution or persistence of anything that matters. Never trust that the client sent a valid graph.

Because `validateGraph` is pure, it's trivial to unit test with small graphs ([testing pure logic](../../18-testing-and-debugging/00-testing-fundamentals.md#testing-pure-logic)), which is where most of the editor's *correctness* lives.

## Node-level validation

Each node also has its own **configuration** to validate (required fields, value ranges). Combine:

- Form validation inside the node or inspector ([forms](../../06-forms/01-form-validation.md)), storing a `valid` flag or error list derived from `data`.
- The graph validator reading the same rules (so "send email requires a recipient" isn't coded twice).
- Visual state: a red outline and an error icon on invalid nodes, plus an accessible message (not color alone).

## Handling invalid data on load

When loading a graph with problems, decide a policy rather than crashing:

- **Repair what's safe**: drop edges to missing nodes, assign missing ids.
- **Flag the rest** visibly and let the user fix them.
- **Refuse to run** until errors are resolved.
- Log unexpected repairs to monitoring ([error monitoring](../../19-production/06-error-monitoring-and-logging.md)), since they indicate bugs or version drift.

## Testing validation

- **Unit-test** `isValidConnection` rules and `validateGraph` with tables of small graphs: self-connection, duplicate, wrong types, cycles (direct and indirect), max connections, missing inputs.
- **Component/integration tests** can check handle state and edge creation, but jsdom has no real pointer geometry or layout, so connection **dragging** is best tested in a real browser ([Playwright](../../18-testing-and-debugging/06-e2e-testing-playwright.md)), using explicit mouse down, move, and up steps.
- If you render React Flow in jsdom at all, it needs `ResizeObserver` (and `DOMMatrixReadOnly`) stubs ([mocks](../../18-testing-and-debugging/04-mocking-and-msw.md#browser-apis-jsdom-doesnt-provide)).

## Common mistakes

- **No validation at all**, so users build impossible graphs the engine can't run.
- **Validating only at drag time**, missing loaded, pasted, imported, or programmatic edges.
- **Closing over stale `nodes`/`edges`** inside `isValidConnection` instead of `getNodes()`/`getEdges()`.
- **Slow `isValidConnection`** (heavy traversal, async work), making dragging laggy.
- **Silent refusal** with no feedback, so users think the editor is broken.
- **Forgetting cycle detection** in DAG-only systems (and runaway execution).
- **Trusting the client's graph on the server.**
- **Allowing multiple edges into single-input handles**, producing ambiguous execution.
- **Putting rules in several places** (node UI, validator, engine) that drift apart. Use one node spec registry.
- **Not validating reconnections** (`onReconnect`) the way you validate new connections.
- **Dangling edges after node deletion**, invisible but still in your data.
- **Relying on color alone** to show invalid state.

## Quick summary

- Default React Flow allows any connection. **Your rules** go in `isValidConnection` (live, during drag), `onConnect`/`onReconnect` (defense in depth), and a **whole-graph validator** (load/save/run).
- Typical rules: no self or duplicate connections, **typed ports**, direction/role, **max connections** (or replace), and **no cycles** via `getOutgoers` traversal.
- Use `getNodes()`/`getEdges()` inside the validator (avoid stale closures), and keep it **fast and pure**.
- Drive handles, validation, and execution from one **node spec registry**.
- Give users **visual feedback and reasons** (handle states, connection line, messages).
- Write `validateGraph(nodes, edges): Issue[]` as a pure, tested function; run it on load, before save/run, live (debounced), and **on the server**.
- Client validation is UX; the server must re-validate anything it executes or trusts.

## Next

[04 — Drag and drop](./04-drag-and-drop.md)