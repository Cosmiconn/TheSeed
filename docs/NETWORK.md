# TheSeed Network Protocol – Transport Layer Specification

**Version:** 1.0  
**Date:** 2026-08-03  
**Submodule:** `seed-network`  
**Phase:** P1-M7

---

## 1. Overview

TheSeed verwendet ein eigenes UDP-basiertes Protokoll mit folgenden Eigenschaften:

- **Reliable Ordered Channel:** ACK-basiert, Sliding-Window, Out-of-Order-Pufferung
- **Unreliable Channel:** Fire-and-forget, keine Garantie
- **Fragmentation:** Eigene Reassembly auf Transport-Ebene (MTU 1200)
- **Heartbeat:** Keepalive + Disconnect-Detection
- **Cross-Platform:** Windows (WinSock2) + Linux (BSD sockets)

---

## 2. Packet Format

### 2.1 Packet Header – 24 Bytes (Big-Endian)

```
 0-3  : sequenceNumber   – monotonic sequence for this channel
 4-7  : ackSequence      – highest received sequence we ACK
 8-11 : ackBitfield      – 32 previous packets ACKed (bit 0 = ackSequence-1)
12-13 : fragmentId       – 0-based fragment index
14-15 : fragmentCount    – total fragments (1 = unfragmented)
  16  : channelId        – 0 = reliable ordered, 1 = unreliable
  17  : flags            – reserved
18-23 : reserved         – padding for future use
```

```cpp
#pragma pack(push, 1)
struct PacketHeader {
    uint32_t sequenceNumber = 0;
    uint32_t ackSequence    = 0;
    uint32_t ackBitfield    = 0;
    uint16_t fragmentId     = 0;
    uint16_t fragmentCount  = 0;
    uint8_t  channelId      = 0;
    uint8_t  flags          = 0;
    uint8_t  reserved[6]    = {};

    static constexpr size_t SIZE = 24;
    static constexpr uint8_t CHANNEL_RELIABLE   = 0;
    static constexpr uint8_t CHANNEL_UNRELIABLE = 1;

    static constexpr uint16_t MAX_FRAGMENTS = 1024;
    static constexpr uint16_t MTU           = 1200;
    static constexpr uint16_t MAX_PAYLOAD_PER_FRAGMENT = MTU - SIZE;
};
#pragma pack(pop)
```

### 2.2 Maximum Payload Sizes

| Szenario | Max Payload |
|----------|-------------|
| Unfragmented reliable | 1176 bytes |
| Unfragmented unreliable | 1176 bytes |
| Fragmented (max) | 1024 * 1176 = 1,204,224 bytes (~1.2 MB) |

---

## 3. Transport State Machine

```
+-------------+     connect()      +-------------+
|             | -----------------> |             |
| Disconnected|                    | Connecting  |
|             | <----------------- |             |
+-------------+   first packet     +------+------+
    ^   ^                                 |
    |   |                                 | packet received
    |   |                                 v
    |   |                          +-------------+
    |   |                          |  Connected  |
    |   |                          +------+------+
    |   |                                 |
    |   |          disconnect()           | disconnectTimeout
    |   +---------------------------------+
    |                                     |
    +-------------------------------------+
```

**State transitions:**
- `Disconnected → Connecting`: `connect(address, port)` called
- `Connecting → Connected`: First packet received from remote peer
- `Any → Disconnected`: `disconnect()` called OR `disconnectTimeout` exceeded

---

## 4. ReliableChannel Algorithm

### 4.1 Outgoing (Sender)

```
queuePacket(data):
    assign nextSequence++
    push to sendQueue

popOutgoing():
    move packet from sendQueue to unacked_
    record sendTime
    return packet

processAck(ackSeq, ackBitfield):
    for each packet in unacked_:
        if packet.seq == ackSeq OR
           (packet.seq < ackSeq AND bitfield[ackSeq - packet.seq - 1] == 1):
            update RTT (EWMA, alpha = 0.125)
            remove from unacked_

getPacketsToResend():
    for each packet in unacked_:
        if now - sendTime > resendTimeout:
            update sendTime = now
            return packet for resend
```

### 4.2 Incoming (Receiver)

```
onPacketReceived(header, payload):
    if header.sequenceNumber < nextExpectedSequence:
        return  // duplicate, ignore

    if header.sequenceNumber == nextExpectedSequence:
        store payload in pendingIncoming_
        nextExpectedSequence++
        while pendingIncoming_ contains nextExpectedSequence:
            nextExpectedSequence++
        return

    // Out-of-order
    if diff < 32:
        receivedBitfield |= (1 << (diff - 1))
    store payload in pendingIncoming_ for later

pollIncoming():
    deliver all consecutive packets from deliveredSequence_+1
    up to nextExpectedSequence_-1
    update deliveredSequence_
    return vector of payloads
```

