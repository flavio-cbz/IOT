(function () {
  const body = document.body;
  const stationId = body.dataset.stationId || "adi_4";
  
  // Elements fetched dynamically to survive HTMX swaps
  const getViewport = () => document.getElementById("plant-3d");
  const getTimelineCanvas = () => document.getElementById("health-timeline");
  const getTooltip = () => document.getElementById("timeline-tooltip");
  const getTimelineAlert = () => document.getElementById("timeline-alert");
  const getTimelineDetail = () => document.getElementById("timeline-detail");
  const getTimelineTitle = () => document.getElementById("timeline-title");
  const getTimelineRange = () => document.getElementById("timeline-range");

  let lastReading = parseReading(getViewport()?.dataset.reading);
  let readings = [];
  let timelineHits = [];
  let three = null;
  const historyWindows = {
    "10m": { label: "10 min", ms: 10 * 60 * 1000 },
    "1h": { label: "1 h", ms: 60 * 60 * 1000 },
    "4h": { label: "4 h", ms: 4 * 60 * 60 * 1000 },
    "1d": { label: "1 j", ms: 24 * 60 * 60 * 1000 },
  };
  let selectedWindow = "4h"; // Initialized later using the DOM element

  const problemMeta = {
    0: { color: "#34d399", label: "Parfait", text: "Je me sens parfaitement bien" },
    1: { color: "#fb923c", label: "Soif", text: "J'ai soif !" },
    2: { color: "#38bdf8", label: "Trop d'eau", text: "Je me noie !" },
    3: { color: "#a78bfa", label: "Sombre", text: "Je suis dans le noir..." },
    4: { color: "#67e8f9", label: "Froid", text: "J'ai froid !" },
    5: { color: "#fb7185", label: "Chaud", text: "J'ai trop chaud !" },
  };

  function parseReading(value) {
    if (!value) return null;
    try {
      const reading = JSON.parse(value);
      // Correction robuste: Si le backend envoie une date naïve, on force en UTC
      if (reading.timestamp && !reading.timestamp.includes('+') && !reading.timestamp.endsWith('Z')) {
        reading.timestamp += 'Z';
      }
      // Enregistrer l'heure locale exacte de réception pour ignorer le décalage d'horloge serveur/navigateur
      reading._localReceivedAt = Date.now(); 
      return reading;
    } catch (_error) {
      return null;
    }
  }

  function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  function uniq(items) {
    return [...new Set(items)];
  }

  function detectReadingEvents(reading) {
    const temperature = Number(reading?.temperature ?? NaN);
    const soil = Number(reading?.soil_humidity ?? NaN);
    const light = Number(reading?.light_level ?? NaN);
    const events = [];

    if (Number.isFinite(soil) && soil < 40) {
      events.push({ id: "soif", label: "Soif", severity: soil < 30 ? "critical" : "warn" });
    }
    if (Number.isFinite(soil) && soil > 70) {
      events.push({ id: "trop-eau", label: "Trop d'eau", severity: soil > 85 ? "critical" : "warn" });
    }
    if (Number.isFinite(light) && light < 200) {
      events.push({ id: "sombre", label: "Sombre", severity: light < 50 ? "critical" : "warn" });
    }
    if (Number.isFinite(temperature) && temperature < 18) {
      events.push({ id: "froid", label: "Froid", severity: temperature < 15 ? "critical" : "warn" });
    }
    if (Number.isFinite(temperature) && temperature > 26) {
      events.push({ id: "chaud", label: "Chaud", severity: temperature > 30 ? "critical" : "warn" });
    }
    return events;
  }

  function timelineStatusForReading(reading) {
    const events = detectReadingEvents(reading);
    if (!events.length) {
      return { rank: 0, label: "Stable", color: "#34d399", events: [] };
    }
    const hasCritical = events.some((event) => event.severity === "critical");
    return {
      rank: hasCritical ? 2 : 1,
      label: hasCritical ? "Critique" : "Alerte",
      color: hasCritical ? "#fb7185" : "#f59e0b",
      events,
    };
  }

  function updateTimelineTitle() {
    const timelineTitle = getTimelineTitle();
    if (!timelineTitle) return;
    const windowLabel = historyWindows[selectedWindow]?.label || "4 h";
    timelineTitle.textContent = `Journal de ma plante - ${windowLabel}`;
  }

  function selectedWindowMs() {
    return historyWindows[selectedWindow]?.ms || historyWindows["4h"].ms;
  }

  function formatWindowLeftLabel() {
    if (selectedWindow === "10m") return "-10 min";
    if (selectedWindow === "1h") return "-1h";
    if (selectedWindow === "1d") return "-1j";
    return "-4h";
  }

  function ageSeconds(reading) {
    if (!reading?.timestamp) return Infinity;
    if (reading._localReceivedAt) {
      return Math.max(0, Math.floor((Date.now() - reading._localReceivedAt) / 1000));
    }
    return Math.max(0, Math.floor((Date.now() - new Date(reading.timestamp).getTime()) / 1000));
  }

  function stationFreshness(reading) {
    const age = ageSeconds(reading);
    if (!Number.isFinite(age)) return { status: "offline", label: "Station inactive", detail: "Aucune lecture" };
    if (age <= 30) return { status: "active", label: "Station active", detail: "Lecture reçue à l'instant" };
    if (age <= 120) return { status: "stale", label: "Station en retard", detail: "Dernière lecture il y a moins de 2 min" };
    return { status: "offline", label: "Station inactive", detail: `Dernière lecture il y a ${Math.max(2, Math.floor(age / 60))} min` };
  }

  function setStationStatus(reading) {
    const node = document.getElementById("station-status");
    if (!node) return;
    const freshness = stationFreshness(reading);
    node.classList.remove("station-active", "station-stale", "station-offline");
    node.classList.add(`station-${freshness.status}`);
    node.querySelector("strong").textContent = freshness.label;
    node.querySelector("small").textContent = freshness.detail;
  }


    function initThree() {
    const viewport = getViewport();
    if (!viewport) return;
    viewport.innerHTML = buildPlantSVG();
    three = { svg: viewport.querySelector('svg'), state: 0 };
    if (lastReading) updatePlant(lastReading);
    else applyPalette(paletteForProblem(0));
  }

  function buildPlantSVG() {
    return `<svg id="plant-svg" viewBox="0 0 400 460" xmlns="http://www.w3.org/2000/svg" style="width:100%;height:100%;display:block;--data-droop:0deg;--data-shiver:0;--data-light:1;--data-bloom:1" data-state="0">
<defs>
<style>
/* Base Animations & Layout */
.sky { fill: url(#skyGrad); }
.sun-glow { fill: var(--sunC, #fcd34d); opacity: calc(0.22 * var(--data-light)); }
.sun { fill: var(--sunC, #fcd34d); opacity: var(--data-light); }
.ray { stroke: var(--sunC, #fcd34d); stroke-width: 2.5; stroke-linecap: round; opacity: calc(0.65 * var(--data-light)); }

/* Terrain */
.terrain { fill: url(#terrainGrad); }
.terrain-top { fill: var(--terrainTopC, #1f2937); }
.terrain-edge { fill: var(--terrainEdgeC, #111827); }
.grass { stroke: var(--grassC, #10b981); stroke-width: 1.8; stroke-linecap: round; fill: none; opacity: 0.8; }
.grass-tuft { fill: var(--grassC, #10b981); opacity: 0.7; }
.pebble { fill: var(--pebbleC, #374151); opacity: 0.6; }
.crack { stroke: var(--crackC, #92400e); stroke-width: 1.4; fill: none; stroke-linecap: round; opacity: 0; transition: opacity 0.8s ease; }
.dust { fill: var(--dustC, #d97706); opacity: 0; transition: opacity 0.8s ease; }
.puddle { fill: url(#puddleGrad); opacity: 0; transition: opacity 0.8s ease; }
.root { stroke: var(--rootC, #4b5563); stroke-width: 2.5; fill: none; stroke-linecap: round; opacity: 0.4; }

/* Plant Components */
.stem { fill: none; stroke: url(#stemGrad); stroke-linecap: round; stroke-linejoin: round; stroke-width: 7; filter: drop-shadow(0 2px 8px rgba(0,0,0,0.4)); }
.branch { fill: none; stroke: url(#stemGrad); stroke-linecap: round; stroke-linejoin: round; stroke-width: 4; filter: drop-shadow(0 2px 4px rgba(0,0,0,0.3)); }
.leaf { fill: url(#leafGrad); filter: drop-shadow(0 4px 6px rgba(0,0,0,0.3)); }
.leaf.alt { fill: url(#leafGradAlt); }

.flower-bud { fill: url(#budGrad); }
.flower-center { fill: var(--centerC, #fbbf24); filter: url(#neonGlow); }

/* Weather / Effects */
.bg-hill { fill: var(--hillC, #0f172a); opacity: 0.6; }
.bg-hill2 { fill: var(--hill2C, #020617); opacity: 0.5; }
.cloud { fill: white; opacity: 0.04; }
.pt { fill: var(--lc1, #34d399); opacity: 0.6; filter: url(#neonGlow); }
.frost-crystal { stroke: #bae6fd; stroke-width: 1.8; fill: none; stroke-linecap: round; opacity: 0; transition: opacity 0.8s ease; filter: drop-shadow(0 0 4px #bae6fd); }
.raindrop { stroke: #38bdf8; stroke-width: 2; stroke-linecap: round; fill: none; display: none; }
.hwave { stroke: var(--hwaveC, #fb923c); stroke-width: 2; fill: none; stroke-linecap: round; display: none; filter: blur(1px); }
.moon { fill: #e2e8f0; opacity: 0; transition: opacity 0.8s ease; filter: drop-shadow(0 0 12px #e2e8f0); }
.moon-shadow { fill: var(--skyB, #03060a); opacity: 0; transition: opacity 0.8s ease; }

/* Keyframes */
@keyframes sway { 0%, 100% { transform: rotate(calc(var(--data-droop) - 1.5deg)) } 50% { transform: rotate(calc(var(--data-droop) + 1.5deg)) } }
@keyframes shiver { 0%, 100% { transform: translateX(0) } 25% { transform: translateX(calc(var(--data-shiver) * -1px)) } 75% { transform: translateX(calc(var(--data-shiver) * 1px)) } }
@keyframes pulse { 0%, 100% { transform: scale(1); opacity: 0.7; } 50% { transform: scale(1.08); opacity: 0.9; } }
@keyframes ray-pulse { 0%, 100% { opacity: 0.3; } 50% { opacity: 0.7; } }
@keyframes float-up { 0% { transform: translateY(0); opacity: 0.8; } 100% { transform: translateY(-100px); opacity: 0; } }
@keyframes rain { 0% { transform: translateY(-30px); opacity: 0.8; } 100% { transform: translateY(160px); opacity: 0; } }
@keyframes hwave-anim { 0% { transform: translateY(0) scaleX(1); opacity: 0.6; } 100% { transform: translateY(-60px) scaleX(1.4); opacity: 0; } }
@keyframes dust-drift { 0% { transform: translateX(0); opacity: 0.5; } 100% { transform: translateX(80px); opacity: 0; } }
@keyframes float-up-down { 0%, 100% { transform: translateY(0); } 50% { transform: translateY(-6px); } }
@keyframes bloom-pulse { 0%, 100% { transform: scale(var(--data-bloom)); } 50% { transform: scale(calc(var(--data-bloom) * 1.05)); } }
@keyframes petal-drop { 0% { transform: translate(0,0) rotate(0deg); opacity: 1; } 100% { transform: translate(30px, 180px) rotate(120deg); opacity: 0; } }

/* Groups & Base Transforms */
.plant-group { transform-origin: 200px 370px; animation: sway 5s cubic-bezier(0.4, 0, 0.2, 1) infinite; transition: filter 1s; }
.sun-rays { animation: ray-pulse 4s ease-in-out infinite; transform-origin: 335px 65px; }
.sun-glow { animation: pulse 4.5s ease-in-out infinite; transform-origin: 335px 65px; transform-box: fill-box; }
.pt { animation: float-up linear infinite; }
.pt1 { animation-duration: 4s; animation-delay: 0s; } .pt2 { animation-duration: 5s; animation-delay: 1.2s; } .pt3 { animation-duration: 4.5s; animation-delay: 2.5s; } .pt4 { animation-duration: 6s; animation-delay: 0.8s; } .pt5 { animation-duration: 3.8s; animation-delay: 1.8s; }
.raindrop { animation: rain linear infinite; }
.rd1 { animation-duration: 1.1s; animation-delay: 0s; } .rd2 { animation-duration: 1.4s; animation-delay: 0.2s; } .rd3 { animation-duration: 1s; animation-delay: 0.5s; } .rd4 { animation-duration: 1.5s; animation-delay: 0.1s; } .rd5 { animation-duration: 1.2s; animation-delay: 0.7s; }
.hwave { animation: hwave-anim ease-out infinite; }
.hw1 { animation-duration: 2.5s; animation-delay: 0s; } .hw2 { animation-duration: 2.8s; animation-delay: 0.9s; } .hw3 { animation-duration: 2.2s; animation-delay: 1.6s; }
.flower { animation: bloom-pulse 4s ease-in-out infinite; transform-origin: 200px 115px; transition: filter 1s; }
.speech-bubble { animation: float-up-down 4.5s ease-in-out infinite; transform-origin: 200px 60px; transition: opacity 0.6s, transform 0.6s; }

/* Continuous bindings overriding states where necessary */
.plant-group.is-shivering { animation: shiver 0.25s infinite; }

/* State Specifics (0: OK, 1: Soif, 2: Trop eau, 3: Sombre, 4: Froid, 5: Chaud) */
[data-state="1"] .crack { opacity: 1; }
[data-state="1"] .dust { opacity: 1; animation: dust-drift 3.5s ease-out infinite; }
[data-state="1"] .p1 { animation: petal-drop 4.5s ease-in infinite; animation-delay: 0.2s; transform-origin: 200px 115px; }
[data-state="1"] .p3 { animation: petal-drop 5s ease-in infinite; animation-delay: 1.8s; transform-origin: 200px 115px; }
[data-state="2"] .raindrop { display: block; }
[data-state="2"] .puddle { opacity: 0.7; }
[data-state="3"] .sun-group { opacity: 0; }
[data-state="3"] .moon { opacity: 1; }
[data-state="3"] .moon-shadow { opacity: 1; }
[data-state="4"] .frost-crystal { opacity: 1; }
[data-state="5"] .hwave { display: block; }

/* Glassmorphism Bubble */
.glass-panel { fill: rgba(15, 23, 42, 0.5); stroke: rgba(255, 255, 255, 0.2); stroke-width: 1.5; filter: drop-shadow(0 12px 24px rgba(0,0,0,0.5)); backdrop-filter: blur(12px); }
.speech-text { font-family: 'Inter', system-ui, sans-serif; font-weight: 700; font-size: 14px; fill: #f8fafc; text-anchor: middle; filter: drop-shadow(0 2px 4px rgba(0,0,0,0.8)); }
</style>

<linearGradient id="skyGrad" x1="0" y1="0" x2="0" y2="1">
  <stop offset="0%" stop-color="var(--skyT, #060e18)"/>
  <stop offset="100%" stop-color="var(--skyB, #02060c)"/>
</linearGradient>
<linearGradient id="terrainGrad" x1="0" y1="0" x2="0" y2="1">
  <stop offset="0%" stop-color="var(--terrainTopC, #1e293b)"/>
  <stop offset="100%" stop-color="var(--terrainC, #0f172a)"/>
</linearGradient>
<linearGradient id="stemGrad" x1="0" y1="1" x2="0" y2="0">
  <stop offset="0%" stop-color="var(--sc1, #047857)"/>
  <stop offset="100%" stop-color="var(--sc2, #10b981)"/>
</linearGradient>
<linearGradient id="leafGrad" x1="0" y1="0" x2="1" y2="1">
  <stop offset="0%" stop-color="var(--lc1, #34d399)"/>
  <stop offset="100%" stop-color="var(--lc2, #059669)"/>
</linearGradient>
<linearGradient id="leafGradAlt" x1="0" y1="0" x2="1" y2="1">
  <stop offset="0%" stop-color="var(--lc2, #059669)"/>
  <stop offset="100%" stop-color="var(--lc1, #34d399)"/>
</linearGradient>
<linearGradient id="budGrad" x1="0" y1="1" x2="0" y2="0">
  <stop offset="0%" stop-color="var(--bud1, #e11d48)"/>
  <stop offset="100%" stop-color="var(--bud2, #fb7185)"/>
</linearGradient>
<radialGradient id="puddleGrad" cx="50%" cy="50%" r="50%">
  <stop offset="0%" stop-color="var(--puddleC, #0ea5e9)" stop-opacity="0.8"/>
  <stop offset="100%" stop-color="var(--puddleC, #0ea5e9)" stop-opacity="0"/>
</radialGradient>

<filter id="neonGlow" x="-50%" y="-50%" width="200%" height="200%">
  <feGaussianBlur in="SourceGraphic" stdDeviation="3" result="blur1" />
  <feGaussianBlur in="SourceGraphic" stdDeviation="8" result="blur2" />
  <feMerge>
    <feMergeNode in="blur2" />
    <feMergeNode in="blur1" />
    <feMergeNode in="SourceGraphic" />
  </feMerge>
</filter>
<filter id="glassBlur">
  <feGaussianBlur in="SourceGraphic" stdDeviation="4"/>
</filter>

<clipPath id="scene-clip"><rect x="0" y="0" width="400" height="460" rx="16"/></clipPath>
</defs>

<g clip-path="url(#scene-clip)">
  <rect class="sky" x="0" y="0" width="400" height="460"/>
  
  <!-- Stars / Particles -->
  <g opacity="0.4">
    <circle cx="45" cy="30" r="1.5" fill="white" opacity="0.6"/><circle cx="120" cy="18" r="1" fill="white" opacity="0.45"/>
    <circle cx="210" cy="10" r="1.5" fill="white" opacity="0.65"/><circle cx="290" cy="25" r="1" fill="white" opacity="0.4"/>
    <circle cx="365" cy="14" r="1.5" fill="white" opacity="0.55"/><circle cx="82" cy="72" r="1" fill="white" opacity="0.35"/>
    <circle cx="175" cy="48" r="1" fill="white" opacity="0.35"/><circle cx="320" cy="60" r="1" fill="white" opacity="0.4"/>
  </g>
  
  <path class="bg-hill2" d="M-10 320 Q60 250 140 285 Q200 310 260 270 Q330 220 410 290 L410 460 L-10 460Z"/>
  <path class="bg-hill" d="M-10 340 Q80 290 160 315 Q220 330 280 295 Q350 250 410 320 L410 460 L-10 460Z"/>
  
  <g class="sun-group">
    <circle cx="335" cy="65" r="55" class="sun-glow" filter="url(#neonGlow)"/>
    <g class="sun-rays">
      <line x1="335" y1="18" x2="335" y2="8" class="ray"/><line x1="372" y1="28" x2="382" y2="18" class="ray"/>
      <line x1="386" y1="65" x2="398" y2="65" class="ray"/><line x1="372" y1="102" x2="382" y2="112" class="ray"/>
      <line x1="335" y1="112" x2="335" y2="122" class="ray"/><line x1="298" y1="102" x2="288" y2="112" class="ray"/>
      <line x1="284" y1="65" x2="272" y2="65" class="ray"/><line x1="298" y1="28" x2="288" y2="18" class="ray"/>
    </g>
    <circle cx="335" cy="65" r="28" class="sun" filter="url(#neonGlow)"/>
  </g>
  <circle cx="335" cy="65" r="26" class="moon"/><circle cx="349" cy="58" r="22" class="moon-shadow"/>
  
  <g>
    <line class="raindrop rd1" x1="80" y1="20" x2="72" y2="60"/><line class="raindrop rd2" x1="150" y1="5" x2="142" y2="45"/>
    <line class="raindrop rd3" x1="220" y1="15" x2="212" y2="55"/><line class="raindrop rd4" x1="280" y1="0" x2="272" y2="40"/>
    <line class="raindrop rd5" x1="340" y1="25" x2="332" y2="65"/>
  </g>
  
  <path class="hwave hw1" d="M120,355 Q160,330 200,355 Q240,380 280,355"/>
  <path class="hwave hw2" d="M90,370 Q140,340 190,370 Q240,400 290,370"/>
  <path class="hwave hw3" d="M160,345 Q180,330 200,345 Q220,360 240,345"/>
  
  <path class="terrain-edge" d="M-10 370 Q50 348 100 362 Q150 374 200 355 Q250 338 300 358 Q350 375 410 350 L410 460 L-10 460Z"/>
  <path class="terrain" d="M-10 375 Q50 355 100 368 Q150 380 200 362 Q250 345 300 364 Q350 380 410 358 L410 460 L-10 460Z"/>
  <path class="terrain-top" d="M-10 375 Q50 355 100 368 Q150 380 200 362 Q250 345 300 364 Q350 380 410 358 L410 365 Q350 388 300 371 Q250 352 200 369 Q150 388 100 375 Q50 362 -10 382Z"/>
  
  <g class="crack">
    <path d="M80 385 Q90 395 88 415"/><path d="M155 378 Q145 390 162 408 Q150 415 153 425"/>
    <path d="M260 382 Q245 395 258 412"/><path d="M330 388 Q320 405 326 415"/>
  </g>
  
  <g class="dust">
    <circle cx="90" cy="365" r="3.5"/><circle cx="310" cy="360" r="2.5"/><circle cx="140" cy="358" r="2"/><circle cx="350" cy="368" r="4"/>
  </g>
  <g class="puddle">
    <ellipse cx="120" cy="390" rx="45" ry="8"/><ellipse cx="280" cy="385" rx="55" ry="10"/><ellipse cx="200" cy="395" rx="35" ry="6"/>
  </g>
  
  <g class="frost-crystal">
    <path d="M85 378 L97 366 M91 378 L91 364 M85 372 L97 372"/><path d="M305 375 L317 363 M311 375 L311 361 M305 369 L317 369"/>
    <path d="M180 373 L190 363 M185 373 L185 361 M180 367 L190 367"/><path d="M240 380 L250 370 M245 380 L245 368 M240 374 L250 374"/>
  </g>
  
  <g class="grass">
    <path d="M45 372 Q48 350 54 338"/><path d="M55 375 Q52 355 58 345"/>
    <path d="M340 365 Q344 345 348 335"/><path d="M355 368 Q350 350 358 340"/>
    <path d="M70 378 Q66 360 74 348"/><path d="M370 372 Q375 352 370 342"/>
  </g>
  <g class="grass-tuft">
    <ellipse cx="48" cy="374" rx="12" ry="4"/><ellipse cx="345" cy="367" rx="14" ry="4"/>
    <ellipse cx="375" cy="373" rx="10" ry="3.5"/><ellipse cx="30" cy="380" rx="10" ry="3.5"/>
  </g>
  <circle class="pebble" cx="62" cy="400" r="4.5"/><circle class="pebble" cx="335" cy="395" r="5.5"/>
  <circle class="pebble" cx="370" cy="410" r="4"/><circle class="pebble" cx="40" cy="420" r="3.5"/>
  
  <g class="root">
    <path d="M200 370 Q180 385 160 400 Q140 415 125 418"/><path d="M200 370 Q220 390 245 405 Q265 415 285 415"/>
    <path d="M200 370 Q190 395 185 420 Q182 435 188 445"/>
  </g>
  
  <!-- Plant Body -->
  <g class="plant-group">
    <path class="stem" d="M200 370 C195 330 190 280 200 230 C208 190 195 155 200 120"/>
    
    <path class="branch" d="M200 280 C170 265 140 255 110 245"/>
    <path class="branch" d="M198 250 C230 235 260 225 295 215"/>
    <path class="branch" d="M202 215 C175 200 145 190 115 185"/>
    <path class="branch" d="M199 180 C225 170 255 160 280 155"/>
    
    <!-- Leaves designed with elegant cubic bezier curves -->
    <path class="leaf" d="M110 245 C90 230 80 250 90 265 C100 275 120 260 110 245Z"/>
    <path class="leaf alt" d="M130 252 C115 240 105 260 115 270 C125 280 140 265 130 252Z"/>
    
    <path class="leaf" d="M295 215 C315 200 330 220 315 235 C300 245 280 230 295 215Z"/>
    <path class="leaf alt" d="M275 220 C290 205 300 225 290 240 C280 250 260 235 275 220Z"/>
    
    <path class="leaf alt" d="M115 185 C95 170 85 190 95 205 C105 215 125 200 115 185Z"/>
    <path class="leaf" d="M135 190 C120 180 110 200 120 210 C130 220 145 205 135 190Z"/>
    
    <path class="leaf" d="M280 155 C300 140 310 160 300 175 C290 185 270 170 280 155Z"/>
    <path class="leaf alt" d="M260 160 C275 145 285 165 275 180 C265 190 245 175 260 160Z"/>
    
    <!-- Top small leaves -->
    <path class="leaf" d="M200 160 C185 150 175 165 185 175 C195 180 205 170 200 160Z"/>
    <path class="leaf alt" d="M198 140 C185 130 175 145 185 155 C195 160 205 150 198 140Z"/>
    <path class="leaf" d="M202 145 C215 135 225 150 215 160 C205 165 195 155 202 145Z"/>

    <!-- Flower -->
    <g class="flower">
      <!-- Outer glowing halo -->
      <circle cx="200" cy="115" r="42" fill="url(#budGrad)" opacity="0.15" filter="url(#neonGlow)"/>
      
      <!-- Petals using path for organic look -->
      <path class="flower-bud p1" d="M200 115 C170 85 185 55 200 70 C215 55 230 85 200 115Z"/>
      <path class="flower-bud p2" d="M200 115 C230 85 260 100 245 115 C260 130 230 145 200 115Z"/>
      <path class="flower-bud p3" d="M200 115 C230 115 245 145 200 160 C155 145 170 115 200 115Z"/>
      <path class="flower-bud p4" d="M200 115 C170 145 140 130 155 115 C140 100 170 85 200 115Z"/>
      
      <!-- Inner Petals -->
      <path class="flower-bud p5" d="M200 115 C185 95 195 75 200 85 C205 75 215 95 200 115Z"/>
      <path class="flower-bud p6" d="M200 115 C215 95 235 105 225 115 C235 125 215 135 200 115Z"/>
      <path class="flower-bud p7" d="M200 115 C185 135 195 155 200 145 C205 155 215 135 200 115Z"/>
      <path class="flower-bud p8" d="M200 115 C185 115 165 105 175 115 C165 125 185 135 200 115Z"/>

      <!-- Center -->
      <circle class="flower-center" cx="200" cy="115" r="14"/>
      <circle cx="195" cy="110" r="4" fill="white" opacity="0.4" filter="url(#glassBlur)"/>
    </g>
  </g>
  
  <!-- Floating Particles (Pollen/Spores) -->
  <circle class="pt pt1" cx="160" cy="330" r="2.5"/>
  <circle class="pt pt2" cx="240" cy="310" r="2"/>
  <circle class="pt pt3" cx="180" cy="280" r="3"/>
  <circle class="pt pt4" cx="220" cy="250" r="1.5"/>
  <circle class="pt pt5" cx="190" cy="190" r="2.5"/>
  
  <!-- Premium Glassmorphism Bubble -->
  <g class="speech-bubble">
    <rect x="70" y="25" width="260" height="42" rx="21" class="glass-panel"/>
    <path d="M190 66 L200 82 L210 66 Z" class="glass-panel" style="stroke-width:1"/>
    <!-- Overlap line to hide border -->
    <line x1="191" y1="66" x2="209" y2="66" stroke="rgba(15,23,42,0.5)" stroke-width="3"/>
    <text x="200" y="52" class="speech-text">...</text>
  </g>
</g></svg>`;
  }

    function applyPalette(p) {
    if (!three?.svg) return;
    const h = n => '#' + (n || 0).toString(16).padStart(6, '0');
    const s = three.svg.style;
    s.setProperty('--lc1',   h(p.leaf1));
    s.setProperty('--lc2',   h(p.leaf2));
    s.setProperty('--sc1',   h(p.stem1));
    s.setProperty('--sc2',   h(p.stem2));
    s.setProperty('--sunC',  h(p.sun));
    s.setProperty('--skyT',  h(p.skyTop));
    s.setProperty('--skyB',  h(p.skyBot));
    s.setProperty('--terrainC',    h(p.terrain));
    s.setProperty('--terrainTopC', h(p.terrainTop));
    s.setProperty('--terrainEdgeC',h(p.terrainEdge));
    s.setProperty('--grassC',  h(p.grass));
    s.setProperty('--crackC',  h(p.crack));
    s.setProperty('--pebbleC', h(p.pebble));
    s.setProperty('--rootC',   h(p.root));
    s.setProperty('--hillC',   h(p.hill));
    s.setProperty('--hill2C',  h(p.hill2));
    s.setProperty('--dustC',   h(p.dust || 0xd97706));
    s.setProperty('--puddleC', h(p.puddle || 0x0ea5e9));
    s.setProperty('--hwaveC',  h(p.hwave || 0xfb923c));
    s.setProperty('--bud1',    h(p.bud1));
    s.setProperty('--bud2',    h(p.bud2));
    s.setProperty('--centerC', h(p.center));
  }

    function paletteForProblem(code) {
    const palettes = {
      0: { skyTop:0x0f172a, skyBot:0x020617, sun:0xfde047, terrain:0x0f172a, terrainTop:0x1e293b, terrainEdge:0x020617, grass:0x10b981, crack:0x475569, pebble:0x334155, root:0x475569, hill:0x0f172a, hill2:0x020617, stem1:0x047857, stem2:0x10b981, leaf1:0x34d399, leaf2:0x059669, center:0xfbbf24, bud1:0xe11d48, bud2:0xfb7185, dust:0xd97706, puddle:0x0ea5e9, hwave:0xfb923c }, // Parfait (Neon Cyber Nature)
      1: { skyTop:0x2a1610, skyBot:0x0a0502, sun:0xfb923c, terrain:0x3b2010, terrainTop:0x5e3318, terrainEdge:0x1f1008, grass:0x854d0e, crack:0x9a3412, pebble:0x57341e, root:0x78350f, hill:0x291206, hill2:0x180a03, stem1:0x65a30d, stem2:0x84cc16, leaf1:0xa3e635, leaf2:0x4d7c0f, center:0xf59e0b, bud1:0xd97706, bud2:0xfcd34d, dust:0xd97706, puddle:0x0ea5e9, hwave:0xfb923c }, // Soif (Warm, Dry)
      2: { skyTop:0x0c1e35, skyBot:0x040b18, sun:0x94a3b8, terrain:0x0f1c30, terrainTop:0x1e3a5f, terrainEdge:0x081020, grass:0x0369a1, crack:0x1e3a5f, pebble:0x1e293b, root:0x334155, hill:0x0f1c30, hill2:0x040b18, stem1:0x0f766e, stem2:0x14b8a6, leaf1:0x2dd4bf, leaf2:0x0d9488, center:0x64748b, bud1:0x475569, bud2:0x94a3b8, dust:0xd97706, puddle:0x38bdf8, hwave:0xfb923c }, // Trop d'eau (Deep Blue, Flooded)
      3: { skyTop:0x170f2e, skyBot:0x070414, sun:0x4c1d95, terrain:0x1a1235, terrainTop:0x2e2055, terrainEdge:0x0b0718, grass:0x312e81, crack:0x2e2055, pebble:0x1e1b4b, root:0x312e81, hill:0x170f2e, hill2:0x070414, stem1:0x3730a3, stem2:0x4f46e5, leaf1:0x818cf8, leaf2:0x4338ca, center:0x8b5cf6, bud1:0xc084fc, bud2:0xe879f9, dust:0xd97706, puddle:0x0ea5e9, hwave:0xfb923c }, // Sombre (Purple, Gloomy)
      4: { skyTop:0x0c2a3d, skyBot:0x04111c, sun:0xe0f2fe, terrain:0x13374f, terrainTop:0x235a7d, terrainEdge:0x091b2b, grass:0x0284c7, crack:0x235a7d, pebble:0x1e293b, root:0x475569, hill:0x0f2334, hill2:0x040b14, stem1:0x0284c7, stem2:0x38bdf8, leaf1:0x7dd3fc, leaf2:0x0369a1, center:0xbae6fd, bud1:0x7dd3fc, bud2:0xe0f2fe, dust:0xd97706, puddle:0x0ea5e9, hwave:0xfb923c }, // Froid (Cyan, Frost)
      5: { skyTop:0x3c1111, skyBot:0x1a0505, sun:0xef4444, terrain:0x451616, terrainTop:0x732222, terrainEdge:0x230909, grass:0xb91c1c, crack:0x991b1b, pebble:0x450a0a, root:0x7f1d1d, hill:0x2e0c0c, hill2:0x130303, stem1:0x9a3412, stem2:0xf97316, leaf1:0xfdba74, leaf2:0xc2410c, center:0xef4444, bud1:0xd97706, bud2:0xfcd34d, dust:0xd97706, puddle:0x0ea5e9, hwave:0xf87171 }, // Chaud (Crimson, Heat)
    };
    return palettes[code] || palettes[0];
  }

  function resizeThree() { /* SVG is responsive - no-op */ }

      function updatePlant(reading) {
    if (!three?.svg || !reading) return;
    const code = Number(reading.problem_code || 0);
    three.state = code;
    three.svg.setAttribute('data-state', code);
    applyPalette(paletteForProblem(code));

    // Continuous data bindings for CSS variables
    const temp = Number(reading.temperature ?? 22);
    const soil = Number(reading.soil_humidity ?? 50);
    const light = Number(reading.light_level ?? 500);

    // Droop based on soil (less soil = more droop)
    // Map soil 0-100 to angle 45deg-0deg
    const droopAngle = clamp(45 - (soil / 100) * 45, 0, 45);
    
    // Shiver intensity based on temp (only shiver if < 18)
    const shiverScale = temp < 18 ? clamp((18 - temp) * 0.8, 0, 5) : 0;
    
    // Brightness bloom scale based on light
    const lightFactor = clamp(light / 800, 0.25, 1.3);
    
    // Bloom scale for the flower (more light = bigger bloom)
    const bloomScale = clamp(light / 600, 0.7, 1.15);

    three.svg.style.setProperty('--data-droop', `${droopAngle}deg`);
    three.svg.style.setProperty('--data-shiver', shiverScale);
    three.svg.style.setProperty('--data-light', lightFactor);
    three.svg.style.setProperty('--data-bloom', bloomScale);

    // Dynamic shivering class
    if (shiverScale > 0) {
      three.svg.querySelector('.plant-group')?.classList.add('is-shivering');
    } else {
      three.svg.querySelector('.plant-group')?.classList.remove('is-shivering');
    }

    const speechText = three.svg.querySelector('.speech-text');
    if (speechText) {
      const meta = problemMeta[code] || problemMeta[0];
      speechText.textContent = meta.text || "Mmm...";
    }
  }

  function animateThree() { /* CSS handles all animations */ }

  function drawTimeline() {
    const timelineCanvas = getTimelineCanvas();
    if (!timelineCanvas) return;
    const rect = timelineCanvas.getBoundingClientRect();
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    timelineCanvas.width = Math.max(1, Math.floor(rect.width * dpr));
    timelineCanvas.height = Math.max(1, Math.floor(rect.height * dpr));
    const ctx = timelineCanvas.getContext("2d");
    ctx.scale(dpr, dpr);
    ctx.clearRect(0, 0, rect.width, rect.height);
    timelineHits = [];

    const pad = { left: 22, right: 20, top: 26, bottom: 30 };
    const barsTop = pad.top;
    const barsBottom = rect.height - pad.bottom;
    const barsHeight = Math.max(20, barsBottom - barsTop);
    const start = Date.now() - selectedWindowMs();
    const end = Date.now();
    const width = rect.width - pad.left - pad.right;

    ctx.fillStyle = "rgba(167, 176, 195, 0.9)";
    ctx.font = "700 11px system-ui";
    ctx.fillText(formatWindowLeftLabel(), pad.left, rect.height - 10);
    ctx.fillText("maintenant", rect.width - pad.right - 66, rect.height - 16);

    const slotCount = clamp(Math.floor(width / 8), 42, 120);
    const slotDuration = (end - start) / slotCount;
    const slots = Array.from({ length: slotCount }, (_, index) => ({
      index,
      start: start + index * slotDuration,
      end: start + (index + 1) * slotDuration,
      rank: -1,
      label: "Sans donnée",
      color: "#64748b",
      events: [],
      reading: null,
    }));

    if (!readings.length) {
      const timelineAlert = getTimelineAlert();
      if (timelineAlert) {
        timelineAlert.className = "timeline-alert is-empty";
        timelineAlert.textContent = `Aucune donnée reçue sur ${historyWindows[selectedWindow]?.label || "la période"}`;
      }
      const timelineDetail = getTimelineDetail();
      if (timelineDetail) {
        timelineDetail.textContent = "Les mini-bâtons apparaîtront dès qu'une lecture sera reçue.";
      }

      const step = width / slotCount;
      const barWidth = Math.max(2, Math.min(8, step * 0.62));
      slots.forEach((slot) => {
        const x = pad.left + slot.index * step + (step - barWidth) / 2;
        ctx.fillStyle = slot.color;
        ctx.fillRect(x, barsTop, barWidth, barsHeight);
        timelineHits.push({ x: x + barWidth / 2, y: barsTop + barsHeight / 2, slot });
      });
      ctx.fillStyle = "rgba(167, 176, 195, 0.9)";
      ctx.fillText("Aucune lecture sur cette période", pad.left, 14);
      return;
    }

    readings.forEach((reading) => {
      const time = new Date(reading.timestamp).getTime();
      if (time < start || time > end) return;
      const slotIndex = clamp(Math.floor((time - start) / slotDuration), 0, slotCount - 1);
      const slot = slots[slotIndex];
      const status = timelineStatusForReading(reading);
      if (status.rank > slot.rank) {
        slot.rank = status.rank;
        slot.label = status.label;
        slot.color = status.color;
      }
      slot.events = uniq([...slot.events, ...status.events.map((event) => event.label)]);
      if (!slot.reading || new Date(reading.timestamp).getTime() >= new Date(slot.reading.timestamp).getTime()) {
        slot.reading = reading;
      }
    });

    const step = width / slotCount;
    const barWidth = Math.max(2, Math.min(8, step * 0.62));
    slots.forEach((slot) => {
      const x = pad.left + slot.index * step + (step - barWidth) / 2;
      ctx.fillStyle = slot.color;
      ctx.fillRect(x, barsTop, barWidth, barsHeight);
      timelineHits.push({ x: x + barWidth / 2, y: barsTop + barsHeight / 2, slot });
    });

    const warnSlots = slots.filter((slot) => slot.rank === 1).length;
    const criticalSlots = slots.filter((slot) => slot.rank === 2).length;
    const issueSlots = warnSlots + criticalSlots;

    const timelineAlert = getTimelineAlert();
    if (timelineAlert) {
      if (criticalSlots > 0) {
        timelineAlert.className = "timeline-alert is-critical";
        timelineAlert.textContent = `Incident détecté : ${criticalSlots} créneau(x) critique(s)`;
      } else if (warnSlots > 0) {
        timelineAlert.className = "timeline-alert is-warn";
        timelineAlert.textContent = `Alerte : ${warnSlots} créneau(x) à surveiller`;
      } else {
        timelineAlert.className = "timeline-alert is-ok";
        timelineAlert.textContent = "Aucun incident détecté";
      }
    }

    const timelineDetail = getTimelineDetail();
    if (timelineDetail) {
      const slotMinutes = Math.max(1, Math.round(slotDuration / 60000));
      timelineDetail.textContent = `${slotCount} bâtons affichés · 1 bâton ≈ ${slotMinutes} min · ${issueSlots} créneau(x) non stable(s)`;
    }
  }

  function addPoint(reading) {
    if (!reading || !reading.timestamp) return;
    lastReading = reading;
    readings.push(reading);
    const cutoff = Date.now() - selectedWindowMs();
    readings = readings.filter((item) => new Date(item.timestamp).getTime() >= cutoff);
    updatePlant(reading);
    setStationStatus(reading);
    drawTimeline();
  }

  window.stationDashboard = { addPoint, updatePlant };

  function loadHistory() {
    updateTimelineTitle();
    return fetch(`/api/history/?station_id=${encodeURIComponent(stationId)}&window=${encodeURIComponent(selectedWindow)}`)
      .then((response) => response.json())
    .then((data) => {
      readings = data.readings || [];
      // S'assurer que les dates de l'historique sont bien lues en UTC
      readings.forEach((r) => {
        if (r.timestamp && !r.timestamp.includes('+') && !r.timestamp.endsWith('Z')) {
          r.timestamp += 'Z';
        }
      });
      drawTimeline();
      if (!lastReading && readings.length) {
        lastReading = readings[readings.length - 1];
        updatePlant(lastReading);
      }
      setStationStatus(lastReading);
    })
    .catch(() => drawTimeline());
  }

  const timelineRange = getTimelineRange();
  if (timelineRange) {
    selectedWindow = historyWindows[timelineRange.value] ? timelineRange.value : "4h";
  }

  loadHistory();

  document.body.addEventListener("change", (event) => {
    if (event.target.id === "timeline-range") {
      if (!historyWindows[event.target.value]) return;
      selectedWindow = event.target.value;
      loadHistory();
    }
  });

  function handleDataUpdate() {
    const viewport = getViewport();
    if (viewport && !viewport.querySelector('svg')) {
      initThree();
    }

    const payloadEl = document.getElementById("live-payload");
    if (payloadEl && payloadEl.dataset.reading) {
      const reading = parseReading(payloadEl.dataset.reading);
      if (!lastReading || reading.timestamp !== lastReading.timestamp) {
        addPoint(reading);
      } else {
        updatePlant(lastReading);
      }
    }
  }

  document.body.addEventListener("htmx:afterSettle", handleDataUpdate);
  document.body.addEventListener("htmx:oobAfterSwap", handleDataUpdate);
  document.body.addEventListener("htmx:wsAfterMessage", handleDataUpdate);

  document.body.addEventListener("mousemove", (event) => {
    const timelineCanvas = getTimelineCanvas();
    const tooltip = getTooltip();
    if (!timelineCanvas || !tooltip || event.target !== timelineCanvas) return;
    
    if (!timelineHits.length) return;
    const rect = timelineCanvas.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const nearest = timelineHits.reduce((best, hit) => (Math.abs(hit.x - x) < Math.abs(best.x - x) ? hit : best), timelineHits[0]);
    if (Math.abs(nearest.x - x) > 16) {
      tooltip.hidden = true;
      return;
    }
    const slot = nearest.slot;
    const reading = slot.reading;
    const meta = reading ? (problemMeta[reading.problem_code] || problemMeta[0]) : null;
    const startLabel = new Date(slot.start).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    const endLabel = new Date(slot.end).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    tooltip.hidden = false;
    tooltip.style.left = `${clamp(nearest.x + 12, 8, rect.width - 290)}px`;
    tooltip.style.top = `${Math.max(8, nearest.y - 82)}px`;

    if (!reading) {
      tooltip.innerHTML = `<strong>Sans donnée</strong><br><span>${startLabel} → ${endLabel}</span>`;
      return;
    }

    const eventText = slot.events.length ? ` · ${slot.events.join(" + ")}` : "";
    tooltip.innerHTML = `<strong>${slot.label}${eventText}</strong><br><span>${startLabel} → ${endLabel} · Terre ${Math.round(reading.soil_humidity)}% · ${Math.round(reading.light_level)} lux · ${Number(reading.temperature).toFixed(1)} °C</span>`;
  });

  document.body.addEventListener("mouseleave", (event) => {
    if (event.target.id === "health-timeline") {
      const tooltip = getTooltip();
      if (tooltip) tooltip.hidden = true;
    }
  }, true);

  window.addEventListener("resize", () => {
    resizeThree();
    drawTimeline();
  });

  function initDiagnosticToggle() {
    const grid = document.querySelector(".dashboard-grid");
    if (!grid) return;

    // Load state from localStorage
    const isCollapsed = localStorage.getItem("diagnostic-collapsed") === "true";
    if (isCollapsed) {
      grid.classList.add("diagnostic-collapsed");
    }

    // Use event delegation so it works even if the DOM changes/swaps
    document.addEventListener("click", (event) => {
      const btn = event.target.closest("#toggle-diagnostic-btn");
      if (!btn) return;

      event.preventDefault();
      const currentCollapsed = grid.classList.toggle("diagnostic-collapsed");
      localStorage.setItem("diagnostic-collapsed", currentCollapsed ? "true" : "false");

      // Trigger three.js resize to update canvas dimensions smoothly during transition
      setTimeout(() => { resizeThree(); }, 50);
      setTimeout(() => { resizeThree(); }, 150);
      setTimeout(() => { resizeThree(); }, 350);
    });
  }

  setInterval(() => setStationStatus(lastReading), 1000);
  updateTimelineTitle();
  initThree();
  setStationStatus(lastReading);
  initDiagnosticToggle();
})();

