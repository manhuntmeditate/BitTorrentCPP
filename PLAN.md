# BitTorrent Client in C++ — Implementation Plan

## Notes & Explanations
> behave like a teacher 
> ans in short and explain only when asked for detail
> explain the concepts for networking,cpp and thought process as we go along
> write the code template for each step when asked in it keep purpose ,steps, expected input output
> import namespace std in each file so it becomes easier to read 
> use easier cpp index wherever possible even if it means a bigger code, becoming understanable is more important 


## Tech Stack
- **Language:** C++20
- **Build:** CMake
- **Libraries:**
  - `libcurl` — HTTP tracker requests
  - `openssl` (or `picosha2` header-only) — SHA-1 hashing
  - `asio` (standalone, header-only) — async TCP networking
  - `nlohmann/json` (optional) — magnet link metadata

---

## Phase 1: Bencode (Days 1–2)

| Step | Task | Difficulty |
|------|------|-----------|
| 1.1 | Decode bencoded strings (`4:spam` → `"spam"`) | Very Easy |
| 1.2 | Decode bencoded integers (`i42e` → `42`) | Easy |
| 1.3 | Decode bencoded lists (`l4:spami42ee`) | Easy |
| 1.4 | Decode bencoded dictionaries (`d3:foo3:bare`) | Easy |
| 1.5 | Encode back to bencode (needed for info_hash) | Easy |

**File:** `src/bencode.h / bencode.cpp`  
**Python ref:** `BitTorrent-Client-main/parser.py`  
**Tip:** Use `std::variant` to represent bencode values (string, int, list, dict).

---

## Phase 2: Torrent Parsing (Day 3)

| Step | Task | Difficulty |
|------|------|-----------|
| 2.1 | Parse `.torrent` file into bencode dict | Easy |
| 2.2 | Extract `announce`, `info.name`, `info.length`, `info.piece length` | Easy |
| 2.3 | Extract piece hashes (split 20-byte SHA-1 chunks) | Easy |
| 2.4 | Calculate `info_hash` = SHA-1 of bencoded `info` dict | Medium |

**File:** `src/torrent_file.h / torrent_file.cpp`  
**Python ref:** `BitTorrent-Client-main/calc_hash.py`, `BitTorrent-Client-main/parser.py`

---

## Phase 3: Tracker & Peer Discovery (Day 4)

| Step | Task | Difficulty |
|------|------|-----------|
| 3.1 | Build tracker announce URL with params (info_hash, peer_id, port, etc.) | Medium |
| 3.2 | HTTP GET request to tracker | Easy (libcurl) |
| 3.3 | Parse compact peer list (6 bytes per peer: 4 IP + 2 port) | Easy |

**File:** `src/tracker.h / tracker.cpp`  
**Python ref:** `BitTorrent-Client-main/get_peers.py`

---

## Phase 4: Peer Wire Protocol (Days 5–7)

| Step | Task | Difficulty |
|------|------|-----------|
| 4.1 | TCP connect to peer | Easy |
| 4.2 | Handshake (68 bytes: pstrlen + pstr + reserved + info_hash + peer_id) | Medium |
| 4.3 | Message framing (4-byte length prefix + 1-byte message ID) | Medium |
| 4.4 | Handle bitfield, choke, unchoke, interested | Medium |
| 4.5 | Request blocks (16KB chunks within a piece) | Medium |
| 4.6 | Receive piece data, verify SHA-1 | Hard |

**File:** `src/peer.h / peer.cpp`  
**Python ref:** `BitTorrent-Client-main/connect_to_peer.py`

---

## Phase 5: Download (Days 8–10)

| Step | Task | Difficulty |
|------|------|-----------|
| 5.1 | Download a single piece (request all blocks → assemble → verify hash) | Hard |
| 5.2 | Download all pieces (piece queue + multiple async peer connections) | Hard |
| 5.3 | Write assembled file to disk | Easy |

**File:** `src/download.h / download.cpp`  
**Python ref:** `BitTorrent-Client-main/connect_to_peer_async.py`, `BitTorrent-Client-main/main.py`

---

## Phase 6: Magnet Links (Days 11–14)

| Step | Task | Difficulty |
|------|------|-----------|
| 6.1 | Parse magnet URI (`magnet:?xt=urn:btih:...&dn=...&tr=...`) | Easy |
| 6.2 | In handshake, set reserved bit 20 (extension protocol support) | Easy |
| 6.3 | Send extension handshake (`BEP 10`) | Easy |
| 6.4 | Receive extension handshake, get `ut_metadata` ID | Easy |
| 6.5 | Request metadata pieces | Easy |
| 6.6 | Receive & assemble metadata, verify against info_hash | Easy |
| 6.7 | Download piece / whole file (reuse Phase 5) | Hard |

**File:** `src/magnet.h / magnet.cpp`, `src/extension.h / extension.cpp`

---

## Project Structure

```
BitTorrent_cpp/
├── CMakeLists.txt
├── PLAN.md
├── src/
│   ├── main.cpp
│   ├── bencode.h / bencode.cpp
│   ├── torrent_file.h / torrent_file.cpp
│   ├── tracker.h / tracker.cpp
│   ├── peer.h / peer.cpp
│   ├── download.h / download.cpp
│   ├── magnet.h / magnet.cpp
│   └── extension.h / extension.cpp
└── tests/
    └── test_bencode.cpp
```

---

## Order of Implementation

```
bencode → torrent parsing → info_hash → tracker → handshake → download piece → download file → magnet
```

Each step builds on the previous. Test each step independently before moving on.


