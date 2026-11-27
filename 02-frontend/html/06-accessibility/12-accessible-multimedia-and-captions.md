# Accessible Multimedia, Audio & Captions

| Metadata | Details |
|:---|:---|
| **Level** | Intermediate to Advanced |
| **Prerequisites** | `04-media-and-embeds/01-audio-and-video.md`, `04-media-and-embeds/07-webvtt-and-subtitles.md` |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Current |
| **Baseline Status** | Widely Available (HTML5 Media & WebVTT Standards) |
| **Browsers Tested** | Chrome 120+, Firefox 121+, Safari 17.2+, Edge 120+ |
| **Tested With** | NVDA 2024.1, JAWS 2024, VoiceOver, Keyboard Controls |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
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

### What is Multimedia Accessibility?
Multimedia accessibility is the engineering practice of making time-based audio and video content perceptible and operable to individuals with sensory disabilities:
- **Deaf or hard-of-hearing users** require accurate text representations of dialogue and sound effects (**Captions** and **Transcripts**).
- **Blind or low-vision users** require spoken descriptions of key visual action, facial expressions, and onscreen text (**Audio Descriptions**).
- **Users with vestibular or photosensitive seizure disorders** must be protected from flashing video and un-pausable motion.
- **Motor-impaired users** must be able to control playback entirely via keyboard.

### WCAG 2.2 Time-Based Media Requirements
WCAG Guideline 1.2 establishes strict legal benchmarks for multimedia:
- **1.2.1 Audio-only and Video-only (Prerecorded) (Level A)**: Podcasts must provide a text transcript; silent video must provide an audio track or text description.
- **1.2.2 Captions (Prerecorded) (Level A)**: All recorded video with sound must have synchronized captions.
- **1.2.3 Audio Description or Media Alternative (Prerecorded) (Level A)**: Narration of visual information must be provided.
- **1.2.4 Captions (Live) (Level AA)**: Real-time broadcasts must provide synchronized live captions.
- **1.2.5 Audio Description (Prerecorded) (Level AA)**: Full synchronized audio description track.
- **1.4.2 Audio Control (Level A)**: Any audio playing automatically for more than 3 seconds must have a mechanism to pause, stop, or control volume independently from the system.
- **2.3.1 Three Flashes or Below Threshold (Level A)**: Content must not flash more than three times per second to prevent seizures.

---

## 2. Core Concepts

### Captions vs Subtitles vs Transcripts
A common source of confusion in web engineering is conflating these three distinct modalities:

```
+--------------------------------------------------------------------------+
|                     MULTIMEDIA TEXT ALTERNATIVES                         |
+--------------------------------------------------------------------------+
| Feature     | Intended Audience   | Content Included                     |
+-------------+---------------------+--------------------------------------+
| Captions    | Deaf or hard of     | Spoken dialogue PLUS non-speech audio|
|             | hearing             | cues: [Door slams], [Upbeat music]   |
+-------------+---------------------+--------------------------------------+
| Subtitles   | Hearing viewers who | Spoken dialogue translated into      |
|             | don't speak language| another language. Assumes user can   |
|             |                     | hear gunshots and thunder claps.     |
+-------------+---------------------+--------------------------------------+
| Transcript  | Everyone (Deaf,     | Complete text document of dialogue   |
|             | blind, low bandwidth| and visual actions. Allows reading,  |
|             | users, search bots) | searching, and braille display access|
+--------------------------------------------------------------------------+
```

### The Native `<track>` Element
HTML5 `<video>` and `<audio>` elements natively support synchronized text tracks via the `<track>` tag, referencing **WebVTT** (`.vtt`) files:

```html
<video controls width="800" height="450">
  <source src="keynote.mp4" type="video/mp4">
  <!-- Captions for deaf/hard-of-hearing users -->
  <track 
    kind="captions" 
    src="keynote-captions.vtt" 
    srclang="en" 
    label="English (CC)" 
    default
  >
  <!-- Audio description for visually impaired users -->
  <track 
    kind="descriptions" 
    src="keynote-descriptions.vtt" 
    srclang="en" 
    label="English Audio Description"
  >
</video>
```

---

## 3. How It Works Internally

### Browser Media Pipeline & WebVTT Synchronization
The browser engine parses WebVTT cues into an internal cue timeline synchronized to the video playback clock:

