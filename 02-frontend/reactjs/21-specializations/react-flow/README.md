# React Flow

**React Flow** (`@xyflow/react`) is a library for building **node-based interfaces**: canvases where users place boxes ("nodes"), connect them with lines ("edges"), and pan and zoom around. If you've used a workflow automation tool, a visual programming environment, a data-pipeline editor, a diagram tool, an org chart, or a mind-mapping app, you've used this kind of UI.

Building one from scratch means solving: infinite pan/zoom canvas, hit-testing, dragging with snapping, selection boxes, connection dragging with preview lines, edge routing, minimaps, keyboard shortcuts, accessibility, and performance with hundreds of elements. React Flow does all of that, and **leaves the node contents, the data model, and the rules to you**.

```text
        ┌────────────┐          ┌────────────┐          ┌────────────┐
        │  Trigger   │─────────►│  Filter    │─────────►│  Send email │
        │  webhook   │          │  amount>10 │──┐       └────────────┘
        └────────────┘          └────────────┘  │       ┌────────────┐
                                                └──────►│  Log       │
                                                        └────────────┘
          nodes = React components you design        edges = connections between handles
```

## The mental model

A flow is **just data**: two arrays, rendered by React Flow.

```ts
const nodes = [{ id: "a", position: { x: 0, y: 0 }, data: { label: "A" } }, …]
const edges = [{ id: "a-b", source: "a", target: "b" }, …]
```

- **You own the state.** In the standard "controlled" setup, React Flow tells you what changed (a node was dragged, an edge was connected, something was deleted) through callbacks, and **you** apply the changes to your state.
- **Nodes are React components.** A custom node is a normal component (forms, charts, previews, anything) with **handles** where edges attach.
- **Edges are SVG paths** (and can be custom components too).
- The **viewport** (pan and zoom) is separate from node positions. Positions live in "flow coordinates," screen pixels in a different space.

That separation is the most important design idea: the graph is a **serializable data structure**, which makes saving, loading, undo/redo, validating, and executing it straightforward ([node state](./02-node-state.md)).

## Version note

This folder covers **React Flow v12** (package **`@xyflow/react`**). Older tutorials use **`reactflow`** (v11) with several differences:

| v11 (`reactflow`) | v12 (`@xyflow/react`) |
|---|---|
| `import ReactFlow from "reactflow"` | `import { ReactFlow } from "@xyflow/react"` (named export) |
| `import "reactflow/dist/style.css"` | `import "@xyflow/react/dist/style.css"` |
| `node.width` / `node.height` | **`node.measured.width` / `.height`** (measured by the library); `width`/`height` on a node now *set* its size |
| `parentNode` | **`parentId`** |
| `onEdgeUpdate` | **`onReconnect`** |
| `project()` | **`screenToFlowPosition()`** |
| `useHandleConnections` | `useNodeConnections` (later v12 releases) |

Check the [migration guide and docs](https://reactflow.dev) for your version. Package names, hook names, and option names have been changing.

## Setup

```bash
npm install @xyflow/react
```

```tsx
import { ReactFlow, Background, Controls, MiniMap, useNodesState, useEdgesState, addEdge, type Node, type Edge, type OnConnect } from "@xyflow/react"
import "@xyflow/react/dist/style.css"          // required: the library's base styles

const initialNodes: Node[] = [
  { id: "1", type: "input", position: { x: 0, y: 0 }, data: { label: "Start" } },
  { id: "2", position: { x: 200, y: 100 }, data: { label: "Next step" } },
]
const initialEdges: Edge[] = [{ id: "e1-2", source: "1", target: "2" }]

export function Flow() {
  const [nodes, setNodes, onNodesChange] = useNodesState(initialNodes)
  const [edges, setEdges, onEdgesChange] = useEdgesState(initialEdges)
  const onConnect: OnConnect = (connection) => setEdges((eds) => addEdge(connection, eds))

  return (
    <div style={{ width: "100%", height: "600px" }}>            {/* the canvas needs an explicit size */}
      <ReactFlow
        nodes={nodes}
        edges={edges}
        onNodesChange={onNodesChange}
        onEdgesChange={onEdgesChange}
        onConnect={onConnect}
        fitView
      >
        <Background />
        <Controls />
        <MiniMap />
      </ReactFlow>
    </div>
  )
}
```

**The #1 beginner problem:** a blank canvas. `ReactFlow` fills its **parent**, so the parent must have a **non-zero width and height**. A `div` with no height renders nothing. Give it `height: 100vh` or a fixed height, or make sure a flex/grid parent gives it size.

## Licensing and attribution

React Flow is open source (MIT), and displays a small attribution link on the canvas by default. There are options to hide it, along with paid subscription tiers that support the project. Check the current license terms and attribution policy before shipping commercially.

## Where it fits in this repo

- The [workflow builder project](../../22-projects/README.md) applies everything here: custom nodes, validation, drag-and-drop, persistence, and execution.
- It leans on [external stores](../../16-advanced-react/02-external-stores.md) (React Flow uses a store internally and you can use [Zustand](../../13-state-management/03-zustand.md) for yours) and [refs](../../16-advanced-react/01-refs-and-imperative-handles.md).
- [Performance](../../14-performance/README.md) matters: large graphs re-render quickly if you aren't careful.

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [Nodes and edges](./00-nodes-and-edges.md) | The data model, built-in types, viewport, controls, events |
| 01 | [Custom nodes and edges](./01-custom-nodes-and-edges.md) | Your own node components, handles, custom edges, `nodrag` |
| 02 | [Node state](./02-node-state.md) | Updating data immutably, stores, sharing data between nodes, persistence |
| 03 | [Connection validation](./03-connection-validation.md) | Rules for what can connect, cycle detection, whole-graph validation |
| 04 | [Drag and drop](./04-drag-and-drop.md) | Dragging from a palette, positions, grouping, touch and keyboard alternatives |

## Suggested order

00 → 01 → 02 for the core, then 03 and 04 for the interactions that make an editor useful.

## When *not* to use it

- You need a **chart** (use a charting library).
- You need a **freeform drawing/whiteboard** with shapes, ink, and text (a canvas/whiteboard library fits better).
- You have a **tiny static diagram** (SVG or a Mermaid diagram is simpler).
- You need **huge graphs** (thousands of nodes with heavy interactivity), where DOM-based rendering strains. Consider canvas/WebGL graph libraries.

## Next

[00 — Nodes and edges](./00-nodes-and-edges.md)