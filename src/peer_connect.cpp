#include "peer_connect.h"
#include "calc_hash.h"
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>

using namespace std;

// ============================================================
// STEP 4.1: TCP Connect
// ============================================================
PeerConnection connect_to_peer(const string& ip, uint16_t port) {
    PeerConnection conn;
    conn.ip = ip;
    conn.port = port;

    // TODO:
    // 1. Create socket: socket(AF_INET, SOCK_STREAM, 0)
    // 2. Fill sockaddr_in with ip and port (use inet_pton)
    // 3. Call connect()
    // 4. Store fd in conn.socket_fd
    // 5. Return conn
    conn.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn.socket_fd < 0) {
        cerr << "Error creating socket" << endl;
        return conn;
    }
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        cerr << "Invalid IP address: " << ip << endl;
        close(conn.socket_fd);
        conn.socket_fd = -1;
        return conn;
    }
    if (connect(conn.socket_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Error connecting to " << ip << ":" << port << endl;
        close(conn.socket_fd);
        conn.socket_fd = -1;
        return conn;
    }
    return conn;
}

// ============================================================
// STEP 4.2: Handshake
// ============================================================
bool perform_handshake(PeerConnection& conn,
                       const array<uint8_t, 20>& info_hash,
                       const array<uint8_t, 20>& peer_id) {
    // TODO:
    // 1. Build 68-byte buffer:
    //    buf[0] = 19
    //    buf[1..19] = "BitTorrent protocol"
    //    buf[20..27] = 8 zero bytes (reserved)
    //    buf[28..47] = info_hash
    //    buf[48..67] = peer_id
    // 2. send(conn.socket_fd, buf, 68, 0)
    // 3. recv(conn.socket_fd, response, 68, 0)
    // 4. Check response[28..47] == info_hash
    // 5. Return true/false
    uint8_t bufer[68];
    bufer[0] = 19;
    memcpy(bufer + 1, "BitTorrent protocol", 19);
    memset(bufer + 20, 0, 8);
    memcpy(bufer + 28, info_hash.data(), 20);
    memcpy(bufer + 48, peer_id.data(), 20);
    if (send(conn.socket_fd, bufer, 68, 0) != 68) {
        cerr << "Error sending handshake" << endl;
        close(conn.socket_fd);
        return false;
    }

    uint8_t response[68];
    if (recv(conn.socket_fd, response, 68, 0) != 68) {
        cerr << "Error receiving handshake" << endl;
        close(conn.socket_fd);
        return false;
    }

    if (memcmp(response + 28, info_hash.data(), 20) != 0) {
        cerr << "Info hash mismatch" << endl;
        close(conn.socket_fd);
        return false;
    }

    return true;
}

// ============================================================
// STEP 4.3: Message Framing
// ============================================================
PeerMessage receive_message(PeerConnection& conn) {
    PeerMessage msg;

    // TODO:
    // 1. Read exactly 4 bytes into a uint32_t (big-endian → host order with ntohl)
    // 2. If length == 0 → keep-alive, return msg with length=0
    // 3. Read `length` bytes
    // 4. First byte = message id
    // 5. Remaining bytes = payload
    uint32_t length_be;
    ssize_t bytes_read = recv(conn.socket_fd, &length_be, 4, 0);
    if (bytes_read != 4) {
        cerr << "Error reading message length" << endl;
        close(conn.socket_fd);
        return msg;
    }
    uint32_t length = ntohl(length_be);
    if (length == 0) {
        msg.length = 0;
        return msg;
    }
    vector<uint8_t> buffer(length);
    size_t total_read = 0;
    while (total_read < length) {
        ssize_t chunk = recv(conn.socket_fd, buffer.data() + total_read, length - total_read, 0);
        if (chunk <= 0) {
            cerr << "Error reading message payload" << endl;
            close(conn.socket_fd);
            return msg;
        }
        total_read += chunk;
    }

    msg.id = static_cast<MessageId>(buffer[0]);
    msg.payload = vector<uint8_t>(buffer.begin() + 1, buffer.end());

    return msg;
}

void send_message(PeerConnection& conn, MessageId id, const vector<uint8_t>& payload) {
    // TODO:
    // 1. Calculate length = 1 + payload.size()
    // 2. Convert length to big-endian (htonl)
    // 3. Build buffer: [4 bytes length][1 byte id][payload]
    // 4. send() the buffer
    uint32_t length = 1 + payload.size();
    uint32_t length_be = htonl(length);
    vector<uint8_t> buffer(4 + length);
    memcpy(buffer.data(), &length_be, 4);
    buffer[4] = static_cast<uint8_t>(id);
    memcpy(buffer.data() + 5, payload.data(), payload.size());
    if (send(conn.socket_fd, buffer.data(), buffer.size(), 0) != static_cast<ssize_t>(buffer.size())) {
        cerr << "Error sending message" << endl;
        close(conn.socket_fd);
        conn.socket_fd = -1;
    }
}

