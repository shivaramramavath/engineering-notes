# Recharts — Basic to Advanced

A comprehensive reference card for `recharts` in React 19.

> **Target Version:** Recharts v2.13+  
> **Philosophy:** Declarative, composable, component-driven SVG charting  
> **Ecosystem:** SaaS metrics, analytics dashboards, data telemetry  

---

## Level 0 — Concepts

- **Composable Component Hierarchy**: Charts are constructed by nesting declarative JSX components (`<ResponsiveContainer>` -> `<LineChart>` -> `<XAxis>` -> `<Line>` -> `<Tooltip>`).
- **SVG Vector Rendering**: Renders standard DOM SVG elements. Crisp resolution scaling across Retina displays, easily inspected in browser DevTools.
- **Core Primitives Table**:

| Component | Purpose |
| :--- | :--- |
| `<ResponsiveContainer>` | Automatically resizes charts based on parent container dimensions |
| `<LineChart>`, `<AreaChart>` | Base Cartesian charts for continuous time-series data |
| `<BarChart>` | Categorical bar charts with stacking and grouping |
| `<PieChart>` | Proportional distribution circular and donut charts |
| `<XAxis>`, `<YAxis>` | Configures ticks, labels, domains, and custom formatters |
| `<Tooltip>` | Contextual hover inspection popup with custom component support |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install recharts
```

### 1.2 Basic Area Chart
```tsx
import React from 'react';
import {
  ResponsiveContainer,
  AreaChart,
  Area,
  XAxis,
  YAxis,
  Tooltip,
  CartesianGrid,
} from 'recharts';

const DATA = [
  { month: 'Jan', revenue: 12000 },
  { month: 'Feb', revenue: 19000 },
  { month: 'Mar', revenue: 24000 },
  { month: 'Apr', revenue: 31000 },
];

export function RevenueAreaChart() {
  return (
    <div className="w-full h-64 p-4 border rounded-xl bg-white">
      <h3 className="font-bold text-sm mb-2">Q1 Revenue</h3>
      <ResponsiveContainer width="100%" height="90%">
        <AreaChart data={DATA}>
          <CartesianGrid strokeDasharray="3 3" vertical={false} />
          <XAxis dataKey="month" />
          <YAxis tickFormatter={(val) => `$${val / 1000}k`} />
          <Tooltip />
          <Area type="monotone" dataKey="revenue" stroke="#2563eb" fill="#93c5fd" />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
}
```

---

## Level 2 — Intermediate: Custom Tooltip Component

```tsx
function CustomTooltip({ active, payload, label }: any) {
  if (active && payload && payload.length) {
    return (
      <div className="bg-gray-900 text-white p-2.5 rounded shadow text-xs">
        <p className="font-bold">{label}</p>
        <p className="text-blue-400">Revenue: ${payload[0].value.toLocaleString()}</p>
      </div>
    );
  }
  return null;
}

// In chart: <Tooltip content={<CustomTooltip />} />
```

---

## Level 3 — Advanced: Streaming Telemetry with Disabled Animations

In real-time charts updating several times per second, animations cause frame drops:
```tsx
<Line
  type="monotone"
  dataKey="cpuUsage"
  stroke="#10b981"
  isAnimationActive={false} // CRITICAL for live streaming feeds!
/>
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Mounting `ResponsiveContainer` Inside Zero-Height Flex**| Recharts logs console warnings and renders a blank 0x0 container. | Give parent container an explicit height (`h-64` or `min-h-[250px]`). |
| **Rendering 10,000+ SVG Data Points** | Exhausts browser DOM nodes, causing severe UI lag. | Use HTML5 Canvas or WebGL for datasets exceeding 2,000 points. |
| **Omitting Accessible Data Summaries** | Screen readers cannot parse raw SVG vector paths, failing WCAG compliance. | Provide an accessible data table or descriptive `aria-label` summary. |
| **Inline Color Definitions Across Multiple Series**| Makes updating brand design tokens tedious and inconsistent. | Reference CSS variables or theme tokens in stroke and fill props. |
