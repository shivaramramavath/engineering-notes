# Node State

A node-based editor has a lot of state: node positions, each node's configuration, which nodes are selected, connections, the viewport, undo history, and saved versions. The central question is **where each piece lives and how it changes**. Get this right and saving, validating, and running the graph is easy. Get it wrong and every feature fights stale or mutated objects.

## The graph is data

React Flow's `nodes` and `edges` are **plain serializable objects**. Treat them as the document your editor edits:

```ts
type Graph = { nodes: AppNode[]; edges: AppEdge[]; viewport?: Viewport }
```

Because they're plain data, you can `JSON.stringify` them, store them in a database, diff them, validate them, undo to earlier versions, and send them to a server to execute.

### Keep UI-only state out of the document

Separate **what the user is building** from **how it's currently being displayed**:

| In the saved graph | UI-only (don't save, or save separately) |
|---|---|
| Node `id`, `type`, `data` (configuration) | `selected`, `dragging` flags |
| `position` | Hover state, open dropdowns inside a node |
| Edges (`source`, `target`, handles, `data`) | `measured` width/height (derived) |
| Viewport (optional, per user) | Validation highlighting, temporary connection preview |

When saving, either strip UI fields or accept them and ignore on load. Keep **domain data** (what a "send email" step means: `to`, `subject`) in `node.data` separate from layout/visual state so the same graph can be executed without a browser.

## Updating nodes immutably

React Flow detects changes by **reference**. Mutating a node in place, or pushing onto the array, doesn't re-render:

```ts
// ✗ mutation: nothing updates
nodes[0].data.label = "New"
setNodes(nodes)

// ✓ new array, new node object, new data object
setNodes((nds) =>
  nds.map((n) => (n.id === id ? { ...n, data: { ...n.data, label: "New" } } : n))
)
```

Use the **functional form** of `setNodes` (`nds => …`) so you update from the latest state and avoid stale closures, especially in event handlers and async code ([stale closures](../../17-react-internals/02-how-hooks-work.md#closures-why-values-go-stale)).

### The `updateNodeData` helper

For the common case of editing a node's `data`, `useReactFlow` provides a shortcut that merges into `data` and does the immutable update:

```tsx
const { updateNodeData } = useReactFlow()
updateNodeData(id, { value: "42" })                              // shallow-merges into node.data
updateNodeData(id, (node) => ({ count: node.data.count + 1 }))   // function form
```

Use it inside custom nodes to wire up their inputs ([01](./01-custom-nodes-and-edges.md#a-custom-node)).

### Adding and removing

```ts
setNodes((nds) => [...nds, { id: crypto.randomUUID(), type: "action", position, data: { … } }])   // add
deleteElements({ nodes: [{ id }] })                                                              // remove (and connected edges)
```

- **Generate unique IDs** (`crypto.randomUUID()`, `nanoid`). A counter in memory collides after reload.
- **Prefer `deleteElements`** (or the built-in delete key) over filtering `setNodes` by hand: it also removes **connected edges** and fires `onNodesDelete`/`onEdgesDelete`. If you remove nodes manually with `setNodes`, you must remove their edges too, or you leave edges pointing at nothing.
- Deleting runs through `onBeforeDelete` (a hook in recent versions) if you need to confirm or block it. Check the docs for your version.

## Where should the state live?

| Approach | Fits when | Trade-off |
|---|---|---|
| **`useNodesState` / `useEdgesState`** in the editor component | Small to medium editors, one component owns the graph | Simple; state is awkward to read from far-away components |
| **A store (Zustand or similar)** | Sidebars, inspectors, toolbars, and many components need the graph; undo/redo; persistence | A bit more setup; excellent ergonomics and performance with selectors |
| **Uncontrolled** (`defaultNodes`) + `useReactFlow` | Prototypes and demos | You don't own the data; hard to validate, save, and sync |
| **Server state in a query cache** | A graph that is a persisted resource (saved workflows) | Keep the **working copy** in local state/store and treat the server copy as the saved version ([server state](../../12-server-state/README.md)) |

React Flow itself uses [Zustand](../../13-state-management/03-zustand.md) internally, and a Zustand store is a popular fit for your own graph state, since you can call actions from anywhere (sidebar, toolbar, keyboard shortcuts) without prop drilling:

```ts
import { create } from "zustand"
import { addEdge, applyEdgeChanges, applyNodeChanges, type Connection, type EdgeChange, type NodeChange } from "@xyflow/react"

type FlowState = {
  nodes: AppNode[]
  edges: AppEdge[]
  onNodesChange: (changes: NodeChange<AppNode>[]) => void
  onEdgesChange: (changes: EdgeChange<AppEdge>[]) => void
  onConnect: (connection: Connection) => void
  addNode: (node: AppNode) => void
  updateNodeData: (id: string, data: Partial<AppNode["data"]>) => void
}

export const useFlowStore = create<FlowState>()((set, get) => ({
  nodes: [],
  edges: [],
  onNodesChange: (changes) => set({ nodes: applyNodeChanges(changes, get().nodes) }),
  onEdgesChange: (changes) => set({ edges: applyEdgeChanges(changes, get().edges) }),
  onConnect: (connection) => set({ edges: addEdge(connection, get().edges) }),
  addNode: (node) => set({ nodes: [...get().nodes, node] }),
  updateNodeData: (id, data) =>
    set({ nodes: get().nodes.map((n) => (n.id === id ? { ...n, data: { ...n.data, ...data } } : n)) }),
}))
```

```tsx
const nodes = useFlowStore((s) => s.nodes)
const onNodesChange = useFlowStore((s) => s.onNodesChange)
<ReactFlow nodes={nodes} onNodesChange={onNodesChange} … />
```

`applyNodeChanges` / `applyEdgeChanges` are the helpers `useNodesState` uses. In a store you call them yourself. (Typing details around these generics vary by version, so check the docs.)

## Reading state efficiently inside nodes

Dragging a node produces a stream of change events. Any component subscribed to *all* nodes re-renders on every one:

```tsx
// ✗ re-renders on every drag tick of ANY node
const nodes = useNodes()

// ✓ subscribe to just what you need
const data = useNodesData<AppNode>(sourceId)                    // data of one specific node
const count = useStore((s) => s.nodeLookup.size)                // a derived primitive from React Flow's store
const connections = useNodeConnections({ handleType: "target" }) // this node's connections (name varies by version)
```

- **`useNodesData(id | ids)`** returns just the `data` of the given node(s), and only changes when that data does.
- **`useStore(selector)`** (React Flow's own store) with a selector returning a **primitive or stable value** avoids needless renders, with the same selector rules as [Zustand](../../13-state-management/03-zustand.md#selectors-the-key-to-performance).
- Reading `useNodes()`/`useEdges()` is fine in the *editor-level* component for something that genuinely depends on all nodes (a "node count" badge), but expensive inside every node.

## Sharing data between nodes

A powerful pattern: a node **reads the data of the nodes connected to its input**. Think "calculator" nodes, or a "preview" node showing upstream output.

```tsx
function AddNodeView({ id }: NodeProps<AddNode>) {
  const connections = useNodeConnections({ handleType: "target" })          // edges arriving at this node
  const upstream = useNodesData<NumberNode>(connections.map((c) => c.source))
  const total = upstream.reduce((sum, n) => sum + (n?.data.value ?? 0), 0)

  return (
    <div className="rounded border bg-background p-3">
      <Handle type="target" position={Position.Left} />
      <p>Sum: {total}</p>
      <Handle type="source" position={Position.Right} />
    </div>
  )
}
```

The node re-renders only when its **connections or upstream data** change, since the hooks subscribe narrowly. Names of the connection hook have changed across v12 releases (`useHandleConnections` → `useNodeConnections`), so check yours.

For **workflow execution** (not just display), don't compute through React components. Walk the **graph as data** instead, outside React:

```ts
function executionOrder(nodes: AppNode[], edges: AppEdge[]): AppNode[] {
  // topological sort (Kahn's algorithm): dependencies before dependents
  const incoming = new Map(nodes.map((n) => [n.id, 0]))
  edges.forEach((e) => incoming.set(e.target, (incoming.get(e.target) ?? 0) + 1))
  const queue = nodes.filter((n) => incoming.get(n.id) === 0)
  const order: AppNode[] = []
  while (queue.length) {
    const node = queue.shift()!
    order.push(node)
    for (const e of edges.filter((e) => e.source === node.id)) {
      incoming.set(e.target, incoming.get(e.target)! - 1)
      if (incoming.get(e.target) === 0) queue.push(nodes.find((n) => n.id === e.target)!)
    }
  }
  return order            // if order.length < nodes.length, the graph has a cycle
}
```

This is **pure domain logic**: testable without a canvas ([layered architecture](../../20-frontend-architecture/01-layered-architecture.md)). Execution typically happens on the **server**, which must **re-validate the graph itself** ([03](./03-connection-validation.md#validating-the-whole-graph)).

## Selection and other UI state

- Selected nodes are tracked in the node objects (`selected: true`) and reported through `onSelectionChange`. For an inspector panel, store the **selected node IDs**, and look the node up by ID from the current state (rather than holding a copy of the node object, which goes stale).

```tsx
const selectedId = useFlowStore((s) => s.nodes.find((n) => n.selected)?.id)
const selected = useFlowStore((s) => s.nodes.find((n) => n.id === selectedId))
```

- Keep **panel/inspector open state**, hover state, and similar in ordinary component state or a separate store slice. Don't put it into the node `data` that gets saved.

## Persistence

```tsx
const { toObject, setViewport } = useReactFlow()

// Save
const snapshot = toObject()                    // { nodes, edges, viewport }
await api.saveWorkflow(id, JSON.stringify(snapshot))

// Load
const saved = JSON.parse(raw) as Graph         // validate before trusting! (below)
setNodes(saved.nodes)
setEdges(saved.edges)
if (saved.viewport) setViewport(saved.viewport)
```

Practical concerns:

- **Validate on load.** Saved data can be old, hand-edited, or from a previous schema. Parse it with a schema ([validating responses](../../11-api-integration/02-api-client.md#validating-responses)) and **migrate** older versions (a `version` field in the document).
- **Strip transient fields** (`selected`, `dragging`, `measured`) before saving, so diffs and autosaves aren't noisy.
- **Autosave** debounced, and also on `onNodeDragStop` (not on every drag tick). Show save status, and handle failures ([mutations](../../12-server-state/05-mutations.md), [optimistic updates](../../12-server-state/06-optimistic-updates.md)).
- **Guard unsaved changes** when navigating away ([`useBlocker`](../../10-routing/03-navigation.md#guarding-against-lost-work)).
- **Concurrent editing** (multiple users) is a hard problem needing an operational transform/CRDT approach (for example Yjs), conflict handling, and presence. It's a specialization of its own.
- After loading, call **`fitView`** (or restore the saved viewport), and remember measured sizes arrive *after* first render.

## Undo and redo

Because the graph is immutable data, undo/redo is a **history of snapshots**:

```ts
type History = { past: Graph[]; present: Graph; future: Graph[] }

function commit(next: Graph) {            // call on meaningful actions, NOT on every drag tick
  set(({ past, present }) => ({ past: [...past, present].slice(-50), present: next, future: [] }))
}
function undo() { /* pop from past → present; push old present onto future */ }
function redo() { /* the reverse */ }
```

- **Record snapshots at the end of gestures** (`onNodeDragStop`, `onConnect`, after edits, `onNodesDelete`), not for every change event, or one drag fills your history with hundreds of entries.
- Keep a **cap** (50 steps), and consider structural sharing (immutable updates already share unchanged objects).
- Don't record **selection or viewport changes** as history steps.
- Bind to **Ctrl/Cmd+Z / Shift+Z** and provide visible buttons.

## Performance notes

- Dragging triggers many `onNodesChange` calls. Keep handlers **cheap**, and avoid expensive work (validation, saving, network calls) per change. Do it on gesture end.
- **`memo`** node components ([01](./01-custom-nodes-and-edges.md#performance-of-custom-nodes)), use **selectors**, and keep `nodeTypes` stable.
- Prefer `useNodesData`/targeted selectors over `useNodes()`.
- For large graphs: `onlyRenderVisibleElements`, simpler nodes, and lighter edge styles (many animated edges are costly).
- Don't mirror the whole `nodes` array into other state (derive instead, [derive don't store](../../12-server-state/00-server-vs-client-state.md#derive-dont-store)).

## Common mistakes

- **Mutating nodes or edges** instead of creating new objects.
- **Non-functional `setNodes`** in handlers, overwriting newer state with stale closures.
- **Removing nodes with `setNodes` only**, leaving dangling edges.
- **Counter-based IDs** that collide after reload.
- **Saving UI state** (`selected`, `dragging`, `measured`) into the document.
- **Saving on every `onNodesChange`** tick.
- **Trusting loaded JSON** without validation or migration.
- **Subscribing every node to everything** (`useNodes()` inside nodes).
- **Holding a copy of a selected node** in state, which goes stale as the node changes. Store the ID.
- **Computing execution inside React components** instead of from the graph data.
- **Relying on the client's graph for execution** without re-validating on the server.
- **Recording undo history per drag tick**, producing unusable history.
- **Forgetting `ReactFlowProvider`** when state hooks are used outside the canvas.

## Quick summary

- The graph is **plain, serializable data**. Keep the document (nodes, edges, domain `data`) separate from UI state (selection, hover, measured sizes, open panels).
- Update **immutably** (new objects/arrays), prefer **functional `setNodes`**, use **`updateNodeData`** for node data, **`deleteElements`** for removal (it handles connected edges), and generate **unique IDs**.
- For anything beyond a small editor, hold graph state in a **store (e.g. Zustand)** using `applyNodeChanges`/`applyEdgeChanges`, so toolbars and inspectors can read and act without prop drilling.
- Read state narrowly (`useNodesData`, `useStore` selectors, connection hooks), not `useNodes()` everywhere; share data between nodes via their connections.
- **Execute from the graph as data** (topological order), not from components, and validate again on the server.
- **Persist** with `toObject()`: validate and migrate on load, strip transient fields, debounce autosave, and save at gesture end.
- **Undo/redo** = snapshots recorded at the end of gestures.

## Next

[03 — Connection validation](./03-connection-validation.md)