# network/

UDP multicast broadcasters for inter-instance state/data sync.

## `MulticastBroadcasterBase`
**Purpose**
- Shared socket lifecycle + receiver-thread base for multicast channels.

**Need to know**
- `initialize()` sets up sockets + starts receiver thread.
- `shutdown()` stops thread + closes sockets.
- Localhost-oriented assumption in design comments.
- Has per-instance ID and user channel index.

**Apply when**
- Creating new UDP multicast channel types.

**Don’t apply when**
- You need reliable ordered transport (TCP/ACK required).

---

## `StatefulBroadcaster`
**Purpose**
- Helpers for mutex-protected `map<instanceID, state>` snapshots + pruning.

**Need to know**
- Utility methods: `clearRemoteStates`, `getRemoteStates`, `getNumRemoteStates`.

**Apply when**
- Receiver keeps latest state per peer.

---

## `CommandBroadcasterBase`
**Purpose**
- Shared helpers for command-style channels.

**Need to know**
- Peer filtering (`self`, `target-group`).
- Thread-safe listener add/remove/dispatch helpers.

---

## `SpectrumBroadcaster`
**Purpose**
- Broadcast + receive compressed spectrum snapshots.

**Need to know**
- dB-domain quantization to 8-bit bins.
- `broadcastSpectrum(...)` sends current spectrum.
- `getReceivedSpectrums()` returns latest per remote peer and prunes stale entries.

**Apply when**
- Need lightweight visual spectrum sharing across plugin instances.

**Don’t apply when**
- Need full-resolution lossless spectrum transfer.

**Example**
```cpp
spectrum.setBroadcastInterval(33);
spectrum.broadcastSpectrum(mags, numBins, sampleRate);
auto remotes = spectrum.getReceivedSpectrums();
```

---

## `SampleBroadcaster`
**Purpose**
- Broadcast + receive raw mono audio chunks tagged with PPQ/BPM metadata.

**Need to know**
- `broadcastRawSamples(...)` supports audio-thread sender path.
- Receiver stores latest packet per remote instance.
- Sequence number handling avoids replacing with older wrapped packets.

**Apply when**
- Need remote scope/raw sample visual alignment between peers.

**Don’t apply when**
- Need large historical buffers from peers (this is latest-state snapshot model).

**Example**
```cpp
sample.broadcastRawSamples(samples, n, ppqFirst, bpm, displayBeats, seq++);
std::vector<phu::network::SampleBroadcaster::RemoteRawPacket> packets;
sample.getReceivedPackets(packets);
```

---

## `CommandBroadcaster`
**Purpose**
- Broadcast + receive discrete control commands (solo/mute/custom).

**Need to know**
- Wire packet: `CommandPacket` with `targetGroup` + payload.
- Listener callback: `CommandListener::onCommandReceived(...)`.
- Convenience senders: `sendSoloCommand`, `sendMuteCommand`.

**Apply when**
- Need peer control/event propagation.

**Don’t apply when**
- Need high-rate streaming data.

**Example**
```cpp
cmd.setOwnGroup("bus-A");
cmd.sendSoloCommand(2, true, "bus-A");
```

---

## `CtrlBroadcaster`
**Purpose**
- Broadcast instance identity/control metadata (announce, label, range, goodbye, mode).

**Need to know**
- Event enum: `CtrlEventType`.
- `sendCtrl(...)` sends metadata packet.
- `getRemoteInfos(...)` returns online peers, prunes stale/offline.
- One-shot consume APIs: inbound count, peers-broadcast-only command.

**Apply when**
- Need remote channel labels/colors/range/sample-rate/plugin identity sync.

**Don’t apply when**
- Need arbitrary command payload protocol (use `CommandBroadcaster`).

**Example**
```cpp
ctrl.sendCtrl(phu::network::CtrlEventType::Announce,
              "Kick", 4.0f, bpm, sampleRate, maxBlock, rgba,
              "phu-scope", 1);
auto peers = ctrl.getRemoteInfos();
```
