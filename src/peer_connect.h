#pragma once

// Standard library includes
#include <string>       // for string (text like "192.168.1.1")
#include <vector>       // for vector (resizable array)
#include <cstdint>      // for uint8_t, uint16_t, uint32_t (fixed-size numbers)
#include <array>        // for array (fixed-size array)

using namespace std;

// ============================================================
// PURPOSE:
//   Implement the BitTorrent Peer Wire Protocol.
//
//   THE BIG PICTURE — what happens when you talk to a peer:
//
//   1. Open a TCP connection (like dialing a phone number)
//   2. Both sides say "hello" with 68 bytes (torrent handshake)
//   3. Peer tells you what pieces it has (bitfield message)
//   4. You say "I'm interested in your pieces"
//   5. Peer says "OK you're unchoked, you can ask for data"
//   6. You request data in small 16KB chunks (blocks)
//   7. Peer sends you those chunks
//   8. You glue chunks together into a piece, verify SHA-1 hash
// ============================================================


// ============================================================
// MESSAGE IDS
// ============================================================
// After the handshake, every message has a 1-byte "type" number.
// Instead of remembering "0 means choke, 1 means unchoke...",
// we give them names:
//
//   MessageId::Choke       →  actually stored as number 0
//   MessageId::Unchoke     →  actually stored as number 1
//   MessageId::Interested  →  actually stored as number 2
//   ... and so on
//
// "enum class" = a named group of constants (NOT a real class with methods)
// ": uint8_t" = store each value as a 1-byte unsigned number (0-255)

enum class MessageId : uint8_t {
    Choke         = 0,   // Peer says: "stop asking me for data"
    Unchoke       = 1,   // Peer says: "OK you can ask me for data now"
    Interested    = 2,   // "I want pieces that you have"
    NotInterested = 3,   // "I don't need anything from you"
    Have          = 4,   // "Hey, I just got piece #X"
    Bitfield      = 5,   // "Here's ALL the pieces I have" (bitmap)
    Request       = 6,   // "Please send me this specific block"
    Piece         = 7,   // "Here's the block data you asked for"
    Cancel        = 8,   // "Nevermind, don't send that block"
};


// ============================================================
// DATA TYPES EXPLAINED
// ============================================================
//
//   string       = text, like "192.168.1.1"
//   int          = a number (size varies by system — don't use for network!)
//
//   uint8_t      = exactly 1 byte  (0 to 255)         — for message IDs, raw bytes
//   uint16_t     = exactly 2 bytes (0 to 65535)       — for port numbers
//   uint32_t     = exactly 4 bytes (0 to ~4 billion)  — for lengths, piece indices
//
//   "u" = unsigned (no negative numbers)
//   "int" = integer
//   "_t" = it's a type
//   The number (8, 16, 32) = how many BITS
//
//   Why not just use "int"?
//   Because on the network, both sides must agree on EXACT byte sizes.
//   "int" could be 2 or 4 or 8 bytes depending on the computer.
//   uint32_t is ALWAYS 4 bytes on every machine. That's critical for protocols.
//
//   vector<uint8_t> = a resizable array of raw bytes (like a byte buffer)
//   array<uint8_t, 20> = a fixed-size array of exactly 20 bytes (for SHA-1 hashes)
//   vector<bool> = a resizable array of true/false values (for bitfield)
// ============================================================


// ============================================================
// STRUCT: PeerConnection
// ============================================================
// This holds everything about one connection to one peer.
// Think of it as a "phone call" object — who you called, the line,
// and the current state of your conversation.

struct PeerConnection {
    // --- WHO are we connected to? ---
    string ip;              // e.g. "192.168.1.5"
    uint16_t port;          // e.g. 6881 (2 bytes, max 65535)

    // --- THE CONNECTION itself ---
    int socket_fd = -1;     // "file descriptor" — the OS gives you a number
                            // that represents your open TCP connection.
                            // -1 means "not connected yet"
                            // Think of it like a phone line number.

    // --- CONVERSATION STATE ---
    // BitTorrent uses "choking" to control who can request data.
    // It's like a traffic light:
    //   - peer_choking = true  → RED LIGHT, you cannot request data
    //   - peer_choking = false → GREEN LIGHT, go ahead and request

    bool am_choking = true;      // Are WE blocking the peer? (we don't send to them)
    bool am_interested = false;  // Have we told them "I want your pieces"?
    bool peer_choking = true;    // Is the PEER blocking us? (they won't send to us)
    bool peer_interested = false;// Has the peer said they want our pieces?

    // --- WHAT PIECES does this peer have? ---
    // bitfield[0] = true means peer has piece #0
    // bitfield[5] = true means peer has piece #5
    vector<bool> bitfield;
};


// ============================================================
// STRUCT: PeerMessage
// ============================================================
// Every message after the handshake looks like this on the wire:
//
//   [4 bytes: length] [1 byte: message type] [rest: data/payload]
//
// Example: peer sends Unchoke (no extra data)
//   length = 1 (just the type byte, no payload)
//   id = 1 (Unchoke)
//   payload = empty
//
// Example: peer sends a Piece (block of file data)
//   length = 9 + block_size
//   id = 7 (Piece)
//   payload = [piece_index][offset][actual file bytes...]

struct PeerMessage {
    uint32_t length;            // How many bytes follow (4-byte number)
    MessageId id;               // What type of message (see enum above)
    vector<uint8_t> payload;    // The actual data (can be empty)
};


