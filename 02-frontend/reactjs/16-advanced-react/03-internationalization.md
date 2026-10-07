# Internationalization

**Internationalization (i18n)** is designing an app so it *can* support multiple languages and regions. **Localization (l10n)** is doing it for a specific one: translating text, formatting dates and numbers, supporting right-to-left layouts. It's far easier to build in from the start than to retrofit, because the hard part isn't translating strings, it's finding every place your code **assumes English**.

## What actually needs to change

| Concern | Examples |
|---|---|
| **Text** | Labels, messages, errors, `aria-label`s, `<title>` |
| **Plurals** | "1 item" / "2 items", and languages with *more* than two plural forms (Arabic has six, Polish has several) |
| **Interpolation** | "Hello, {name}": word order differs between languages |
| **Numbers and currency** | `1,234.56` vs `1.234,56` vs `1 234,56`; where the currency symbol goes |
| **Dates and times** | Order, separators, 12/24-hour clock, first day of the week, time zones |
| **Text direction** | Right-to-left (Arabic, Hebrew, Persian, Urdu) |
| **Layout** | German text can run ~30% longer than English; some scripts need more line height |
| **Sorting and search** | Alphabetical order differs per language |
| **Images/icons** | Text baked into images; culturally specific icons; directional arrows |

## Use a library for messages

Don't hand-roll translation lookup, plural rules, or interpolation. The major options:

| Library | Style | Notes |
|---|---|---|
| **react-i18next** (i18next) | Key-based JSON files | Most widely used; large ecosystem; namespaces, lazy loading, plugins |
| **react-intl** (FormatJS) | ICU MessageFormat strings | Standards-based message syntax; strong plural/select support |
| **Lingui** | Extracts messages from source; ICU | Compile-time optimizations; small runtime |

They solve the same problems. Choose by team familiarity and ecosystem. The examples here use react-i18next; the concepts carry over. API details change between versions, so confirm against the library's docs.

### Setup (react-i18next)

```bash
npm install i18next react-i18next
```

```ts
// i18n.ts
import i18n from "i18next"
import { initReactI18next } from "react-i18next"
import en from "./locales/en.json"
import es from "./locales/es.json"

i18n.use(initReactI18next).init({
  resources: {
    en: { translation: en },
    es: { translation: es },
  },
  lng: "en",                       // initial language
  fallbackLng: "en",               // used when a key is missing
  interpolation: { escapeValue: false },   // React already escapes output
})

export default i18n
```

```tsx
// main.tsx: import once, before rendering
import "./i18n"
```

```json
// locales/en.json
{
  "greeting": "Hello, {{name}}!",
  "inbox": {
    "title": "Inbox",
    "unread_one": "You have {{count}} unread message",
    "unread_other": "You have {{count}} unread messages"
  }
}
```

```tsx
import { useTranslation } from "react-i18next"

function Inbox({ name, unread }: { name: string; unread: number }) {
  const { t, i18n } = useTranslation()

  return (
    <section>
      <h1>{t("inbox.title")}</h1>
      <p>{t("greeting", { name })}</p>
      <p>{t("inbox.unread", { count: unread })}</p>      {/* picks _one / _other by plural rules */}
      <button onClick={() => i18n.changeLanguage("es")}>Español</button>
    </section>
  )
}
```

Key ideas:

- Components use **keys** (`inbox.title`), never literal English.
- **Interpolation** (`{{name}}`) lets translators reorder words. Never build sentences by concatenation.
- **Plurals** use `count` with suffixed keys (`_one`, `_other`, plus `_zero`, `_two`, `_few`, `_many` for languages that need them). The library picks the right form using the locale's real plural rules.
- `i18n.changeLanguage` re-renders components that use `useTranslation`.

### Rich text inside a message

A message that contains a link or bold text must not be split into fragments (word order varies). Use the library's component for embedding elements:

```tsx
import { Trans } from "react-i18next"

// en.json: "terms": "I agree to the <1>terms of service</1>."
<Trans i18nKey="terms" components={{ 1: <a href="/terms" /> }} />
```

The translator controls where the link falls in the sentence. Check the library docs for the exact syntax of your version.

## Never do these

```tsx
// ✗ Concatenation: breaks word order, grammar, and gender in other languages
<p>{t("you_have")} {count} {t("messages")}</p>

// ✗ Manual plurals
<p>{count} {count === 1 ? "message" : "messages"}</p>

// ✗ Hardcoded format
<span>${price.toFixed(2)}</span>
<span>{date.getMonth() + 1}/{date.getDate()}/{date.getFullYear()}</span>

// ✗ Untranslated attributes
<button aria-label="Close" title="Close">×</button>
```