```
HTMLMediaElement Playback Clock (Time: 00:01:24.500)
                         │
                         ▼
             TextTrackList Cue Dispatcher
                         │
        ┌────────────────┴────────────────┐
        ▼                                 ▼
   Active Cue Detected:            Display Formatting:
   00:01:23.000 --> 00:01:26.000   CSS ::cue styling applied
   [Suspenseful violin music]
                         │
                         ▼
       Rendered into Browser Video Sub-tree
       (Exposed to Accessibility Tree as active caption text)
```

### The CORS Requirement for Subtitles
A notorious browser security requirement: **WebVTT track files fetched from external CDNs must be served with CORS headers**, and the parent `<video>` element **must include `crossorigin="anonymous"`**:
```html
<video controls crossorigin="anonymous">
  <source src="https://cdn.example.com/video.mp4">
  <track src="https://cdn.example.com/captions.vtt" kind="captions">
</video>
```
If `crossorigin="anonymous"` is missing, the browser will silently block the `.vtt` file for security reasons, completely disabling captions!

---

## 4. Syntax & Usage

### WebVTT File Format Anatomy
An accessible WebVTT file includes speaker identifications and non-speech sound descriptions:

```webvtt
WEBVTT - Product Keynote Accessible Captions

00:00:01.000 --> 00:00:04.500
[Upbeat electronic music fades in]

00:00:05.000 --> 00:00:08.200
<v Sarah Connor>Good morning, everyone. Welcome to Summit 2026.

00:00:08.500 --> 00:00:11.000
[Audience applauds enthusiastically]

00:00:11.500 --> 00:00:15.000
<v Sarah Connor>Today, we are announcing our carbon-negative data centers.
```

---

## 5. Practical Examples

### Example 1: Basic — Native Video with Default Closed Captions
Providing an accessible baseline with standard browser controls:

```html
<video controls width="640" height="360" poster="thumb.jpg">
  <source src="intro.mp4" type="video/mp4">
  <source src="intro.webm" type="video/webm">
  
  <!-- default attribute enables captions automatically unless user disabled -->
  <track 
    kind="captions" 
    src="intro-en.vtt" 
    srclang="en" 
    label="English (CC)" 
    default
  >
  
  <p>Your browser does not support HTML5 video. <a href="intro.mp4">Download video</a>.</p>
</video>
```

### Example 2: Real-World — Interactive Synchronized Transcript
An interactive transcript allows users to read the full text, search for keywords, and click any sentence to jump the video directly to that timestamp:

```html
<div class="media-player-wrapper">
  <video id="lecture-video" controls width="640" height="360">
    <source src="lecture.mp4" type="video/mp4">
    <track id="lecture-track" kind="captions" src="lecture.vtt" srclang="en" default>
  </video>

  <div class="transcript-box" aria-label="Interactive Transcript">
    <h3>Transcript</h3>
    <div id="transcript-content">
      <p>
        <button type="button" class="transcript-cue" onclick="jumpTo(5.2)">
          <time>00:05</time> Sarah: In this module, we explore accessibility tree mechanics.
        </button>
      </p>
      <p>
        <button type="button" class="transcript-cue" onclick="jumpTo(12.8)">
          <time>00:12</time> Sarah: Notice how the browser bridges DOM nodes to OS APIs.
        </button>
      </p>
    </div>
  </div>
</div>

<script>
  function jumpTo(seconds) {
    const video = document.getElementById('lecture-video');
    video.currentTime = seconds;
    video.play();
  }
</script>
```

### Example 3: Edge Case — Ambient Hero Background Video with Pause Button
WCAG 1.4.2 mandates that background video playing automatically must have a visible pause mechanism:

```html
<div class="hero-video-container">
  <!-- Muted is mandatory for autoplay; loop creates ambient movement -->
  <video 
    id="hero-bg-video" 
    autoplay 
    muted 
    loop 
    playsinline 
    aria-hidden="true"
  >
    <source src="ambient-clouds.mp4" type="video/mp4">
  </video>

  <!-- WCAG 1.4.2 & 2.2.2 Compliance: Accessible pause toggle -->
  <button 
    type="button" 
    id="video-pause-btn" 
    class="video-control"
    aria-pressed="false"
    onclick="toggleBackgroundVideo(this)"
  >
    <span class="btn-text">Pause Background Animation</span>
  </button>
</div>

<script>
  function toggleBackgroundVideo(btn) {
    const video = document.getElementById('hero-bg-video');
    if (video.paused) {
      video.play();
      btn.setAttribute('aria-pressed', 'false');
      btn.querySelector('.btn-text').textContent = 'Pause Background Animation';
    } else {
      video.pause();
      btn.setAttribute('aria-pressed', 'true');
      btn.querySelector('.btn-text').textContent = 'Play Background Animation';
    }
  }
</script>
```

