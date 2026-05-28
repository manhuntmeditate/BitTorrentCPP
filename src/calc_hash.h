#pragma once

#include <string>
#include <vector>
#include "bencode.h"

// Holds all parsed info from a .torrent file
struct TorrentFile {
    std::string announce;       // tracker URL
    std::string name;           // file name
    long long length;           // file size in bytes
    long long piece_length;     // size of each piece
    std::string pieces;         // concatenated 20-byte SHA-1 hashes (raw binary)
    std::string info_hash;      // 20-byte SHA-1 of bencoded info dict (raw binary)
};

// Step 2.1: Read .torrent file from disk into a string (binary)
// INPUT:  file path like "test.torrent"
// OUTPUT: raw bytes as std::string
std::string read_file_bytes(const std::string& path);

// Step 2.2 + 2.3: Parse the torrent file, extract all fields
// INPUT:  raw bytes of .torrent file
// OUTPUT: filled TorrentFile struct
TorrentFile parse_torrent(const std::string& file_content);

// Step 2.3: Split pieces string into individual 20-byte hashes
// INPUT:  pieces string (e.g., 60 bytes = 3 pieces)
// OUTPUT: vector of 20-byte hash strings
std::vector<std::string> split_piece_hashes(const std::string& pieces);

// Step 2.4: Calculate SHA-1 hash of a string
// INPUT:  any string (we'll pass bencoded info dict)
// OUTPUT: 20-byte raw SHA-1 hash
std::string sha1_hash(const std::string& data);
