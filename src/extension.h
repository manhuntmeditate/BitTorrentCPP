#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>

#include "peer_connect.h"

using namespace std;

// ============================================================
// PURPOSE:
//   Implement the BitTorrent Extension Protocol (BEP 10) and
//   Metadata Exchange (BEP 9).
//
// THE BIG PICTURE:
//
//   Normal handshake:
//     [pstrlen][pstr][reserved 8 bytes][info_hash][peer_id]
//                     ^^^^^^^^^^^^^^^^
//                     reserved[5] |= 0x10  ← "I support extensions"
//
//   After normal handshake, extension messages use message ID = 20:
//     [4-byte length][msg_id = 20][ext_msg_id][payload...]
//
//   ext_msg_id = 0 means "extension handshake" (both sides send this)
//   ext_msg_id = N means "this is extension N" (agreed during ext handshake)
//
//   Extension Handshake payload (bencoded dict):
//     {
//       "m": {                    ← map of extension names → IDs
//         "ut_metadata": 1       ← "I call metadata exchange ID 1"
//       },
//       "metadata_size": 31235   ← total size of info dict (only if peer has it)
//     }
//
//   Metadata Exchange (BEP 9) message types:
//     msg_type = 0 → request   ("give me metadata piece N")
//     msg_type = 1 → data      ("here's metadata piece N" + raw bytes after the dict)
//     msg_type = 2 → reject    ("I don't have the metadata")
//
//   Metadata pieces are 16KB (16384 bytes) each, just like data blocks.
// ============================================================


// ============================================================
// CONSTANTS
// ============================================================

const uint8_t EXTENSION_MSG_ID = 20;        // BitTorrent message ID for all extension msgs
const uint8_t EXT_HANDSHAKE_ID = 0;         // ext_msg_id 0 = extension handshake
const uint32_t METADATA_PIECE_SIZE = 16384; // 16KB per metadata piece

// Metadata message types (inside the bencoded payload)
const int METADATA_REQUEST = 0;
const int METADATA_DATA    = 1;
const int METADATA_REJECT  = 2;


// ============================================================
// STRUCT: ExtensionState
// ============================================================
// Tracks extension protocol state for a connection.
// After the extension handshake, we know:
//   - What ID the peer assigned to ut_metadata
//   - How big the metadata is

struct ExtensionState {
    uint8_t peer_metadata_id = 0;   // The ID the PEER uses for ut_metadata
    uint32_t metadata_size = 0;     // Total size of the info dict in bytes
    bool supported = false;         // Did the peer support ut_metadata?
};


// ============================================================
// STEP 6.2: Modify Handshake for Extension Support
// ============================================================
// What: Before connecting, set the extension bit in the reserved bytes.
//
// In the normal 68-byte handshake, bytes 20-27 are "reserved" (all zeros).
// To signal extension support:
//   reserved[5] |= 0x10
//
// This is bit 20 counting from the left (byte 5, bit 4 within that byte).
//
// We'll modify perform_handshake() or create a wrapper that sets this bit.
//
// Input:  PeerConnection (already connected)
// Output: true if handshake succeeded (peer also supports extensions if
//         their reserved[5] & 0x10 is set)

bool perform_handshake_with_extensions(
    PeerConnection& conn,
    const array<uint8_t, 20>& info_hash,
    const array<uint8_t, 20>& peer_id
);


// ============================================================
// STEP 6.3: Send Extension Handshake
// ============================================================
// What: After the normal handshake, send our extension handshake.
//
// We send a message with:
//   - Message ID: 20 (extension)
//   - Extended msg ID: 0 (handshake)
//   - Payload: bencoded dict { "m": { "ut_metadata": 1 } }
//
// This tells the peer: "I support metadata exchange. When you send
// metadata messages to me, use ext_msg_id = 1."
//
// Input:  connected + handshaked PeerConnection
// Output: (sends the extension handshake message)

void send_extension_handshake(PeerConnection& conn);


// ============================================================
// STEP 6.4: Receive Extension Handshake
// ============================================================
// What: Parse the peer's extension handshake to learn:
//   - What ID they assigned to ut_metadata (so we know what ID to use
//     when SENDING metadata requests TO THEM)
//   - metadata_size (how big the info dict is)
//
// Input:  the PeerMessage we received (msg.id == 20, payload[0] == 0)
// Output: ExtensionState with peer_metadata_id and metadata_size
//
// Parsing:
//   1. Skip first byte of payload (it's the ext_msg_id = 0)
//   2. Bdecode the rest → get a dict
//   3. dict["m"]["ut_metadata"] → peer's metadata ID
//   4. dict["metadata_size"] → total metadata size

ExtensionState parse_extension_handshake(const PeerMessage& msg);


// ============================================================
// STEP 6.5: Request Metadata Pieces
// ============================================================
// What: Ask the peer for a specific metadata piece.
//
// Send extension message:
//   - Message ID: 20
//   - Extended msg ID: peer's ut_metadata ID (from ExtensionState)
//   - Payload: bencode({ "msg_type": 0, "piece": piece_index })
//
// Input:  connection, ExtensionState (has peer's metadata ID), piece index
// Output: (sends the request)

void request_metadata_piece(
    PeerConnection& conn,
    const ExtensionState& ext_state,
    uint32_t piece_index
);


// ============================================================
// STEP 6.6: Receive & Parse Metadata Piece
// ============================================================
// What: Receive a metadata data message and extract the raw bytes.
//
// The response format:
//   [msg_id=20][ext_msg_id=our_metadata_id][bencoded_dict][raw_metadata_bytes]
//
// The bencoded dict is: { "msg_type": 1, "piece": N, "total_size": S }
// After the dict ends, the remaining bytes are the actual metadata piece data.
//
// Tricky part: finding where the bencoded dict ENDS and raw data BEGINS.
// Since we wrote a bencode decoder, we can decode and track how many bytes
// it consumed, then the rest is raw metadata.
//
// Input:  PeerMessage (extension message with metadata data)
// Output: pair<uint32_t, vector<uint8_t>> = (piece_index, raw_data)
//         Returns piece_index = UINT32_MAX if it was a reject

struct MetadataPiece {
    uint32_t piece_index;
    vector<uint8_t> data;
    bool rejected;          // true if peer sent msg_type = 2
};

MetadataPiece parse_metadata_piece(const PeerMessage& msg);


// ============================================================
// STEP 6.6 (continued): Fetch Full Metadata
// ============================================================
// What: Request ALL metadata pieces, assemble them, verify hash.
//
// Steps:
//   1. Calculate num_metadata_pieces = ceil(metadata_size / 16384)
//   2. Request each piece (0, 1, 2, ...)
//   3. Receive responses, store each piece's data
//   4. Concatenate all pieces in order → full metadata bytes
//   5. SHA1(full_metadata) == info_hash? → success!
//
// Input:  connection, ExtensionState, expected info_hash
// Output: the raw metadata bytes (bencoded info dict), or empty on failure

vector<uint8_t> fetch_metadata(
    PeerConnection& conn,
    const ExtensionState& ext_state,
    const array<uint8_t, 20>& info_hash
);

