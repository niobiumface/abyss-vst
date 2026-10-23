# Build-Auftrag: VST3-Plugin „Abyss“ (Dark Sub-Octaver / Darkener, Pixelart-Arcade)

Du bist ein erfahrener C++/JUCE-Audio-Entwickler und arbeitest im Cursor-Agent-Modus mit Terminal-Zugriff auf einem Windows-PC. Deine Aufgabe ist es, das Plugin **„Abyss“** komplett zu bauen, selbst zu prüfen, als Release zu kompilieren und zu installieren.

## 0. Arbeitsmodus (verbindlich)
- Arbeite **selbstständig und ohne Rückfragen** alle Phasen 1–7 nacheinander durch. Wenn etwas unklar ist, triff eine sinnvolle Entscheidung im Sinne dieses Dokuments und halte sie im Abschlussbericht fest.
- Brich nie ab, weil etwas fehlt oder fehlschlägt. Fehlt ein Asset, baust du es im selben Stil nach (Abschnitt 4.8). Scheitert ein Build oder ein Test, analysierst du den Fehler, behebst ihn und wiederholst den Schritt, bis alles grün ist.
- Nach jeder Phase: kompilieren, Tests laufen lassen, einen lokalen Git-Commit machen (`git add -A; git commit -m "Phase N: …"`) und 2–3 Zeilen Status in den Chat schreiben. **Kein** `git push`, kein Remote anlegen.
- Shell ist **PowerShell**. Der Repo-Pfad enthält ein **Leerzeichen**. Quote jeden Pfad immer mit doppelten Anführungszeichen und rufe Programme mit Leerzeichen im Pfad mit dem Call-Operator `&` auf.
- Ändere oder lösche nichts außerhalb des Repo-Ordners. Einzige Ausnahmen sind die beiden VST3-Installationsziele in Phase 7 und `%APPDATA%\n-f.pro\Abyss\Presets` (wird vom Plugin selbst angelegt).
- Die Assets im Repo sind **verbindlich und fertig**. Zeichne nichts nach, was vorhanden ist, und verändere keine Asset-Datei.

## 1. Umgebung (bereits geprüft, so verwenden)
| Was | Wert |
|---|---|
| Repo-Ordner | `C:\Users\glabu\Desktop\abyss vst` (alle relativen Pfade unten beziehen sich darauf) |
| Visual Studio | Community 2022 unter `E:\Microsoft Visual Studio\2022\Community` (Workload „Desktopentwicklung mit C++“) |
| CMake | `C:\Program Files\CMake\bin\cmake.exe` und `ctest.exe` (nicht auf PATH verlassen, sondern vollen Pfad nutzen) |
| Generator | `"Visual Studio 17 2022"`, `-A x64`, Konfiguration `Release` |
| Git | vorhanden |
| JUCE | **nicht** installiert. Hole es per CMake `FetchContent`, fester Tag **`8.0.15`** (`GIT_SHALLOW TRUE`) |
| pluginval | **nicht** installiert. Lade `https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip` nach `tools\pluginval\` (gitignored) und entpacke es dort |

Am Anfang einmal definieren und danach immer benutzen:
```powershell
$Repo  = "C:\Users\glabu\Desktop\abyss vst"
$CMake = "C:\Program Files\CMake\bin\cmake.exe"
$CTest = "C:\Program Files\CMake\bin\ctest.exe"
Set-Location -LiteralPath $Repo
```
Wenn FetchContent wegen zu langer Pfade scheitert (MAX_PATH), konfiguriere mit `-DFETCHCONTENT_BASE_DIR="C:/abyss_deps"` neu und vermerke das im Bericht. Findet CMake Visual Studio nicht automatisch, setze `-DCMAKE_GENERATOR_INSTANCE="E:/Microsoft Visual Studio/2022/Community"`.

## 2. Vorhandene Assets im Repo (zuerst prüfen!)
Erwarteter Inhalt (vom Nutzer hineinkopiert):
```
anim\depth_000_f0.png … anim\depth_100_f2.png   33 Dateien = 11 Stufen (000,010,…,100) × 3 Frames (f0,f1,f2)
                                                je 112×74 px, NUR der Arcade-Bildschirminhalt, voll deckend
ui\ui_frame.png            240×186  Komplettes Fenster: dunkle Spielhalle, Automat, Logo „ABYSS“, Bedienpult mit gebackenen
                                    Labels DEPTH / IN / OUT / PRESSURE / BYPASS, Ticks, Mulden, Werte-Plakette.
                                    Bildschirm x 64–175, y 38–111 ist transparent (abgerundete CRT-Ecken, 8268 transparente Pixel)