// ============================================================
// STEP 4.1: TCP Connect
// ============================================================
// What: Open a raw TCP connection to a peer's IP and port.
//       This is like dialing a phone number.
//       The OS does the 3-way handshake (SYN/SYN-ACK/ACK) for you.
//       When this returns, you have an open pipe.
//
// Input:  ip = "178.62.85.20", port = 6881
// Output: A PeerConnection with socket_fd set (the open pipe)

PeerConnection connect_to_peer(const string& ip, uint16_t port);


// ============================================================
// STEP 4.2: BitTorrent Handshake (68 bytes)
// ============================================================
// What: First thing sent after TCP connects. Both sides send 68 bytes.
//       This proves you're both talking about the SAME torrent file.
//
// The 68 bytes look like:
//   Byte  0:       19              (the number nineteen)
//   Bytes 1-19:    "BitTorrent protocol"  (literally this text)
//   Bytes 20-27:   00 00 00 00 00 00 00 00  (8 zeros, reserved for extensions)
//   Bytes 28-47:   <info_hash>     (20-byte SHA-1 of the torrent's info dict)
//   Bytes 48-67:   <peer_id>       (20-byte unique ID you made up for yourself)
//
// After sending, you receive 68 bytes back from the peer.
// If their info_hash (bytes 28-47) doesn't match yours → DISCONNECT.
// It means they're sharing a different file.
//
// Input:  conn (open connection), info_hash, peer_id
// Output: true = handshake OK, false = mismatch or error

bool perform_handshake(PeerConnection& conn,
                       const array<uint8_t, 20>& info_hash,
                       const array<uint8_t, 20>& peer_id);


// ============================================================
// STEP 4.3: Message Framing (reading/writing messages)
// ============================================================
// What: After the handshake, ALL messages use this format:
//
//   ┌──────────────┬────────────┬─────────────────┐
//   │ 4 bytes      │ 1 byte     │ variable length │
//   │ LENGTH       │ MESSAGE ID │ PAYLOAD (data)  │
//   └──────────────┴────────────┴─────────────────┘
//
//   LENGTH = how many bytes come after it (id + payload)
//            stored in "big-endian" (most significant byte first)
//
//   Special case: LENGTH = 0 → "keep-alive" (no id, no payload)
//                 Just means "I'm still here, don't disconnect me"
//
// Why big-endian?
//   Your CPU might store 256 as [00 01] or [01 00] depending on architecture.
//   Network protocols always use big-endian so both sides agree.
//   htonl() = "host to network long" — converts your CPU's format to big-endian
//   ntohl() = "network to host long" — converts big-endian back to your CPU's format

PeerMessage receive_message(PeerConnection& conn);
void send_message(PeerConnection& conn, MessageId id, const vector<uint8_t>& payload = {});


// ============================================================
// STEP 4.4: Handle Bitfield, Choke, Unchoke, Interested
// ============================================================
// What: A state machine. When you receive a message, update your state.
//
// Typical flow after handshake:
//   1. Peer sends Bitfield → "here are all pieces I have"
//   2. You send Interested → "I want some of those pieces"
//   3. Peer sends Unchoke  → "OK, you may request now"
//   4. NOW you can start requesting blocks
//
// Bitfield example (peer has pieces 0,1,3 out of 8):
//   Binary: 1 1 0 1 0 0 0 0  →  Byte value: 0xD0
//   bit 7 (leftmost) = piece 0 = HAS IT
//   bit 6 = piece 1 = HAS IT
//   bit 5 = piece 2 = doesn't have
//   bit 4 = piece 3 = HAS IT
//   ... etc

void handle_message(PeerConnection& conn, const PeerMessage& msg);


// ============================================================
// STEP 4.5: Request Blocks
// ============================================================
// What: Ask the peer for a specific chunk of a specific piece.
//
// A "piece" might be 256KB. That's too big for one request.
// So we split each piece into "blocks" of 16KB (16384 bytes).
//
// To request a block, you send a Request message with 12 bytes:
//   [4 bytes] which piece number (e.g. piece #5)
//   [4 bytes] byte offset within that piece (e.g. 0, 16384, 32768...)
//   [4 bytes] how many bytes you want (always 16384, except last block)
//
// Example: piece #5 is 50000 bytes
//   Request: piece=5, offset=0,     length=16384   (block 1)
//   Request: piece=5, offset=16384, length=16384   (block 2)
//   Request: piece=5, offset=32768, length=16384   (block 3)
//   Request: piece=5, offset=49152, length=848     (last block, remaining bytes)

void send_request(PeerConnection& conn,
                  uint32_t piece_index,
                  uint32_t offset,
                  uint32_t length = 16384);


// ============================================================
// STEP 4.6: Download a Full Piece & Verify
// ============================================================
// What: Request ALL blocks of one piece, assemble them, check SHA-1.
//
// Flow:
//   1. Calculate how many 16KB blocks fit in this piece
//   2. Send a Request for each block
//   3. Receive Piece messages back (each contains one block)
//   4. Glue all blocks together into one big buffer
//   5. SHA-1 hash the buffer
//   6. Compare with the expected hash from the .torrent file
//   7. If match → success! If not → data was corrupted, discard.
//
// Input:  piece_index, piece_length, expected 20-byte hash, connection
// Output: the piece data (or empty vector if hash didn't match)

vector<uint8_t> download_piece(PeerConnection& conn,
                               uint32_t piece_index,
                               uint32_t piece_length,
                               const array<uint8_t, 20>& expected_hash);


// ============================================================
// Helper: Close the connection (hang up the phone)
// ============================================================
void disconnect(PeerConnection& conn);