### 4.3 ACK Piggy-Backing

Jedes gesendete Paket (egal welcher Channel) enthaelt die aktuellen ACK-Informationen:

```cpp
void sendPacket(header, payload):
    ack = reliableChannel.buildAckHeader()
    header.ackSequence = ack.ackSequence
    header.ackBitfield = ack.ackBitfield
    serialize and send
```

Wenn der Empfaenger Pakete erhaelt aber selbst nichts zu senden hat, wird ein standalone ACK-Paket (unreliable, leerer Payload) gesendet.

---

## 5. Fragmentation

### 5.1 Fragmentation (Sender)

```
fragment(data, size, baseSeq, channelId):
    totalFragments = ceil(size / MAX_PAYLOAD_PER_FRAGMENT)
    for i in 0..totalFragments-1:
        header.sequenceNumber = baseSeq + i
        header.fragmentId     = i
        header.fragmentCount  = totalFragments
        header.channelId      = channelId
        copy chunk[i] into packet
    return packets
```

### 5.2 Reassembly (Receiver)

```
onFragmentReceived(header, payload):
    baseSeq = header.sequenceNumber - header.fragmentId
    entry = reassembly_[baseSeq]
    if entry is new:
        entry.fragments.resize(header.fragmentCount)
        entry.startTime = now

    if entry.fragments[header.fragmentId] is empty:
        entry.fragments[header.fragmentId] = payload
        entry.receivedCount++

    if entry.receivedCount == header.fragmentCount:
        concatenate all fragments
        remove entry from reassembly_
        return completed payload

    return null (incomplete)
```

### 5.3 Stale Cleanup

Fragmente, die laenger als 5 Sekunden unvollstaendig sind, werden verworfen:

```
cleanupStale(maxAge = 5.0s):
    for each entry in reassembly_:
        if now - entry.startTime > maxAge:
            remove entry
```

---

## 6. Threading Model

```
[Main Thread]          [Network Thread]
     |                        |
     | update(deltaTime)      | recvfrom() (non-blocking)
     |  -> send outgoing      |  -> process ACKs
     |  -> resend timeouts    |  -> queue incoming
     |  -> heartbeat          |  (sleep 100us)
     |                        |
     | receive()              |
     |  <- dequeue recvQueue_ | <- push to recvQueue_
```

**Thread-Safety:**
- `recvQueue_` ist durch `recvMutex_` geschuetzt
- `ReliableChannel` ist intern mutex-geschuetzt
- `Fragmenter::reassembly_` wird nur vom Network-Thread genutzt (kein Lock noetig)

---

## 7. Configuration

```cpp
struct TransportConfig {
    uint16_t localPort           = 0;      // 0 = ephemeral
    float    heartbeatIntervalSec = 1.0f;  // Heartbeat-Sendeintervall
    float    disconnectTimeoutSec = 5.0f;  // Timeout fuer Disconnect
    float    resendTimeoutSec     = 0.1f;  // Resend-Timeout (RTT-basiert)
};
```

**Empfohlene Werte:**

| Szenario | heartbeatIntervalSec | disconnectTimeoutSec | resendTimeoutSec |
|----------|---------------------|---------------------|------------------|
| LAN (localhost) | 0.5s | 2.0s | 0.05s |
| WAN (Internet) | 1.0s | 5.0s | 0.3s |

---

## 8. Error Handling

| Fehler | Verhalten |
|--------|-----------|
| Socket bind failed | `initialize()` returns false, spdlog error |
| sendto() failed | Warn-Log, Paket wird ueber Resend-Mechanismus erneut versucht |
| recvfrom() failed | Warn-Log, Thread laeuft weiter |
| Fragment timeout | Fragment wird verworfen, Sender resendet bei ACK-Timeout |
| Disconnect timeout | State -> Disconnected, reliableChannel reset |

---

## 9. Performance Characteristics

| Metrik | Ziel | Gemessen |
|--------|------|----------|
| RTT (localhost) | < 1ms | TBD |
| Throughput | 1000 Pakete/sec | TBD |
| Memory per connection | < 1MB | TBD |
| unacked_ window | 32+ Pakete | 32 (Bitfield-Groesse) |
| pendingIncoming_ | unbegrenzt (RAM) | durch Sequenz-Reservierung begrenzt |

---

## 10. Future Work

- **Verschluesselung:** DTLS oder Noise-Protocol (Monat 12+)
- **Compression:** LZ4 fuer grosse Pakete (Monat 8)
- **Rate Limiting:** Token-Bucket fuer Bandbreiten-Kontrolle (Monat 12)
- **NAT Traversal:** STUN/TURN fuer P2P (Monat 11+)
