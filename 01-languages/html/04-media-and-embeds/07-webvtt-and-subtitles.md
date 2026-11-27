# WebVTT, Closed Captions, and Video Text Tracks

| Metadata | Details |
| :--- | :--- |
| **Specification** | W3C WebVTT: The Web Video Text Tracks Format / WHATWG HTML Living Standard (§4.8.11 The `track` element) |
| **Prerequisites** | `04-media-and-embeds/01-audio-and-video.md`, `06-accessibility/README.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`<track>`, WebVTT parser, `::cue` CSS pseudo-element) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, axe-core 4.9, NVDA 2024.1, VoiceOver macOS 14.4 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Production Multi-Language Captions with Defaults](#example-1-production-multi-language-captions-with-defaults)
   - [Example 2: Complete WebVTT File Structure with Voice Spans and Styling](#example-2-complete-webvtt-file-structure-with-voice-spans-and-styling)
   - [Example 3: Styling Subtitles Using the CSS `::cue` Pseudo-Element](#example-3-styling-subtitles-using-the-css-cue-pseudo-element)
   - [Example 4: Interactive Video Navigation Using `kind="chapters"`](#example-4-interactive-video-navigation-using-kindchapters)
   - [Example 5: Cross-Origin CDN Subtitle Track Loading with CORS](#example-5-cross-origin-cdn-subtitle-track-loading-with-cors)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Video is one of the most compelling mediums on the web, but uncaptioned video creates catastrophic barriers:
- **Accessibility**: Deaf and hard-of-hearing users cannot perceive spoken dialogue or auditory context.
- **Cognitive & Comprehension**: Non-native speakers, neurodivergent individuals, and users in sound-sensitive environments (commuter transit, open offices, libraries) rely on visual text tracks to consume media.
- **Searchability**: Web search crawlers cannot index raw binary video streams; synchronized text tracks provide high-quality semantic metadata for search engines.

HTML provides native timed text synchronization via the `<track>` element and the **WebVTT (Web Video Text Tracks)** format. Standardized by W3C and WHATWG, WebVTT enables timed captions, subtitles, audio descriptions, and interactive chapter navigation directly within native browser `<video>` and `<audio>` players without third-party plugins.

---

## 2. Core Concepts

### The `<track>` Element
The `<track>` element is a void child of `<video>` or `<audio>` that specifies a timed text track file. Key attributes include:
- **`kind`**: Specifies how the text track is intended to be used:
  - `captions`: Transcription of dialogue, sound effects, and musical cues for Deaf or hard-of-hearing users. Displayed by default if the user has closed-captioning preferences enabled.
  - `subtitles`: Translation of dialogue for users who can hear the audio but do not understand the spoken language.
  - `descriptions`: Textual description of visual video actions, spoken aloud by screen readers for blind users.
  - `chapters`: Timed chapter titles used to build navigation menus within the video timeline.
  - `metadata`: Data parsed by JavaScript scripts for timed programmatic events (not displayed visually).
- **`src`**: URL of the `.vtt` file.
- **`srclang`**: The BCP 47 language code of the text track (e.g., `en`, `es`, `fr`). Required if `kind="subtitles"`.
- **`label`**: A user-readable title for the track shown in the browser's native caption selection menu (e.g., `"English [CC]"`).
- **`default`**: Boolean specifying that the track should be enabled by default unless user preferences dictate otherwise.

### Captions vs. Subtitles: The Critical Distinction
- **Subtitles (`kind="subtitles"`)**: Assume the viewer **can hear** the audio track, but needs translation of the spoken dialogue. Background music, ambient sound effects, and speaker identification are omitted.
- **Captions (`kind="captions"`)**: Assume the viewer **cannot hear** the audio track. They include spoken words PLUS all critical non-speech audio cues: `[Door slams shut]`, `[Ominous orchestral music]`, `[Speaker sighs]`. **WCAG 2.2 Criterion 1.2.2 strictly requires Captions, NOT Subtitles.**

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  Browser WebVTT Parsing & Rendering Lifecycle                           |
+-----------------------------------------------------------------------------------------+

  1. HTML Parser encounters <track src="subs-en.vtt" kind="captions"> inside <video>
       |
       v
  2. Check CORS Policy:
     - If .vtt file is on a different domain, <video> MUST have crossorigin="anonymous"!
     - Otherwise, track loading is BLOCKED by CORS!
       |
       v
  3. WebVTT Parser reads text stream:
     - Verifies "WEBVTT" magic header
     - Parses timestamps: 00:01:20.000 --> 00:01:24.500
     - Stores Cue instances in `video.textTracks[0].cues`
       |
       v
  4. Playback Synchronization:
     - As video playback timeline reaches 00:01:20.000:
       Browser renders text cue inside a dedicated Shadow DOM container
       overlaid on top of the video viewport!
     - Applies CSS ::cue styling rules.
```

