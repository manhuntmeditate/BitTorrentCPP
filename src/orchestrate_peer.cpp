#include "orchestrate_peer.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

using namespace std;

// ============================================================
// STEP 5.1: Worker Thread — One Thread Per Peer
// ============================================================
// One thread runs this function for its assigned peer.
// The connection stays open the ENTIRE time.
// It keeps downloading pieces until the queue is empty.

void peer_worker(
    const PeerInfo& peer,
    PieceQueue& piece_queue,
    vector<vector<uint8_t>>& results,
    mutex& results_mutex,
    const TorrentMetadata& metadata,
    const array<uint8_t, 20>& info_hash,
    const array<uint8_t, 20>& our_peer_id) {

    // TODO:
    //
    // --- SETUP (once per peer) ---
    //   1. connect_to_peer(peer.ip, peer.port)
    //   2. perform_handshake(conn, info_hash, our_peer_id)
    //   3. receive messages until we get Bitfield (now we know what peer has)
    //   4. send Interested
    //   5. wait for Unchoke
    //
    // --- DOWNLOAD LOOP ---
    //   6. while (true):
    //        a. peek a piece_index from piece_queue,check if its present in the bitfield of the peer
    //        b. if queue empty → break (we're done)
    //        c. check conn.bitfield[piece_index]
    //             → if peer DOESN'T have it → push back into queue, continue
    //             → if peer HAS it → proceed to download
    //        d. calculate piece_length (last piece may be shorter)
    //        e. call download_piece(conn, piece_index, piece_length, expected_hash)
    //        f. if success → lock results_mutex, store in results[piece_index]
    //        g. if fail → push piece_index back into queue
    //
    // --- CLEANUP ---
    //   7. disconnect(conn)
    PeerConnection conn = connect_to_peer(peer.ip, peer.port);
    if (conn.socket_fd < 0) {
        cerr << "Failed to connect to peer " << peer.ip << ":" << peer.port << endl;
        return;
    }
    if (!perform_handshake(conn, info_hash, our_peer_id)) {
        cerr << "Handshake failed with peer " << peer.ip << ":" << peer.port << endl;
        disconnect(conn);
        return;
    }
    PeerMessage msg;
    while (true) {
        msg = receive_message(conn);
        if (msg.length == 0) {
            // Keep-alive: peer is saying "I'm still here"
            // Wait a bit before reading again
            this_thread::sleep_for(chrono::seconds(1));
            continue;
        }
        if (msg.id == MessageId::Bitfield) {
            handle_message(conn, msg);
            break;
        }
        // Handle other message types as needed
        handle_message(conn, msg);
    }
    send_message(conn, MessageId::Interested);
    while (true) {
        msg = receive_message(conn);
        if (msg.length == 0) {
            // Keep-alive: peer is saying "I'm still here"
            // Wait a bit before reading again
            this_thread::sleep_for(chrono::seconds(1));
            continue;
        }
        if (msg.id == MessageId::Unchoke) {
            handle_message(conn, msg);
            break;
        }
        // Handle other message types as needed
        handle_message(conn, msg);
    }
    int skip_count = 0;
    while (true) { 
        uint32_t piece_index;
        if (!piece_queue.peek(piece_index)) {
            // Queue is empty, we're done
            break;
        }
        if (piece_index >= conn.bitfield.size() || !conn.bitfield[piece_index]) {
            // Peer doesn't have this piece, skip it
            piece_queue.pop(piece_index); // Remove it from queue
            piece_queue.push(piece_index); // Add it back to the end of the queue
            skip_count++;
            if (skip_count > (int)metadata.num_pieces) {
                // We've gone through the entire queue and can't download anything
                // This peer has nothing useful left to offer
                break;
            }
            continue;
        }
        // Peer has the piece — reset skip counter
        skip_count = 0;

        // Peer has the piece, let's try to download it
        if (!piece_queue.pop(piece_index)) {
            // Queue is empty, we're done
            break;
        }
        // get the piece length and hash
        uint32_t piece_length = (piece_index == metadata.num_pieces - 1) ?
            (metadata.total_length - piece_index * metadata.piece_length) :
            metadata.piece_length;
        array<uint8_t, 20> expected_hash = metadata.piece_hashes[piece_index];
        vector<uint8_t> piece_data = download_piece(conn, piece_index, piece_length, expected_hash);
        if (!piece_data.empty()) {
            // Success! Store the piece data
            lock_guard<mutex> lock(results_mutex);
            results[piece_index] = move(piece_data);
        } else {
            // Failed to download, put piece back in queue
            piece_queue.push(piece_index);
        }
    }

}

