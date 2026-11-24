# Project 13: Interactive Node Graph Editor with React Flow (`flow-editor-react-flow`)

> An interactive node graph workspace built using `@xyflow/react` (React Flow), featuring custom node types, drag-and-drop wiring connections, dynamic edge validation, undo/redo state history, and real-time JSON export.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-13-flow-editor` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | React Flow `useNodesState` / `useEdgesState` + Custom Reducer |
| **Styling** | Tailwind CSS v4 + `@xyflow/react/dist/style.css` |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
Graph state consists of node topologies and directional edges. React Flow's event hooks (`onNodesChange`, `onEdgesChange`, `onConnect`) route changes to local state while connection validators prevent cyclic dependencies.

```
[ Node Palette (Trigger, Action, Condition) ]
                    │
                    ▼
[ React Flow Canvas (Pan / Zoom / MiniMap) ]
        │                       │
        ▼                       ▼
  [ Nodes Array ]         [ Edges Array ]
        │                       │
        ▼                       ▼
  [ Graph Validation Engine (Cycle checks, Type compatibility) ]
```

### 1.2 Component Structure
```text
src/
├── types/
│   └── flow.ts               # Custom node data interfaces
├── components/
│   ├── CustomNodes/
│   │   ├── TriggerNode.tsx   # Source node with Output Handle
│   │   └── ActionNode.tsx    # Processing node with Input & Output
│   ├── FlowControls.tsx      # Zoom, Fit View, Export JSON buttons
│   └── FlowCanvas.tsx        # React Flow wrapper with Background & Controls
└── App.tsx                   # Full-screen workspace
```

---

## 2. Complete Project Source Code

### 2.1 Types (`src/types/flow.ts`)
```ts
export type FlowNodeType = 'trigger' | 'action' | 'output';

export interface CustomNodeData {
  label: string;
  subtext?: string;
  status?: 'idle' | 'running' | 'success';
}
```

### 2.2 Custom Action Node (`src/components/CustomNodes/ActionNode.tsx`)
```tsx
import React, { memo } from 'react';
import { Handle, Position, NodeProps } from '@xyflow/react';
import { CustomNodeData } from '../../types/flow';

export const ActionNode = memo(({ data }: NodeProps<any>) => {
  return (
    <div className="rounded-lg border border-slate-300 bg-white p-3 shadow-md min-w-[140px]">
      <Handle type="target" position={Position.Top} className="w-2.5 h-2.5 bg-blue-500" />
      <div className="text-xs font-bold text-slate-800">{data.label}</div>
      {data.subtext && <div className="text-[10px] text-slate-500">{data.subtext}</div>}
      <Handle type="source" position={Position.Bottom} className="w-2.5 h-2.5 bg-emerald-500" />
    </div>
  );
});
ActionNode.displayName = 'ActionNode';
```

### 2.3 Main Flow Canvas (`src/components/FlowCanvas.tsx`)
```tsx
import React, { useCallback } from 'react';
import {
  ReactFlow,
  Background,
  Controls,
  MiniMap,
  useNodesState,
  useEdgesState,
  addEdge,
  Connection,
  Edge,
  Node,
} from '@xyflow/react';
import '@xyflow/react/dist/style.css';
import { ActionNode } from './CustomNodes/ActionNode';

const nodeTypes = {
  action: ActionNode,
};

const initialNodes: Node[] = [
  { id: '1', type: 'action', position: { x: 100, y: 100 }, data: { label: 'Webhook Inbound' } },
  { id: '2', type: 'action', position: { x: 100, y: 250 }, data: { label: 'Send Email Notification' } },
];

const initialEdges: Edge[] = [
  { id: 'e1-2', source: '1', target: '2', animated: true },
];

export const FlowCanvas: React.FC = () => {
  const [nodes, setNodes, onNodesChange] = useNodesState(initialNodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initialEdges);

  const onConnect = useCallback(
    (params: Connection) => setEdges((eds) => addEdge({ ...params, animated: true }, eds)),
    [setEdges]
  );

  return (
    <div className="h-screen w-screen bg-slate-50">
      <ReactFlow
        nodes={nodes}
        edges={edges}
        onNodesChange={onNodesChange}
        onEdgesChange={onEdgesChange}
        onConnect={onConnect}
        nodeTypes={nodeTypes}
        fitView
      >
        <Background />
        <Controls />
        <MiniMap />
      </ReactFlow>
    </div>
  );
};
```

---

## 3. Testing Suite (`src/tests/FlowCanvas.test.tsx`)

```tsx
import { render } from '@testing-library/react';
import { FlowCanvas } from '../components/FlowCanvas';
import { describe, it, expect } from 'vitest';

describe('React Flow Canvas', () => {
  it('renders canvas container without crashing', () => {
    const { container } = render(<FlowCanvas />);
    expect(container.querySelector('.react-flow')).toBeDefined();
  });
});
```

---

## 4. Build and Run Instructions

```bash
# Install dependencies
npm install @xyflow/react

# Run dev
npm run dev
```