```tsx
// ✓
<p>{t("messages.count", { count })}</p>
<span>{formatCurrency(price)}</span>
<button aria-label={t("common.close")}>×</button>
```

Also avoid: **text inside images**, **splitting a sentence across elements**, **reusing one key for two different meanings** (the same English word can be two different words elsewhere), and **assuming a word is gender- or case-neutral**.

## Numbers, dates, and more: use `Intl`

The browser's built-in `Intl` APIs already know every locale's formatting rules, with zero bundle cost:

```ts
const locale = "de-DE"

new Intl.NumberFormat(locale).format(1234567.89)                         // "1.234.567,89"
new Intl.NumberFormat(locale, { style: "currency", currency: "EUR" }).format(49.9)   // "49,90 €"
new Intl.NumberFormat(locale, { style: "percent" }).format(0.256)         // "26 %"
new Intl.NumberFormat(locale, { notation: "compact" }).format(1_200_000)  // "1,2 Mio."

new Intl.DateTimeFormat(locale, { dateStyle: "long" }).format(new Date()) // "7. Oktober 2026"
new Intl.DateTimeFormat(locale, { timeStyle: "short", timeZone: "America/New_York" }).format(new Date())

new Intl.RelativeTimeFormat(locale, { numeric: "auto" }).format(-1, "day") // "gestern"
new Intl.ListFormat(locale, { type: "conjunction" }).format(["A", "B", "C"]) // "A, B und C"
new Intl.PluralRules("ar").select(3)                                      // "few"
["z", "ä", "a"].sort(new Intl.Collator("de").compare)                     // locale-aware sorting
```

Practical notes:

- **Currency is not locale.** Locale controls *formatting*; the **currency code** (`EUR`, `USD`) comes from your data. Formatting `49.9` in `de-DE` doesn't mean it's euros.
- **Time zones**: store and transmit timestamps in **UTC** (ISO strings), and format in the user's zone at display time (`timeZone` option, or the default from their browser).
- **Cache formatters.** Creating an `Intl.NumberFormat` is relatively expensive. Create one per locale/options and reuse it, rather than constructing it inside a loop or on every render:

```tsx
export function useFormatters() {
  const { i18n } = useTranslation()
  return useMemo(() => ({
    number: new Intl.NumberFormat(i18n.language),
    currency: (code: string) => new Intl.NumberFormat(i18n.language, { style: "currency", currency: code }),
    date: new Intl.DateTimeFormat(i18n.language, { dateStyle: "medium" }),
  }), [i18n.language])
}
```

(i18next and FormatJS also offer built-in formatting helpers; they use `Intl` underneath.)

## Choosing and storing the locale

Typical order of precedence:

1. **URL**, such as `/es/pricing`. This is shareable, bookmarkable, and good for SEO (add `hreflang` links when server rendering).
2. **User's saved preference** (account setting, or `localStorage`/cookie).
3. **Browser language** (`navigator.languages`) for first-time visitors.
4. **Fallback** default.

```tsx
// URL-based locale with React Router: a leading optional segment
{ path: ":lang?", element: <LocaleLayout />, children: [ /* …all routes… */ ] }
```