---

## 4. Syntax & Usage

```html
<video controls width="960" height="540" playsinline>
  <source src="/media/keynote.mp4" type="video/mp4" />
  
  <!-- English Closed Captions (Default) -->
  <track 
    kind="captions" 
    src="/media/keynote-en.vtt" 
    srclang="en" 
    label="English [CC]" 
    default 
  />

  <!-- Spanish Translation Subtitles -->
  <track 
    kind="subtitles" 
    src="/media/keynote-es.vtt" 
    srclang="es" 
    label="Español" 
  />

  <!-- Interactive Chapters -->
  <track 
    kind="chapters" 
    src="/media/keynote-chapters.vtt" 
    srclang="en" 
    label="Chapters" 
  />
</video>
```

> **Cross-Browser & Baseline Status Box**
>
> - `<track>` element and WebVTT: **Baseline Widely Available** across all major browsers since 2014.
> - CSS `::cue` pseudo-element: **Baseline Widely Available** (Chrome 26+, Firefox 31+, Safari 7+, Edge 79+).

---

## 5. Practical Examples

### Example 1: Production Multi-Language Captions with Defaults
A complete, accessible video player configured with closed captions, translated subtitles, and accessible fallback.

```html
<figure class="video-player-wrapper">
  <video 
    controls 
    preload="metadata" 
    width="1280" 
    height="720" 
    poster="/media/interstellar-poster.jpg"
    playsinline
    aria-labelledby="video-caption"
  >
    <source src="/media/interstellar.webm" type="video/webm; codecs=vp9,opus" />
    <source src="/media/interstellar.mp4" type="video/mp4; codecs=avc1.42E01E,mp4a.40.2" />

    <!-- English Captions (WCAG 1.2.2 compliant) -->
    <track 
      kind="captions" 
      src="/media/captions/interstellar-en.vtt" 
      srclang="en" 
      label="English [CC]" 
      default 
    />

    <!-- French Subtitles -->
    <track 
      kind="subtitles" 
      src="/media/captions/interstellar-fr.vtt" 
      srclang="fr" 
      label="Français" 
    />

    <!-- German Subtitles -->
    <track 
      kind="subtitles" 
      src="/media/captions/interstellar-de.vtt" 
      srclang="de" 
      label="Deutsch" 
    />

    <p>
      Your browser cannot play this video. 
      <a href="/media/interstellar.mp4">Download MP4</a> or read the 
      <a href="/media/transcript.html">full text transcript</a>.
    </p>
  </video>
  <figcaption id="video-caption">Deep Space Exploration: Mission Overview</figcaption>
</figure>
```

---

### Example 2: Complete WebVTT File Structure with Voice Spans and Styling
An authoritative `.vtt` file demonstrating standard syntax, cue settings (positioning, line, align), voice tags (`<v Speaker>`), and formatting tags.

```vtt
WEBVTT - Space Mission Debrief Interview

NOTE
This WebVTT file demonstrates compliant formatting, speaker identification,
and non-speech audio sound effects for WCAG 2.2 AA captions.

00:00:01.000 --> 00:00:04.500 line:85% align:center
[Rocket thrusters rumble loudly in background]

00:00:05.000 --> 00:00:08.200 position:20% align:left
<v Flight Director>Telemetry confirms orbit insertion at 28,000 km/h.

00:00:08.500 --> 00:00:12.000 position:80% align:right
<v Commander Cooper>Copy that, Houston. <b>Visual contact</b> established with the orbital station.

00:00:12.500 --> 00:00:15.000
[Alarm chime beeps twice]

00:00:15.500 --> 00:00:20.000
<v Commander Cooper>Docking sequence initiated. Switching to manual RCS thrusters.
```

