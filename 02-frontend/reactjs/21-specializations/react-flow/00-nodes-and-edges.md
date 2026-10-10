# Nodes and Edges

Everything in React Flow is built from two kinds of objects: **nodes** (the things) and **edges** (the connections). This note covers the data model, the built-in pieces, how the viewport works, and the props and events you'll use constantly.

## Nodes

A node is a plain object:

```ts
import type { Node } from "@xyflow/react"

const node: Node = {
  id: "step-1",                       // unique string: the identity
  type: "default",                    // which component renders it ("default", "input", "output", "group", or your own)
  position: { x: 120, y: 80 },        // top-left corner, in FLOW coordinates
  data: { label: "Fetch users" },     // anything you want: your payload for this node
}
```

| Field | Notes |
|---|---|
| `id` | **Required, unique, a string.** Edges refer to nodes by id |
| `position` | **Required.** `{ x, y }` in the flow's coordinate space |
| `data` | **Required.** Your data. Custom nodes read it from props |
| `type` | Selects the renderer. Omit for `"default"` |
| `selected`, `hidden`, `draggable`, `selectable`, `connectable`, `deletable` | Per-node behavior flags |
| `className`, `style` | Styling for the node wrapper |
| `parentId`, `extent` | Grouping/sub-flows ([04](./04-drag-and-drop.md#grouping-and-sub-flows)) |
| `width`, `height` | Set an explicit size (v12). Measured size is in `node.measured` |
| `dragHandle` | CSS selector limiting which part of the node starts a drag |
| `sourcePosition`, `targetPosition` | For built-in nodes: which side the handles sit on |
| `ariaLabel` | Accessible name for the node (check the current docs for a11y fields) |

### Built-in node types

| `type` | Handles | Use for |
|---|---|---|
| `"default"` | one target (top) + one source (bottom) | Generic steps |
| `"input"` | source only | A starting node |
| `"output"` | target only | An ending node |
| `"group"` | none | A container for other nodes ([sub-flows](./04-drag-and-drop.md#grouping-and-sub-flows)) |

Anything else is a **custom node** ([01](./01-custom-nodes-and-edges.md)). Real applications mostly use custom nodes, with the built-ins for prototypes.

### Typing nodes

```ts
type TextNode = Node<{ text: string }, "text">
type ImageNode = Node<{ src: string; alt: string }, "image">
type AppNode = TextNode | ImageNode              // a discriminated union on `type`
```

Type the generics (`<Data, Type>`) so `data` is checked in handlers and custom nodes. Using a union lets `switch (node.type)` narrow `node.data` safely.

## Edges

```ts
import type { Edge } from "@xyflow/react"

const edge: Edge = {
  id: "e1-2",
  source: "step-1",                  // node id where it starts
  target: "step-2",                  // node id where it ends
  sourceHandle: "out-true",          // optional: which handle, if the node has several
  targetHandle: "in-a",
  type: "smoothstep",                // built-in path style or your custom edge
  label: "if true",
  animated: true,                    // marching-ants dashed animation
  markerEnd: { type: MarkerType.ArrowClosed },
  style: { stroke: "#6366f1", strokeWidth: 2 },
}
```

- **`source`** and **`target`** must be existing node ids. An edge pointing at a missing node is silently not rendered, which is a common debugging puzzle after deleting or loading data.
- **`sourceHandle`/`targetHandle`** only matter when a node has multiple handles of that kind. Without them React Flow uses the node's default handle.
- Edge `id`s must be unique. `addEdge` generates one from the connection if you don't provide it.

### Built-in edge types

| `type` | Shape |
|---|---|
| (default) | Smooth bezier curve |
| `"straight"` | Straight line |
| `"step"` | Right-angle (orthogonal) path |
| `"smoothstep"` | Orthogonal path with rounded corners |
| `"simplebezier"` | A simpler bezier |

### Arrowheads

```tsx
import { MarkerType } from "@xyflow/react"
markerEnd: { type: MarkerType.ArrowClosed, width: 18, height: 18, color: "#6366f1" }
```

Set a default for all new edges with `defaultEdgeOptions={{ type: "smoothstep", markerEnd: { type: MarkerType.ArrowClosed } }}` on `<ReactFlow>`.

## Controlled state

The standard setup keeps `nodes` and `edges` in React state and updates them from change callbacks:

```tsx
const [nodes, setNodes, onNodesChange] = useNodesState(initialNodes)
const [edges, setEdges, onEdgesChange] = useEdgesState(initialEdges)
```

- **`onNodesChange`** receives a list of **change objects** (position moved, dimensions measured, selected, removed, added). `useNodesState` applies them for you using `applyNodeChanges`. Same for edges.
- You can intercept changes to apply your own rules, such as preventing deletion or clamping positions:

```tsx
const handleNodesChange: OnNodesChange = (changes) => {
  const allowed = changes.filter((c) => !(c.type === "remove" && c.id === "start"))   // can't delete the start node
  setNodes((nds) => applyNodeChanges(allowed, nds))
}
```

- **`onConnect`** is called when the user completes a new connection. **You** decide whether and how to add it (`addEdge(connection, edges)`), which is where rules go ([03](./03-connection-validation.md)).
- There's also an **uncontrolled** mode (`defaultNodes`/`defaultEdges`). React Flow keeps the state internally, and you read it with `useReactFlow()`. It's convenient for demos, but real apps need to own the state for persistence, validation, and sync.

**State must be treated immutably.** To change a node, create a new object. Mutating the array or objects in place doesn't trigger updates ([node state](./02-node-state.md#updating-nodes-immutably)).

## The viewport

React Flow has an **infinite canvas** with a viewport defined by `{ x, y, zoom }`:

```text
flow coordinates (where nodes live)         screen coordinates (pixels in the browser)
   node at (500, 300)  ──[ viewport: pan + zoom ]──►  appears at (clientX, clientY)
```

- Users **pan** (drag the background, or scroll/space-drag depending on config) and **zoom** (wheel, pinch, controls).
- **`fitView`** (prop) fits all nodes in view on load. The `fitView()` function from `useReactFlow()` does it on demand.
- `defaultViewport`, `minZoom`, `maxZoom`, `snapToGrid`, `snapGrid={[16, 16]}` control the canvas.
- **Converting coordinates** matters for drag-and-drop and context menus: `screenToFlowPosition({ x: clientX, y: clientY })` ([04](./04-drag-and-drop.md#dropping-onto-the-canvas)), and `flowToScreenPosition` for the reverse.

```tsx
const { fitView, zoomIn, zoomOut, setViewport, getViewport, zoomTo } = useReactFlow()
fitView({ padding: 0.2, duration: 300 })
setViewport({ x: 0, y: 0, zoom: 1 }, { duration: 200 })
```

## Helper components

Place these as **children of `<ReactFlow>`**:

```tsx
<ReactFlow …>
  <Background variant={BackgroundVariant.Dots} gap={16} size={1} />   {/* dots, lines, or cross grid */}
  <Controls />                                                         {/* zoom in/out, fit, lock */}
  <MiniMap pannable zoomable />                                        {/* overview map */}
  <Panel position="top-left"><button>Add node</button></Panel>         {/* fixed UI overlay on the canvas */}
</ReactFlow>
```

- **`Panel`** is the right place for toolbars and buttons that live on the canvas but don't move with it.
- `MiniMap` can color nodes (`nodeColor`) and is itself interactive with `pannable`/`zoomable`.
- `Controls` has `showInteractive`, `showFitView`, and so on.
- `NodeToolbar`, `NodeResizer`, and `EdgeLabelRenderer` are used inside custom nodes and edges ([01](./01-custom-nodes-and-edges.md)).

## Interaction props you'll reach for

```tsx
<ReactFlow
  nodesDraggable            // default true
  nodesConnectable          // default true
  elementsSelectable        // default true
  panOnDrag                 // drag the background to pan
  panOnScroll={false}       // false: wheel zooms; true: wheel pans
  zoomOnScroll
  selectionOnDrag           // drag on the background draws a selection box (pair with panOnDrag={[1, 2]} to pan with middle/right button)
  multiSelectionKeyCode="Shift"
  deleteKeyCode={["Backspace", "Delete"]}
  snapToGrid snapGrid={[16, 16]}
  minZoom={0.2} maxZoom={2}
  colorMode="system"        // "light" | "dark" | "system" (v12)
/>
```

Disable features for **read-only viewers** (`nodesDraggable={false}`, `nodesConnectable={false}`, `elementsSelectable={false}`) or set `fitView` with fixed viewport.

## Events

```tsx
<ReactFlow
  onNodeClick={(event, node) => select(node.id)}
  onNodeDoubleClick={(event, node) => openEditor(node.id)}
  onEdgeClick={(event, edge) => …}
  onPaneClick={() => clearSelection()}
  onSelectionChange={({ nodes, edges }) => setSelected(nodes.map((n) => n.id))}
  onNodeDragStop={(event, node) => save(node)}
  onNodesDelete={(deleted) => …}
  onEdgesDelete={(deleted) => …}
  onConnect={onConnect}
  onInit={(instance) => …}
/>
```

- **`onNodeDragStop`** is the right hook for persisting a moved node. `onNodesChange` fires many times *during* a drag.
- **`onSelectionChange`** is a callback that must be **stable** (`useCallback`), or it can retrigger. Defining it inline can cause loops.
- Context menus: `onNodeContextMenu`, `onPaneContextMenu` (prevent default, then position your menu using the event's client coordinates).

## Accessing the instance: `useReactFlow`

```tsx
const { getNodes, getEdges, setNodes, setEdges, updateNodeData, addNodes, deleteElements, getIntersectingNodes, toObject } = useReactFlow()
```

`useReactFlow` works in any component **inside** a `ReactFlowProvider` (or inside `<ReactFlow>`'s children). If you need it in a sidebar or toolbar **outside** the canvas, wrap the whole editor:

```tsx
<ReactFlowProvider>
  <Sidebar />          {/* can call useReactFlow() */}
  <Canvas />           {/* contains <ReactFlow> */}
</ReactFlowProvider>
```

Forgetting `ReactFlowProvider` produces errors about missing context. Use `getNodes()`/`getEdges()` for **current values inside event handlers** (they avoid stale closures), and the `nodes` state for rendering.

## Styling basics

React Flow ships CSS variables and class names (`react-flow__node`, `react-flow__edge`, `react-flow__handle`, …). Style nodes through `className`/`style` on the node objects or inside your custom node components, with Tailwind or CSS modules as usual ([styling](../../07-styling/README.md)). `colorMode` switches the library's built-in theme, and its CSS variables can be overridden to match your [design system](../../09-ui-components/09-design-system.md).

## Common mistakes

- **No height on the parent**, so nothing renders.
- **Forgetting to import the stylesheet** (`@xyflow/react/dist/style.css`), so nodes and handles look broken.
- **Duplicate node or edge ids.**
- **Edges referencing nonexistent nodes** (silently not drawn).
- **Mutating `nodes`/`edges` in place** instead of creating new objects.
- **Not applying changes** (`onNodesChange` omitted), so nodes can't be dragged or selected in controlled mode.
- **Using v11 tutorials** (`reactflow`, `parentNode`, `node.width` for measured size) with v12.
- **Persisting positions in `onNodesChange`** on every pixel instead of `onNodeDragStop`.
- **Using `useReactFlow` outside a provider.**
- **Inline unstable callbacks** passed to props like `onSelectionChange`.
- **Reading `node.width` for measured size** in v12 (use `node.measured`).
- **Treating positions as screen pixels**, rather than flow coordinates that need converting.

## Quick summary

- A flow is two arrays: **nodes** (`id`, `position`, `data`, optional `type`) and **edges** (`id`, `source`, `target`, optional handles and `type`).
- Built-in node types: `default`, `input`, `output`, `group`. Built-in edge types: bezier (default), `straight`, `step`, `smoothstep`, `simplebezier`. Custom ones come next ([01](./01-custom-nodes-and-edges.md)).
- In the **controlled** pattern you own the state: `useNodesState`/`useEdgesState` apply change objects, and you handle `onConnect` yourself.
- State is **immutable**: replace objects rather than mutating them.
- The **viewport** (`x`, `y`, `zoom`) is separate from node positions. Convert with `screenToFlowPosition`.
- Add `Background`, `Controls`, `MiniMap`, and `Panel` as children; use `ReactFlowProvider` for hooks outside the canvas.
- Persist on `onNodeDragStop`, not on every change; remember the parent needs a size and the CSS must be imported.

## Next

[01 — Custom nodes and edges](./01-custom-nodes-and-edges.md)