// ============================================================
// STEP 4.4: Handle Messages
// ============================================================
void handle_message(PeerConnection& conn, const PeerMessage& msg) {
    switch (msg.id) {
        case MessageId::Choke:
            conn.peer_choking = true;
            break;
        case MessageId::Unchoke:
            conn.peer_choking = false;
            break;
        case MessageId::Interested:
            conn.peer_interested = true;
            break;
        case MessageId::NotInterested:
            conn.peer_interested = false;
            break;
        case MessageId::Bitfield:
            // TODO:
            // Each bit in payload represents a piece
            // Byte 0, bit 7 = piece 0; byte 0, bit 6 = piece 1; etc.
            // Populate conn.bitfield
            conn.bitfield.clear();
            for (uint8_t byte : msg.payload) {
                for (int i = 7; i >= 0; i--) {
                    conn.bitfield.push_back((byte >> i) & 1);
                }
            }
            break;
        case MessageId::Have:
            // TODO:
            // payload is 4 bytes = piece index (big-endian)
            // Set conn.bitfield[piece_index] = true
            if (msg.payload.size() == 4) {
                uint32_t piece_index;
                memcpy(&piece_index, msg.payload.data(), 4);
                piece_index = ntohl(piece_index);
                if (piece_index < conn.bitfield.size()) {
                    conn.bitfield[piece_index] = true;
                }
            }
            break;
        default:
            break;
    }
}

// ============================================================
// STEP 4.5: Request Blocks
// ============================================================
void send_request(PeerConnection& conn, uint32_t piece_index,
                  uint32_t offset, uint32_t length) {
    // TODO:
    // 1. Build 12-byte payload:
    //    [4 bytes] htonl(piece_index)
    //    [4 bytes] htonl(offset)
    //    [4 bytes] htonl(length)
    // 2. send_message(conn, MessageId::Request, payload)
    vector<uint8_t> payload(12);
    uint32_t piece_index_be = htonl(piece_index);
    uint32_t offset_be = htonl(offset);
    uint32_t length_be = htonl(length);
    memcpy(payload.data(), &piece_index_be, 4);
    memcpy(payload.data() + 4, &offset_be, 4);
    memcpy(payload.data() + 8, &length_be, 4);
    send_message(conn, MessageId::Request, payload);
}

// ============================================================
// STEP 4.6: Download & Verify Piece
// ============================================================
vector<uint8_t> download_piece(PeerConnection& conn,
                               uint32_t piece_index,
                               uint32_t piece_length,
                               const array<uint8_t, 20>& expected_hash) {
    // TODO:
    // 1. Calculate number of blocks: ceil(piece_length / 16384)
    // 2. Allocate piece buffer of piece_length bytes
    // 3. For each block:
    //      a. send_request(conn, piece_index, offset, block_size)
    //      b. receive_message() — expect MessageId::Piece
    //      c. Extract offset from payload bytes [4..7]
    //      d. Copy block data (payload[8:]) into piece buffer at that offset
    // 4. Compute SHA-1 of piece buffer
    // 5. Compare with expected_hash
    // 6. If match → return piece buffer; else → return empty vector
    uint32_t block_size = 16384;
    uint32_t num_blocks = (piece_length + block_size - 1) / block_size;
    vector<uint8_t> piece_data(piece_length);
    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t offset = i * block_size;
        uint32_t length = min(block_size, piece_length - offset);
        send_request(conn, piece_index, offset, length);

        // Read messages until we get a real one (skip keep-alives)
        PeerMessage msg;
        while (true) {
            msg = receive_message(conn);
            if (msg.length == 0) {
                // Keep-alive: peer is saying "I'm still here"
                // Wait a bit before reading again
                this_thread::sleep_for(chrono::seconds(1));
                continue;
            }
            break;
        }
        if (msg.id != MessageId::Piece || msg.payload.size() < 8) {
            cerr << "Error receiving piece block" << endl;
            return {};
        }
        uint32_t recv_piece_index, recv_offset;
        memcpy(&recv_piece_index, msg.payload.data(), 4);
        memcpy(&recv_offset, msg.payload.data() + 4, 4);
        recv_piece_index = ntohl(recv_piece_index);
        recv_offset = ntohl(recv_offset);
        if (recv_piece_index != piece_index || recv_offset != offset) {
            cerr << "Received unexpected piece block" << endl;
            return {};
        }
        memcpy(piece_data.data() + offset, msg.payload.data() + 8, length);
    }
    string piece_str(piece_data.begin(), piece_data.end());
    string piece_hash = sha1_hash(piece_str);
    if (piece_hash == string((char*)expected_hash.data(), 20)) {
        return piece_data;
    } 
    else {
        cerr << "Piece hash mismatch" << endl;
    }
    return {};
}

// ============================================================
// Helper: Disconnect
// ============================================================
void disconnect(PeerConnection& conn) {
    if (conn.socket_fd >= 0) {
        close(conn.socket_fd);
        conn.socket_fd = -1;
    }
}