---

### Example 3: Styling Subtitles Using the CSS `::cue` Pseudo-Element
Customizing caption appearance, background opacity, font family, and specific speaker styles.

```html
<video id="styled-video" controls width="800" height="450">
  <source src="/media/presentation.mp4" type="video/mp4" />
  <track kind="captions" src="/media/presentation.vtt" srclang="en" label="English" default />
</video>

<style>
  /* 1. Global styling for all WebVTT cues in the video */
  #styled-video::cue {
    background-color: rgba(15, 23, 42, 0.85); /* Dark slate background */
    color: #f8fafc; /* Crisp white text */
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    font-size: 1.1rem;
    line-height: 1.4;
    padding: 0.25rem 0.5rem;
    border-radius: 4px;
    text-shadow: 0 1px 2px rgba(0, 0, 0, 0.8);
  }

  /* 2. Target specific voice tag (<v Flight Director>) */
  #styled-video::cue(v[voice="Flight Director"]) {
    color: #38bdf8; /* Cyan color for Flight Director */
    font-weight: 600;
  }

  /* 3. Target bold styling tags within cues */
  #styled-video::cue(b) {
    color: #f59e0b; /* Amber highlight for critical terms */
  }
</style>
```

---

### Example 4: Interactive Video Navigation Using `kind="chapters"`
Using WebVTT chapter tracks and JavaScript to dynamically construct a clickable video table of contents.

```vtt
WEBVTT - Lecture Chapters

00:00:00.000 --> 00:02:30.000
Introduction and Overview

00:02:30.000 --> 00:07:45.000
Understanding DOM Tokenization

00:07:45.000 --> 00:14:10.000
The CSSOM and Render Tree Pipeline

00:14:10.000 --> 00:20:00.000
Summary and Key Takeaways
```

```html
<div class="video-chapters-layout">
  <video id="lecture-vid" controls width="800" height="450">
    <source src="/media/lecture.mp4" type="video/mp4" />
    <track 
      id="chapter-track" 
      kind="chapters" 
      src="/media/lecture-chapters.vtt" 
      srclang="en" 
      default 
    />
  </video>

  <nav class="chapters-nav" aria-label="Video Chapter Navigation">
    <h4>Video Chapters</h4>
    <ul id="chapters-list"></ul>
  </nav>
</div>

<script>
  const video = document.getElementById('lecture-vid');
  const trackEl = document.getElementById('chapter-track');
  const listEl = document.getElementById('chapters-list');

  // Wait for track cues to load
  trackEl.addEventListener('load', () => {
    const textTrack = trackEl.track;
    textTrack.mode = 'hidden'; // Keep cues in memory without rendering text on screen
    const cues = textTrack.cues;

    listEl.innerHTML = '';
    for (let i = 0; i < cues.length; i++) {
      const cue = cues[i];
      const li = document.createElement('li');
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.textContent = `${cue.text} (${Math.floor(cue.startTime / 60)}:${String(Math.floor(cue.startTime % 60)).padStart(2, '0')})`;
      
      btn.onclick = () => {
        video.currentTime = cue.startTime;
        video.play();
      };

      li.appendChild(btn);
      listEl.appendChild(li);
    }
  });
</script>
```

---

### Example 5: Cross-Origin CDN Subtitle Track Loading with CORS
Subtitle files loaded from an external CDN will fail silently unless `crossorigin` is declared on `<video>`.

```html
<!-- 
  CRITICAL: If subtitle files are hosted on cdn.videos.example.com
  while the HTML page is on www.example.com, the video tag MUST specify
  crossorigin="anonymous" and the CDN must return Access-Control-Allow-Origin: *
-->
<video 
  controls 
  width="960" 
  height="540" 
  crossorigin="anonymous" 
  playsinline
>
  <source src="https://cdn.videos.example.com/asset-101.mp4" type="video/mp4" />
  
  <track 
    kind="captions" 
    src="https://cdn.videos.example.com/captions-en.vtt" 
    srclang="en" 
    label="English [CC]" 
    default 
  />
</video>
```

