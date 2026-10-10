# Drag and Drop

"Drag and drop" in a node editor means several different things, and React Flow handles some of them for you:

| Interaction | Who implements it |
|---|---|
| **Dragging nodes around the canvas** | React Flow (built in) |
| **Panning and zooming** the canvas | React Flow (built in) |
| **Dragging a connection** from a handle | React Flow (built in, [03](./03-connection-validation.md)) |
| **Dragging a new node from a palette onto the canvas** | **You**, using DnD plus React Flow's coordinate conversion |
| **Dragging nodes into groups** | You, using React Flow's parent/child and intersection helpers |

This note covers the last two, and configuring the first.

## Dragging nodes: the built-ins

Node dragging works without any code. Tune it with props:

```tsx
<ReactFlow
  nodesDraggable                          // default true
  snapToGrid snapGrid={[16, 16]}          // align to a grid
  nodeExtent={[[0, 0], [2000, 1200]]}     // keep all nodes inside this world rectangle
  nodeDragThreshold={4}                   // pixels of movement before a drag starts (avoids accidental drags on click)
  autoPanOnNodeDrag                       // pan the viewport when dragging near the edge
  onNodeDragStart={…} onNodeDrag={…} onNodeDragStop={…}
/>
```

Per node:

```ts
{ id: "a", position: { x: 0, y: 0 }, data: {…}, draggable: false }              // pinned
{ id: "b", …, dragHandle: ".drag-handle" }                                        // only an element with this class starts a drag
{ id: "c", …, extent: "parent" }                                                  // can't leave its parent group
```

