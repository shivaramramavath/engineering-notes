# Custom Nodes and Edges

The built-in nodes are labelled boxes. Real editors need nodes that contain **forms, previews, dropdowns, status indicators, and multiple connection points**. A **custom node** is just a React component. React Flow handles positioning, dragging, and selection, and you render whatever goes inside.

## A custom node

```tsx
import { Handle, Position, type Node, type NodeProps } from "@xyflow/react"

type FilterNode = Node<{ field: string; operator: string; value: string }, "filter">

export function FilterNodeView({ id, data, selected }: NodeProps<FilterNode>) {
  const { updateNodeData } = useReactFlow()

  return (
    <div className={cn("w-56 rounded-lg border bg-background p-3 shadow-sm", selected && "ring-2 ring-primary")}>
      <Handle type="target" position={Position.Left} />          {/* where edges arrive */}

      <p className="mb-2 text-sm font-medium">Filter</p>
      <input
        className="nodrag w-full rounded border px-2 py-1 text-sm"
        value={data.value}
        onChange={(e) => updateNodeData(id, { value: e.target.value })}
        aria-label="Filter value"
      />

      <Handle type="source" position={Position.Right} />         {/* where edges leave */}
    </div>
  )
}
```

Register it by `type`:

```tsx
const nodeTypes = { filter: FilterNodeView, action: ActionNodeView }     // defined OUTSIDE the component

<ReactFlow nodeTypes={nodeTypes} nodes={nodes} … />

// a node of this kind:
{ id: "n1", type: "filter", position: { x: 0, y: 0 }, data: { field: "amount", operator: ">", value: "10" } }
```

**Define `nodeTypes` (and `edgeTypes`) outside the component, or memoize them.** If you create the object inside your component body, it's a **new object every render**. React Flow warns about it, and nodes may remount repeatedly (losing focus and state), the same trap as [defining components inside components](../../17-react-internals/01-reconciliation.md#component-identity-the-inner-component-trap).

### What a node component receives

`NodeProps` includes (among others): `id`, `data`, `type`, `selected`, `dragging`, `isConnectable`, `positionAbsoluteX/Y`, and `width`/`height`. Use `selected` and `dragging` for styling. Use `id` to update this node's data.

### Typing

Pass the **node type** to `NodeProps` so `data` is typed:

```ts
type TextNode = Node<{ text: string }, "text">
function TextNodeView({ data }: NodeProps<TextNode>) { return <p>{data.text}</p> }     // data.text is a string
```

For a whole app, define `type AppNode = TextNode | FilterNode | ActionNode`, and use `useReactFlow<AppNode, AppEdge>()` and `useNodesState<AppNode>` so the entire flow is typed. In v12 you pass the **full node type**, not just the data type, so older v11 snippets with `NodeProps<Data>` need updating.

## Handles: where connections attach

```tsx
<Handle type="target" position={Position.Left} id="in" />
<Handle type="source" position={Position.Right} id="out" />
```