---

## 6. Best Practices

- [ ] **Always start WebVTT files with the `WEBVTT` header line**: The file must strictly begin with `WEBVTT` (all uppercase). Omitting this or placing characters before it causes the browser parser to abort.
- [ ] **Use `kind="captions"` for accessibility compliance**: Do not rely on `kind="subtitles"` for hearing-impaired users. Captions must transcribe non-speech audio cues (`[Laughter]`, `[Glass shattering]`, speaker identity).
- [ ] **Specify `srclang` and `label` on every `<track>`**: `srclang` provides programmatic language identification; `label` provides the human-readable string in the browser's audio/subtitle selection menu.
- [ ] **Always declare `crossorigin="anonymous"` on `<video>` when tracks come from a CDN**: Browsers strictly enforce CORS on timed text tracks to protect user privacy.
- [ ] **Serve `.vtt` files with the correct MIME type**: The web server must return `Content-Type: text/vtt; charset=utf-8`. If served as `text/plain`, many mobile browsers will reject the track.
- [ ] **Use high-contrast backgrounds for `::cue`**: Ensure captions have at least a 4.5:1 contrast ratio against the video content (typically a semi-transparent black background with white text).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Omitting `crossorigin="anonymous"` on `<video>` with CDN tracks | **Critical** | Subtitles fail to load silently due to CORS security block; zero errors in console. | Add `crossorigin="anonymous"` to the parent `<video>`. |
| Missing `WEBVTT` header as line 1 of `.vtt` file | **Critical** | Parser throws syntax error and ignores the entire track file. | Ensure line 1 of the file begins with `WEBVTT`. |
| Using `kind="subtitles"` instead of `kind="captions"` for accessibility | **High** | Fails WCAG 2.2 Criterion 1.2.2; deaf users miss sound effects, music, and speaker changes. | Use `kind="captions"` and include non-speech auditory cues. |
| Serving `.vtt` files as `text/plain` | **High** | Firefox and Safari may refuse to parse tracks served with incorrect MIME types. | Configure web server to return `Content-Type: text/vtt`. |
| Forgetting timestamps format requirement | **Moderate** | Timestamps must use `HH:MM:SS.mmm` or `MM:SS.mmm` with a dot (`.`), not a comma (`,`). | Use dots for milliseconds (`00:01:20.500`). |
| Omitting the `default` attribute when captions are primary | **Moderate** | User must manually open the CC menu and toggle captions on every video load. | Add `default` to the primary accessibility track. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 1.2.2 Captions (Prerecorded - Level A)**: Captions are provided for all prerecorded audio content in synchronized media.
- **Guideline 1.2.4 Captions (Live - Level AA)**: Captions are provided for all live audio content in synchronized media.
- **Guideline 1.2.5 Audio Description (Prerecorded - Level AA)**: Audio description is provided for all prerecorded video content via `kind="descriptions"` or a secondary audio track.

### SEO & Web Crawlers
- Search engines (Googlebot) read and index WebVTT caption files directly. Transcripts embedded in WebVTT provide rich topical keyword signals that allow search engines to rank specific timestamps within long-form video content.

---

## 9. Comparison / When to Use What

| Track Kind (`kind=`) | Target Audience | Content Included | User Need |
| :--- | :--- | :--- | :--- |
| **`captions`** | Deaf / Hard of hearing, sound-off environments | Spoken dialogue + sound effects + speaker IDs | **Mandatory for WCAG 1.2.2** |
| **`subtitles`** | Viewers who can hear but don't know the language | Translated spoken dialogue only | Foreign language comprehension |
| **`descriptions`** | Blind / Low-vision users | Spoken narration of visual action on screen | **Mandatory for WCAG 1.2.5** |
| **`chapters`** | All users | Topic headings and section boundaries | Timeline scrubbing & navigation |
| **`metadata`** | JavaScript applications | JSON or raw data payloads for code events | Interactive video apps (e.g., clickable links) |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between <code>kind="captions"</code> and <code>kind="subtitles"</code>?</strong></summary>

