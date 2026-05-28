#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <mutex>
#include <thread>
#include <queue>

#include "peer_connect.h"

using namespace std;

// ============================================================
// PURPOSE:
//   Orchestrate the full file download:
//   - Download a single piece from a single peer (reuses peer.cpp)
//   - Download ALL pieces using multiple peers in parallel
//   - Write the final assembled file to disk
//
// THE BIG PICTURE:
//
//   .torrent file tells us:
//     - Total file size (e.g. 1MB)
//     - Piece length (e.g. 256KB)
//     - SHA-1 hash for each piece
//     - Tracker URL → gives us list of peers
//
//   Our job:
//     1. Get list of peers from tracker (already done in Phase 3)
//     2. Connect to multiple peers
//     3. Assign pieces to peers (who has what)
//     4. Download all pieces in parallel
//     5. Glue pieces together → write file to disk
//
//   Visual:
//
//     Peer A ──► piece 0, piece 3
//     Peer B ──► piece 1, piece 4
//     Peer C ──► piece 2, piece 5
//                    │
//                    ▼
//     [piece 0][piece 1][piece 2][piece 3][piece 4][piece 5]
//                    │
//                    ▼
//              output_file.dat (written to disk)
// ============================================================


// ============================================================
// STRUCT: TorrentMetadata
// ============================================================
// Holds everything we know about the torrent (from Phase 2 parsing).
// This is passed into the download functions so they know:
//   - How big each piece is
//   - How many pieces there are
//   - What hash each piece should have
//   - Where to save the file

struct TorrentMetadata {
    string output_file;             // e.g. "movie.mp4"
    uint64_t total_length;          // total file size in bytes
    uint32_t piece_length;          // size of each piece (e.g. 262144 = 256KB)
    uint32_t num_pieces;            // total number of pieces
    vector<array<uint8_t, 20>> piece_hashes;  // expected SHA-1 for each piece
    // Note: last piece may be smaller than piece_length
};


// ============================================================
// STRUCT: PeerInfo
// ============================================================
// A peer's address (from tracker response in Phase 3).

struct PeerInfo {
    string ip;
    uint16_t port;
};


// ============================================================
// STEP 5.1: Worker Thread — One Thread Per Peer
// ============================================================
// What: Each thread owns ONE persistent connection to ONE peer.
//       It keeps the connection open and downloads multiple pieces
//       through it until the queue is empty.
//
// Flow for each thread:
//   1. connect_to_peer(ip, port)
//   2. perform_handshake(conn, info_hash, our_peer_id)
//   3. receive Bitfield → now we know which pieces this peer has
//   4. send Interested
//   5. wait for Unchoke
//   6. LOOP:
//        a. peek at next piece in queue
//        b. does this peer have it? (check conn.bitfield[piece_index])
//             - NO  → skip it, put it back, try next piece
//             - YES → pop it from queue, download it
//        c. call download_piece(conn, piece_index, piece_length, expected_hash)
//        d. if success → store in results
//        e. if fail → push piece back into queue
//        f. repeat until queue is empty
//   7. disconnect (only when queue is fully empty or no more pieces for this peer)
//
// This way: connect + handshake happens ONCE per peer, not once per piece.

void peer_worker(
    const PeerInfo& peer,
    PieceQueue& piece_queue,
    vector<vector<uint8_t>>& results,
    mutex& results_mutex,
    const TorrentMetadata& metadata,
    const array<uint8_t, 20>& info_hash,
    const array<uint8_t, 20>& our_peer_id
);


// ============================================================
// STEP 5.2: Download All Pieces (Multi-Peer, Parallel)
// ============================================================
// What: Spawn threads and coordinate them.
//
// Strategy:
//   - Fill piece queue with [0, 1, 2, ..., num_pieces-1]
//   - Spawn one thread per peer (up to a limit, e.g. 5)
//   - Each thread runs peer_worker() above
//   - Threads grab pieces from the SHARED queue
//   - A thread checks its peer's bitfield before downloading:
//       → "Does my peer have piece #X?"
//       → If yes: pop from queue, download
//       → If no: skip, let another thread/peer handle it
//   - When queue is empty, all threads exit
//
// Visual:
//
//   Piece Queue: [0, 1, 2, 3, 4, 5, 6, 7]
//
//   Thread 1 (Peer A, has pieces 0,1,3,5):
//     peek piece 0 → has it → pop → download ✓
//     peek piece 2 → doesn't have → skip (put back)
//     peek piece 3 → has it → pop → download ✓
//
//   Thread 2 (Peer B, has pieces 1,2,4,6):
//     peek piece 1 → has it → pop → download ✓
//     peek piece 2 → has it → pop → download ✓
//     peek piece 4 → has it → pop → download ✓
//
//   Queue empty → all threads finish → assemble file
//
// Input:  list of peers, torrent metadata, info_hash, our_peer_id
// Output: vector<vector<uint8_t>> — all pieces in order (index 0 = piece 0)

vector<vector<uint8_t>> download_all_pieces(
    const vector<PeerInfo>& peers,
    const TorrentMetadata& metadata,
    const array<uint8_t, 20>& info_hash,
    const array<uint8_t, 20>& our_peer_id
);


// ============================================================
// STEP 5.3: Write File to Disk
// ============================================================
// What: Take all downloaded pieces (in order) and write them
//       sequentially to the output file.
//
// Steps:
//   1. Open output file for binary writing
//   2. For each piece (0, 1, 2, ...):
//        write piece data to file
//   3. Close file
//
// Input:  ordered pieces, output filename
// Output: true if file written successfully

bool write_file(const vector<vector<uint8_t>>& pieces, const string& output_path);


// ============================================================
// HELPER: Thread-safe piece queue
// ============================================================
// A queue that multiple threads can safely grab pieces from.
// Uses a mutex to prevent two threads from grabbing the same piece.

class PieceQueue {
public:
    // Add a piece index to the queue
    void push(uint32_t piece_index);

    // Look at the next piece WITHOUT removing it. Returns false if empty.
    bool peek(uint32_t& piece_index);

    // Remove and return the next piece. Returns false if queue is empty.
    bool pop(uint32_t& piece_index);

    // Check if empty
    bool empty();

private:
    queue<uint32_t> q;
    mutex mtx;     // "mutual exclusion" — only one thread can access at a time
};