### Example 4: Failure Case — Broken Multimedia Anti-Patterns
Common implementations that trigger instant accessibility lawsuits:

```html
<!-- BROKEN: Autoplay audio with NO pause/mute button (Violates WCAG 1.4.2) -->
<!-- Deafens screen reader users and drowns out screen reader speech synthesis! -->
<audio autoplay src="welcome-jingle.mp3"></audio>

<!-- BROKEN: Custom video player with <div> buttons and no keyboard handling -->
<div class="custom-player">
  <video src="clip.mp4"></video>
  <!-- Cannot be focused with Tab; cannot be clicked with Space/Enter -->
  <div class="play-icon" onclick="play()">Play</div>
</div>

<!-- BROKEN: Flashing strobe light animation (Violates WCAG 2.3.1 Seizure Threshold) -->
<!-- Flashing more than 3 times per second triggers epileptic seizures! -->
```

### Example 5: Accessible Production-Grade — Custom Player with Full Keyboard & ARIA Controls
When building a branded video player, you must wire complete keyboard accessibility:

```html
<div class="player-container" role="region" aria-label="Product Showcase Video">
  <video id="player-media" width="640" height="360">
    <source src="showcase.mp4" type="video/mp4">
    <track kind="captions" src="showcase.vtt" srclang="en" default>
  </video>

  <div class="player-controls" role="toolbar" aria-label="Media Controls">
    <!-- Play/Pause Toggle Button -->
    <button 
      type="button" 
      id="play-toggle" 
      aria-label="Play video" 
      onclick="togglePlay()"
    >
      ▶
    </button>

    <!-- Accessible Mute Toggle -->
    <button 
      type="button" 
      id="mute-toggle" 
      aria-pressed="false" 
      aria-label="Mute audio" 
      onclick="toggleMute()"
    >
      🔊
    </button>

    <!-- Fullscreen Toggle -->
    <button 
      type="button" 
      aria-label="Toggle Fullscreen" 
      onclick="toggleFullscreen()"
    >
      ⛶
    </button>
  </div>
</div>

<script>
  const vid = document.getElementById('player-media');
  const playBtn = document.getElementById('play-toggle');
  const muteBtn = document.getElementById('mute-toggle');

  function togglePlay() {
    if (vid.paused) {
      vid.play();
      playBtn.setAttribute('aria-label', 'Pause video');
      playBtn.textContent = '⏸';
    } else {
      vid.pause();
      playBtn.setAttribute('aria-label', 'Play video');
      playBtn.textContent = '▶';
    }
  }

  function toggleMute() {
    vid.muted = !vid.muted;
    muteBtn.setAttribute('aria-pressed', String(vid.muted));
    muteBtn.setAttribute('aria-label', vid.muted ? 'Unmute audio' : 'Mute audio');
    muteBtn.textContent = vid.muted ? '🔇' : '🔊';
  }

  function toggleFullscreen() {
    if (!document.fullscreenElement) {
      vid.requestFullscreen();
    } else {
      document.exitFullscreen();
    }
  }
</script>
```

---

## 6. Best Practices

