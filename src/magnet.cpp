#include "magnet.h"
#include "extension.h"
#include "bencode.h"
#include "calc_hash.h"
#include "get_peers.h"
#include "orchestrate_peer.h"

#include <iostream>
#include <sstream>
#include <cstring>

using namespace std;

// ============================================================
// HELPER: URL decode
// ============================================================
// "%20" → ' ', "%3A" → ':', '+' → ' '
// Scan character by character:
//   - If '%' → next 2 chars are hex → convert to byte
//   - If '+' → space
//   - Otherwise → keep as-is

string url_decode(const string& encoded) {
    // TODO:
    //   1. Create output string
    //   2. Loop through each character:
    //        if '%' and next 2 chars are hex digits:
    //            convert hex pair to char, append to output, skip ahead by 2
    //        else if '+':
    //            append ' '
    //        else:
    //            append character as-is
    //   3. Return output
    return "";
}


// ============================================================
// HELPER: Hex string to bytes
// ============================================================
// "d69f91e6..." (40 chars) → {0xd6, 0x9f, 0x91, 0xe6, ...} (20 bytes)
// Take 2 hex chars at a time, convert to a byte.

array<uint8_t, 20> hex_to_bytes(const string& hex) {
    // TODO:
    //   1. Create array<uint8_t, 20> result
    //   2. For i = 0 to 19:
    //        take hex[i*2] and hex[i*2 + 1]
    //        convert those 2 hex chars to one byte
    //        (hint: stoul(hex_pair, nullptr, 16) or manual conversion)
    //   3. Return result
    array<uint8_t, 20> result = {};
    return result;
}


// ============================================================
// STEP 6.1: Parse Magnet URI
// ============================================================
MagnetLink parse_magnet(const string& magnet_uri) {
    // TODO:
    //   1. Verify starts with "magnet:?"
    //   2. Get everything after "magnet:?" → that's the query string
    //   3. Split query string by '&' → vector of "key=value" strings
    //   4. For each key=value:
    //        - if key == "xt" and value starts with "urn:btih:":
    //            extract the 40-char hex after "urn:btih:"
    //            call hex_to_bytes() → store in result.info_hash
    //        - if key == "dn":
    //            url_decode(value) → store in result.display_name
    //        - if key == "tr":
    //            url_decode(value) → push_back to result.trackers
    //   5. Return result

    MagnetLink result;
    return result;
}


// ============================================================
// STEP 6.7: Full Magnet Download Flow
// ============================================================
bool download_from_magnet(const string& magnet_uri) {
    // TODO:
    //
    // --- STEP 1: Parse magnet URI ---
    //   MagnetLink magnet = parse_magnet(magnet_uri);
    //
    // --- STEP 2: Contact tracker ---
    //   Build tracker URL using magnet.info_hash and magnet.trackers[0]
    //   (Note: get_peers() currently takes a TorrentFile. You may need to
    //    build the tracker URL manually or create an overload that takes
    //    info_hash + tracker_url directly)
    //   → get list of peers
    //
    // --- STEP 3: Connect to a peer with extension support ---
    //   PeerConnection conn = connect_to_peer(peer.ip, peer.port);
    //   perform_handshake_with_extensions(conn, magnet.info_hash, our_peer_id);
    //
    // --- STEP 4: Extension handshake ---
    //   send_extension_handshake(conn);
    //   receive messages until we get extension handshake back
    //   ExtensionState ext = parse_extension_handshake(msg);
    //
    // --- STEP 5: Fetch metadata ---
    //   vector<uint8_t> metadata = fetch_metadata(conn, ext, magnet.info_hash);
    //   if metadata is empty → failed, try another peer
    //
    // --- STEP 6: Parse metadata into TorrentMetadata ---
    //   Bdecode the metadata bytes → it's the info dict
    //   Extract: name, length, piece_length, pieces (raw hash string)
    //   Build TorrentMetadata struct (same as what main.cpp builds from .torrent)
    //
    // --- STEP 7: Download using Phase 5 ---
    //   vector<vector<uint8_t>> pieces = download_all_pieces(peers, metadata, ...);
    //   write_file(pieces, metadata.output_file);
    //
    // --- STEP 8: Return success ---

    return false;
}

