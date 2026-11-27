# Audio, Video, and Native Multimedia Elements

| Metadata | Details |
| :--- | :--- |
| **Specification** | WHATWG HTML Living Standard (§4.8.9 The `video` element, §4.8.10 The `audio` element) |
| **Prerequisites** | `01-fundamentals/01-document-structure.md`, `01-fundamentals/06-images-and-alt-text.md` |
| **Estimated Reading Time** | 26 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`<video>`, `<audio>`, `<source>`, `HTMLMediaElement` API) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, axe-core 4.9, NVDA 2024.1, VoiceOver macOS 14.4 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Production Multi-Codec Responsive Video with Poster](#example-1-production-multi-codec-responsive-video-with-poster)
   - [Example 2: Accessible Podcast Audio Player with Preload Control](#example-2-accessible-podcast-audio-player-with-preload-control)
   - [Example 3: Mobile-Optimized Inline Autoplaying Background Video](#example-3-mobile-optimized-inline-autoplaying-background-video)
   - [Example 4: Custom Accessible Media Controls with `HTMLMediaElement` API](#example-4-custom-accessible-media-controls-with-htmlmediaelement-api)
   - [Example 5: Audio/Video Error Recovery and Fallback Markup](#example-5-audiovideo-error-recovery-and-fallback-markup)
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

Before HTML5, embedding audio and video on the web was a fragile, proprietary nightmare that relied on third-party browser plugins like Adobe Flash, Apple QuickTime, and Microsoft Silverlight. These plugins were notorious security vectors, crashed mobile browsers, drained battery life, and were completely opaque to search engines and screen readers.

The introduction of the `<video>` and `<audio>` elements in HTML5 revolutionized the open web by establishing native, hardware-accelerated, scriptable multimedia playback directly inside the browser engine. 

Today, native media elements power everything from global streaming platforms (YouTube, Netflix) and podcast networks to interactive training portals and ambient UI backgrounds. Mastering media attributes (`controls`, `preload`, `poster`, `playsinline`), multi-codec negotiation via `<source>`, autoplay policies, and accessible custom controls ensures high performance, battery efficiency, and universal cross-device playback.

---

## 2. Core Concepts

### Core Attributes Matrix
Both `<video>` and `<audio>` inherit from the common `HTMLMediaElement` DOM interface and share primary attributes:
- **`controls`**: Boolean attribute rendering native browser media playback controls (play/pause, timeline scrubber, volume, fullscreen).
- **`autoplay`**: Boolean requesting immediate playback upon load. Subject to strict browser autoplay policies (generally blocked unless `muted`).
- **`muted`**: Boolean defaulting audio output to silent.
- **`loop`**: Boolean causing media to seek back to the beginning upon reaching the end.
- **`preload`**: Directs the browser how much data to pre-fetch before user interaction:
  - `none`: Fetches zero bytes until user clicks play. Essential for pages with dozens of podcast episodes or video galleries to conserve mobile bandwidth.
  - `metadata`: Default in most browsers. Fetches only header metadata (duration, dimensions, first frame, track list).
  - `auto`: Directs the browser to download the entire media file eagerly.
- **`poster`** (`<video>` only): URL of an image shown while video data is downloading or until the user hits play. Prevents black boxes before playback.
- **`playsinline`** (`<video>` only): Boolean critical for iOS Safari. Forces video to play inline within the document flow rather than hijacking the screen into native fullscreen playback.
- **`crossorigin`**: Configures CORS credential sharing (`anonymous` or `use-credentials`) when media or subtitle tracks are hosted on external CDNs.

### Multi-Codec Negotiation via `<source>`
No single media container or codec is universally supported across every legacy and modern browser. Browsers inspect child `<source>` tags from top to bottom and execute the **first** codec combination they support, ignoring subsequent sources.

| Format | Video Codec | Audio Codec | Browser Support | Typical Container |
| :--- | :--- | :--- | :--- | :--- |
| **AV1** | AV01 | Opus / AAC | Chrome, Firefox, Safari 17+ (M3 chips) | `.mp4`, `.webm` |
| **VP9** | VP9 | Opus / Vorbis | Chrome, Firefox, Edge, Safari 14+ | `.webm` |
| **H.264 / AVC** | avc1 | AAC / MP3 | 100% Universal Legacy & Modern | `.mp4` |
| **H.265 / HEVC** | hvc1 | AAC | Apple ecosystem, select Android/Windows | `.mp4` |

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                    Browser Media Loading & Playback Pipeline                            |
+-----------------------------------------------------------------------------------------+

  1. HTML Parser encounters <video> or <audio>
       |
       +--> Inspect child <source> elements in document tree order
               |
               v
  +---------------------------------------------------------------+
  | Codec Check: canPlayType("video/webm; codecs=vp9,opus")       |
  |   - Returns "probably", "maybe", or "" (unsupported)          |
  +---------------------------------------------------------------+
       |
       +--> First supported source selected! (Remaining ignored)
       |
  2. Evaluate preload attribute:
       |
       +-- preload="none"     --> Stop! Do not open network socket.
       +-- preload="metadata" --> HTTP GET with 'Range: bytes=0-102400' (Fetch moov/header)
       +-- preload="auto"     --> Eagerly stream and buffer media into memory
       |
  3. Video Rendering & Compositing:
       |
       +--> Hardware Decoder (GPU) unpacks compressed video frames
       +--> Frames pushed to dedicated GPU Compositor Layer
       +--> Sits directly in Render Tree at specified width/height
```

---

## 4. Syntax & Usage

```html
<video 
  controls 
  preload="metadata" 
  poster="/images/poster.webp" 
  width="1280" 
  height="720"
  playsinline
>
  <source src="/media/intro.webm" type="video/webm; codecs=vp9,opus" />
  <source src="/media/intro.mp4" type="video/mp4; codecs=avc1.42E01E,mp4a.40.2" />
  <p>Your browser does not support native HTML5 video. <a href="/media/intro.mp4">Download video</a>.</p>
</video>
```

> **Cross-Browser & Baseline Status Box**
>
> - `<video>` and `<audio>` elements: **Baseline Widely Available** since 2012 across all major desktop and mobile browsers.
> - `playsinline`: **Baseline Widely Available** (mandatory on iOS Safari since iOS 10 to suppress unwanted fullscreen takeover).
> - Modern AV1 codec: Supported natively across Chrome 70+, Firefox 67+, and Safari 17+ (hardware accelerated on Apple M3+).

---

## 5. Practical Examples

### Example 1: Production Multi-Codec Responsive Video with Poster
A robust, accessible video player offering next-gen WebM/VP9 with fallback to universal MP4/H.264, complete with intrinsic dimensions to avoid Cumulative Layout Shift (CLS).

```html
<figure class="video-container">
  <video 
    controls 
    preload="metadata" 
    poster="/assets/trailers/aurora-poster.jpg" 
    width="1920" 
    height="1080" 
    playsinline
    aria-labelledby="video-title"
  >
    <!-- Modern WebM container with VP9 codec (High compression) -->
    <source 
      src="/assets/trailers/aurora.webm" 
      type="video/webm; codecs=vp9,opus" 
    />

    <!-- Universal MP4 container with H.264 and AAC (Universal legacy compatibility) -->
    <source 
      src="/assets/trailers/aurora.mp4" 
      type="video/mp4; codecs=avc1.4D401F,mp4a.40.2" 
    />

    <!-- Captions track for accessibility -->
    <track 
      kind="subtitles" 
      src="/assets/trailers/aurora-en.vtt" 
      srclang="en" 
      label="English Subtitles" 
      default 
    />

    <!-- Graceful degradation fallback for non-supporting browsers -->
    <div class="media-fallback">
      <p>
        Your browser does not support HTML5 video. 
        You can <a href="/assets/trailers/aurora.mp4" download>download the MP4 video</a> directly.
      </p>
    </div>
  </video>

  <figcaption id="video-title">
    Timelapse observation of Aurora Borealis in Tromsø, Norway.
  </figcaption>
</figure>

<style>
  .video-container {
    max-width: 100%;
    margin: 2rem 0;
  }
  .video-container video {
    width: 100%;
    height: auto;
    aspect-ratio: 16 / 9;
    border-radius: 8px;
    background-color: #000;
  }
  figcaption {
    font-size: 0.9rem;
    color: #555;
    margin-top: 0.5rem;
    text-align: center;
  }
</style>
```

---

### Example 2: Accessible Podcast Audio Player with Preload Control
Audio player configuration tuned for long-form podcasts: uses `preload="none"` to save mobile data, coupled with fallback download links and clear metadata labelling.

```html
<div class="podcast-episode">
  <h3 id="ep-title">Episode 42: The Future of Web Platform Architecture</h3>
  <p class="episode-meta">Duration: 48 mins | Host: Alex Rivera | Guest: Dr. Elena Vance</p>

  <audio 
    controls 
    preload="none" 
    aria-labelledby="ep-title" 
    class="audio-player"
  >
    <!-- High-efficiency Opus stream -->
    <source src="/podcast/ep42.opus" type="audio/ogg; codecs=opus" />
    
    <!-- Standard MP3 stream for universal playback -->
    <source src="/podcast/ep42.mp3" type="audio/mpeg" />

    <!-- Accessible fallback text -->
    <p>
      Your browser does not support native audio playback. 
      <a href="/podcast/ep42.mp3" download>Download Episode 42 MP3 (45MB)</a> to listen offline.
    </p>
  </audio>

  <!-- Supplemental transcript link satisfying WCAG 1.2.1 -->
  <p class="transcript-link">
    <a href="/podcast/ep42-transcript.html">Read complete text transcript of Episode 42</a>
  </p>
</div>
```

---

### Example 3: Mobile-Optimized Inline Autoplaying Background Video
Hero background video configured strictly according to modern browser autoplay policies (must be `muted`, `playsinline`, and `loop`).

```html
<section class="hero-banner">
  <!-- Autoplay background: Muted, looped, inline, and accessible decorative handling -->
  <video 
    autoplay 
    muted 
    loop 
    playsinline 
    preload="auto" 
    poster="/assets/hero-poster.webp" 
    class="hero-video" 
    aria-hidden="true" 
    tabindex="-1"
  >
    <source src="/assets/hero-waves.webm" type="video/webm" />
    <source src="/assets/hero-waves.mp4" type="video/mp4" />
  </video>

  <div class="hero-overlay-content">
    <h1>Cloud Infrastructure at Global Scale</h1>
    <p>Deploy resilient compute clusters in under 60 seconds.</p>
    <a href="/signup" class="cta-button">Get Started</a>
  </div>
</section>

<style>
  .hero-banner {
    position: relative;
    width: 100%;
    min-height: 80vh;
    display: flex;
    align-items: center;
    justify-content: center;
    overflow: hidden;
    color: #fff;
  }
  .hero-video {
    position: absolute;
    top: 50%;
    left: 50%;
    min-width: 100%;
    min-height: 100%;
    width: auto;
    height: auto;
    transform: translate(-50%, -50%);
    object-fit: cover;
    z-index: 1;
  }
  .hero-overlay-content {
    position: relative;
    z-index: 2;
    text-align: center;
    background: rgba(0, 0, 0, 0.4);
    padding: 2rem;
    border-radius: 8px;
  }
</style>
```

---

### Example 4: Custom Accessible Media Controls with `HTMLMediaElement` API
When designing branded media controls, developers must replicate native keyboard and screen reader accessibility using ARIA attributes and focus management.

```html
<div class="custom-player" role="region" aria-label="Audio player">
  <audio id="media-element" preload="metadata">
    <source src="/assets/audio/ambient-sample.mp3" type="audio/mpeg" />
  </audio>

  <div class="controls-toolbar">
    <!-- Play/Pause Toggle -->
    <button type="button" id="play-pause-btn" aria-label="Play audio">
      <span aria-hidden="true">▶</span>
    </button>

    <!-- Scrubber Progress Bar -->
    <label for="seek-bar" class="visually-hidden">Audio timeline</label>
    <input 
      type="range" 
      id="seek-bar" 
      value="0" 
      min="0" 
      step="1" 
      aria-label="Seek time in seconds" 
    />

    <!-- Current / Total Time Readout -->
    <span id="time-display" aria-live="off">00:00 / 00:00</span>

    <!-- Volume / Mute Toggle -->
    <button type="button" id="mute-btn" aria-label="Mute audio">
      <span aria-hidden="true">🔊</span>
    </button>
  </div>
</div>

<script>
  const media = document.getElementById('media-element');
  const playBtn = document.getElementById('play-pause-btn');
  const seekBar = document.getElementById('seek-bar');
  const timeDisplay = document.getElementById('time-display');
  const muteBtn = document.getElementById('mute-btn');

  function formatTime(seconds) {
    const mins = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return `${String(mins).padStart(2, '0')}:${String(secs).padStart(2, '0')}`;
  }

  // Play / Pause Toggle
  playBtn.addEventListener('click', () => {
    if (media.paused || media.ended) {
      media.play();
      playBtn.setAttribute('aria-label', 'Pause audio');
      playBtn.firstElementChild.textContent = '⏸';
    } else {
      media.pause();
      playBtn.setAttribute('aria-label', 'Play audio');
      playBtn.firstElementChild.textContent = '▶';
    }
  });

  // Track Metadata Loaded -> Setup Scrubber
  media.addEventListener('loadedmetadata', () => {
    seekBar.max = Math.floor(media.duration);
    timeDisplay.textContent = `00:00 / ${formatTime(media.duration)}`;
  });

  // Update Time Display as Media Plays
  media.addEventListener('timeupdate', () => {
    seekBar.value = Math.floor(media.currentTime);
    timeDisplay.textContent = `${formatTime(media.currentTime)} / ${formatTime(media.duration)}`;
  });

  // User Seeks via Range Slider
  seekBar.addEventListener('input', () => {
    media.currentTime = seekBar.value;
  });

  // Mute / Unmute
  muteBtn.addEventListener('click', () => {
    media.muted = !media.muted;
    muteBtn.setAttribute('aria-label', media.muted ? 'Unmute audio' : 'Mute audio');
    muteBtn.firstElementChild.textContent = media.muted ? '🔇' : '🔊';
  });
</script>
```

---

### Example 5: Audio/Video Error Recovery and Fallback Markup
Demonstrating how to intercept network stalls, codec decode errors, and present actionable recovery links to end users.

```html
<div class="video-wrapper">
  <video id="resilient-video" controls width="640" height="360">
    <source src="/media/corrupted-sample.webm" type="video/webm" />
    <source src="/media/working-backup.mp4" type="video/mp4" />
  </video>

  <div id="media-error-panel" class="error-banner" hidden role="alert">
    <h4>Playback Error</h4>
    <p id="error-description">An error occurred while loading this video.</p>
    <button type="button" id="retry-media-btn">Retry Playback</button>
  </div>
</div>

<script>
  const vid = document.getElementById('resilient-video');
  const errorPanel = document.getElementById('media-error-panel');
  const errorDesc = document.getElementById('error-description');
  const retryBtn = document.getElementById('retry-media-btn');

  vid.addEventListener('error', (e) => {
    errorPanel.hidden = false;
    const mediaError = vid.error;

    if (!mediaError) return;

    switch (mediaError.code) {
      case MediaError.MEDIA_ERR_ABORTED:
        errorDesc.textContent = 'Playback was aborted by the client.';
        break;
      case MediaError.MEDIA_ERR_NETWORK:
        errorDesc.textContent = 'A network error caused the video download to fail.';
        break;
      case MediaError.MEDIA_ERR_DECODE:
        errorDesc.textContent = 'The video could not be decoded. The file may be corrupt.';
        break;
      case MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED:
        errorDesc.textContent = 'The video format or codec is not supported by your browser.';
        break;
    }
  });

  retryBtn.addEventListener('click', () => {
    errorPanel.hidden = true;
    vid.load(); // Reload media elements
    vid.play().catch(err => console.log('Autoplay blocked on retry:', err));
  });
</script>
```

---

## 6. Best Practices

- [ ] **Always declare `width` and `height` (or CSS `aspect-ratio`) on `<video>`**: Prevents severe Cumulative Layout Shift (CLS) when metadata loads and updates layout geometry.
- [ ] **Always set `preload="metadata"` or `preload="none"` on multi-media pages**: Never use `preload="auto"` across feeds, lists, or multiple video cards. It saturates user bandwidth and consumes excessive memory.
- [ ] **Pair `autoplay` with `muted` and `playsinline`**: Unmuted autoplay is blocked by default across all modern browser engines. On iOS Safari, `playsinline` is required to prevent unwanted full-screen modal hijacking.
- [ ] **Provide WebM (VP9/AV1) before MP4 (H.264)**: Place smaller, higher-compression modern formats first in tree order; legacy browsers will skip them and select MP4.
- [ ] **Include accessibility caption tracks (`<track kind="captions">`)**: WCAG 2.2 Criterion 1.2.2 requires synchronized captions for all pre-recorded video content with speech.
- [ ] **Always offer a text transcript alternative**: Provide a link to an HTML transcript for podcasts, lectures, and interviews to support Deaf, hard-of-hearing, and low-bandwidth users.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Omitting `muted` on an `autoplay` video | **Critical** | Browser autoplay policy blocks playback entirely; video stays frozen. | Add the `muted` attribute alongside `autoplay`. |
| Omitting `playsinline` on mobile video | **High** | Video automatically forces iOS Safari into full-screen mode upon playing. | Add `playsinline` to `<video>`. |
| Setting `preload="auto"` on long-form audio/video lists | **High** | Browser downloads megabytes of media simultaneously, exhausting user cellular data. | Set `preload="none"` or `preload="metadata"`. |
| Omitting `width` and `height` attributes on `<video>` | **High** | Causes Cumulative Layout Shift (CLS) when video metadata finishes downloading. | Specify explicit intrinsic dimensions or CSS `aspect-ratio: 16/9`. |
| Supplying codecs in the wrong order | **Moderate** | If MP4 is listed before WebM, modern browsers download the larger MP4 file unnecessarily. | Order `<source>` tags from most efficient (AV1 &rarr; VP9 &rarr; H.264). |
| Building custom video controls without keyboard access or ARIA labels | **Critical** | Violates WCAG 2.1.1; keyboard and screen reader users cannot operate playback. | Implement standard `<button>` tags with `aria-label` and `Space`/`Enter` listeners. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 1.2.1 Audio-only and Video-only (Prerecorded)**: Provide an equivalent text transcript or audio description.
- **Guideline 1.2.2 Captions (Prerecorded)**: Captions are provided for all prerecorded audio content in synchronized media via `<track kind="captions">`.
- **Guideline 1.4.2 Audio Control**: If any audio plays automatically for more than 3 seconds, provide a mechanism to pause, stop, or adjust volume independently from system volume.

### Performance & Core Web Vitals
- **Cumulative Layout Shift (CLS)**: Unsized `<video>` elements render as `300x150px` default boxes until headers arrive, causing massive layout shifts. Explicit attributes (`width="1280" height="720"`) allocate layout geometry in the initial layout pass.
- **Largest Contentful Paint (LCP)**: If a video poster is the LCP candidate, optimize the poster image (using AVIF/WebP) and consider adding `<link rel="preload" as="image" href="poster.webp" fetchpriority="high">`.

---

## 9. Comparison / When to Use What

| Media Format / Approach | Compression Efficiency | CPU / Decode Overhead | Browser Compatibility | When to Use |
| :--- | :--- | :--- | :--- | :--- |
| **Native `<video>` with multi-source** | High (AV1 / VP9) | Hardware Accelerated | 100% | Primary choice for all self-hosted video content. |
| **Native `<audio>`** | High (Opus / AAC) | Low | 100% | Podcasts, sound effects, voice recordings. |
| **Embedded `<iframe>` (YouTube/Vimeo)** | Managed by Host | Moderate to High (Third-party JS) | 100% | Offloading video CDN bandwidth, adaptive bitrate streaming (HLS/DASH) without custom infrastructure. |
| **Media Source Extensions (MSE)** | Ultra High (Dynamic Bitrate) | High (JS chunk streaming) | Modern Browsers | Enterprise streaming platforms building custom DASH/HLS players. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why will an unmuted <code>&lt;video autoplay&gt;</code> element fail to start automatically in modern browsers?</strong></summary>

Modern web browsers enforce strict Autoplay Policies to prevent abusive, noisy advertisements and conserve user mobile data. Browsers require either an explicit user gesture (such as clicking, tapping, or typing) before audio can play, or that the media element is explicitly muted (`muted` attribute or `video.muted = true`). If a video has audio and lacks the `muted` attribute, the browser's autoplay algorithm automatically rejects the playback promise.
</details>

<details>
<summary><strong>2. How does the browser determine which <code>&lt;source&gt;</code> element to play inside a <code>&lt;video&gt;</code> container?</strong></summary>

The browser evaluates child `<source>` elements in document tree order from top to bottom. For each `<source>`, it checks the `type` attribute (including optional `codecs` strings) against its internal decoder capabilities via `mediaElement.canPlayType(type)`. The browser selects the very first source that returns "probably" or "maybe" and begins fetching that file, ignoring all subsequent `<source>` elements.
</details>

<details>
<summary><strong>3. Why is the <code>playsinline</code> attribute mandatory for mobile video on iOS Safari?</strong></summary>

Historically on iOS devices, tapping play on any `<video>` element immediately forced the browser into native fullscreen playback, taking the user away from the webpage layout. Adding `playsinline` tells WebKit to render and play the video directly within the inline document layout (essential for background video, interactive tutorials, or compact players).
</details>

<details>
<summary><strong>4. What is the difference between <code>preload="none"</code>, <code>preload="metadata"</code>, and <code>preload="auto"</code>?</strong></summary>

- `preload="none"`: The browser initiates zero network connections until the user explicitly requests playback. Best for saving bandwidth on pages with many media items.
- `preload="metadata"`: The browser sends a HTTP byte-range request to fetch only media container headers (duration, intrinsic dimensions, first frame).
- `preload="auto"`: The browser actively buffers the entire media file in anticipation of immediate user playback.
</details>

<details>
<summary><strong>5. How can un-styled or unsized <code>&lt;video&gt;</code> elements negatively impact Core Web Vitals?</strong></summary>

By default, the browser engine assigns an arbitrary default intrinsic size of `300x150` pixels to `<video>` elements before metadata is fetched. When the video metadata finally loads and reveals the true dimensions (e.g., 1920x1080), the element expands abruptly, pushing all surrounding content down. This causes severe Cumulative Layout Shift (CLS). Providing `width`, `height`, or CSS `aspect-ratio` reserves layout space upfront.
</details>

<details>
<summary><strong>6. What is the difference between the <code>ended</code>, <code>pause</code>, and <code>waiting</code> events on <code>HTMLMediaElement</code>?</strong></summary>

- `pause`: Fires whenever playback is halted, either by user interaction, script command, or when reaching the end.
- `ended`: Fires specifically when playback reaches the end of the media duration (if `loop` is not active).
- `waiting`: Fires when playback has stopped because the media buffer is temporarily exhausted and the player is waiting for subsequent bytes to download.
</details>

<details>
<summary><strong>7. How do you handle Cross-Origin Resource Sharing (CORS) when loading subtitle tracks from an external CDN?</strong></summary>

Subtitle files loaded via `<track>` are subject to strict CORS security restrictions. If subtitles reside on an external CDN, the server must return `Access-Control-Allow-Origin: *` (or matching domain), and the parent `<video>` element must declare the `crossorigin="anonymous"` attribute. Without this, the browser will refuse to parse and render WebVTT text cues.
</details>

<details>
<summary><strong>8. What is the purpose of the <code>poster</code> attribute on a <code>&lt;video&gt;</code> element?</strong></summary>

The `poster` attribute specifies the URL of an image to be displayed while the video is downloading or until the user initiates playback. Without a poster or preloaded frame, the video player often displays an unsightly solid black or transparent box, harming visual UX.
</details>

---

## 11. Practice Exercises

### Exercise 1: Multi-Format Video Player (Easy)
- **Task**: Author an HTML snippet containing a `<video>` with native controls, a poster image, explicit intrinsic dimensions (`1280x720`), and two `<source>` formats: WebM and MP4.

<details>
<summary>View Solution</summary>

```html
<video 
  controls 
  preload="metadata" 
  poster="/images/nature-poster.jpg" 
  width="1280" 
  height="720"
  playsinline
>
  <source src="/media/nature.webm" type="video/webm; codecs=vp9,opus" />
  <source src="/media/nature.mp4" type="video/mp4; codecs=avc1.42E01E,mp4a.40.2" />
  <p>Your browser does not support HTML5 video. <a href="/media/nature.mp4">Download MP4</a>.</p>
</video>
```
</details>

---

### Exercise 2: Bandwidth-Conserving Audio Playlist Item (Medium)
- **Task**: Create an audio player for a song in a playlist that downloads zero bytes before user interaction (`preload="none"`), provides fallback MP3/AAC sources, and links to an accessible lyric transcript.

<details>
<summary>View Solution</summary>

```html
<article class="track-item">
  <h4 id="song-01">Track 1: Electric Sunrise</h4>
  <p>Artist: Neon Waves | Album: Synthopia</p>

  <audio controls preload="none" aria-labelledby="song-01">
    <source src="/music/track01.m4a" type="audio/mp4; codecs=mp4a.40.2" />
    <source src="/music/track01.mp3" type="audio/mpeg" />
    <p>Audio playback unsupported. <a href="/music/track01.mp3" download>Download track</a>.</p>
  </audio>

  <p><a href="/lyrics/track01.html">Read Song Lyrics & Details</a></p>
</article>
```
</details>

---

### Exercise 3: Accessible Custom Video Player with Scrubbing & Time Display (Hard)
- **Task**: Build a custom video player using HTML, CSS, and JavaScript. Implement Play/Pause toggle with ARIA status updates, an interactive progress slider, current time readout, and keyboard accessibility (`Space` to toggle playback).

<details>
<summary>View Solution</summary>

```html
<div class="custom-video-box" role="region" aria-label="Product Showcase Video">
  <video id="demo-vid" width="800" height="450" preload="metadata" playsinline>
    <source src="/media/showcase.mp4" type="video/mp4" />
  </video>

  <div class="toolbar">
    <button type="button" id="ctrl-play" aria-label="Play video">Play</button>
    
    <label for="ctrl-seek" class="sr-only">Seek position</label>
    <input type="range" id="ctrl-seek" value="0" min="0" step="0.1" />

    <span id="ctrl-time" aria-live="off">00:00 / 00:00</span>
  </div>
</div>

<style>
  .sr-only {
    position: absolute;
    width: 1px;
    height: 1px;
    padding: 0;
    margin: -1px;
    overflow: hidden;
    clip: rect(0,0,0,0);
    border: 0;
  }
  .custom-video-box {
    max-width: 800px;
    background: #111;
    color: #fff;
    border-radius: 8px;
    overflow: hidden;
  }
  .custom-video-box video {
    display: block;
    width: 100%;
    height: auto;
  }
  .toolbar {
    display: flex;
    align-items: center;
    gap: 1rem;
    padding: 0.5rem 1rem;
    background: #222;
  }
</style>

<script>
  const vid = document.getElementById('demo-vid');
  const playBtn = document.getElementById('ctrl-play');
  const seek = document.getElementById('ctrl-seek');
  const time = document.getElementById('ctrl-time');

  function fmt(s) {
    const m = Math.floor(s / 60);
    const sec = Math.floor(s % 60);
    return `${String(m).padStart(2,'0')}:${String(sec).padStart(2,'0')}`;
  }

  function togglePlay() {
    if (vid.paused || vid.ended) {
      vid.play();
      playBtn.textContent = 'Pause';
      playBtn.setAttribute('aria-label', 'Pause video');
    } else {
      vid.pause();
      playBtn.textContent = 'Play';
      playBtn.setAttribute('aria-label', 'Play video');
    }
  }

  playBtn.addEventListener('click', togglePlay);
  
  // Spacebar toggle when video or controls are focused
  window.addEventListener('keydown', (e) => {
    if (e.code === 'Space' && (document.activeElement === playBtn || document.activeElement === vid)) {
      e.preventDefault();
      togglePlay();
    }
  });

  vid.addEventListener('loadedmetadata', () => {
    seek.max = vid.duration;
    time.textContent = `00:00 / ${fmt(vid.duration)}`;
  });

  vid.addEventListener('timeupdate', () => {
    seek.value = vid.currentTime;
    time.textContent = `${fmt(vid.currentTime)} / ${fmt(vid.duration)}`;
  });

  seek.addEventListener('input', () => {
    vid.currentTime = seek.value;
  });
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 HTML MEDIA QUICK REFERENCE                                      |
+-------------------------------------------------------------------------------------------------+
| Element / Attribute      | Purpose / Behavior                                                   |
+-------------------------------------------------------------------------------------------------+
| <video> / <audio>        | Native hardware-accelerated media containers                         |
| controls                 | Displays native browser playback controls                            |
| preload="none|meta|auto" | Manages data buffering prior to user interaction                     |
| poster="img.webp"        | Splash preview image shown before video starts                       |
| playsinline              | Mandates inline playback on iOS Safari (prevents fullscreen modal)   |
| muted + autoplay         | Required combination for allowed native background autoplay          |
| <source type="...codecs">| Negotiates optimal compression formats (order: AV1 -> VP9 -> H.264) |
| <track kind="captions">  | Synchronized accessibility captions (WCAG 1.2.2 requirement)         |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [WHATWG HTML Living Standard: Media elements](https://html.spec.whatwg.org/multipage/media.html#media-elements)
- [MDN Web Docs: Video and audio content](https://developer.mozilla.org/en-US/docs/Learn/HTML/Multimedia_and_embedding/Video_and_audio_content)
- [MDN Web Docs: Autoplay guide for media and Web Audio APIs](https://developer.mozilla.org/en-US/docs/Web/Media/Autoplay_guide)
- [W3C Web Content Accessibility Guidelines (WCAG) 2.2: Time-based Media](https://www.w3.org/WAI/WCAG22/quickref/?showtechniques=121%2C122#time-based-media)

---

[← Previous: Module Overview](README.md) | [Section Index](README.md) | [Next: Responsive Images in Depth →](02-responsive-images-in-depth.md)