1. **Always Supply Synchronized Captions**: Every prerecorded video with audio must feature a `<track kind="captions">` file.
2. **Include Non-Speech Audio in Captions**: Describe meaningful background sounds (e.g., `[ominous music]`, `[phone rings]`), not just dialogue.
3. **Never Autoplay Unmuted Audio**: Unmuted autoplay video drowns out screen reader speech synthesis, rendering the device temporarily unusable.
4. **Provide Full Interactive Transcripts**: Pair podcasts and keynote videos with a complete, readable text transcript placed directly below the player.
5. **Ensure Keyboard Operability of Custom Controls**: If you hide native `controls` to render custom CSS buttons, every control must be a native `<button>` operable via `Enter` and `Space`.
6. **Eliminate Rapid Flashing**: Verify that animations, strobe lights, and video effects do not exceed 3 flashes per second.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | WCAG Success Criterion | Correction |
|:---|:---|:---|:---|
| Video with audio lacking captions | Critical | 1.2.2 Captions (Prerecorded) (A) | Add `<track kind="captions">` WebVTT file |
| Autoplay audio > 3s without pause button | Critical | 1.4.2 Audio Control (A) | Remove autoplay or add prominent pause toggle |
| Subtitles that omit sound effects | High | 1.2.2 Captions (Prerecorded) (A) | Add non-speech cues (`[Thunder rumbles]`) |
| Missing CORS headers on CDN `.vtt` file | High | 1.2.2 Captions (Prerecorded) (A) | Add `crossorigin="anonymous"` to `<video>` |
| Un-focusable custom play/pause buttons | High | 2.1.1 Keyboard (A) | Use native `<button>` elements for player UI |
| Video content flashing > 3 times per second | Critical | 2.3.1 Three Flashes (A) | Edit video to reduce flash frequency and intensity |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Closed Captions as Search Engines Superfood
Search engine spiders (Googlebot) cannot listen to audio files or watch video MP4 pixels. However, **Google crawls WebVTT track files and text transcripts directly**. Providing synchronized captions and transcripts allows search engines to index spoken dialogue, dramatically improving video ranking and organic search traffic.

### Video Transcoding Costs
Generating human-verified WebVTT caption files reduces user bounce rates on mobile devices, where over 70% of social and web videos are watched with volume muted in public environments.

### Security: WebVTT Script Injection
WebVTT files allow HTML formatting tags (like `<b>`, `<i>`, `<v>`). Ensure that user-uploaded caption files are strictly sanitized on the server before serving, preventing attackers from injecting `<script>` tags or malicious HTML into track cues.

---

## 9. Comparison / When to Use What

### Time-Based Media Alternatives Comparison

| Requirement | Captions (`kind="captions"`) | Subtitles (`kind="subtitles"`) | Audio Description (`kind="descriptions"`) | Transcript |
|:---|:---:|:---:|:---:|:---:|
| **Dialogue Included?** | YES | YES | NO (Narration only) | YES |
| **Sound Effects Included?**| YES | NO | YES | YES |
| **Synchronized to Video?**| YES | YES | YES | Optional |
| **Deaf Users** | Primary | Incomplete | Inapplicable | Essential |
| **Blind Users** | Inapplicable | Inapplicable | Primary | Essential |
| **Search Engine Bots** | Crawlable | Crawlable | Crawlable | Highly Crawlable |

---

## 10. Interview Questions

### Q1: What is the technical and semantic difference between Captions and Subtitles?
<details>
<summary>View Answer</summary>

- **Captions** (`<track kind="captions">`): Designed specifically for **deaf or hard-of-hearing viewers**. Captions include all spoken dialogue *plus* non-speech auditory cues that are essential for understanding the scene (e.g., `[Doorbell chimes]`, `[Eerie whisper]`, `[Thunder rumbles]`). They are typically in the same language as the audio.
- **Subtitles** (`<track kind="subtitles">`): Designed for **hearing viewers who do not understand the spoken language**. Subtitles translate dialogue only, operating under the assumption that the viewer can still hear gunshots, laughter, and musical cues.
</details>

### Q2: Why is the `crossorigin="anonymous"` attribute required on `<video>` when fetching captions from a CDN?
<details>
<summary>View Answer</summary>

Under the HTML security specification, WebVTT track files are subject to **Cross-Origin Resource Sharing (CORS)** policies. 

If the video player is hosted on `example.com` and the `.vtt` file is loaded from `cdn.example.com`, the browser will refuse to load the captions unless:
1. The CDN server responds with `Access-Control-Allow-Origin: *` (or matching origin).
2. The `<video>` element explicitly includes the `crossorigin="anonymous"` attribute.
Without this attribute, the browser silently blocks the captions.
</details>

### Q3: What is WCAG Success Criterion 1.4.2 (Audio Control), and why is it essential for screen reader users?
<details>
<summary>View Answer</summary>

WCAG 1.4.2 (Level A) mandates that if any audio on a webpage plays automatically for more than 3 seconds, a mechanism must be available to **pause, stop, or control the audio volume independently from the system volume**.

This is vital because screen reader users navigate by listening to speech synthesis (e.g., NVDA or VoiceOver). If a webpage autoplays background music or marketing video, the loud audio drowns out the screen reader's voice. The blind user cannot hear their screen reader to locate the mute button, effectively locking them out of the computer.
</details>

### Q4: What is an Audio Description (WCAG 1.2.3 / 1.2.5), and how is it delivered?
<details>
<summary>View Answer</summary>

