#include <iostream>
#include <iomanip>
#include <cstring>
#include "bencode.h"
#include "calc_hash.h"
#include "get_peers.h"
#include "orchestrate_peer.h"

using namespace std;

int main(int argc, char* argv[]) {
    // ===== Usage =====
    // ./bt_client <path_to_torrent_file>
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <torrent_file>" << endl;
        return 1;
    }

    string torrent_path = argv[1];

    // ===== Phase 1 & 2: Parse .torrent file =====
    cout << "--- Parsing torrent file ---" << endl;
    string content = read_file_bytes(torrent_path);
    TorrentFile torrent = parse_torrent(content);

    cout << "Name:         " << torrent.name << endl;
    cout << "Announce:     " << torrent.announce << endl;
    cout << "File size:    " << torrent.length << " bytes" << endl;
    cout << "Piece length: " << torrent.piece_length << " bytes" << endl;

    vector<string> piece_hash_strings = split_piece_hashes(torrent.pieces);
    uint32_t num_pieces = piece_hash_strings.size();
    cout << "Num pieces:   " << num_pieces << endl;

    cout << "Info hash:    ";
    for (unsigned char c : torrent.info_hash) {
        cout << hex << setfill('0') << setw(2) << (int)c;
    }
    cout << dec << endl;  // reset to decimal

    // ===== Phase 3: Get peers from tracker =====
    cout << "\n--- Contacting tracker ---" << endl;
    vector<Peer> peers = get_peers(torrent);
    cout << "Found " << peers.size() << " peers" << endl;
    for (auto& p : peers) {
        cout << "  " << p.ip << ":" << p.port << endl;
    }

    if (peers.empty()) {
        cerr << "No peers found. Exiting." << endl;
        return 1;
    }

    // ===== Build TorrentMetadata for Phase 5 =====
    // Convert from Phase 2 types to Phase 5 types
    TorrentMetadata metadata;
    metadata.output_file = torrent.name;
    metadata.total_length = torrent.length;
    metadata.piece_length = torrent.piece_length;
    metadata.num_pieces = num_pieces;

    // Convert piece hashes from string format to array<uint8_t,20> format
    for (const string& hash_str : piece_hash_strings) {
        array<uint8_t, 20> hash_arr;
        memcpy(hash_arr.data(), hash_str.data(), 20);
        metadata.piece_hashes.push_back(hash_arr);
    }

    // Convert Peer structs to PeerInfo structs
    vector<PeerInfo> peer_list;
    for (const Peer& p : peers) {
        peer_list.push_back({p.ip, (uint16_t)p.port});
    }

    // Build info_hash and peer_id as array<uint8_t, 20>
    array<uint8_t, 20> info_hash_arr;
    memcpy(info_hash_arr.data(), torrent.info_hash.data(), 20);

    // Generate a random-ish peer_id (20 bytes, starts with "-BT0001-")
    array<uint8_t, 20> our_peer_id;
    string peer_id_str = "-BT0001-";
    for (int i = 0; i < 12; i++) {
        peer_id_str += char('0' + (rand() % 10));
    }
    memcpy(our_peer_id.data(), peer_id_str.data(), 20);

    // ===== Phase 5: Download all pieces =====
    cout << "\n--- Starting download ---" << endl;
    vector<vector<uint8_t>> pieces = download_all_pieces(peer_list, metadata, info_hash_arr, our_peer_id);

    // Check if all pieces were downloaded
    bool all_done = true;
    for (uint32_t i = 0; i < num_pieces; i++) {
        if (pieces[i].empty()) {
            cerr << "Missing piece " << i << "!" << endl;
            all_done = false;
        }
    }

    if (!all_done) {
        cerr << "Download incomplete. Some pieces are missing." << endl;
        return 1;
    }

    // ===== Write file to disk =====
    cout << "\n--- Writing file ---" << endl;
    if (write_file(pieces, metadata.output_file)) {
        cout << "Download complete! File: downloads/" << metadata.output_file << endl;
    } else {
        cerr << "Failed to write file." << endl;
        return 1;
    }

    return 0;
}