- **`dragHandle`** is helpful for nodes with lots of interactive content: only the title bar drags the node. (Mark interactive children with `nodrag`, [01](./01-custom-nodes-and-edges.md#interactive-content-inside-nodes).)
- **`onNodeDragStop`** is the place to save positions or take undo snapshots, and `onNodeDrag` fires continuously, so keep it cheap ([node state](./02-node-state.md#performance-notes)).
- Multi-select (Shift-click, or selection box with `selectionOnDrag`) lets users drag many nodes together.

## Dropping onto the canvas

The classic editor UI: a **sidebar palette** of node types the user drags onto the canvas. With the browser's native HTML5 drag-and-drop:

```tsx
// Sidebar.tsx: the draggable palette items
const nodeKinds = [
  { type: "trigger", label: "Trigger" },
  { type: "filter", label: "Filter" },
  { type: "action", label: "Action" },
]

export function Sidebar() {
  function onDragStart(event: React.DragEvent, type: string) {
    event.dataTransfer.setData("application/reactflow", type)     // what are we dragging?
    event.dataTransfer.effectAllowed = "move"
  }

  return (
    <aside className="w-56 space-y-2 border-r p-3">
      {nodeKinds.map((k) => (
        <div
          key={k.type}
          draggable
          onDragStart={(e) => onDragStart(e, k.type)}
          className="cursor-grab rounded border bg-background p-2"
        >
          {k.label}
        </div>
      ))}
    </aside>
  )
}
```

```tsx
// Canvas.tsx: the drop target
function Canvas() {
  const { screenToFlowPosition, setNodes } = useReactFlow()

  const onDragOver = useCallback((event: React.DragEvent) => {
    event.preventDefault()                         // REQUIRED: without it, the browser refuses drops here
    event.dataTransfer.dropEffect = "move"
  }, [])

  const onDrop = useCallback((event: React.DragEvent) => {
    event.preventDefault()
    const type = event.dataTransfer.getData("application/reactflow")
    if (!type) return                              // ignore drags that aren't from our palette

    const position = screenToFlowPosition({ x: event.clientX, y: event.clientY })   // screen px → flow coordinates

    setNodes((nds) => [...nds, { id: crypto.randomUUID(), type, position, data: defaultDataFor(type) }])
  }, [screenToFlowPosition, setNodes])

  return <ReactFlow onDrop={onDrop} onDragOver={onDragOver} … />
}
```

Key points:

- **`onDragOver` must call `preventDefault()`**, or `onDrop` never fires (the browser's default is "not a drop target"). Forgetting this is the most common "my drop does nothing" bug.
- **`screenToFlowPosition`** converts the mouse's `clientX/clientY` into flow coordinates, accounting for the current **pan and zoom**. Using raw client coordinates places nodes in the wrong spot as soon as the user pans or zooms. (In v11 this was `project`, and it required subtracting the canvas's bounding box.)
- The dragged data travels as a **string** through `dataTransfer`. Send a **type key**, and look up the full default node definition on drop. Don't serialize whole nodes through it.
- The returned position is the node's **top-left**. To center the node under the cursor, subtract half its expected size:

```ts
const position = screenToFlowPosition({ x: event.clientX - 80, y: event.clientY - 24 })   // roughly centered for a 160×48 node
```

- The sidebar must be **inside `ReactFlowProvider`** if it calls hooks like `useReactFlow` ([00](./00-nodes-and-edges.md#accessing-the-instance-usereactflow)).
- **Validate** what you drop: check the type is one you support, and enforce rules like "only one trigger node" before adding ([03](./03-connection-validation.md#validating-the-whole-graph)).

### Giving feedback

- Style the canvas on `onDragEnter`/`onDragLeave` (a highlighted border while a palette item is over it).
- Customize the **drag image** with `event.dataTransfer.setDragImage(el, x, y)` for a nicer preview.
- Disable palette items that can't be added (an already-present singleton).

## Touch and the limits of HTML5 DnD

The native HTML5 drag-and-drop API has **poor or no support on touch devices**: dragging from the palette simply won't work on many phones and tablets. Options:

- **Click or tap to add**: tapping a palette item adds the node at the viewport center (or the next free spot). It's simple, works everywhere, and doubles as the keyboard/screen reader alternative.

```tsx
function addNodeAtCenter(type: string) {
  const { x, y, zoom } = getViewport()
  const bounds = canvasRef.current!.getBoundingClientRect()
  const position = screenToFlowPosition({ x: bounds.left + bounds.width / 2, y: bounds.top + bounds.height / 2 })
  setNodes((nds) => [...nds, { id: crypto.randomUUID(), type, position, data: defaultDataFor(type) }])
}
```

- **Pointer-event–based dragging** (your own `pointerdown/move/up` handlers that render a "ghost" element and call `screenToFlowPosition` on release), or a library built on pointer events such as **dnd-kit**, which supports touch, keyboard, and custom drag overlays. Libraries add a dependency but also solve auto-scroll, collision detection, and accessibility concerns.
- Don't make drag the **only** way to add nodes on a product that must work on touch.

## Grouping and sub-flows

React Flow supports **nested nodes**: a node can belong to a parent, and moves with it. Use it for containers (a "loop" or "section" box) and visual grouping.

```ts
const nodes: Node[] = [
  { id: "group-1", type: "group", position: { x: 0, y: 0 }, style: { width: 320, height: 200 }, data: {} },
  { id: "child-1", parentId: "group-1", extent: "parent", position: { x: 20, y: 40 }, data: { label: "Inside" } },
]
```

- **`parentId`** (v12; `parentNode` in v11) links a child to its parent. The child's **`position` is relative to the parent**.
- **`extent: "parent"`** keeps the child inside the parent's bounds. Omit it to allow dragging out.
- **Ordering matters**: **parents must appear before their children** in the `nodes` array, or rendering and positioning break. Keep this invariant when adding nodes dynamically (insert a new parent before its children, or sort on load).
- Moving the parent moves its children. **Deleting a parent** should also delete or re-parent its children. Decide the policy, and implement it in `onNodesDelete`/`onBeforeDelete`.
- The `group` type is a plain container. Make a custom group node type for labels, collapse buttons, and resizing (`NodeResizer`).

### Dragging a node into (or out of) a group

React Flow doesn't auto-reparent. You detect where a node was dropped, and set its `parentId`, adjusting the position because child positions are parent-relative:

```tsx
const { getIntersectingNodes, getInternalNode } = useReactFlow()

const onNodeDragStop = useCallback((_: React.MouseEvent, node: Node) => {
  const group = getIntersectingNodes(node).find((n) => n.type === "group")      // which group is under the dragged node?

  setNodes((nds) =>
    nds.map((n) => {
      if (n.id !== node.id) return n
      if (group && n.parentId !== group.id) {
        // convert the node's absolute position into the group's coordinate space
        const groupPos = getInternalNode(group.id)!.internals.positionAbsolute
        const nodePos = getInternalNode(node.id)!.internals.positionAbsolute
        return { ...n, parentId: group.id, position: { x: nodePos.x - groupPos.x, y: nodePos.y - groupPos.y } }
      }
      if (!group && n.parentId) {
        // dropped outside any group: detach and convert back to absolute coordinates
        const abs = getInternalNode(node.id)!.internals.positionAbsolute
        return { ...n, parentId: undefined, position: abs }
      }
      return n
    })
  )
}, [getIntersectingNodes, getInternalNode, setNodes])
```

Details to confirm against the docs for your version: `getIntersectingNodes` (nodes overlapping a given node or rect), and the internal-node accessors for absolute positions (`positionAbsolute` lives under `internals` in v12). Remember to keep **parents before children** in the array after reparenting, since you may need to reorder.

Highlight the target group during the drag (`onNodeDrag` + intersection check, writing a `highlight` flag to a store or a ref-driven class) so users see where it will land.

## Deleting by drag

A "drop on the trash zone to delete" affordance is hit-testing on `onNodeDragStop`:

```tsx
const onNodeDragStop = (event: React.MouseEvent, node: Node) => {
  const trash = trashRef.current?.getBoundingClientRect()
  if (trash && event.clientX >= trash.left && event.clientX <= trash.right && event.clientY >= trash.top && event.clientY <= trash.bottom) {
    deleteElements({ nodes: [{ id: node.id }] })
  }
}
```

Always provide the usual **delete key**, a button, and a context menu too. Drag-to-delete is a convenience, not the only route.

## Copy, paste, and duplicate

Not drag-and-drop strictly, but expected alongside it:

```ts
function duplicateSelection() {
  const selected = getNodes().filter((n) => n.selected)
  const idMap = new Map(selected.map((n) => [n.id, crypto.randomUUID()]))
  const copies = selected.map((n) => ({ ...n, id: idMap.get(n.id)!, selected: true, position: { x: n.position.x + 24, y: n.position.y + 24 } }))
  const internalEdges = getEdges()
    .filter((e) => idMap.has(e.source) && idMap.has(e.target))                       // only edges fully inside the selection
    .map((e) => ({ ...e, id: crypto.randomUUID(), source: idMap.get(e.source)!, target: idMap.get(e.target)! }))
  setNodes((nds) => [...nds.map((n) => ({ ...n, selected: false })), ...copies])
  setEdges((eds) => [...eds, ...internalEdges])
}
```

Remap **ids** (nodes and edges), offset **positions**, copy only **edges within the selection**, and remember to preserve **parent-before-child order** and remap `parentId`. Hook it to Ctrl/Cmd+D (and C/V with clipboard support), and record **one undo step** for the whole operation.

## Accessibility and touch

A node canvas is a pointer-heavy interface, so deliberate alternatives are necessary. Drag-and-drop requirements include a single-pointer alternative to dragging ([gestures accessibility](../animation/03-gestures.md#accessibility-of-gestures), [accessibility checklist](../../08-accessibility/04-accessibility-checklist.md)):

- **Keyboard add**: palette items as real `<button>`s that add the node on Enter/Space (the tap-to-add pattern above), with the new node **focused** afterward and announced ("Filter node added").
- **Keyboard move and select**: nodes are focusable, and arrow keys move a focused node (React Flow's built-in keyboard support; don't disable it with `disableKeyboardA11y` unless you replace it). Check the docs for the exact key bindings.
- **Keyboard connect**: provide an **inspector** where a selected node's inputs and outputs can be chosen from a list ("Connect output to… [dropdown]"), since dragging a connection line with a pointer isn't keyboard-operable.
- **Announce changes** in an `aria-live` region: node added/moved/deleted, connection made or refused (with the reason).
- **Visible focus** on nodes, handles, and edges; **don't rely on color** for validity or selection.
- **Touch targets**: handles are small by default. Enlarge them (CSS) and use `connectionRadius` for a more forgiving snap area on touch screens.
- **Alternative representation**: a tree or list view of the graph for screen reader users and for complex graphs.
- Respect **reduced motion** for animated edges and viewport transitions ([animation accessibility](../animation/04-animation-performance-and-accessibility.md#accessibility)).

## Testing

- **Logic** (position conversion, reparenting math, duplication, "only one trigger", validation) as pure functions with unit tests.
- **HTML5 DnD** doesn't run in jsdom. Test the **drop handler's logic** by calling it with a fake event (a stubbed `dataTransfer.getData` and `clientX/clientY`) and mocking `screenToFlowPosition`, and cover the real drag in **Playwright** ([E2E](../../18-testing-and-debugging/06-e2e-testing-playwright.md)). Its `dragTo()` often handles HTML5 DnD, otherwise use `page.mouse` steps with intermediate moves.
- Rendering React Flow in jsdom needs `ResizeObserver` and `DOMMatrixReadOnly` stubs, and has no layout, so measurements are zero. Prefer real-browser tests for canvas interactions.
- Test the **non-drag alternatives** (tap to add, inspector connections) in component tests. They're also your accessible paths.

## Performance

- Keep `onNodeDrag` handlers light. Do intersection checks and highlighting **throttled** or only when needed.
- Use `onNodeDragStop` for saves and snapshots.
- `getIntersectingNodes` is O(n). For big graphs, throttle it or restrict candidates to group nodes.
- Many animated edges, shadows, and large node content hurt drag smoothness. Simplify during drags if needed (a "dragging" class that disables expensive effects).

## Common mistakes

- **Forgetting `preventDefault()` in `onDragOver`**, so `onDrop` never fires.
- **Using raw `clientX/clientY`** as node position, ignoring pan and zoom. Use `screenToFlowPosition`.
- **Palette drag as the only way to add nodes**, which breaks on touch and for keyboard users.
- **Parents after children** in the `nodes` array (broken group rendering).
- **Forgetting that child positions are parent-relative** when reparenting.
- **Mutating `nodes`** when adding or reparenting. Use new objects.
- **Not validating drops** (duplicate singletons, unknown types).
- **Saving on every `onNodeDrag` tick** instead of `onNodeDragStop`.
- **Heavy work in `onNodeDrag`** (intersection checks on every frame for large graphs).
- **Drag handles missing**, so form controls inside nodes drag the node (`nodrag`/`dragHandle`).
- **Using `reactflow` v11 names** (`project`, `parentNode`) in v12.
- **No keyboard or screen-reader path** for creating, moving, and connecting nodes.
- **Testing DnD in jsdom** and trusting the result.

## Quick summary

- Node dragging, panning, zooming, and connection dragging are **built in**; you implement **palette → canvas drops** and **grouping**.
- For palette drops: `draggable` items set a **type key** in `dataTransfer`; the canvas needs **`onDragOver` with `preventDefault()`** and an `onDrop` that converts `clientX/clientY` with **`screenToFlowPosition`** and adds a node immutably with a unique id.
- HTML5 DnD doesn't work well on touch, so offer **tap-to-add** or a pointer-event/dnd-kit approach, which also serves keyboard users.
- **Groups**: `parentId` + `extent: "parent"`; parents must precede children; child positions are **relative**; reparent on `onNodeDragStop` using intersection helpers and absolute-position conversion.
- Tune dragging with `snapToGrid`, `nodeExtent`, `dragHandle`, `nodeDragThreshold`, and `autoPanOnNodeDrag`; save on **`onNodeDragStop`**.
- Provide **accessible alternatives** (keyboard add/move, inspector-based connecting, live announcements), and test logic in unit tests with real drags in a browser.

## Next

Continue to [22 — Projects](../../22-projects/README.md).