// ============================================================
// STEP 5.2: Download All Pieces (Coordinator)
// ============================================================
vector<vector<uint8_t>> download_all_pieces(
    const vector<PeerInfo>& peers,
    const TorrentMetadata& metadata,
    const array<uint8_t, 20>& info_hash,
    const array<uint8_t, 20>& our_peer_id) {

    // TODO:
    //   1. Create PieceQueue piece_queue
    //   2. Push all piece indices [0 .. metadata.num_pieces - 1] into it
    //   3. Create vector<vector<uint8_t>> results(metadata.num_pieces)
    //   4. Create a mutex to protect shared writes into `results`
    //   5. Spawn threads — one per peer (limit to ~5):
    //        thread t(peer_worker, peer, ref(piece_queue), ref(results),
    //                 ref(results_mutex), ref(metadata), ref(info_hash), ref(our_peer_id));
    //   6. join() all threads (wait for them to finish)
    //   7. return results
    PieceQueue piece_queue;
    for (uint32_t i = 0; i < metadata.num_pieces; i++) {
        piece_queue.push(i);
    }
    vector<vector<uint8_t>> results(metadata.num_pieces);
    mutex results_mutex;
    vector<thread> threads;
    for (size_t i = 0; i < peers.size() && i < 5; i++) {
        threads.emplace_back(peer_worker, peers[i], ref(piece_queue), ref(results),
                             ref(results_mutex), ref(metadata), ref(info_hash), ref(our_peer_id));
    }
    for (auto& t : threads) {
        t.join();
    }
    return results;
}


// ============================================================
// STEP 5.3: Write File to Disk
// ============================================================
bool write_file(const vector<vector<uint8_t>>& pieces, const string& output_path) {
    // Add a "downloads/" prefix so files go into a downloads folder
    string full_path = "downloads/" + output_path;

    // Open the file in binary mode
    // ios::binary = don't mess with the bytes (no newline conversion etc.)
    ofstream outfile;
    outfile.open(full_path, ios::binary);

    // Check if file opened
    if (!outfile.is_open()) {
        cerr << "Error opening file for writing: " << full_path << endl;
        return false;
    }

    // Write each piece one by one
    for (int i = 0; i < pieces.size(); i++) {
        // Get pointer to raw bytes and how many bytes
        const uint8_t* data = pieces[i].data();
        int size = pieces[i].size();

        // Cast to char* because write() expects char*, not uint8_t*
        // This does NOT change the bytes — just tells the compiler "treat these as chars"
        outfile.write((const char*)data, size);

        // Check if write succeeded
        if (!outfile) {
            cerr << "Error writing piece " << i << " to file" << endl;
            outfile.close();
            return false;
        }
    }

    outfile.close();
    cout << "File saved to: " << full_path << endl;
    return true;
}


// ============================================================
// PieceQueue Implementation
// ============================================================
void PieceQueue::push(uint32_t piece_index) {
    // Template only:
    //   1. lock the mutex
    //   2. q.push(piece_index)
    lock_guard<mutex> lock(mtx);
    q.push(piece_index);
}

bool PieceQueue::peek(uint32_t& piece_index) {
    // Template only:
    //   1. lock the mutex
    //   2. if q is empty, return false
    //   3. copy q.front() into piece_index (DON'T remove it)
    //   4. return true
    lock_guard<mutex> lock(mtx);
    if (q.empty()) return false;
    piece_index = q.front();
    return true;
}

bool PieceQueue::pop(uint32_t& piece_index) {
    // Template only:
    //   1. lock the mutex
    //   2. if q is empty, return false
    //   3. copy q.front() into piece_index
    //   4. q.pop()
    //   5. return true
    lock_guard<mutex> lock(mtx);
    if (q.empty()) return false;
    piece_index = q.front();
    q.pop();
    return true;
}


bool PieceQueue::empty() {
    // Template only:
    //   1. lock the mutex
    //   2. return q.empty()
    lock_guard<mutex> lock(mtx);
    return q.empty();
}
