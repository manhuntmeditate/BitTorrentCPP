# BitTorrent Client in C++

A fully functional BitTorrent client built from scratch in C++20. Implements the core protocol stack — from bencode parsing to multi-peer parallel downloads — with magnet link support via BEP 9/10.

## Architecture

```
┌─────────────┐     ┌──────────────┐     ┌─────────────────┐
│  .torrent   │────▶│ Bencode      │────▶│ Torrent Parser  │
│  or magnet  │     │ Codec        │     │ (info_hash,     │
└─────────────┘     └──────────────┘     │  piece hashes)  │
                                          └────────┬────────┘
                                                   │
                    ┌──────────────┐               ▼
                    │  Tracker     │◀──── HTTP GET /announce
                    │  (libcurl)   │────▶ compact peer list
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │ Thread 1 │ │ Thread 2 │ │ Thread N │  ← 1 persistent TCP
        │ Peer A   │ │ Peer B   │ │ Peer C   │    connection per thread
        └────┬─────┘ └────┬─────┘ └────┬─────┘
             │             │             │
             └─────────────┼─────────────┘
                           ▼
                ┌─────────────────────┐
                │ Shared Piece Queue  │ ← mutex-protected
                │ + SHA-1 verification│
                └─────────┬───────────┘
                          ▼
                    output file
```

## Protocol Implementation

| Layer | What | How |
|-------|------|-----|
| Encoding | Bencode codec | `std::variant<string, int64_t, vector, map>` recursive parser |
| Hashing | info_hash + piece verification | OpenSSL SHA-1 |
| Tracker | Announce + peer discovery | libcurl HTTP GET, compact peer format (6 bytes/peer) |
| Wire Protocol | Handshake → Bitfield → Interested → Unchoke → Request → Piece | Raw TCP sockets, 4-byte length-prefix framing |
| Download | Multi-peer parallel piece fetching | `std::thread` pool, shared queue with skip counter |
| Extensions | BEP 10 + BEP 9 metadata exchange | Extension handshake, 16KB metadata pieces |

## Key Design Decisions

- **Full protocol implementation from raw TCP** — no BitTorrent libraries; hand-crafted 68-byte handshake, length-prefix message framing, and binary protocol parsing on raw POSIX sockets
- **Persistent connections with bitfield-aware scheduling** — one long-lived TCP session per peer, with intelligent piece selection based on each peer's advertised bitfield
- **Thread-per-peer concurrency model** — parallel downloads via shared mutex-protected piece queue; peers independently claim and download pieces without central coordination
- **Extension protocol negotiation (BEP 10/9)** — dynamic capability advertisement and metadata exchange, enabling magnet link bootstrapping without `.torrent` files
- **Recursive variant-based bencode codec** — `std::variant<string, int64_t, vector, map>` with position-tracked parsing, used across torrent parsing, tracker responses, and extension payloads

## Magnet Link Support (BEP 9/10)

When no `.torrent` file exists — only a `magnet:?xt=urn:btih:...` URI — the client bootstraps the entire download from just a 20-byte info_hash:

```
magnet URI ──▶ parse info_hash + tracker URL
                        │
                        ▼
              tracker announce (same as .torrent flow)
                        │
                        ▼
              connect to peer with reserved[5] |= 0x10
              (signals extension protocol support)
                        │
                        ▼
              BEP 10: exchange extension handshake
              ← peer reports ut_metadata ID + metadata_size
                        │
                        ▼
              BEP 9: request metadata in 16KB pieces
              ← receive + assemble raw info dict
                        │
                        ▼
              verify: SHA1(metadata) == info_hash ✓
                        │
                        ▼
              parse info dict → piece_length, pieces, file name
                        │
                        ▼
              normal Phase 5 download (reuse existing code)
```

The metadata itself *is* the bencoded `info` dictionary — the same blob whose SHA-1 produces the info_hash. Once fetched and verified, the client has everything a `.torrent` file would have provided.

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

```bash
# Download from .torrent file
./bt_client path/to/file.torrent

# Download from magnet link (Phase 6)
./bt_client "magnet:?xt=urn:btih:..."
```

Output goes to `downloads/`.

## Tech Stack

- **C++20** — `std::variant`, structured bindings, `array<uint8_t, 20>`
- **CMake** — build system
- **libcurl** — tracker HTTP requests
- **OpenSSL** — SHA-1 (info_hash + piece verification)
- **POSIX sockets** — raw TCP for peer wire protocol
- **pthreads** — parallel peer connections via `std::thread`

## File Map

```
src/
├── bencode.cpp          # Bencode encode/decode (Phase 1)
├── calc_hash.cpp        # Torrent parsing + SHA-1 (Phase 2)
├── get_peers.cpp        # Tracker announce + peer list (Phase 3)
├── peer_connect.cpp     # Peer wire protocol (Phase 4)
├── orchestrate_peer.cpp # Multi-peer download orchestration (Phase 5)
├── extension.cpp        # BEP 10/9 extension protocol (Phase 6)
├── magnet.cpp           # Magnet URI parsing + flow (Phase 6)
└── main.cpp             # Entry point, ties all phases together
```