- `kind="subtitles"` assumes the viewer can hear the audio, but does not understand the spoken language; it provides text translations of dialogue only.
- `kind="captions"` assumes the viewer cannot hear the audio; it provides a complete textual representation of the auditory experience, including spoken dialogue, speaker identification (`<v Speaker>`), and critical non-speech sound effects (`[Applause]`, `[Thunder cracks]`, `[Suspenseful music]`). Only captions satisfy WCAG 2.2 Criterion 1.2.2.
</details>

<details>
<summary><strong>2. Why will an external WebVTT track hosted on a CDN fail to display unless <code>crossorigin</code> is set?</strong></summary>

Browser security specifications subject timed text tracks to strict Cross-Origin Resource Sharing (CORS) rules to prevent unauthorized cross-origin data leakage. If the `.vtt` file originates from a different origin than the host webpage, the parent `<video>` element must declare `crossorigin="anonymous"` (or `use-credentials`), and the CDN server must respond with the HTTP header `Access-Control-Allow-Origin: *`. Without this, the browser halts text track parsing.
</details>

<details>
<summary><strong>3. What are the syntax rules for timestamps in a WebVTT cue?</strong></summary>

WebVTT timestamps must strictly follow the format `HH:MM:SS.mmm` or `MM:SS.mmm`. Hours are optional if less than 1, minutes and seconds must be two digits, and milliseconds must be three digits separated by a **period** (`.`), NOT a comma. Timestamps are joined by the separator string ` --> ` (with exact spaces on either side):
`00:01:25.500 --> 00:01:29.000`
</details>

<details>
<summary><strong>4. How can developers customize the visual appearance of video subtitles using CSS?</strong></summary>

By using the `::cue` CSS pseudo-element. It matches all WebVTT cues rendered in the video's media overlay:
```css
video::cue {
  background-color: rgba(0, 0, 0, 0.8);
  color: #ffffff;
  font-size: 1.2rem;
}
```
Specific cues can be targeted using voice selectors (e.g., `video::cue(v[voice="Narrator"])`) or CSS classes defined inside the VTT file (`<c.alert>Caution</c>`).
</details>

<details>
<summary><strong>5. What is the purpose of the <code>default</code> attribute on a <code>&lt;track&gt;</code> element?</strong></summary>

The `default` attribute instructs the browser to enable and display that specific text track automatically when the video begins playing, unless the user's operating system or browser settings explicitly have closed captioning disabled. Only one track per media element can carry the `default` attribute.
</details>

<details>
<summary><strong>6. How does <code>kind="chapters"</code> function, and how is it exposed to users?</strong></summary>

`kind="chapters"` provides timed metadata that segments the video timeline into logical chapters or topics. Browsers with native chapter support render visual tick marks or chapter titles on the scrub bar. Furthermore, JavaScript can access `video.textTracks` to extract chapter start times and titles dynamically to build custom table-of-contents navigation menus.
</details>

<details>
<summary><strong>7. What HTTP Content-Type header must a web server return for <code>.vtt</code> files?</strong></summary>

The official MIME type is `text/vtt; charset=utf-8`. If a server incorrectly returns `text/plain` or `application/octet-stream`, modern browsers (notably Firefox and Safari) will reject the track file as invalid and refuse to render subtitles.
</details>

<details>
<summary><strong>8. Can a WebVTT cue specify where on the video screen the text should appear?</strong></summary>

Yes. WebVTT supports cue settings placed directly after the timestamps. Common settings include:
- `line:XX%`: Vertical positioning on screen (e.g., `line:10%` for top, `line:90%` for bottom).
- `position:XX%`: Horizontal positioning of the cue box.
- `size:XX%`: Width of the cue box.
- `align:left|center|right`: Text alignment within the cue box.
</details>

---

## 11. Practice Exercises

### Exercise 1: Video with English Captions and Spanish Subtitles (Easy)
- **Task**: Author an HTML `<video>` element with native controls and two text tracks:
  1. English Closed Captions marked as `default`.
  2. Spanish Translated Subtitles.

<details>
<summary>View Solution</summary>