An **Audio Description** is an audio narration track that describes important visual details in a video that cannot be understood from the main audio soundtrack alone (e.g., facial expressions, silent actions, onscreen text, scene changes).

It can be delivered:
1. As a separate audio track selected in the player.
2. Synthesized automatically via a text track: `<track kind="descriptions" src="desc.vtt">`.
3. As a dedicated alternate version of the video with descriptions baked into the master audio mix.
</details>

### Q5: What is the "Three Flashes" threshold (WCAG 2.3.1), and why is it a Level A requirement?
<details>
<summary>View Answer</summary>

WCAG 2.3.1 (Level A) dictates that web pages must not contain anything that flashes more than **three times in any one-second period**, or the flash must be below the general flash and red flash thresholds.

Flashing or strobing lights in video or CSS animations at frequencies between 3 Hz and 50 Hz can trigger photogenic epileptic seizures in individuals with photosensitive epilepsy. It is a Level A criterion because violating it can cause immediate physical harm or death.
</details>

### Q6: What is a Synchronized Interactive Transcript, and why is it considered the most universally accessible media format?
<details>
<summary>View Answer</summary>

An interactive transcript is a complete text representation of a video or podcast presented alongside the player, where each paragraph is timestamped and highlighted synchronously as media plays.

It is universally accessible because:
- Deaf users can read the entire dialogue.
- Blind users can navigate it via screen reader at their own reading pace or output it to a refreshable Braille display.
- Low-bandwidth users can read content without downloading heavy video files.
- Users can click any paragraph to jump playback directly to that moment.
- Search engines index the full transcript for SEO.
</details>

### Q7: What are the allowed values for the `kind` attribute on the `<track>` element?
<details>
<summary>View Answer</summary>

- `captions`: Synchronized dialogue + sound effects for deaf/hard-of-hearing viewers.
- `subtitles`: Synchronized translated dialogue for viewers who don't understand the audio language.
- `descriptions`: Synchronized textual audio description of visual action.
- `chapters`: Timestamps defining chapter navigation titles.
- `metadata`: Script-readable timed metadata (invisible to users).
</details>

### Q8: What CSS pseudo-element is used to style WebVTT captions?
<details>
<summary>View Answer</summary>

The **`::cue`** pseudo-element:
```css
video::cue {
  background-color: rgba(0, 0, 0, 0.8);
  color: #ffffff;
  font-family: sans-serif;
  font-size: 1.1rem;
}
```
It allows authors to customize caption colors, backgrounds, and fonts for maximum readability.
</details>

---

## 11. Practice Exercises

### Exercise 1: Easy — Adding Captions to HTML5 Video
**Problem**: Write the HTML for a video element that includes an English closed-caption track set to active by default.

<details>
<summary>View Solution</summary>

```html
<video controls width="640" height="360">
  <source src="presentation.mp4" type="video/mp4">
  <track 
    kind="captions" 
    src="captions-en.vtt" 
    srclang="en" 
    label="English (CC)" 
    default
  >
</video>
```
</details>

### Exercise 2: Easy — Styling Captions with High Contrast
**Problem**: Write CSS using `::cue` to style video captions with yellow text on a semi-transparent black background.

<details>
<summary>View Solution</summary>

```css
video::cue {
  color: #ffff00;
  background-color: rgba(0, 0, 0, 0.85);
  font-family: system-ui, sans-serif;
  font-size: 1.2rem;
  line-height: 1.4;
}
```
</details>

### Exercise 3: Medium — Enabling CORS for Remote Subtitles
**Problem**: A video hosted on `myapp.com` loads WebVTT captions from `cdn.mediaprovider.com/subtitles.vtt`. Write the `<video>` tag with the necessary attributes to prevent the browser from blocking the track.

<details>
<summary>View Solution</summary>

```html
<video controls crossorigin="anonymous" width="800" height="450">
  <source src="https://cdn.mediaprovider.com/video.mp4" type="video/mp4">
  <track 
    kind="captions" 
    src="https://cdn.mediaprovider.com/subtitles.vtt" 
    srclang="en" 
    label="English" 
    default
  >
</video>
```
</details>

### Exercise 4: Medium — Writing Accessible WebVTT with Sound Cues
**Problem**: Write a short 3-cue WebVTT file for a scene where a phone rings, Sarah answers, and sighs with relief.

<details>
<summary>View Solution</summary>