(See [dynamic routes](../10-routing/02-dynamic-routes-and-params.md#optional-segments).) Validate the segment against your supported locales and redirect or fall back otherwise.

### Keep the document in sync

When the language changes, update the page's metadata so browsers and **screen readers** use the right pronunciation and rules:

```tsx
useEffect(() => {
  document.documentElement.lang = i18n.language
  document.documentElement.dir = i18n.dir()      // "ltr" | "rtl" (i18next provides dir())
}, [i18n.language])
```

A missing or wrong `lang` attribute means screen readers read Spanish text with English pronunciation. Also translate the document title, and mark inline foreign-language phrases with `lang="…"` on their element.

## Right-to-left (RTL)

For Arabic, Hebrew, Persian, and others, the whole layout mirrors. Build for it with **logical** CSS properties instead of physical ones:

| Physical (avoid) | Logical (use) | Tailwind |
|---|---|---|
| `margin-left` | `margin-inline-start` | `ms-4` |
| `padding-right` | `padding-inline-end` | `pe-4` |
| `text-align: left` | `text-align: start` | `text-start` |
| `left: 0` | `inset-inline-start: 0` | `start-0` |
| `border-left` | `border-inline-start` | `border-s` |
| `float: left` | `float: inline-start` | n/a |

```tsx
<button className="ms-2 ps-4 text-start">…</button>     // flips automatically with dir="rtl"
```

Set `dir="rtl"` on `<html>` (see above) and logical properties flip on their own. Also:

- **Mirror directional icons** (back arrows, chevrons, progress). Don't mirror logos, media controls, or numbers.
- Flexbox/grid order already follows `dir`, so `flex-row` reverses in RTL, which is usually what you want.
- Mixed content (English product names inside Arabic text) is handled by the Unicode bidi algorithm. Use `<bdi>` for user-generated text of unknown direction.
- **Test it.** Flip `dir="rtl"` in DevTools and look for broken alignment, overlapping, and wrongly oriented icons.

## Layout resilience

- Translated text length varies: German and Finnish run long, Chinese and Japanese run short but can need more line height. Avoid fixed widths on labels and buttons, so use `min-width`, wrapping, and flexible layouts.
- **Pseudo-localization** is a great testing trick: replace text with accented, lengthened placeholders (`Ṧàvé ḿéẃ`) to reveal hardcoded strings, truncation, and layout breakage before real translations exist.
- Don't truncate with `…` as a default fix. A cropped translation can change meaning.

## Loading translations efficiently

Shipping every language to every user wastes bytes. Load the active locale on demand ([code splitting](../14-performance/03-code-splitting-and-lazy-loading.md)):

```ts
// Dynamic import per language
export async function loadLocale(lng: string) {
  const messages = await import(`./locales/${lng}.json`)
  i18n.addResourceBundle(lng, "translation", messages.default)
  await i18n.changeLanguage(lng)
}
```

or use the library's backend plugin (such as `i18next-http-backend`) to fetch JSON files at runtime, and split large catalogs into **namespaces** (`common`, `billing`, `admin`) loaded per route or feature. Suspend or show a fallback while a locale loads so the UI doesn't flash untranslated keys.

## Workflow and quality

- **Key naming**: stable, hierarchical, by feature and meaning (`checkout.payment.title`), not by the English text. Changing English copy shouldn't rename keys.
- **Missing keys**: configure the library to log them in development (and fall back to a default locale in production) so gaps are caught.
- **Type safety**: i18next supports typing the resource shape (declare the resources type) so `t("typo.key")` fails at compile time. Check the docs for your version.
- **Extraction and tooling**: tools can scan source for keys, flag unused and missing ones, and sync with translation management platforms. Translators work best with context (screenshots, descriptions) and a platform, not a raw JSON file.
- **Don't translate on the fly** with machine translation at runtime for core UI. Pre-translate and review.
- **Tests**: render with the i18n provider in test setup, and assert on keys or on a known locale. Add a test that every locale file has the same key set as the source.

## Server rendering

With [SSR](../15-concurrent-and-modern-react/07-server-components-and-ssr.md), the server must pick the locale (URL, cookie, `Accept-Language`) **per request** and render in it; the client must start with the **same** locale and messages or hydration mismatches. Per-request i18n instances (not a shared global) avoid leaking one user's language into another's response. Frameworks provide integrations for this.

## Common mistakes

- **Hardcoded strings** (including `aria-label`, `title`, `placeholder`, alt text, and error messages).
- **Concatenating translated fragments** into sentences.
- **Manual plural logic** (`count === 1 ? … : …`).
- **Hardcoded date, number, and currency formats**, or confusing locale with currency.
- **Using physical CSS properties** (`margin-left`) and breaking RTL.
- **Not setting `lang` and `dir`** on `<html>` (accessibility and pronunciation).
- **Fixed-width layouts** that break with longer translations.
- **Keys named after English text**, so copy edits cascade into key renames.
- **Creating `Intl` formatters on every render.**
- **Bundling all locales** into the main chunk.
- **Storing local-time strings** instead of UTC timestamps.
- **One key reused for different meanings**, so translators can't disambiguate.
- **A shared global i18n instance on the server**, leaking locale between requests.
- **Treating translation as a late-stage task** rather than designing for it from the start.

## Quick summary

- i18n is about removing **English assumptions**: text, plurals, interpolation, numbers, dates, direction, layout, sorting.
- Use a library (react-i18next, react-intl, Lingui) for messages; use keys, interpolation, and the library's plural rules. **Never concatenate or hand-pluralize.**
- Use **`Intl`** (`NumberFormat`, `DateTimeFormat`, `RelativeTimeFormat`, `ListFormat`, `PluralRules`, `Collator`) for formatting, and cache formatter instances. Currency comes from data, not locale.
- Choose the locale from **URL → saved preference → browser → fallback**, and keep `<html lang>` and `dir` in sync.
- Support RTL with **logical CSS properties**; test with `dir="rtl"` and pseudo-localization.
- Load locales lazily, split large catalogs into namespaces, and catch missing keys in development.
- Plan for i18n early: retrofitting means hunting down every hardcoded assumption.

## Next

Continue to [17 — React internals](../17-react-internals/README.md).