ui\knob_depth_strip.png    24×1536  64 Frames à 24×24, vertikal, Frame 0 = Minimum (−135°), Frame 63 = Maximum (+135°), Zeiger knochenweiß
ui\knob_pressure_strip.png 24×1536  64 Frames à 24×24, vertikal, Zeiger blutrot
ui\knob_small_strip.png    12×384   32 Frames à 12×12, vertikal (cyanfarbene Drehregler für IN und OUT)
ui\bypass_off.png          20×16    roter Arcade-Button, dunkel = Plugin aktiv
ui\bypass_on.png           20×16    roter Arcade-Button, leuchtend mit Glühen = Plugin gebypasst
ui\screen_off.png          112×74   Bildschirm im Bypass („INSERT COIN“), voll deckend
palette.json               Palette (Indizes/Namen/Hex), Layout, Animation, PRESSURE-Effekt, Tiefenzähler
PROMPT_BUILD.md            dieses Dokument
```
Fakten zu den Bildern (geprüft, in Tests absichern):
- Alle Bilder bestehen **ausschließlich** aus den 19 Palettenfarben (Abschnitt 4.11, deckend). In `ui\ui_frame.png`, den Strips und den Bypass-Sprites gibt es zusätzlich voll transparentes Alpha 0. Es gibt **keine Halbtransparenz**. `anim\*` und `screen_off.png` sind voll deckend.
- `ui_frame.png` ist **außerhalb** des Bildschirmrechtecks vollständig deckend. Alle transparenten Pixel liegen im Bildschirmrechteck. Der Bildschirm-Inhalt wird also immer **unter** den Rahmen gezeichnet, die Ecken verdeckt der Rahmen.
- In **allen** Stufen unterscheiden sich f0 und f1 (CRT-Flackern, Rollbalken, Figur wackelt, Staub). f2 ist der **Blinzel-Frame**: Bei 000–030 ist f2 identisch mit f0, ab 040 sind dort die Augen geschlossen bzw. halb zu.
- Rote Augen (Farben `R1`/`R2`/`R3`) gibt es erst ab Stufe 040, ihre Pixelzahl steigt danach streng monoton (040: 4 px … 100: ~1450 px). Die mittlere Helligkeit des Felsens sinkt mit der Stufe. Die Figur (`N2`) ist in jeder Stufe sichtbar.

**Erste Aktion in Phase 1:** Schreibe `scripts\check_assets.ps1`, das alle oben gelisteten Dateien, ihre Dimensionen (per `System.Drawing`) und die Frame-Anzahl prüft und eine Tabelle ausgibt. Fehlt etwas, gilt Abschnitt 4.8.

## 3. Projekt & Plugin-Daten

### 3.1 Struktur (genau so anlegen)
```
abyss vst\
├─ anim\  ui\  palette.json  PROMPT_BUILD.md   (Assets, nicht verschieben, nicht ändern)
├─ CMakeLists.txt
├─ cmake\JuceFetch.cmake
├─ Source\
│  ├─ PluginProcessor.h/.cpp  PluginEditor.h/.cpp  Parameters.h/.cpp  Version.h
│  ├─ dsp\  FastMath.h  PitchTracker.h/.cpp  SubOscillator.h/.cpp  Darkener.h/.cpp  PressureComp.h/.cpp
│  │        PressureDrive.h/.cpp  AutoGain.h/.cpp  Clipper.h/.cpp  AbyssEngine.h/.cpp
│  ├─ ui\   IndexedImage.h/.cpp  SpriteBank.h/.cpp  PixelScaler.h/.cpp  PixelFont.h/.cpp  KnobStrip.h/.cpp
│  │        ScreenComponent.h/.cpp  BypassButton.h/.cpp  DepthMapping.h  PressureFx.h/.cpp  AbyssLookAndFeel.h/.cpp
│  └─ presets\ PresetManager.h/.cpp  factory\*.xml
├─ tests\  AbyssTests.cpp (+ weitere .cpp)          → Konsolen-Testprogramm, über CTest registriert
├─ bench\  AbyssBench.cpp                           → CPU-Benchmark
├─ scripts\ check_assets.ps1  build_win.ps1  install.ps1  install_system.ps1
├─ tools\pluginval\ (heruntergeladen, gitignored)
└─ .gitignore, README.md
```
Kein eigener Code (auch nicht Processor/Editor) verwendet `JucePlugin_*`-Makros: `getName()` liefert das Literal `"Abyss"`, Versionen kommen aus `Source\Version.h`. So kann `AbyssTests` Processor **und** Editor direkt mitkompilieren (für Null-, State- und Render-Tests). Falls JUCE intern Makros braucht, setze sie im Test-Target per `target_compile_definitions`.

### 3.2 Git
Wenn `.git` fehlt: `git init`, dann `.gitignore` mit mindestens:
```
build/
out/
.vs/
tools/pluginval/
*.user
*.log
```
Erster Commit „Initial assets“ mit den vorhandenen Assets, bevor du Code schreibst.

### 3.3 CMake
- `cmake_minimum_required(VERSION 3.22)`, `project(Abyss VERSION 1.0.0 LANGUAGES C CXX)`, `CMAKE_CXX_STANDARD 20`, MSVC-Runtime statisch (`set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")`), damit das Plugin ohne VC-Redistributable läuft.
- JUCE per `FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.15 GIT_SHALLOW TRUE)` + `FetchContent_MakeAvailable(JUCE)` in `cmake\JuceFetch.cmake`.
- `option(ABYSS_STANDALONE "Build Standalone" ON)`, `option(ABYSS_AVX2 "AVX2" OFF)`.
- `juce_add_plugin(Abyss …)` mit:
  - `PLUGIN_NAME "Abyss"`, `PRODUCT_NAME "Abyss"` (→ `Abyss.vst3`)
  - `COMPANY_NAME "n-f.pro"`, `BUNDLE_ID "pro.n-f.abyss"` (gleicher Hersteller wie Boilin')
  - `PLUGIN_MANUFACTURER_CODE Nfpr`, `PLUGIN_CODE Abys`
  - `FORMATS VST3 Standalone` (Standalone nur wenn `ABYSS_STANDALONE`)
  - `VST3_CATEGORIES Fx Filter Distortion`
  - `IS_SYNTH FALSE`, `NEEDS_MIDI_INPUT FALSE`, `NEEDS_MIDI_OUTPUT FALSE`, `IS_MIDI_EFFECT FALSE`, `EDITOR_WANTS_KEYBOARD_FOCUS TRUE`
  - **`COPY_PLUGIN_AFTER_BUILD FALSE`** (Installation macht Phase 7)
- `juce_add_binary_data(AbyssAssets NAMESPACE AbyssData SOURCES …)` mit **allen 33** `anim\depth_XXX_fN.png`, allen **7** Dateien aus `ui\`, `palette.json` und `Source\presets\factory\*.xml`. Die Dateiliste wird **explizit** in CMake erzeugt (Schleife über 000,010,…,100 und f0–f2), nicht per `GLOB`. Fehlt eine Datei, bricht CMake mit klarer Meldung ab. BinaryData-Bezeichner werden aus Dateinamen gebildet (`depth_000_f0_png`). Greife über `AbyssData::getNamedResource("depth_000_f0_png", size)` zu und baue die Namen im Code aus Stufe/Frame zusammen.
- Links: `juce::juce_audio_utils juce::juce_dsp AbyssAssets` + `juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags`.
- Compile-Defs: `JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0`.
- Warnungen: Für die eigenen Targets `target_compile_options(... PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8 /MP>)`. Release zusätzlich `/O2 /fp:fast`, bei `ABYSS_AVX2` `/arch:AVX2`.
- Zusätzliche Targets: `AbyssTests` (`juce_add_console_app`, kompiliert alle Dateien aus `Source\` sowie `tests\*`, linkt `AbyssAssets`, `juce_audio_utils`, `juce_dsp`) und `AbyssBench` (`juce_add_console_app`). `enable_testing()` + `add_test(NAME AbyssTests COMMAND AbyssTests)`.

## 4. Funktionen & Features

### 4.1 Konzept
Abyss macht jedes Signal **tiefer und dunkler**. Ein großer Regler **DEPTH** legt eine Oktave tiefer (und ab der Hälfte zusätzlich zwei Oktaven tiefer) unter das Signal und nimmt gleichzeitig die Höhen weg. **PRESSURE** gibt Druck: Sättigung plus leichte Kompression. Zielgruppe: Bass, 808, Synth-Leads, Vocals für Dark Techno, Trap, Drone, Horror-Sounddesign. Es soll sofort düster, schwer und „unter Wasser gedrückt“ klingen, nie matschig-zerbrochen (sauberes Tracking), und sehr CPU-sparsam sein (Abschnitt 4.7).

### 4.2 DSP-Kette
Stereo/linked, Mono und Stereo (Bus-Layouts mono→mono und stereo→stereo, sonst ablehnen), `float`. **Keine Allokation, keine Locks, kein Logging im Audio-Thread.**
```
In → Input Gain ─┬─ Sidechain (Mono-Summe) → PitchTracker → SubOscillator (Sub −1, Sub −2) ─┐
                 └─ Main (Stereo) → Darkener (dyn. Tiefpass + Low-Shelf) ──────────────────┤
                                                                         Sum = Main·m + Sub1·g1 + Sub2·g2 (Sub mono auf L+R)
   → PressureComp (leicht, Basisrate) → [Upsample Off/2x/4x] → PressureDrive (asym. Soft-Clip)
   → AutoGain (blockweise, gerampt) → Clipper (soft→hard, Ceiling) → [Downsample]
   → Mix (latenzkompensiert) → Output Gain → Bypass-XFade → Out
```
`d = depth/100`, `p = pressure/100`. Alle Kurven unten sind verbindlich, Feintuning per Hörvergleich ist erlaubt, muss aber im Bericht stehen.

**Input Gain:** −24 … +24 dB, Default 0, Smoothing 20 ms (multiplikativ).

#### 4.2.1 PitchTracker (Zero-Crossing mit Hysterese, Basisrate)
1. Sidechain `s = 0.5·(L+R)` (Mono: `L`). Tracking-Filter: HPF 2. Ordnung 35 Hz → LPF 2. Ordnung 260 Hz (TPT-SVF, Q 0.707), damit nur die Grundschwingung zählt.
2. Hüllkurve `env` des gefilterten Signals: Peak-Follower, Attack 2 ms, Release 60 ms.
3. Schmitt-Trigger mit Schwelle `thr = max(1e-5, 0.12·env)`: „armed“ nach Unterschreiten von `−thr`, Trigger beim Überschreiten von `+thr`. Den genauen Nulldurchgang bestimmst du **fraktional** (lineare Interpolation zwischen den beiden Samples um den letzten Vorzeichenwechsel vor dem Trigger).
4. Periode `T` (in Samples, fraktional) = Abstand zum vorherigen Nulldurchgang. Gültig, wenn `fs/T` in **30 … 600 Hz** liegt. Glättung: Wenn `|T − Tavg|/Tavg < 0.25`, dann `Tavg = 0.7·Tavg + 0.3·T` und `confidence = min(confidence+1, 4)`, sonst `Tavg = T`, `confidence = 1`. Keine gültige Periode für mehr als 50 ms → `valid = false`.
5. Zwei Flip-Flops: `ff1` kippt bei jedem Trigger (f/2), `ff2` kippt bei jeder steigenden Flanke von `ff1` (f/4). Sie werden beim Sub-Oszillator für die Phasensynchronisation und im Grit-Modus als Rechteck genutzt.

#### 4.2.2 SubOscillator (zwei Modi, Parameter `subMode`)
- **Clean** (Default, „PLL“-artig, sauber für 808/Bass): zwei Phasenakkumulatoren `φ1 += 0.5/Tavg`, `φ2 += 0.25/Tavg` (Zyklen pro Sample). Bei jeder steigenden Flanke von `ff1` (bzw. `ff2`) wird der Phasenfehler `e = wrap(φ) ∈ [−0.5, 0.5)` gemessen und weich korrigiert: `φ −= 0.3·e`, zusätzlich `Δ`-Korrektur `±0.02·e/Tavg` (PLL 2. Ordnung, keine harten Sprünge). Ausgang `sub1 = fastSin(2π·φ1)`, `sub2 = fastSin(2π·φ2)`. `fastSin` als Polynom (7. Ordnung, Fehler < 1e-4) oder 2048er-LUT mit linearer Interpolation.
- **Grit** (analog, Boss-OC-2-Stil): `sub1 = trackSig·ff1` (Signal mal Rechteck, folgt der Wellenform), `sub2 = env·ff2`. Beides durch LPF 4. Ordnung bei `lerp(420, 180, d)` Hz, danach Pegel auf `env` normiert (Division mit `max(env, 1e-4)`, auf ±2 begrenzt).
- **Amplitude:** Beide Subs werden mit `envSub` (Hüllkurve des Tracking-Signals, Attack 5 ms, Release 120 ms) multipliziert, sodass der Sub der Dynamik folgt.
- **Gate:** `gate`-Gain (Rampe 5 ms auf, 30 ms zu) ist 1, wenn `valid && env > −55 dBFS`, sonst 0. Bei `confidence ≤ 1` (unsicheres Tracking, z. B. Akkorde) wird der Sub auf 50 % gezogen (Rampe 20 ms) statt hart zu schalten.
- **Mono:** Die Subs sind mono und gehen gleich laut auf L und R.

#### 4.2.3 DEPTH-Mapping (0–100 %)
- `g1 = 1.1·smoothstep(0, 0.55, d)` (Sub −1)
- `g2 = 0.75·smoothstep(0.45, 1.0, d)` (Sub −2, nur wenn `sub2 == true`)
- `m = 1 − 0.30·d` (das Original wird etwas zurückgenommen, damit der Sub Platz hat)
- **Darkener** (auf Main, stereo): zwei kaskadierte TPT-SVF-Tiefpässe (24 dB/Okt), Q1 = `lerp(0.707, 1.05, d)` (leichte, dunkle Resonanz), Q2 = 0.54.
  - Grund-Cutoff `fc = 20000·(400/20000)^(d^0.85)` Hz (25 %: ≈ 6 kHz, 50 %: ≈ 2,2 kHz, 100 %: 400 Hz), auf `0.45·fs` begrenzt.
  - **Dynamisch:** Hüllkurve des Main-Signals über HPF 2 kHz (Peak, Attack 1 ms, Release 80 ms) in dB. `open = clamp((envDb + 36)/24, 0, 1)`, `fcEff = fc·2^(open·d)` (Transienten öffnen den Filter um bis zu 1 Oktave, „atmet“).
  - Koeffizienten nur alle **16 Samples** neu (schnelle `tan`-Näherung), dazwischen konstant. Q/fc-Änderungen sind dadurch klickfrei (TPT-SVF ist modulationsstabil).
  - Low-Shelf `+lerp(0, 3, d)` dB bei 110 Hz (Gewicht).
- Bei `d == 0` werden Tracker-Ausgang, Darkener und Shelf **exakt umgangen** (Bit-Null). Der Tracker läuft intern weiter (damit DEPTH-Automation ohne Einschwingen startet), sein Beitrag ist dann aber exakt 0.

#### 4.2.4 PRESSURE (0–100 %)
1. **PressureComp** (feed-forward, linked, Basisrate, vor dem Drive): RMS-Detektor (5 ms), Threshold `lerp(0, −20, p)` dBFS, Ratio `lerp(1, 3.5, p)`, Soft-Knee 8 dB, Attack `lerp(15, 6, p)` ms, Release 140 ms. dB-Rechnung per schnellem `log2/exp2`-Approx, Kennlinie per 256er-LUT, pro Sample nur die Hüllkurve.
2. **PressureDrive** (im Oversampling-Bereich): `pre = dbToGain(lerp(0, 18, p^1.3))`, `b = 0.2·p`, `sat(x) = (fastTanh(pre·x + b) − fastTanh(b)) / pre^0.5`, danach 1-Pol-DC-Blocker (8 Hz, an die OS-Rate angepasst). Damit es bei kleinen Werten nicht springt: `y = x + w·(sat(x) − x)` mit `w = smoothstep(0, 0.15, p)`. `fastTanh` als rationale Padé-Näherung (7/6) mit Clamp ±3, **kein** `std::tanh`.
3. Bei `p == 0` sind Comp und Drive **exakt umgangen**.

#### 4.2.5 AutoGain, Clipper, Mix, Output, Bypass
- **AutoGain** (`autoGain`, Default an): Open-Loop-Lautheitsangleich. Gemessen werden RMS des Eingangs (nach Input Gain) und RMS des Signals **vor** dem AutoGain (nach dem Drive), beide als 1-Pol über `x²` mit τ = 300 ms, blockweise. Korrektur `k = sqrt(rmsIn/rmsProc)`, begrenzt auf ±12 dB, mit 200 ms geglättet, pro Sample gerampt. Unter −60 dBFS Eingang wird `k` eingefroren (keine Pumpen in Pausen). Bei `d == 0 && p == 0` ist `k` **exakt 1.0** (Stufe übersprungen).
- **Clipper:** Soft-Knee (kubisch) ab 3 dB unter Ceiling, dann Hard-Clip auf Ceiling, im Oversampling-Bereich. Ceiling −6 … 0 dBFS, Default −0,3. Bei `d == 0 && p == 0` übersprungen (Neutralpfad bit-exakt).
- **Oversampling:** Parameter `os` Off/2x/4x, Default 2x. `juce::dsp::Oversampling<float>` Polyphase-IIR (minimale Latenz). Alle drei Instanzen in `prepareToPlay` vorbereiten. Umschalten setzt nur einen Index (keine Allokation, im Audio-Thread erkannt) und meldet die neue Latenz per `setLatencySamples()` (bei Off = 0). Gemeldete Latenz = tatsächliche Latenz auf ganze Samples gerundet, den Bruchteil gleichst du im Dry-Pfad aus. Tracker, Sub und Darkener laufen auf der Basisrate (Sub ist tief, braucht kein OS).
- **Mix:** 0–100 % (Default 100). Dry-Pfad über eine in `prepareToPlay` allokierte Delay-Line mit exakt der aktuellen Latenz.
- **Output Gain:** −24 … +12 dB.
- **Bypass:** `AudioParameterBool "bypass"`, über `getBypassParameter()` als Host-Bypass. Intern 10 ms Crossfade (equal-gain) zwischen verarbeitetem und **latenzverzögertem** Dry-Signal. Die Latenz bleibt im Bypass gleich. Nach Abschluss des Crossfades wird nur noch das verzögerte Dry kopiert (CPU-Early-out), der Tracker wird dann nicht mehr gerechnet und beim Verlassen des Bypass zurückgesetzt. `processBlockBypassed` liefert ebenfalls das latenzverzögerte Signal.

### 4.3 Parameter (APVTS, `juce::ParameterID{id, 1}`)
| ID | Name | Bereich | Default | Smoothing |
|---|---|---|---|---|
| `depth` | DEPTH | 0 … 100 %, Schritt 0.1 | 0 | 40 ms |
| `pressure` | PRESSURE | 0 … 100 %, Schritt 0.1 | 0 | 30 ms |
| `inGain` | Input | −24 … +24 dB, Schritt 0.1 | 0 | 20 ms |
| `outGain` | Output | −24 … +12 dB, Schritt 0.1 | 0 | 20 ms |
| `mix` | Mix | 0 … 100 % | 100 | 20 ms |
| `subMode` | Sub Mode | Choice Clean / Grit | Clean | 15-ms-Crossfade |
| `sub2` | Sub −2 | bool | true | 20 ms Rampe |
| `ceiling` | Ceiling | −6 … 0 dBFS | −0.3 | 20 ms |
| `autoGain` | Auto Gain | bool | true | – |
| `os` | Oversampling | Choice Off / 2x / 4x | 2x | – |
| `bypass` | Bypass | bool | false | 10 ms XFade |
| `uiScale` | nur State-Property, kein Host-Parameter | 2…5 | 3 | – |
| `animate` | nur State-Property | bool | true | – |
| `crtFx` | nur State-Property (PRESSURE-Bildschirmeffekt) | bool | true | – |

Parameterwerte im Audio-Thread nur über gecachte `std::atomic<float>*` (`getRawParameterValue`) lesen. Werte-Strings: `"63.0 %"`, `"+3.5 dB"`, `"-0.3 dBFS"`.

### 4.4 Presets, State, Undo
- `getStateInformation`/`setStateInformation`: APVTS-State als XML (`copyState()`), inklusive `uiScale`, `animate`, `crtFx` und `stateVersion=1`. Unbekannte/fehlende Werte → Defaults, nie crashen (auch bei leerem/kaputtem Blob).
- Factory-Presets (XML in BinaryData), DEPTH/PRESSURE/subMode, Rest Default:
  `Insert Coin` (0/0, Clean), `First Step` (15/10, Clean), `Freefall` (30/20, Clean), `Black Shaft` (45/30, Clean), `Low Light` (55/15, Clean), `Red Eyes` (65/50, Clean), `Bottomless` (80/40, Clean), `Crushing Dark` (60/90, Grit), `Game Over` (90/75, Grit), `The Abyss` (100/100, Grit). Diese auch als Host-Programme (`getNumPrograms` = 10, `setCurrentProgram`, `getProgramName`).
- User-Presets: `%APPDATA%\n-f.pro\Abyss\Presets\*.abyss` (XML, über `File::userApplicationDataDirectory`).
- Klick auf das Logo „ABYSS“ (Rechteck x 50–189, y 4–31) öffnet das Preset-Menü (`juce::PopupMenu`, asynchron): Factory ▸, User ▸, Save…, Load…, A/B tauschen, A→B kopieren.
- Undo/Redo per `juce::UndoManager` an der APVTS, Strg+Z / Strg+Y im Editor.

### 4.5 Rechtsklick-Menü (überall im Editor außer auf Knöpfen)
Mix (0/25/50/75/100 %), Ceiling (−6/−3/−1/−0.3/0 dBFS), Sub Mode Clean/Grit, Sub −2 ✓, Auto Gain ✓, Oversampling Off/2x/4x, UI-Größe 2x/3x/4x/5x, Animation ✓, CRT-Effekt ✓, „Reset all“. Mix/Ceiling/Sub Mode/Sub −2 sind außerdem normale Host-Parameter.

### 4.6 Animation: DEPTH → Bildschirm
- Layout nativ (Fenster 240×186): Zuerst der Bildschirm-Inhalt bei **(64, 38)**, 112×74, dann `ui_frame.png` bei (0,0) darüber (verdeckt die CRT-Ecken), dann Knöpfe, Bypass-Button und Werte-Anzeige.
- **Stufen-Mapping** (in `Source\ui\DepthMapping.h`, pure Funktion, deterministisch ohne Float-Rundungsfallen):
  ```cpp
  inline int depthToLevel (float depthPercent) noexcept {
      const int tenths = (int) std::lround (juce::jlimit (0.0f, 100.0f, depthPercent) * 10.0f); // 0..1000
      return juce::jlimit (0, 10, (tenths + 50) / 100); // = round(DEPTH/10), .5 rundet auf
  }
  ```
  Erwartet: 0→0, 4.9→0, 5→1, 10→1, 14.9→1, 15→2, …, 45→5, 50→5, 55→6, …, 94.9→9, 95→10, 100→10. Stufe L zeigt `anim\depth_{L*10:03d}_fN.png`.
- Für die Anzeige wird der UI-Parameterwert verwendet (nicht der Audio-Smoother).
- **Loop:** `juce::Timer` mit **8 Hz** (`startTimerHz(8)`). Pro Tick ein Schritt der Sequenz **`f0, f1, f0, f1, f0, f1, f0, f2`** (8 Schritte = 1 s; steht auch in `palette.json` → `animation.sequence`). f0/f1 = Flackern/Wackeln, f2 = einmal pro Sekunde Blinzeln. Der Timer läuft **nur**, wenn `animate == true`, Bypass aus und der Editor sichtbar ist (`isShowing()`). Ändert sich eine Bedingung, wird der Timer sofort gestartet bzw. gestoppt. Ohne Animation wird immer f0 gezeigt.
- **Dirty-Rect:** Ein Tick repaintet **nur** das Bildschirmrechteck (skaliert), nie das ganze Fenster. Beim Laden berechnest du pro Stufe die Bounding-Box der Unterschiede f0/f1/f2 und repaintest nur diese.
- **Tiefenzähler** oben rechts im Bildschirm ist in die Frames gebacken (0 %: `0`, 10 %: `-12`, 20 %: `-48`, 30 %: `-130`, 40 %: `-270`, 50 %: `-480`, 60 %: `-760`, 70 %: `-1100`, 80 %: `-1550`, 90 %: `-2300`, 100 %: `-6666`). Nicht nachzeichnen.
- Inhalt der Stufen (steckt in den Sprites): 0 % Figur steht oben am Schachtrand, Nachthimmel, Fels am hellsten, keine Augen · 10–30 % Figur fällt, Schacht wird enger und dunkler · 40–60 % erste kleine rote Augen tief unten · 70–90 % große Augen mit Schlitzpupille, weitere Augenpaare in den Felswänden · 100 % fast schwarz, zwei riesige Augen füllen den Bildschirm.
- **Bypass:** Bildschirm zeigt `ui\screen_off.png` („INSERT COIN“), Button zeigt `bypass_on.png`, Timer aus, kein PRESSURE-Effekt.
- **PRESSURE-Bildschirmeffekt** (wenn `crtFx == true`, nur auf den Bildschirm-Inhalt, siehe 4.10).

### 4.7 Performance-Vorgaben
- DSP ≤ 0,8 % eines Kerns pro Stereo-Instanz bei 48 kHz, Puffer 128, OS 2x. OS 4x ≤ 1,4 %, OS Off ≤ 0,4 %.
- Blockweise, SIMD-freundlich (`juce::dsp::AudioBlock`, innere Schleifen ohne Verzweigungen wo möglich, keine `std::function`/`virtual` in Hot-Loops). Der Tracker ist von Natur aus seriell (Zustandsmaschine pro Sample), halte ihn schlank (nur Mono-Sidechain, keine Divisionen pro Sample außer bei Triggern). `juce::ScopedNoDenormals` in `processBlock`.
- Gains per Sample-Rampe, Filterkoeffizienten nur per 16er-Subblock bzw. Block und nur bei Änderung.
- Early-out: DEPTH = 0 und PRESSURE = 0 → nur Gains/Delay (Neutralpfad). Bypass nach Crossfade → nur Delay-Kopie.
- Keine Allokation nach `prepareToPlay`. Audio → UI nur über `std::atomic`.

### 4.8 Fehlende Assets
Fehlt eine Datei aus Abschnitt 2, brich **nicht** ab. Baue sie im gleichen Stil nach: nur die 19 Palettenfarben, Dithering (Bayer 4×4) statt Verläufen, 1-px-Konturen in `K0`, gleiche Maße/Frame-Zahl/Dateinamen. Dafür legst du ein Konsolen-Target `AbyssAssetGen` an (C++/JUCE, `PNGImageFormat`), das die fehlenden PNGs schreibt, bevor das Plugin gebaut wird. Vorlagen:
- **Knob 24×24:** Kreis r=11, Kontur `K0`, gerändelter Ring (20 Kerben `V0/V1` ↔ `V2/V3`, dreht mit), innere Kappe `V0…V3` mit Licht von oben links, Zeiger 2 px von 0,12·r bis 0,74·r (`N2` für DEPTH, `R3` für PRESSURE), −135° … +135°, 64 Frames. **Klein 12×12:** r=5, Kappe `C0/C1/C2`, Zeiger `K0`, 32 Frames.
- **Bypass 20×16:** Ellipse 20×16 `K0`, Seitenwand `R0` (off) / `R1` (on), Kappe `R0/R1/R2` (off) bzw. `R2/R3` + Glanz `N2` (on), bei on ein Dither-Halo `R1` um den Button.
- **screen_off 112×74:** `K0`, vereinzelte `K1/V0`-Pixel, „INSERT“ / „COIN“ in `C2` mit Schatten `C0` zentriert, darunter „BYPASS“ in `N0`.
- **anim-Frames:** Fehlen einzelne, kopiere den nächstgelegenen vorhandenen Frame derselben Stufe (f1/f2 → f0) und vermerke es. Fehlt `anim\` komplett, ist das ein Fehler, den du im Bericht deutlich meldest. Generiere dann einen einfachen Ersatzsatz (Schacht mit Fels in `RAMP`, Figur `N2`, Augen ab 040).
- **palette.json fehlt:** Werte aus Abschnitt 4.6, 4.9, 4.10, 4.11 hart codieren.
Jedes generierte Asset im Bericht auflisten.

### 4.9 Pixel-Rendering, Skalierung, Knöpfe
- **Nur Nearest-Neighbor mit ganzzahligem Faktor.** UI-Größe 2x/3x (**Default**, 720×558)/4x/5x über das Rechtsklick-Menü, gespeichert im State. `setResizable(false, false)`, `setSize(240*s, 186*s)`.
- Skalieren machst du selbst in `PixelScaler` (Zeilen-/Pixelkopie in ein `juce::Image` der Zielgröße per `Image::BitmapData`), nicht per `drawImage`-Resampling. `paint()` blittet nur fertige Caches 1:1 mit `g.setImageResamplingQuality(Graphics::lowResamplingQuality)`. HiDPI: Wenn `getApproximateScaleFactorForComponent` ≠ 1, zeichne in ein Image mit physischem Integer-Faktor `round(s * displayScale)` und blitte es so, dass ein Logikpixel genau auf ganze physische Pixel fällt. Pixel müssen bei 100 %, 125 %, 150 % Windows-Skalierung quadratisch und scharf bleiben.
- **Layout (native Fensterkoordinaten):**

| Element | Position / Größe | Hit-Area | Strip |
|---|---|---|---|
| Logo / Preset-Menü | x 50–189, y 4–31 | Rechteck | – |
| Bildschirm | (64, 38), 112×74 | – | anim / screen_off |
| DEPTH | (43, 132) 24×24, Mitte (54, 143) | Kreis r=13 | `knob_depth_strip`, `frame = lround(norm*63)` |
| IN | (79, 140) 12×12, Mitte (84, 145) | Kreis r=7 | `knob_small_strip`, `lround(norm*31)` |
| OUT | (101, 140) 12×12, Mitte (106, 145) | Kreis r=7 | `knob_small_strip`, `lround(norm*31)` |
| PRESSURE | (133, 132) 24×24, Mitte (144, 143) | Kreis r=13 | `knob_pressure_strip`, `lround(norm*63)` |
| BYPASS | (182, 136) 20×16, Mitte (192, 144) | Ellipse 20×16 | `bypass_off` / `bypass_on` |
| Werte-Anzeige | Plakette x 74–165, y 113–122, Innenfläche `K0` x 74+1…165−1, y 113+1…122−1 | – | PixelFont |

- `norm` ist der normalisierte Parameterwert (0…1) des jeweiligen Parameters (IN/OUT über ihren dB-Bereich, 0 dB ist also bei IN in der Mitte, bei OUT bei Frame `lround(24/36*31)` = 21).
- **Knöpfe:** `juce::Slider` RotaryVerticalDrag mit eigenem LookAndFeel, das nur den Strip-Frame blittet. Mausrad ±1 %, Shift/Strg-Drag fein (×0,1), Doppelklick = Default, Rechtsklick auf Knopf = Wert eintippen (kleiner `TextEditor` im Pixelstil oder `AlertWindow`). Accessibility-Titel „DEPTH“, „PRESSURE“, „Input“, „Output“, „Bypass“. Gesten mit `beginChangeGesture`/`endChangeGesture` (über `SliderAttachment`). BYPASS ist ein `juce::Button` (Toggle) mit `ButtonAttachment`, gedrückt-Zustand zeigt kurz `bypass_on` 1 px tiefer.
- **Werte-Anzeige:** Dauerhaft in der Plakette, Text `"D 50%  P 30%"` (ganze Prozent, zwei Leerzeichen dazwischen), Farbe `C2` (#59E3EA), y = 115, x = `(74 + 165 + 1 − textWidth) / 2`. Beim Ziehen zusätzlich ein Pixel-Tooltip über dem Knopf (z. B. `"63%"` bzw. `"+3.5DB"` für IN/OUT, Rahmen `K0`, Fläche `N2`, Text `K0`). Nur neu zeichnen, wenn sich der angezeigte Text ändert. Im Bypass steht dort `"BYPASS"` in `R3`.
- **PixelFont 5×7** (`Source\ui\PixelFont.*`), Zeichenbreite 6 px inkl. 1 px Abstand (`textWidth = len*6 − 1`), Höhe 7, `#` = gesetzt. Genau diese Glyphen (identisch zu den gebackenen Labels):
```
A " ### ","#   #","#   #","#####","#   #","#   #","#   #"   B "#### ","#   #","#   #","#### ","#   #","#   #","#### "
C " ####","#    ","#    ","#    ","#    ","#    "," ####"   D "#### ","#   #","#   #","#   #","#   #","#   #","#### "
E "#####","#    ","#    ","#### ","#    ","#    ","#####"   F "#####","#    ","#    ","#### ","#    ","#    ","#    "
G " ####","#    ","#    ","#  ##","#   #","#   #"," ### "   H "#   #","#   #","#   #","#####","#   #","#   #","#   #"
I "#####","  #  ","  #  ","  #  ","  #  ","  #  ","#####"   K "#   #","#  # ","# #  ","##   ","# #  ","#  # ","#   #"
L "#    ","#    ","#    ","#    ","#    ","#    ","#####"   M "#   #","## ##","# # #","#   #","#   #","#   #","#   #"
N "#   #","##  #","# # #","#  ##","#   #","#   #","#   #"   O " ### ","#   #","#   #","#   #","#   #","#   #"," ### "
P "#### ","#   #","#   #","#### ","#    ","#    ","#    "   R "#### ","#   #","#   #","#### ","# #  ","#  # ","#   #"
S " ####","#    ","#    "," ### ","    #","    #","#### "   T "#####","  #  ","  #  ","  #  ","  #  ","  #  ","  #  "
U "#   #","#   #","#   #","#   #","#   #","#   #"," ### "   V "#   #","#   #","#   #","#   #"," # # "," # # ","  #  "
W "#   #","#   #","#   #","#   #","# # #","## ##","#   #"   X "#   #","#   #"," # # ","  #  "," # # ","#   #","#   #"
Y "#   #","#   #"," # # ","  #  ","  #  ","  #  ","  #  "   ' "  #  ","  #  "," #   ","     ","     ","     ","     "
% "##  #","## # ","  #  ","  #  "," #   ","# ## ","#  ##"   - "     ","     ","     ","#####","     ","     ","     "
+ "     ","  #  ","  #  ","#####","  #  ","  #  ","     "   . "     ","     ","     ","     ","     "," ##  "," ##  "
: "     "," ##  "," ##  ","     "," ##  "," ##  ","     "   / "    #","    #","   # ","  #  "," #   ","#    ","#    "
! "  #  ","  #  ","  #  ","  #  ","  #  ","     ","  #  "   ? " ### ","#   #","    #","   # ","  #  ","     ","  #  "
0 " ### ","#   #","#  ##","# # #","##  #","#   #"," ### "   1 "  #  "," ##  ","  #  ","  #  ","  #  ","  #  "," ### "
2 " ### ","#   #","    #","   # ","  #  "," #   ","#####"   3 "#### ","    #","    #"," ### ","    #","    #","#### "
4 "#   #","#   #","#   #","#####","    #","    #","    #"   5 "#####","#    ","#### ","    #","    #","#   #"," ### "
6 " ### ","#    ","#    ","#### ","#   #","#   #"," ### "   7 "#####","    #","   # ","  #  ","  #  ","  #  ","  #  "
8 " ### ","#   #","#   #"," ### ","#   #","#   #"," ### "   9 " ### ","#   #","#   #"," ####","    #","    #"," ### "
(Leerzeichen = 7 leere Zeilen)
```
Keine TrueType-Fonts im Editor.

### 4.10 PRESSURE → Bildschirmeffekt (Rotstich + Bildstörung)
Nur auf den Bildschirm-Inhalt (nicht auf Rahmen/Knöpfe), nur wenn `crtFx == true` und nicht im Bypass. Werte stehen in `palette.json` → `pressure_fx` (aus der Datei lesen, Tabelle = Fallback und Testreferenz).
- Stufe `stage = (lround(pressure) * 3 + 50) / 100` (Integer, also round(P·3/100) → 0…3).
- **Rotstich (Index-Remap, bleibt in der Palette):**

| Stufe | Remap (Farbname → Farbname) |
|---|---|
| 0 | – |
| 1 | `V1→R0`, `B1→V1` |
| 2 | `V0→R0`, `V1→R0`, `B1→R0`, `V2→R1`, `B2→V2` |
| 3 | `K1→R0`, `V0→R0`, `V1→R1`, `B1→R0`, `V2→R1`, `B2→R1`, `V3→R2`, `V4→R2`, `N0→R2`, `N1→R3` |

- **Bildstörung (Zeilenversatz):** pro Stufe und Frame (f0/f1/f2) werden einzelne Zeilen des Bildschirms horizontal verschoben: `out[x] = in[clamp(x − dx, 0, 112−1)]`. Reihenfolge: erst Zeilenversatz, dann Remap (beides ist vertauschbar, Test nutzt diese Reihenfolge).

| Stufe | f0 | f1 | f2 |
|---|---|---|---|
| 0 | – | – | – |
| 1 | – | – | – |
| 2 | y17:+1, y46:-1 | y30:+1, y31:+1, y58:-1 | y9:-1, y52:+1 |
| 3 | y12:+2, y13:+2, y40:-1, y41:-2, y64:+1 | y24:-2, y25:-2, y26:-1, y50:+2, y51:+1 | y6:+1, y33:-2, y34:-2, y60:+2, y61:+2 |

- Umsetzung: Remap = 19-Einträge-Index-LUT → ARGB-LUT, also kein Mehraufwand beim Expandieren (Abschnitt 4.11). Varianten werden pro (Stufe, Frame, PRESSURE-Stufe, Skalierung) einmal erzeugt und gecacht.

### 4.11 Palette & Index-Bilder
Das ganze Plugin besteht aus diesen 19 Farben (Index = Reihenfolge in `palette.json` → `colors`):

| Index | Name | Hex | Rolle |
|---|---|---|---|
| 0 | `K0` | `#050408` | fast Schwarz (Grundton, Konturen) |
| 1 | `K1` | `#0C0A12` | Schwarz-Violett |
| 2 | `V0` | `#16122A` | dunkles Violett |
| 3 | `V1` | `#221B37` | Violett (Gehäuse) |
| 4 | `B1` | `#1B2539` | dunkles Blaugrau (Fels) |
| 5 | `V2` | `#31294B` | Violett hell |
| 6 | `B2` | `#2B3B55` | Blaugrau hell (Fels) |
| 7 | `V3` | `#473D67` | Violett-Grau (Kanten) |
| 8 | `V4` | `#5F5585` | hellstes Violett (Felskante) |
| 9 | `N0` | `#6E6658` | Knochen dunkel (Staub, Ticks) |
| 10 | `N1` | `#ABA28C` | Knochen (Zähler, Ticks) |
| 11 | `N2` | `#E9E2CC` | Knochenweiß (Figur, Labels) |
| 12 | `R0` | `#2E070B` | Blut sehr dunkel (Glut) |
| 13 | `R1` | `#6A0E16` | Blut dunkel |
| 14 | `R2` | `#A8161F` | Blutrot |
| 15 | `R3` | `#E23A32` | Blutrot hell (Augen-Kern, Logo-Glanz) |
| 16 | `C0` | `#0B2E34` | Cyan dunkel |
| 17 | `C1` | `#1A7F8A` | Cyan |
| 18 | `C2` | `#59E3EA` | Cyan hell (Werte-Anzeige) |

1. Beim Editor-Start alle Assets einmal dekodieren und in **Index-Bilder** umwandeln (`uint8` pro Pixel: 0–18, 255 = transparent). Zuordnung per exaktem RGB-Vergleich. Ein unbekannter Pixel ist ein Fehler (Assert + Test).
2. Pro benötigtem Bild (Rahmen, Strips, Bypass, 3 Frames der aktuellen Stufe, ±1 Nachbarstufe vorladen) **einmal** Index → ARGB → Integer-Skalierung und cachen. Cache-Schlüssel (Bild, Stufe, Frame, PRESSURE-Stufe, Skalierung), LRU mit Obergrenze ~24 MB.
3. Die Animation blittet nur fertige Cache-Bilder. Die Index-Bilder dürfen als `SharedResourcePointer` zwischen Editor-Instanzen geteilt werden.

### 4.12 Editor-Performance
- Kein Repaint ohne Änderung. Parameter-Änderungen kommen über `APVTS::Listener` → `std::atomic<bool>`-Flags → `juce::AsyncUpdater` (oder den laufenden Timer) → `repaint(Rectangle)` nur für betroffene Bereiche (Knopf, Plakette, Bildschirm).
- Ziel: Editor < 1 % CPU im Leerlauf (Timer aus), < 2 % mit Animation bei 3x.
- Editor öffnen/schließen darf keinen Speicher verlieren.

## 5. Phasen
1. **Setup:** Assets prüfen (`scripts\check_assets.ps1`), Git, `.gitignore`, CMake-Gerüst mit FetchContent JUCE 8.0.15, leerer Processor/Editor, konfigurieren + bauen:
   ```powershell
   & $CMake -S "$Repo" -B "$Repo\build" -G "Visual Studio 17 2022" -A x64 -DABYSS_STANDALONE=ON
   & $CMake --build "$Repo\build" --config Release --parallel
   ```
2. **DSP:** komplette Kette (Tracker, Sub, Darkener, Comp, Drive, AutoGain, Clipper), Parameter, Oversampling/Latenz, Mix, Bypass, State, Presets. Dazu `AbyssTests` und `AbyssBench`.
3. **UI:** IndexedImage, SpriteBank (33 Frames + 7 UI-Bilder), PixelScaler, Layout, Knöpfe, Bypass-Button, Werte-Anzeige, Animation-Timer mit Sequenz, PRESSURE-Bildschirmeffekt, Rechtsklick- und Preset-Menü, Skalierung, Undo.
4. **Fehlende Assets** (falls nötig, Abschnitt 4.8).
5. **Selbstkontrolle** (Abschnitt 6), bis alles grün ist.
6. **README.md** (Bedienung, Build, Tests) und `scripts\build_win.ps1` (konfigurieren, bauen, Tests, pluginval in einem Rutsch).
7. **Finaler Release-Build + Installation** (Abschnitt 7) und Abschlussbericht (Abschnitt 8).

## 6. Selbstkontrolle (Pflicht-Checkliste, alles muss grün sein)
Führe jeden Punkt tatsächlich aus. Schreibe die Ergebnisse in `build\selfcheck_report.md` (Punkt, Befehl, Ergebnis, Messwert). Ist ein Punkt rot: Ursache beheben, **neu bauen und die komplette Checkliste wiederholen**. Höchstens 5 komplette Runden. Bleibt danach etwas rot, dokumentiere es ehrlich mit Ursache im Bericht und fahre trotzdem mit Phase 7 fort.

1. **Build sauber:** Clean-Rebuild Release (`--clean-first`). **0 Warnungen aus eigenen Dateien** (`Source\`, `tests\`, `bench\`) unter `/W4`. Prüfe das per `Select-String` auf das Build-Log nach `warning C` mit Pfad unter dem Repo, außer `build\_deps`. Warnungen aus JUCE selbst sind erlaubt, aber zählen und im Bericht nennen. 0 Fehler.
2. **pluginval** (Strictness 10) gegen das frisch gebaute Bundle:
   ```powershell
   $PV = "$Repo\tools\pluginval\pluginval.exe"
   & $PV --strictness-level 10 --validate "$Repo\build\Abyss_artefacts\Release\VST3\Abyss.vst3" --output-dir "$Repo\build\pluginval" --timeout-ms 600000
   if ($LASTEXITCODE -ne 0) { throw "pluginval FAILED" }
   ```
   Muss mit Exit-Code 0 durchlaufen (inkl. Editor-Tests). Log im Bericht zusammenfassen.
3. **CTest:** `& $CTest --test-dir "$Repo\build" -C Release --output-on-failure` → alle Tests bestanden. `AbyssTests` muss mindestens enthalten:
   - **Null-Tests** (nach Latenzkompensation, Rosa Rauschen + Sinus, Spitzen ≤ −6 dBFS, 48 kHz und 44,1 kHz, Mono und Stereo): (a) DEPTH 0, PRESSURE 0, Mix 100, Gains 0 dB, OS Off → Ausgang **bit-identisch** zum Eingang. Mit OS 2x/4x Rest < −90 dBFS. (b) Mix 0 % (bei DEPTH 100, PRESSURE 100) → < −120 dBFS. (c) Bypass an (nach Crossfade) bei jeder OS-Stufe, DEPTH 100 und PRESSURE 100 → < −120 dBFS gegenüber dem um die gemeldete Latenz verzögerten Eingang.
   - **Latenz:** `getLatencySamples()` nach jedem OS-Wechsel = gemessene Latenz (Impuls-Test, ±0 Samples).
   - **Sub-Octaver:** Sinus −12 dBFS bei 55, 110 und 196 Hz, DEPTH 100, PRESSURE 0, Mix 100, autoGain aus, OS Off, nach 0,5 s Einschwingen (Goertzel über 1 s): Clean-Modus → Anteil bei f/2 ≥ −24 dBFS, mit `sub2` an Anteil bei f/4 ≥ −30 dBFS, mit `sub2` aus f/4 < −60 dBFS. Grit-Modus → f/2 ≥ −30 dBFS. DEPTH 0 → f/2 < −100 dBFS. Die gemessene Sub-Frequenz (Nulldurchgänge des Sub-Anteils) liegt innerhalb ±1 % von f/2.
   - **Sub klickfrei:** Bei stationärem 110-Hz-Sinus ist im Clean-Modus die maximale Sample-Differenz des Sub-Anteils ≤ 1,5 × `2π·55/fs·Amplitude`. Bei einem Ton-Wechsel 110 → 146,8 Hz (hart) entsteht kein Sample > Ceiling und keine Differenz > 3 × Steady-State.
   - **Gate:** Rosa Rauschen −70 dBFS und Stille → Sub-Anteil < −90 dBFS. 20-kHz-Sinus und Gleichspannung → kein Sub (Tracker `valid == false`).
   - **Darkener:** Rosa Rauschen −18 dBFS, PRESSURE 0, autoGain aus: Energie im Band 5–20 kHz gegenüber dem Eingang bei DEPTH 0 / 25 / 50 / 75 / 100 = 0 dB / ≤ −3 / ≤ −12 / ≤ −20 / ≤ −30 dB, streng monoton fallend.
   - **PRESSURE:** 100-Hz-Sinus −12 dBFS, DEPTH 0: THD steigt streng monoton über PRESSURE 0/25/50/75/100, bei 100 THD ≥ 5 %. Crest-Faktor eines synthetischen Drum-Signals (abklingende 60-Hz-Bursts + Rauschen) sinkt monoton.
   - **AutoGain:** Rosa Rauschen −18 dBFS, alle Kombinationen DEPTH × PRESSURE ∈ {0, 50, 100}, nach 2 s: |RMS_out − RMS_in| ≤ 1,5 dB mit autoGain an.
   - **Robustheit:** keine NaN/Inf/Denormals im Ausgang bei Stille, +40 dBFS-Rauschen, DC, Einzel-Impuls, Sweep 20 Hz–20 kHz, `1e-30`-Rauschen (Denormal-Check: kein |y| zwischen 0 und 1e-30) für DEPTH 0/50/100 × PRESSURE 0/50/100 × subMode × alle OS.
   - **Gain-Staging:** Clipper-Ausgang (im OS-Bereich) nie > Ceiling + 0,05 dB, Plugin-Ausgang nie > Ceiling + 0,5 dB (OS an) bzw. + 0,05 dB (OS Off, Mix 100, Out 0 dB). In/Out-Gain exakt ±dB (bei DEPTH 0, PRESSURE 0, OS Off, Toleranz 0,01 dB).
   - **Automation ohne Klicks:** 1-kHz-Sinus und 110-Hz-Sinus −12 dBFS, DEPTH springt alle 2048 Samples hart zwischen 0 und 100, ebenso PRESSURE 0↔100, subMode, sub2, In/Out ±12 dB, Mix 0↔100, Bypass an/aus, OS-Wechsel. Kriterium: max. |y[n] − y[n−1]| im Umschaltbereich ≤ 1,5 × das Maximum im eingeschwungenen Zustand mit dem jeweils höheren Wert. Kein Sample > Ceiling + 0,5 dB.
   - **`fastTanh`/`fastSin`:** Fehler < 1e-3 bzw. < 1e-4 gegenüber `std::tanh`/`std::sin`, `fastTanh` monoton in ±3.
   - **Keine Allokation im Audio-Thread:** Test-Build mit überschriebenem globalem `operator new`/`delete`, der während `processBlock` (thread_local Flag) zählt. Erwartet 0 Allokationen über 10 s Audio inkl. aller Parameterwechsel und OS-Umschaltungen.
   - **State:** Roundtrip (alle Parameter + uiScale/animate/crtFx), leerer und kaputter Blob crasht nicht, alle 10 Factory-Presets laden und setzen DEPTH/PRESSURE/subMode korrekt.
   - **Assets / Animation:** alle 33 Frames als BinaryData vorhanden und dekodierbar, je 112×74. Alle 7 UI-Bilder mit den Maßen aus Abschnitt 2, Strips 64/64/32 Frames. Jedes Pixel ist eine der 19 Palettenfarben (bzw. Alpha 0 in `ui`, ohne `screen_off`). `ui_frame` hat genau 8268 transparente Pixel, alle im Bildschirmrechteck. f0 ≠ f1 in allen Stufen. f2 == f0 bei 000–030, f2 ≠ f0 ab 040. Keine `R1/R2/R3`-Pixel unter 040, danach streng steigend.
   - **Stufen-Mapping + Render-Test:** `depthToLevel` für 0, 5, 10, …, 100 (alle 21 Werte) gegen die Tabelle `round(d/10)` mit .5 aufrunden, plus 4.9, 14.9, 49.9, 94.9. Editor offscreen erzeugen (`ScopedJuceInitialiser_GUI`, Scale 1x, PRESSURE 0, animate aus) und für jedes DEPTH aus 0, 5, …, 100 per `createComponentSnapshot` prüfen: Pixel in den transparenten Bereichen von `ui_frame` entsprechen **pixelgenau** `anim\depth_{level*10:03d}_f0.png`, alle übrigen Pixel außerhalb von Knöpfen, Button und Plakette **pixelgenau** `ui_frame.png`. Bei Bypass muss `screen_off.png` erscheinen.
   - **PRESSURE-Effekt:** Für PRESSURE 0/33/67/100 bei DEPTH 60 entspricht der Bildschirm exakt der Referenz aus Zeilenversatz + Remap laut Abschnitt 4.10 (im Test selbst aus dem unveränderten Frame berechnet). `crtFx = false` → Bildschirm unverändert.
   - **Skalierung:** Snapshots bei 2x–5x sind exakt das Nearest-Neighbor-Upscaling des 1x-Snapshots.
   - **Timer:** Bei Bypass, animate=false oder unsichtbarem Editor ist `isTimerRunning()` false, sonst true. 8 Ticks durchlaufen exakt f0,f1,f0,f1,f0,f1,f0,f2.
4. **Speicherverhalten:** In `AbyssTests` Editor 50× erzeugen/zerstören und je 1 s animieren. Private Bytes (`GetProcessMemoryInfo`) nach Runde 5 vs. Runde 50: Zuwachs < 5 MB. Debug-Build einmal mit JUCE-Leak-Detector laufen lassen: keine Leaks gemeldet. Speicher-Peak des Editors bei 5x im Bericht nennen.
5. **CPU-Benchmark:** `& "$Repo\build\AbyssBench_artefacts\Release\AbyssBench.exe"` (Pfad ggf. anpassen). 60 s Rosa Rauschen + 60 s Bass-Sinusfolge (Tracker aktiv), 48 kHz, Stereo, Blockgrößen 64/128/512, OS Off/2x/4x, DEPTH × PRESSURE ∈ {0, 50, 100}. Ausgabe als Tabelle: Realtime-Faktor und % eines Kerns. Muss die Grenzen aus 4.7 einhalten. Wenn nicht: optimieren (Autovektorisierung prüfen mit `/Qvec-report:2`, ggf. `juce::dsp::SIMDRegister`) und neu messen. Editor-CPU (Leerlauf und Animation, 3x) per `GetProcessTimes` im Standalone oder Test über 10 s messen.
6. **Asset-Pfade:** Das installierte Plugin darf zur Laufzeit **keine** Dateien aus dem Repo lesen (alles BinaryData). Prüfe das, indem du das Bundle in einen Temp-Ordner kopierst, `anim` vorübergehend umbenennst und pluginval kurz (Strictness 5) gegen die Temp-Kopie laufen lässt. Danach `anim` zurückbenennen und mit `git status` prüfen, dass nichts fehlt.
7. **Standalone-Smoke-Test:** `"$Repo\build\Abyss_artefacts\Release\Standalone\Abyss.exe"` starten, 5 s laufen lassen, beenden. Kein Crash, sauberer Exit-Code.

## 7. Finaler Release-Build & Installation
1. Version im Bericht vermerken, Clean-Rebuild Release, CTest und pluginval noch einmal (beides grün).
2. Quelle: `"$Repo\build\Abyss_artefacts\Release\VST3\Abyss.vst3"`. Das ist ein **Ordner** (Bundle) und muss komplett kopiert werden.
3. **Benutzer-Ordner** (ohne Admin):
   ```powershell
   $Src  = "$Repo\build\Abyss_artefacts\Release\VST3\Abyss.vst3"
   $User = Join-Path $env:LOCALAPPDATA "Programs\Common\VST3"
   New-Item -ItemType Directory -Force -Path $User | Out-Null
   if (Test-Path -LiteralPath "$User\Abyss.vst3") { Remove-Item -LiteralPath "$User\Abyss.vst3" -Recurse -Force }
   Copy-Item -LiteralPath $Src -Destination $User -Recurse -Force
   ```
4. **System-Ordner** `C:\Program Files\Common Files\VST3\` (braucht Admin): Lege `scripts\install_system.ps1` an. Es nimmt `-Src` als Parameter, löscht ein vorhandenes `C:\Program Files\Common Files\VST3\Abyss.vst3` und kopiert per `robocopy "$Src" "C:\Program Files\Common Files\VST3\Abyss.vst3" /MIR /NJH /NJS /NP` (robocopy-Exit-Codes 0–7 = Erfolg) und schreibt ein Log nach `$env:TEMP\abyss_install.log`. Starte es erhöht:
   ```powershell
   Start-Process -FilePath "powershell.exe" -Verb RunAs -Wait -ArgumentList @(
     "-NoProfile","-ExecutionPolicy","Bypass","-File","`"$Repo\scripts\install_system.ps1`"","-Src","`"$Src`"")
   ```
   Schreib vorher in den Chat: „Gleich erscheint eine Windows-Admin-Abfrage (UAC). Bitte mit Ja bestätigen.“ Wird die Abfrage abgelehnt oder schlägt die Elevation fehl, bricht das den Auftrag nicht ab. Melde es im Bericht und gib den exakten Befehl an, den der Nutzer in einer Admin-PowerShell selbst ausführen kann.
5. **Verifikation:** Für beide Ziele prüfen, dass `Abyss.vst3\Contents\x86_64-win\Abyss.vst3` existiert und dieselbe Größe und denselben SHA256-Hash (`Get-FileHash`) wie die Quelle hat. Danach pluginval (Strictness 10) einmal gegen die installierte Kopie in `C:\Program Files\Common Files\VST3\Abyss.vst3` (bzw. die Benutzer-Kopie, falls die System-Kopie fehlt).
6. `scripts\install.ps1` enthält die Schritte 3–5, damit der Nutzer später selbst neu installieren kann. Abschluss-Commit „Release 1.0.0“.

## 8. Abschlussbericht (im Chat, auf Deutsch)
1. **Gebaut:** Kurzbeschreibung, JUCE-Version, Formate, Dateigrößen, Commit-Hash.
2. **Selbstkontrolle:** Tabelle aller Punkte aus Abschnitt 6 mit ✅/❌ und Messwerten (Warnungen, pluginval-Ergebnis, Null-Test-Reste in dBFS, Sub-Pegel f/2 und f/4, Darkener-Dämpfung je DEPTH, THD je PRESSURE, AutoGain-Abweichung, CPU-% pro OS-Stufe, Editor-CPU, Speicherzuwachs). Wie viele Korrekturrunden nötig waren.
3. **Pfade:** Quelle, beide installierten Kopien (mit Hash-Check-Ergebnis), Standalone-EXE, `build\selfcheck_report.md`, pluginval-Logs.
4. **Generierte/fehlende Assets** (falls vorhanden) und alle eigenen Entscheidungen bzw. Abweichungen von diesem Dokument (inkl. Feintuning von Kurven).
5. **Bekannte Einschränkungen:** z. B. dass ein Zero-Crossing-Tracker bei Akkorden und sehr verrauschten Signalen unsicher ist (Sub wird dann auf 50 % gezogen), dass der Klang per Hörvergleich abgestimmt werden muss und dass nur Windows getestet ist.
6. **So testest du in der DAW:** DAW starten → Plugin-Rescan (Ableton: Einstellungen › Plug-Ins › „VST3-Systemordner verwenden“ an + „Neu scannen“; FL Studio: Optionen › Plugin-Manager › „Find plugins“; Reaper: Optionen › Einstellungen › VST › „Re-scan“; Bitwig/Cubase: Neustart scannt automatisch) → „Abyss“ von „n-f.pro“ auf eine Bass-, 808- oder Lead-Spur legen → DEPTH 0→100 drehen (Bildschirm: Figur fällt in den Schacht, rote Augen kommen näher), PRESSURE 0→100 (Bild wird rot und stört) → Bypass für A/B (Bildschirm „INSERT COIN“). Empfohlene Hör-Checks: Mono-Bassline mit DEPTH 50/100 sauber eine bzw. zwei Oktaven tiefer, kein Flattern bei gehaltenen Tönen, Clean vs. Grit, DEPTH-Sweep ohne Zipper, PRESSURE 100 mit OS 2x/4x ohne hörbares Aliasing.