```webvtt
WEBVTT

00:00:01.000 --> 00:00:03.500
[Rotary telephone ringing sharply]

00:00:04.000 --> 00:00:07.000
<v Sarah Connor>Hello? John, is that you?

00:00:07.500 --> 00:00:09.500
[Exhales deeply with relief]
```
</details>

### Exercise 5: Hard — Implementing Autoplay Background Video with Accessible Pause
**Problem**: Write the HTML, CSS, and vanilla JavaScript for an ambient hero background video that autoplays muted, but includes a prominent keyboard-accessible play/pause toggle button complying with WCAG 1.4.2.

<details>
<summary>View Solution</summary>

```html
<div class="hero-container">
  <video id="bg-vid" autoplay muted loop playsinline aria-hidden="true">
    <source src="clouds.mp4" type="video/mp4">
  </video>

  <div class="hero-content">
    <h1>Next-Gen Cloud Architecture</h1>
    <p>Zero latency, multi-region compute.</p>
  </div>

  <button 
    type="button" 
    id="video-toggle" 
    aria-pressed="false"
    onclick="toggleAmbientVideo()"
  >
    Pause Video
  </button>
</div>

<script>
  function toggleAmbientVideo() {
    const vid = document.getElementById('bg-vid');
    const btn = document.getElementById('video-toggle');
    
    if (vid.paused) {
      vid.play();
      btn.setAttribute('aria-pressed', 'false');
      btn.textContent = 'Pause Video';
    } else {
      vid.pause();
      btn.setAttribute('aria-pressed', 'true');
      btn.textContent = 'Play Video';
    }
  }
</script>
```
</details>

### Exercise 6: Hard — Interactive Transcript Seek Implementation
**Problem**: Write a JavaScript snippet that synchronizes video playback with a list of transcript paragraphs, highlighting the currently active paragraph as the video plays.

<details>
<summary>View Solution</summary>

```html
<video id="lecture" controls src="talk.mp4" width="400"></video>
<div id="transcript">
  <p data-start="0" data-end="5">Introduction to web standards.</p>
  <p data-start="5" data-end="12">Understanding the HTML Living Standard.</p>
  <p data-start="12" data-end="20">Accessible multimedia architecture.</p>
</div>

<script>
  const video = document.getElementById('lecture');
  const paragraphs = document.querySelectorAll('#transcript p');

  video.addEventListener('timeupdate', () => {
    const current = video.currentTime;
    paragraphs.forEach(p => {
      const start = parseFloat(p.getAttribute('data-start'));
      const end = parseFloat(p.getAttribute('data-end'));
      if (current >= start && current < end) {
        p.style.backgroundColor = '#fff3cd'; // Highlight active cue
      } else {
        p.style.backgroundColor = 'transparent';
      }
    });
  });
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

- **WCAG 1.2.2**: Captions mandatory for all prerecorded video with audio.
- **Captions vs Subtitles**: Captions include sound effects (`[Music]`, `[Laughs]`) for deaf users; subtitles translate dialogue only.
- **`<track>` Element**: Use `kind="captions"`, `srclang="en"`, and `default`.
- **CORS Requirement**: External WebVTT files require `crossorigin="anonymous"` on `<video>`.
- **Autoplay Audio**: Audio playing > 3s must have pause controls (WCAG 1.4.2). Never autoplay unmuted sound.
- **Seizure Safety**: Never flash content > 3 times per second (WCAG 2.3.1).
- **Interactive Transcripts**: Provide full text transcripts for podcasts and video for maximum accessibility and SEO.

---

## 13. Further Reading

- [W3C WCAG 2.2: Guideline 1.2 Time-based Media](https://www.w3.org/WAI/WCAG22/Understanding/time-based-media.html)
- [W3C WAI: Making Audio and Video Media Accessible](https://www.w3.org/WAI/media/av/)
- [MDN Web Docs: Adding captions and subtitles to HTML5 video](https://developer.mozilla.org/en-US/docs/Web/Guide/Audio_and_video_delivery/Adding_captions_and_subtitles_to_HTML5_video)
- [WebAIM: Captions, Transcripts, and Audio Descriptions](https://webaim.org/techniques/captions/)

---

[Previous: 11 - Live Regions & Status Messages (11-live-regions-and-status-messages.md)](11-live-regions-and-status-messages.md) | [Section Index](README.md) | [Next: 13 - Contrast, Motion & User Preferences ->](13-contrast-motion-and-user-preferences.md)
