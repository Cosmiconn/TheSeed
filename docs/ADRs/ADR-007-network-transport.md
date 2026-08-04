# ADR-007: Network Transport Protocol – UDP + Custom Reliability

**Status:** Accepted  
**Date:** 2026-08-03  
**Deciders:** TheSeed Core Team  
**Scope:** Phase 1, Monat 7 (Transport-Layer)

---

## Context

TheSeed benoetigt einen Netzwerk-Transport fuer einen authoritativen Multiplayer-Game-Server. Die Anforderungen sind:

- **Low Latency:** < 1ms RTT im LAN, < 50ms WAN-Ziel
- **Reliable Ordered:** Game-State-Updates muessen vollstaendig und in Reihenfolge ankommen
- **Unreliable:** Positions-Updates koennen verloren gehen (neue kommen schnell nach)
- **Fragmentation:** 10KB+ Pakete muessen ueber MTU 1200 passen
- **Cross-Platform:** Windows + Linux
- **Keine externe Dependency:** ENet, RakNet etc. sind zu schwer / zu alt

---

## Decision Drivers

| Driver | Gewichtung | Begruendung |
|--------|-----------|-------------|
| Latenz | Kritisch | TCP-Nagle + Head-of-Line-Blocking sind fuer Game-Networking toedlich |
| Kontrolle | Hoch | Eigenes Protokoll erlaubt fine-tuned Resend, Delta-Kompression, AOI |
| Einfachheit | Hoch | Solo-Dev: keine 10k LOC externe Lib debuggen |
| Portabilitaet | Mittel | Muss auf Windows (MSVC) und Linux (GCC/Clang) laufen |

---

## Decisions

### 1. UDP statt TCP

**Entscheidung:** Der Transport-Layer baut auf UDP (SOCK_DGRAM) auf.

**Begruendung:**
- TCPs Head-of-Line-Blocking verzoegert alle Pakete, wenn eines verloren geht
- TCPs congestion control ist fuer Echtzeit-Spiele zu konservativ
- UDP erlaubt gemischte reliable/unreliable Kanäle auf demselben Socket
- Heartbeat + Disconnect-Timeout sind selbst implementiert (mehr Kontrolle)

**Nachteile & Mitigation:**
- NAT-Traversal schwieriger → spaeter STUN/TURN (Monat 11+)
- Keine eingebaute Flusskontrolle → eigene Rate-Limiter (Monat 12)

### 2. Custom ReliableChannel statt ENet

**Entscheidung:** Eigenes ACK-basiertes Reliability-Protokoll statt ENet, yojimbo etc.

**Begruendung:**
- ENet ist C89, nicht C++20-idiomatisch
- yojimbo bindet an GameNetworkingSockets (Google) – zu viele Dependencies
- Eigenes Protokoll erlaubt spaetere Integration mit Delta-Kompression (Monat 8)
- Direkte Kontrolle ueber Sequenznummern fuer Lag Compensation (Monat 10)

### 3. MTU 1200 Bytes

**Entscheidung:** Maximale Fragment-Groesse = 1200 Bytes (Payload = 1176 Bytes nach 24-Byte-Header).

**Begruendung:**
- IPv6-Minimum-MTU ist 1280 Bytes; 1200 laesst Platz fuer Tunnel-Overhead (WireGuard, etc.)
- Ethernet-MTU 1500 ist unsicher – PPPoE, VLAN-Tags, GRE reduzieren effektives MTU
- 1200 ist konservativ und funktioniert in 99% aller Netzwerke ohne IP-Fragmentierung

### 4. ACK-Bitfield (32 previous packets)

**Entscheidung:** Neben dem highest-ACK enthaelt jedes Paket ein 32-Bit-Bitfield fuer die vorherigen 32 Pakete.

**Begruendung:**
- Einfacher als Selective-ACK (SACK) – kein variabler Header
- 32 Bits = ~32 RTTs Abdeckung bei 60Hz; ausreichend fuer LAN + stabiles WAN
- Ermoeglicht schnelles ACK von Out-of-Order-Paketen ohne Extra-RTT

### 5. Fragmentierung auf Transport-Ebene

**Entscheidung:** Der Transport fragmentiert grosse Pakete selbst (max. 1024 Fragmente = ~1.2MB).

**Begruendung:**
- IP-Fragmentierung ist unsicher (firewalls blockieren oft fragmentierte UDP-Pakete)
- Eigene Fragmentierung erlaubt Reassembly mit Timeout + Stale-Cleanup
- Jedes Fragment traegt Sequenznummer des ReliableChannel → Resend auf Fragment-Ebene

### 6. Non-Blocking Socket + Dedizierter Network-Thread

**Entscheidung:** Der Socket ist non-blocking; ein dedizierter Thread polled recvfrom() in einer 100us-Sleep-Schleife.

**Begruendung:**
- Non-blocking verhindert, dass ein langsamer Peer den Thread blockiert
- 100us-Polling ist fuer Game-Networking akzeptabel (10k polls/sec ≈ vernachlaessigbare CPU)
- Der Network-Thread ist vom Game-Thread entkoppelt (spaeter: Input-Queue, Monat 11)

### 7. Heartbeat + Disconnect-Timeout

**Entscheidung:** Heartbeat-Pakete (unreliable, 1 Byte Payload) werden im Config-Intervall gesendet. Disconnect nach Timeout ohne empfangenes Paket.

**Begruendung:**
- UDP hat keine eingebaute Verbindungsueberwachung
- Heartbeats tragen ACK-Informationen → dienen gleichzeitig als implicit ACK
- Konfigurierbar: 0.5s Heartbeat, 5s Timeout (Default)

---

## Consequences

### Positive

- Volle Kontrolle ueber das Protokoll-Stack
- Keine externe Netzwerk-Dependency
- Einfache Integration mit ECS (spaetere Monate)
- Cross-Platform (WinSock2 / BSD sockets)

### Negative

- Eigenes Protokoll = eigene Bugs (siehe Bugfixes August 2026)
- Keine eingebaute Verschluesselung → spaeter DTLS oder Noise (Monat 12+)
- NAT-Traversal muss selbst gebaut werden

---

## Related ADRs

- ADR-001: Meta-Repo-Struktur (seed-network als eigenes Submodule)
- ADR-002: Test-Strategie (doctest + rapidcheck + libFuzzer)
- ADR-006: Performance-Budgets (< 1ms RTT im LAN)

---

## References

- Glenn Fiedler: "Networking for Game Programmers" (gafferongames.com)
- Gabriel Gambetta: "Fast-Paced Multiplayer" (www.gabrielgambetta.com)
- RFC 5405: UDP Guidelines