- **`type`**: `"source"` (edges start here) or `"target"` (edges end here).
- **`position`**: `Position.Top | Right | Bottom | Left`, which side the handle sits on.
- **`id`**: **required when a node has more than one handle of the same type**, so edges can say which one they use (`sourceHandle` / `targetHandle`). One handle per type can omit it.
- **`isConnectable`**, `isConnectableStart`, `isConnectableEnd`: restrict what a handle may do ([03](./03-connection-validation.md#handle-level-rules)).
- Style handles with `className`/`style`, since a handle is an element positioned on the node border.

### Multiple outputs: branching

```tsx
function ConditionNodeView() {
  return (
    <div className="relative rounded-lg border bg-background p-3">
      <Handle type="target" position={Position.Left} />
      <p>If amount &gt; 100</p>
      <Handle type="source" position={Position.Right} id="true"  style={{ top: "30%" }} />
      <Handle type="source" position={Position.Right} id="false" style={{ top: "70%" }} />
    </div>
  )
}
```

Edges leaving this node set `sourceHandle: "true"` or `"false"`, and your execution logic follows those labels.

### Dynamic handles

If handles appear, disappear, or move after the node first renders (a node with a user-defined list of outputs), React Flow needs to re-measure them:

```tsx
const updateNodeInternals = useUpdateNodeInternals()
useEffect(() => { updateNodeInternals(id) }, [id, outputs.length, updateNodeInternals])
```

Without this, edges connect to stale handle positions.

## Interactive content inside nodes

Dragging a node and interacting with its contents compete: pressing on an input would start dragging the node. React Flow uses helper **class names**:

| Class | Effect |
|---|---|
| **`nodrag`** | Pointer events on this element don't drag the node (inputs, buttons, selects, sliders) |
| **`nopan`** | Don't pan the canvas when dragging here |
| **`nowheel`** | Mouse wheel doesn't zoom the canvas here (scrollable text areas, dropdown lists) |

```tsx
<textarea className="nodrag nowheel" … />
<button className="nodrag" onClick={…}>Run</button>
```

Missing `nodrag` is the classic symptom of "I can't select text / use my slider; the node moves instead." Alternatively, restrict dragging to a handle area with `dragHandle: ".drag-handle"` on the node (then only that element drags it).

## More node tools

```tsx
import { NodeToolbar, NodeResizer } from "@xyflow/react"

<NodeToolbar isVisible={selected} position={Position.Top}>   {/* floating toolbar above the node, not scaled by zoom */}
  <button onClick={() => deleteElements({ nodes: [{ id }] })}>Delete</button>
</NodeToolbar>

<NodeResizer isVisible={selected} minWidth={120} minHeight={60} />
```

- **`NodeToolbar`** renders contextual actions without crowding the node's content.
- **`NodeResizer`** adds resize handles (and updates the node's `width`/`height` through the change callbacks).

## Custom edges

An edge component receives the geometry and draws a path:

```tsx
import { BaseEdge, EdgeLabelRenderer, getBezierPath, useReactFlow, type EdgeProps } from "@xyflow/react"

export function DeletableEdge(props: EdgeProps) {
  const { id, sourceX, sourceY, targetX, targetY, sourcePosition, targetPosition, markerEnd, style, data } = props
  const { deleteElements } = useReactFlow()

  const [path, labelX, labelY] = getBezierPath({ sourceX, sourceY, sourcePosition, targetX, targetY, targetPosition })

  return (
    <>
      <BaseEdge id={id} path={path} markerEnd={markerEnd} style={style} />
      <EdgeLabelRenderer>
        <button
          className="nodrag nopan absolute rounded-full border bg-background px-1.5 text-xs"
          style={{ transform: `translate(-50%, -50%) translate(${labelX}px, ${labelY}px)`, pointerEvents: "all" }}
          onClick={() => deleteElements({ edges: [{ id }] })}
          aria-label="Delete connection"
        >
          ×
        </button>
      </EdgeLabelRenderer>
    </>
  )
}

const edgeTypes = { deletable: DeletableEdge }
<ReactFlow edgeTypes={edgeTypes} defaultEdgeOptions={{ type: "deletable" }} … />
```

The pieces:

- **Geometry props** (`sourceX/Y`, `targetX/Y`, `sourcePosition`, `targetPosition`) are computed for you from the connected handles.
- **Path helpers**: `getBezierPath`, `getSmoothStepPath`, `getStraightPath`, each returning `[path, labelX, labelY]` (the midpoint for labels).
- **`BaseEdge`** draws the SVG path (plus an invisible wider "interaction" path for easier clicking: `interactionWidth`).
- **`EdgeLabelRenderer`** renders **HTML** (buttons, text, badges) on top of the SVG layer, positioned with a transform. HTML labels need **`pointerEvents: "all"`** and the **`nodrag nopan`** classes to be clickable.
- **`data`** on edges works like on nodes (conditions, colors, metadata), so custom edges can render it: a "priority" badge, or a styled dashed path for the "false" branch.

```tsx
const color = data?.branch === "false" ? "#ef4444" : "#22c55e"
<BaseEdge … style={{ ...style, stroke: color }} />
```

### Custom connection line

While dragging a new connection, React Flow draws a preview line. Customize with `connectionLineStyle` or a `connectionLineComponent` (for example, coloring the preview red when the target would be invalid, see [03](./03-connection-validation.md#feedback-while-connecting)).

## Performance of custom nodes

Every node is a React component on a canvas that **re-renders often** (dragging fires many updates):

- **`memo` your node components**, or rely on the [React Compiler](../../15-concurrent-and-modern-react/06-react-compiler.md), so moving one node doesn't re-render every node.
- **Keep node `data` small and stable.** Heavy previews or big objects in `data` get re-compared and re-rendered.
- **Don't call `useNodes()`/`useEdges()` inside a node** just to read one thing: they subscribe to *every* change. Use targeted hooks (`useNodesData(id)`, or a `useStore` selector) ([02](./02-node-state.md#reading-state-efficiently-inside-nodes)).
- **Avoid inline `style`/function props** that change identity per render on handles and toolbars, when it causes re-rendering of memoized children.
- Heavy nodes (charts, editors, iframes) multiplied by dozens get expensive. Render lightweight summaries on the canvas and open details in a side panel or modal ([layout and layering](../../20-frontend-architecture/01-layered-architecture.md)).
- For big graphs, `onlyRenderVisibleElements` skips nodes outside the viewport.

## Accessibility

A node canvas is **inherently pointer-oriented**, so accessibility takes deliberate work:

- Nodes are **focusable** by default, and focused nodes can be moved with the arrow keys, selected with Enter/Space, and so on (unless `disableKeyboardA11y` is set). Check the current docs for exact behavior and any ARIA options such as `ariaLabel` / `ariaRole` on nodes and edges.
- Use **real controls inside nodes** (`<button>`, labelled `<input>`), not clickable `div`s ([semantic HTML](../../08-accessibility/00-semantic-html.md)).
- Give interactive elements **accessible names** (`aria-label`), and make sure **focus is visible** on nodes and handles.
- Provide **non-pointer ways** to create and connect nodes: "Add node" buttons or menus, and an **inspector panel** where users can pick a node's connections from a list ([04](./04-drag-and-drop.md#accessibility-and-touch)).
- Don't rely on **color alone** for edge or node status (add icons or text).
- Consider a **list or table view** of the graph (nodes and their connections) as an equivalent representation for screen reader users.

## Common mistakes

- **`nodeTypes`/`edgeTypes` created inside the component**, causing warnings and remounts.
- **Forgetting `nodrag`** on inputs, buttons, and sliders, so the node moves instead.
- **Forgetting `nowheel`** on scrollable content inside nodes, so scrolling zooms the canvas.
- **Multiple handles with no `id`**, so edges attach ambiguously or to the wrong one.
- **Dynamic handles without `useUpdateNodeInternals`**, so edges point to stale positions.
- **HTML edge labels without `pointerEvents: "all"`**, so they can't be clicked.
- **Typing with v11 patterns** (`NodeProps<Data>`) in v12.
- **Storing local component state inside nodes** that must survive saving and loading (put persistent values in `data`).
- **Heavy content in every node**, and un-memoized nodes re-rendering on each drag.
- **Reading all nodes from inside a node** (`useNodes()`), subscribing it to every change.
- **Pointer-only editing** with no keyboard or alternative UI.

## Quick summary

- A **custom node** is a React component registered under a `type` in a **stable, module-level `nodeTypes`** object; it receives `id`, `data`, `selected`, and more.
- **`<Handle type position id>`** defines connection points. Use ids when there are several of a type; call `useUpdateNodeInternals` when handles change dynamically.
- Add **`nodrag`**, **`nopan`**, and **`nowheel`** classes to interactive content so it doesn't fight the canvas gestures; use `NodeToolbar` and `NodeResizer` for common extras.
- A **custom edge** gets geometry props; build the path with `getBezierPath` and friends, render with `BaseEdge`, and use **`EdgeLabelRenderer`** for clickable HTML labels.
- Type nodes with the full `Node<Data, Type>` and a union for the app.
- Keep nodes **cheap**: `memo`, small `data`, targeted hooks, `onlyRenderVisibleElements` for big graphs.
- Provide **keyboard and non-pointer alternatives** for canvas interactions.

## Next

[02 — Node state](./02-node-state.md)