#include "extension.h"
#include "bencode.h"
#include "calc_hash.h"

#include <iostream>
#include <cstring>
#include <cmath>

using namespace std;

// ============================================================
// STEP 6.2: Handshake with Extension Support
// ============================================================
// Same as normal handshake but we set reserved[5] |= 0x10
// to advertise extension protocol support.

bool perform_handshake_with_extensions(
    PeerConnection& conn,
    const array<uint8_t, 20>& info_hash,
    const array<uint8_t, 20>& peer_id) {

    // TODO:
    //   1. Build the 68-byte handshake (same as perform_handshake):
    //        [1 byte]   pstrlen = 19
    //        [19 bytes] pstr = "BitTorrent protocol"
    //        [8 bytes]  reserved (all zeros EXCEPT reserved[5] |= 0x10)
    //        [20 bytes] info_hash
    //        [20 bytes] peer_id
    //
    //   2. Send 68 bytes
    //   3. Receive 68 bytes back
    //   4. Verify peer's info_hash matches ours
    //   5. Optionally check if peer also supports extensions:
    //        peer_reserved[5] & 0x10 → they support extensions too
    //   6. Return true if handshake succeeded

    return false;
}


// ============================================================
// STEP 6.3: Send Extension Handshake
// ============================================================
// After the normal handshake, we send our extension handshake.
// This is a regular BitTorrent message with:
//   msg_id = 20 (extension)
//   payload[0] = 0 (extension handshake)
//   payload[1..] = bencoded dict

void send_extension_handshake(PeerConnection& conn) {
    // TODO:
    //   1. Build the bencoded payload:
    //        dict = { "m": { "ut_metadata": 1 } }
    //        (we're telling the peer: "send metadata messages to me using ext_id=1")
    //
    //   2. Build the full payload:
    //        payload[0] = 0  (ext handshake ID)
    //        payload[1..] = bencoded dict bytes
    //
    //   3. Send as a regular message with msg_id = 20:
    //        send_message(conn, MessageId(20), payload)
    //
    //   Note: MessageId enum doesn't have 20, so you'll need to cast:
    //         send_message(conn, static_cast<MessageId>(EXTENSION_MSG_ID), payload);
}


// ============================================================
// STEP 6.4: Parse Extension Handshake
// ============================================================
// When we receive a message with id=20 and payload[0]=0,
// it's the peer's extension handshake.

ExtensionState parse_extension_handshake(const PeerMessage& msg) {
    // TODO:
    //   1. msg.payload[0] should be 0 (extension handshake ID)
    //   2. Bdecode msg.payload[1..end] → get a dict
    //   3. dict["m"] → another dict of extension names → IDs
    //        dict["m"]["ut_metadata"] → peer's metadata ID (uint8_t)
    //   4. dict["metadata_size"] → total metadata size (uint32_t)
    //   5. Fill and return ExtensionState
    //
    //   Note: To bdecode from a vector<uint8_t> starting at index 1,
    //         you can convert to string: string(msg.payload.begin()+1, msg.payload.end())
    //         then call decode() on it.

    ExtensionState state;
    return state;
}


// ============================================================
// STEP 6.5: Request Metadata Piece
// ============================================================
void request_metadata_piece(
    PeerConnection& conn,
    const ExtensionState& ext_state,
    uint32_t piece_index) {

    // TODO:
    //   1. Build bencoded request:
    //        dict = { "msg_type": 0, "piece": piece_index }
    //        encoded = encode(dict)
    //
    //   2. Build full payload:
    //        payload[0] = ext_state.peer_metadata_id  (the ID peer told us to use)
    //        payload[1..] = encoded bytes
    //
    //   3. Send:
    //        send_message(conn, static_cast<MessageId>(EXTENSION_MSG_ID), payload);
}


// ============================================================
// STEP 6.6: Parse Metadata Piece Response
// ============================================================
MetadataPiece parse_metadata_piece(const PeerMessage& msg) {
    // TODO:
    //   1. msg.payload[0] = our metadata ID (should be 1, what we told them)
    //   2. The rest (payload[1..]) starts with a bencoded dict FOLLOWED by raw data
    //   3. Bdecode from payload[1..] → dict + track how many bytes the dict consumed
    //        dict["msg_type"] → 0=request, 1=data, 2=reject
    //        dict["piece"] → piece index
    //   4. If msg_type == 2 (reject): return {piece_index, {}, true}
    //   5. If msg_type == 1 (data):
    //        The raw metadata bytes start RIGHT AFTER the bencoded dict ends
    //        raw_data = payload[1 + dict_bytes_consumed .. end]
    //   6. Return {piece_index, raw_data, false}
    //
    //   KEY INSIGHT: Your decode() function needs to tell you HOW MANY BYTES
    //   it consumed. If it currently doesn't, you'll need to modify it or
    //   use the position tracking you already have (the size_t& pos parameter).

    MetadataPiece result;
    result.rejected = true;
    return result;
}


// ============================================================
// STEP 6.6 (continued): Fetch Full Metadata
// ============================================================
vector<uint8_t> fetch_metadata(
    PeerConnection& conn,
    const ExtensionState& ext_state,
    const array<uint8_t, 20>& info_hash) {

    // TODO:
    //   1. Calculate number of metadata pieces:
    //        num_pieces = ceil(ext_state.metadata_size / 16384.0)
    //
    //   2. Request each piece:
    //        for i = 0 to num_pieces-1:
    //            request_metadata_piece(conn, ext_state, i)
    //
    //   3. Receive responses:
    //        Create vector<vector<uint8_t>> metadata_pieces(num_pieces)
    //        Loop receiving messages until we have all pieces:
    //            msg = receive_message(conn)
    //            if msg.id == MessageId(20):
    //                MetadataPiece mp = parse_metadata_piece(msg)
    //                if mp.rejected → fail
    //                metadata_pieces[mp.piece_index] = mp.data
    //            else:
    //                handle other messages (keep-alive, etc.)
    //
    //   4. Assemble:
    //        vector<uint8_t> full_metadata
    //        for each piece in order: append to full_metadata
    //
    //   5. Verify:
    //        SHA1(full_metadata) == info_hash?
    //        (convert full_metadata to string, call sha1_hash(), compare)
    //
    //   6. Return full_metadata if verified, empty vector if not

    return {};
}

