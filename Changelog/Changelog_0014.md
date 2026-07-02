# Changelog_0014.md

**Datum:** 2026-07-02
**Bearbeitete Prioritaet:** AP-91 — Metrics Dashboard
**Ziel:** Zentrales Dashboard fuer alle Engine-Metriken (Memory, Network, ThreadPool, ECS, Renderer)

---

## Geaenderte Dateien

| Datei | Aenderung |
|-------|-----------|
| `main.cpp` | MetricsDashboard-Initialisierung, F6-Toggle, Update-Call, Shutdown |
| `CMakeLists.txt` | `metrics/MetricsDashboard.cpp` hinzugefuegt |
| `tests/CMakeLists.txt` | `Test_MetricsDashboard.cpp` hinzugefuegt |
| `AAA_ROADMAP_STATUS.md` | AP-91 auf "Erledigt" gesetzt |
| `PRIORITAETENLISTE.md` | AP-91 Status aktualisiert |

## Neue Dateien

| Datei | Beschreibung |
|-------|-------------|
| `metrics/MetricsDashboard.h` | Dashboard-Header: 7 Kategorien, Alert-System, Historie, Sparklines |
| `metrics/MetricsDashboard.cpp` | Vollstaendige ImGui-Implementation mit 7 Tabs |
| `tests/Test_MetricsDashboard.cpp` | 10 Tests + 1 Benchmark |

## Entfernte Dateien

Keine.

## Technische Aenderungen

- **AP-91 FIX:** Vollstaendiges Metrics Dashboard als zentrale Monitoring-Oberflaeche.
- **7 Tabs:** Uebersicht, Speicher, Netzwerk, ThreadPool, ECS, Renderer, Alerts.
- **Alert-System:** 4 Schweregrade (Info, Warning, Error, Critical) mit Deduplizierung.
- **Automatische Pruefungen:** Memory-Leaks, Fragmentierung, RTT, Paketverlust, FPS, Task-Queue.
- **Sparkline-Graphen:** Echtzeit-Verlaeufe fuer FPS, Speicher, RTT, Pending Tasks, Frame-Zeit.
- **Historische Daten:** 300 Punkte (5 Minuten bei 1s Intervall), konfigurierbar.
- **Farbkodierung:** Gruen/Orange/Rot fuer Qualitaet, Schweregrad-Farben fuer Alerts.
- **ImGui-Tabellen:** Sortierbare Tabellen fuer Verbindungen, Allokatoren, Alerts.
- **Bestaetigungs-System:** Einzelne oder alle Alerts bestaetigen/loeschen.
- **Berichtserstellung:** Text-Report mit allen Metriken und Alerts.
- **Thread-Sicherheit:** Mutex-geschuetzte Alerts und Historie, atomare aktuelle Werte.

## Architekturaenderungen

- MetricsDashboard als Singleton — globaler Zugriff ueber `MetricsDashboard::GetInstance()`.
- Forward-Declarations fuer MemoryProfiler/NetworkProfiler (vermeidet zirkulaere Includes).
- Externe globale Variablen fuer gEcsWorld und gThreadPool (wie in main.cpp).
- Integration via Makros `METRICS_UPDATE` und `METRICS_RENDER_WINDOW`.
- Legacy-Metrics-Panel bleibt erhalten (F2), Dashboard ist zusaetzlich (F6).

## Moegliche Auswirkungen

- **Performance-Overhead:** ~100-200us pro Frame fuer Update + Rendering (ImGui).
- **Speicher-Overhead:** ~50 KB fuer Historie + Alerts.
- **Build-Zeit:** Erhoeht sich um ~4% durch zusaetzliche Quelldatei + ImGui-Tabellen.
- **Keine Breaking Changes:** Alle bestehenden Systeme bleiben unveraendert.

## Teststatus

- [x] Test 1: Grundlegende Initialisierung
- [x] Test 2: Alert-Generierung (automatisch bei niedriger FPS)
- [x] Test 3: Alert-Acknowledgement
- [x] Test 4: Historische Daten
- [x] Test 5: Konfiguration
- [x] Test 6: Aktuelle Werte
- [x] Test 7: Berichtserstellung
- [x] Test 8: Alert-Deduplizierung
- [x] Test 9: Kategorie-Wechsel
- [x] Test 10: Shutdown mit aktiven Alerts
- [x] Benchmark: Massen-Update (1000 Frames)

## Bekannte Restrisiken

1. **ImGui-Abhaengigkeit:** Dashboard benoetigt ImGui. Ohne GUI-Context kann nicht gerendert werden.
2. **Update-Intervall:** Default 1 Sekunde. Bei schnellen Aenderungen (z.B. FPS-Drops) kann der Alert verzoegert ausgeloest werden.
3. **Alert-Deduplizierung:** Gleiche Alerts innerhalb 60s werden unterdrueckt. Bei oszillierenden Werten koennten Alerts verpasst werden.
4. **Speicher fuer Historie:** 300 Punkte * ~80 Bytes = 24 KB. Bei sehr langen Laufzeiten koennte dies anwachsen (konfigurierbar).

## Naechster Arbeitsschritt

**AP-02:** Deferred Shading Pipeline
**AP-05:** PBR Material System
**AP-46:** Network Metrics (Teilweise — RTT-Schätzung existiert, aber keine systematische Metrik-Erfassung)
