#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>

using namespace std;

// ============================================================
// PURPOSE:
//   Parse magnet URIs and orchestrate the "magnet link flow":
//     magnet URI → tracker → peers → fetch metadata → download file
//
// A magnet link looks like:
//   magnet:?xt=urn:btih:d69f91e6b2ae4c542468d1073a71d4ea13879a7f&dn=ubuntu.iso&tr=http://tracker.com/announce
//
// It contains:
//   - xt (exact topic): "urn:btih:<info_hash_hex>" — the 20-byte info hash in hex
//   - dn (display name): human-readable file name (optional, not verified)
//   - tr (tracker): tracker URL (can have multiple tr= params)
//
// With just the info_hash + tracker, we can:
//   1. Contact tracker → get peers
//   2. Connect to peers with extension support
//   3. Ask peers for the metadata (the info dict)
//   4. Verify metadata: SHA1(metadata) == info_hash
//   5. Parse metadata → now we have piece hashes, piece length, etc.
//   6. Download the file (reuse Phase 5)
// ============================================================


// ============================================================
// STRUCT: MagnetLink
// ============================================================
// Holds the parsed components of a magnet URI.

struct MagnetLink {
    array<uint8_t, 20> info_hash;   // 20-byte info hash (decoded from hex)
    string display_name;             // human-readable name (may be empty)
    vector<string> trackers;         // one or more tracker URLs
};


// ============================================================
// STEP 6.1: Parse Magnet URI
// ============================================================
// What: Take a magnet URI string and extract its components.
//
// Input:  "magnet:?xt=urn:btih:abc123...&dn=file.iso&tr=http://tracker.com/announce"
// Output: MagnetLink struct with info_hash, display_name, trackers
//
// Steps:
//   1. Verify it starts with "magnet:?"
//   2. Split by '&' to get key=value pairs
//   3. For each pair:
//        - "xt=urn:btih:<40 hex chars>" → decode hex to 20 bytes → info_hash
//        - "dn=<name>" → URL-decode → display_name
//        - "tr=<url>" → URL-decode → add to trackers vector
//   4. Return filled MagnetLink struct
//
// Helper needed: URL decode (convert %20 → space, %3A → ':', etc.)

MagnetLink parse_magnet(const string& magnet_uri);


// ============================================================
// HELPER: URL decode
// ============================================================
// Converts percent-encoded strings back to normal.
//   "%20" → ' '
//   "%3A" → ':'
//   "hello%20world" → "hello world"
//
// Used for tracker URLs and display names in magnet links.

string url_decode(const string& encoded);


// ============================================================
// HELPER: Hex string to bytes
// ============================================================
// Converts a 40-character hex string to 20 bytes.
//   "d69f91e6b2ae..." → {0xd6, 0x9f, 0x91, 0xe6, ...}
//
// Used to convert the info_hash from the magnet URI.

array<uint8_t, 20> hex_to_bytes(const string& hex);


// ============================================================
// STEP 6.7: Full Magnet Download Flow
// ============================================================
// What: The complete flow from magnet link to downloaded file.
//
// Steps:
//   1. Parse magnet URI
//   2. Contact tracker with info_hash → get peers
//   3. Connect to peers with extension protocol support
//   4. Fetch metadata from peers (using extension.h functions)
//   5. Verify SHA1(metadata) == info_hash
//   6. Parse metadata as bencoded info dict → get piece_length, pieces, name, length
//   7. Build TorrentMetadata struct
//   8. Download all pieces (reuse Phase 5)
//   9. Write file to disk
//
// Input:  magnet URI string
// Output: true if download succeeded

bool download_from_magnet(const string& magnet_uri);

