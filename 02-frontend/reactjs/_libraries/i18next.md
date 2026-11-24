# react-i18next — Basic to Advanced

A comprehensive reference card for `react-i18next` and `i18next` in React 19.

> **Target Version:** `react-i18next` v15+, `i18next` v24+  
> **Philosophy:** Comprehensive enterprise internationalization (i18n) and localization (l10n)  
> **Capabilities:** Pluralization, interpolation, language detection, lazy-loaded translation namespaces  

---

## Level 0 — Concepts

- **Namespaces**: Translation keys are split into logical files (`common.json`, `auth.json`, `billing.json`) to prevent downloading giant multi-megabyte language bundles on initial page load.
- **Interpolation & Pluralization**: Dynamic variable replacement (`Welcome, {{name}}!`) and automated plural rules handling zero, singular, and plural quantities across languages.
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `useTranslation()` | Hook | Provides the `t()` translation function and `i18n` instance |
| `t('key', { ...params })` | Function | Translates a key with dynamic interpolations and plural counts |
| `<Trans />` | Component | Translates complex JSX containing embedded links or bold tags |
| `i18n.changeLanguage()` | Method | Switches active language dynamically without page reloads |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install i18next react-i18next i18next-browser-languagedetector
```

### 1.2 Configuration
```ts
// src/i18n/config.ts
import i18n from 'i18next';
import { initReactI18next } from 'react-i18next';
import LanguageDetector from 'i18next-browser-languagedetector';

i18n
  .use(LanguageDetector)
  .use(initReactI18next)
  .init({
    fallbackLng: 'en',
    debug: false,
    interpolation: {
      escapeValue: false, // React already escapes values safely!
    },
    resources: {
      en: {
        translation: {
          welcome: 'Welcome, {{name}}!',
          notifications_one: 'You have {{count}} notification',
          notifications_other: 'You have {{count}} notifications',
        },
      },
      es: {
        translation: {
          welcome: '¡Bienvenido, {{name}}!',
          notifications_one: 'Tienes {{count}} notificación',
          notifications_other: 'Tienes {{count}} notificaciones',
        },
      },
    },
  });

export default i18n;
```

### 1.3 Consuming in React
```tsx
import React from 'react';
import { useTranslation } from 'react-i18next';

export function UserGreeting({ username, unreadCount }: { username: string; unreadCount: number }) {
  const { t, i18n } = useTranslation();

  return (
    <div className="p-4 border rounded">
      <h2>{t('welcome', { name: username })}</h2>
      <p>{t('notifications', { count: unreadCount })}</p>

      <div className="mt-4 flex gap-2">
        <button onClick={() => i18n.changeLanguage('en')} className="px-2 py-1 text-xs border rounded">
          EN
        </button>
        <button onClick={() => i18n.changeLanguage('es')} className="px-2 py-1 text-xs border rounded">
          ES
        </button>
      </div>
    </div>
  );
}
```

---

## Level 2 — Intermediate: Complex Markup with `<Trans />`

Translating text that contains embedded interactive components (e.g. Terms of Service links):

```tsx
import { Trans } from 'react-i18next';
import { Link } from 'react-router-dom';

export function LegalDisclaimer() {
  return (
    <p className="text-xs text-gray-500">
      <Trans i18nKey="legal_agreement">
        By clicking submit, you agree to our <Link to="/terms" className="underline">Terms of Service</Link> and <Link to="/privacy" className="underline">Privacy Policy</Link>.
      </Trans>
    </p>
  );
}
```

---

## Level 3 — Advanced: Right-to-Left (RTL) Layout Switching

For Arabic, Hebrew, and Persian languages, automatically flip layout direction:

```tsx
import { useEffect } from 'react';
import { useTranslation } from 'react-i18next';

export function useDirectionSync() {
  const { i18n } = useTranslation();

  useEffect(() => {
    const isRtl = ['ar', 'he', 'fa'].includes(i18n.language);
    document.documentElement.dir = isRtl ? 'rtl' : 'ltr';
    document.documentElement.lang = i18n.language;
  }, [i18n.language]);
}
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Enabling `escapeValue: true`** | Double-escapes HTML characters in React (e.g. rendering `&amp;` instead of `&`). | Always set `interpolation: { escapeValue: false }`. |
| **Splitting Sentences into Fragmented Keys** | Different languages order nouns and verbs differently, breaking grammatical word order. | Always translate complete, whole sentences with interpolation parameters. |
| **Hardcoding Translations in Root Bundle** | Bloats initial client download for users who only read one language. | Lazy-load language files on demand via `i18next-http-backend`. |
| **Omitting Plural Suffixes (`_one`, `_other`)** | Causes unnatural robotic grammar (e.g. "1 items found"). | Use built-in pluralization rules for each language. |