```html
<video controls width="800" height="450" playsinline>
  <source src="/media/history.mp4" type="video/mp4" />
  
  <track 
    kind="captions" 
    src="/captions/history-en.vtt" 
    srclang="en" 
    label="English [CC]" 
    default 
  />

  <track 
    kind="subtitles" 
    src="/captions/history-es.vtt" 
    srclang="es" 
    label="Español" 
  />
</video>
```
</details>

---

### Exercise 2: Valid WebVTT File with Non-Speech Audio (Medium)
- **Task**: Write a valid `.vtt` file with 3 cues:
  1. Cue 1 (0 to 3 seconds): Sound effect: `[Upbeat guitar music begins]`.
  2. Cue 2 (3.5 to 7 seconds): Speaker "Presenter": `Welcome to our annual web engineering conference.`
  3. Cue 3 (7.2 to 10 seconds): Speaker "Presenter": `Today we explore WebVTT accessibility.` positioned at `line:80% align:center`.

<details>
<summary>View Solution</summary>

```vtt
WEBVTT - Conference Welcome

00:00:00.000 --> 00:00:03.000
[Upbeat guitar music begins]

00:00:03.500 --> 00:00:07.000
<v Presenter>Welcome to our annual web engineering conference.

00:00:07.200 --> 00:00:10.000 line:80% align:center
<v Presenter>Today we explore WebVTT accessibility.
```
</details>

---

### Exercise 3: Custom Subtitle Styling with Voice Coloring (Hard)
- **Task**: Write CSS using `::cue` to style subtitles for a video with `id="interview-video"`:
  - Base subtitles: 18px sans-serif font, white text, 80% opacity black background, 4px border-radius.
  - Voice "Interviewer": Gold text color (`#fbbf24`).
  - Voice "Candidate": Neon green text color (`#4ade80`).

<details>
<summary>View Solution</summary>

```css
/* Base WebVTT Cue Styling */
#interview-video::cue {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  font-size: 18px;
  color: #ffffff;
  background-color: rgba(0, 0, 0, 0.8);
  border-radius: 4px;
  padding: 2px 6px;
  text-shadow: 0 1px 2px rgba(0, 0, 0, 0.9);
}

/* Interviewer Speaker Voice */
#interview-video::cue(v[voice="Interviewer"]) {
  color: #fbbf24;
  font-weight: 600;
}

/* Candidate Speaker Voice */
#interview-video::cue(v[voice="Candidate"]) {
  color: #4ade80;
  font-weight: 500;
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 WEBVTT & <track> CHEAT SHEET                                    |
+-------------------------------------------------------------------------------------------------+
| Attribute / Syntax       | Purpose / Behavior                                                   |
+-------------------------------------------------------------------------------------------------+
| <track kind="captions">  | Dialogue + sound effects for Deaf users (MANDATORY for WCAG 1.2.2)  |
| <track kind="subtitles"> | Translated dialogue for hearing users who speak another language     |
| <track kind="chapters">  | Video section markers for interactive navigation menus               |
| srclang="en"             | BCP 47 language code of the text track                               |
| label="English [CC]"     | Human-readable title in browser CC track menu                        |
| default                  | Automatically enables this track upon load                           |
| crossorigin="anonymous"  | MANDATORY on <video> if track files originate from a CDN             |
| WEBVTT                   | Magic header; MUST be the very first line of every .vtt file         |
| 00:00:01.000 --> ...     | Cue timestamp syntax (dot for milliseconds, NOT comma)               |
| ::cue                    | CSS pseudo-element for styling caption text and backgrounds          |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [W3C WebVTT: The Web Video Text Tracks Format Specification](https://www.w3.org/TR/webvtt1/)
- [MDN Web Docs: Web Video Text Tracks Format (WebVTT)](https://developer.mozilla.org/en-US/docs/Web/API/WebVTT_API)
- [W3C Web Content Accessibility Guidelines (WCAG) 2.2: Guideline 1.2.2 Captions (Prerecorded)](https://www.w3.org/WAI/WCAG22/Understanding/captions-prerecorded.html)
- [web.dev: Video accessibility best practices](https://web.dev/articles/video-accessibility)

---

[← Previous: Canvas vs. SVG](06-canvas-vs-svg.md) | [Section Index](README.md) | [Next: Object & Embed →](08-object-and-embed.md)
