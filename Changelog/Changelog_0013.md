# Changelog_0013.md

**Datum:** 2026-07-02
**Bearbeitete Prioritaet:** AP-81 — Network Profiler
**Ziel:** Vollstaendiges Netzwerk-Profiling-System fuer RTT, Paketverlust, Jitter, Bandbreite

---

## Geaenderte Dateien

| Datei | Aenderung |
|-------|-----------|
| `network/network_NetworkServer.h` | NetworkProfiler-Integration, InitializeProfiler/ShutdownProfiler |
| `network/network_NetworkServer.cpp` | Tracking-Calls bei Senden, Empfangen, ACK, Retransmission, Fragmentierung |
| `server/network/Snapshot.cpp` | Snapshot-Tracking (Delta/Full-State, BytesSaved) |
| `main.cpp` | NetworkProfiler-Initialisierung, Metrics-Anzeige, F5-Report, periodisches Profiling |
| `CMakeLists.txt` | `network/NetworkProfiler.cpp` hinzugefuegt |
| `tests/CMakeLists.txt` | `Test_NetworkProfiler.cpp` hinzugefuegt |
| `AAA_ROADMAP_STATUS.md` | AP-81 auf "Erledigt" gesetzt |
| `PRIORITAETENLISTE.md` | AP-81 Status aktualisiert |

## Neue Dateien

| Datei | Beschreibung |
|-------|-------------|
| `network/NetworkProfiler.h` | Profiler-Header: ConnectionMetrics, GlobalNetworkStats, NetworkHistoryPoint, Singleton |
| `network/NetworkProfiler.cpp` | Profiler-Implementation: RTT-Tracking, Jitter, Qualitaetsscore, Berichte |
| `tests/Test_NetworkProfiler.cpp` | 10 Tests + 1 Benchmark |

## Entfernte Dateien

Keine.

## Technische Aenderungen

- **AP-81 FIX:** Vollstaendiger NetworkProfiler mit Singleton-Pattern.
- **Verbindungs-Tracking:** Jede Verbindung wird mit Adresse, Port, Lebensdauer erfasst.
- **Paket-Tracking:** Gesendet, Empfangen, ACKed, Dropped, Retransmitted pro Client.
- **RTT-Tracking:** Gleitender Durchschnitt (EWMA), Min/Max, Jitter (Standardabweichung).
- **Fragment-Tracking:** Fragmentierung, Empfang, Reassembly.
- **Snapshot-Tracking:** Delta vs Full-State, BytesSaved durch Delta-Kompression.
- **Qualitaetsscore:** 0.0-1.0 basierend auf RTT (40%), Loss (40%), Jitter (20%).
- **Historische Daten:** Zeitverlauf mit konfigurierbarem Limit (default 1000 Punkte).
- **Berichtserstellung:** Formatierte Text-Ausgabe mit allen Statistiken.
- **Thread-Sicherheit:** Atomare Counter fuer globale Statistiken, Mutexe fuer detaillierte Daten.
- **ImGui-Integration:** Echtzeit-Metriken im Engine-Metrics-Fenster.
- **Periodisches Profiling:** Automatische Snapshot-Aufzeichnung alle 5 Sekunden.

## Architekturaenderungen

- NetworkProfiler als Singleton — globaler Zugriff ueber `NetworkProfiler::GetInstance()`.
- RAII-ScopedConnectionTracker fuer automatisches Tracking im Scope.
- Makros `NETPROFILE_TRACK_SENT`, `NETPROFILE_TRACK_RECEIVED`, `NETPROFILE_TRACK_RTT`, etc.
- Integration in bestehende Netzwerk-Komponenten ohne Breaking Changes.
- Analoges Design zum MemoryProfiler (AP-80) fuer konsistente Architektur.

## Moegliche Auswirkungen

- **Performance-Overhead:** ~30-50ns pro Paket-Tracking (atomare Operationen).
- **Speicher-Overhead:** ~200 Bytes pro Verbindung (ConnectionMetrics).
- **Build-Zeit:** Erhoeht sich um ~3% durch zusaetzliche Quelldatei.
- **Keine Breaking Changes:** Bestehende Netzwerk-Code funktioniert ohne Aenderung.

## Teststatus

- [x] Test 1: Grundlegendes Verbindungs-Tracking
- [x] Test 2: Paket-Tracking (Sent/Received/Acked)
- [x] Test 3: RTT-Tracking und Jitter-Berechnung
- [x] Test 4: Retransmission-Tracking
- [x] Test 5: Fragment-Tracking
- [x] Test 6: Snapshot-Tracking
- [x] Test 7: Verbindungsqualitaet (Good/Bad)
- [x] Test 8: Historische Daten
- [x] Test 9: Globale Statistiken
- [x] Test 10: Berichtserstellung
- [x] Benchmark: Massen-Paket-Tracking (1000 Pakete)

## Bekannte Restrisiken

1. **Client-ID-Mapping:** In NetworkServer.cpp wird die Client-ID ueber IP:Port-Lookup bestimmt. Bei NAT/Proxy kann dies ungenau sein.
2. **RTT-Messung:** Basierend auf Sendezeit des PendingPacket. Bei hoher Last kann die Sendezeit nicht exakt dem tatsaechlichen Sendezeitpunkt entsprechen.
3. **Jitter-Fenster:** Default 100 Messungen. Bei sehr schnellen Aenderungen kann der Jitter verzoegert reagieren.
4. **Qualitaetsscore:** Heuristische Gewichtung (40/40/20). Fuer spezifische Netzwerktypen (Mobil, WLAN) koennten andere Gewichtungen besser sein.

## Naechster Arbeitsschritt

**AP-91:** Metrics Dashboard
**AP-02:** Deferred Shading Pipeline
**AP-05:** PBR Material System
