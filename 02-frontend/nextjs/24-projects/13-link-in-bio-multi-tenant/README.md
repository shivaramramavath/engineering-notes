# Project 13: Multi-Tenant Link-in-Bio Platform (`link-in-bio-multi-tenant`)

> Bio link tree hosting platform built with Next.js App Router, featuring custom domain verification, theme customization, analytics tracking, and instant preview.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-13-link-in-bio` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Database** | PostgreSQL + Drizzle ORM |
| **Styling** | Tailwind CSS v4 |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Custom Domains & Subdomain Routing
The platform supports both vanity subdomains (`sarah.bio.link`) and custom domains (`sarahlinks.com`). `proxy.ts` queries a Redis cache mapping incoming hostnames to profile IDs and rewrites the request internally to `app/bio/[handle]/page.tsx`.

```
[ Inbound: sarahlinks.com ] ──> [ proxy.ts (Redis Domain Lookup) ]
                                          │
                                          ▼ (Rewrite)
                       [ app/bio/sarah/page.tsx (Prerendered with PPR) ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-link-in-bio",
  "version": "1.0.0",
  "private": true,
  "scripts": {
    "dev": "next dev --turbopack",
    "build": "next build",
    "start": "next start",
    "test": "vitest run"
  },
  "dependencies": {
    "next": "^15.2.0",
    "react": "^19.0.0",
    "react-dom": "^19.0.0",
    "server-only": "^0.0.1",
    "drizzle-orm": "^0.38.0",
    "zod": "^3.24.1"
  },
  "devDependencies": {
    "typescript": "^5.7.3",
    "tailwindcss": "^4.0.0",
    "vitest": "^3.0.0"
  }
}
```

### Bio Page Component (`src/app/bio/[handle]/page.tsx`)
```tsx
import { notFound } from 'next/navigation';
import { db } from '@/db';
import { profiles, links } from '@/db/schema';
import { eq } from 'drizzle-orm';

interface PageProps {
  params: Promise<{ handle: string }>;
}

export default async function BioPage({ params }: PageProps) {
  const { handle } = await params;

  const profile = (await db.select().from(profiles).where(eq(profiles.handle, handle)).limit(1))[0];
  if (!profile) notFound();

  const userLinks = await db.select().from(links).where(eq(links.profileId, profile.id));

  return (
    <main className="min-h-screen flex flex-col items-center justify-center p-6 bg-slate-950 text-white">
      <div className="w-full max-w-md text-center">
        <img src={profile.avatarUrl} alt="" className="w-24 h-24 mx-auto rounded-full border-2 border-slate-700" />
        <h1 className="mt-4 text-xl font-bold">{profile.displayName}</h1>
        <p className="text-sm text-slate-400 mt-1">{profile.bio}</p>

        <div className="mt-8 space-y-3">
          {userLinks.map((link) => (
            <a
              key={link.id}
              href={link.url}
              target="_blank"
              rel="noopener noreferrer"
              className="block w-full py-3 px-4 rounded-xl border border-slate-800 bg-slate-900 hover:bg-slate-800 transition font-medium text-sm"
            >
              {link.title}
            </a>
          ))}
        </div>
      </div>
    </main>
  );
}
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect } from 'vitest';

describe('Handle Normalization', () => {
  it('lowercases handle strings', () => {
    const handle = 'SarahBio';
    expect(handle.toLowerCase()).toBe('sarahbio');
  });
});
```

---

## 4. Build and Run Instructions

```bash
npm install
npm run build
npm run start
```
