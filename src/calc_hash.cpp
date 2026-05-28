#include "calc_hash.h"
#include <fstream>
#include <sstream>
#include <CommonCrypto/CommonDigest.h>  // macOS built-in SHA-1 (no extra library needed)

// Step 2.1: Read file from disk as binary
// INPUT:  path = "test.torrent"
// OUTPUT: raw file bytes, e.g. "d8:announce35:http://tracker.com:6969/announce4:infod..."
//         (a long bencoded string — the entire .torrent file contents)
std::string read_file_bytes(const std::string& path) {
    // TODO:
    // 1. Open file in binary mode: std::ifstream file(path, std::ios::binary)
    // 2. Check if file opened successfully
    // 3. Read entire file into a string (use std::ostringstream or istreambuf_iterator)
    // 4. Return the string
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Could not open file: " + path);
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

// Step 2.4: SHA-1 hash using macOS CommonCrypto
// INPUT:  data = "d4:name8:test.txt6:lengthi12345e12:piece lengthi262144e6:pieces20:xxxxxxxxxxxxxxxxxxxx e"
//         (the bencoded info dict)
// OUTPUT: 20 raw bytes like "\xd6\x9f\x91\xe6..." (the info_hash)
//         In hex that's something like "d69f91e6b2ae4c542468d1073a71d4ea13879a7f"
std::string sha1_hash(const std::string& data) {
    // TODO:
    // 1. Create a 20-byte buffer: unsigned char hash[CC_SHA1_DIGEST_LENGTH]
    // 2. Call CC_SHA1(data.c_str(), data.size(), hash)
    // 3. Return std::string((char*)hash, 20)
    unsigned char hash[CC_SHA1_DIGEST_LENGTH];
    CC_SHA1(data.c_str(), data.size(), hash);
    return std::string((char*)hash, CC_SHA1_DIGEST_LENGTH);
}

// Step 2.2 + 2.3: Parse torrent file
// INPUT:  file_content = "d8:announce35:http://tracker.com:6969/announce4:infod6:lengthi92063e4:name10:sample.txt12:piece lengthi32768e6:pieces60:xxxxxyyyyyzzzzzaaaaabbbbbcccccdddddeeeeefffff ggggghhhhhjjjjjee"
// OUTPUT: TorrentFile {
//           announce = "http://tracker.com:6969/announce"
//           name = "sample.txt"
//           length = 92063
//           piece_length = 32768
//           pieces = "xxxxxyyyyyzzzz..." (60 bytes = 3 piece hashes)
//           info_hash = <20-byte SHA-1 of the bencoded info dict>
//         }
TorrentFile parse_torrent(const std::string& file_content) {
    // TODO:
    // 1. Decode the file_content using your bencode decoder: decode(file_content)
    // 2. Get the top-level dict: std::get<BencodeDict>(decoded.data)
    // 3. Extract "announce" → string
    // 4. Get "info" dict
    // 5. From info: extract "name", "length", "piece length", "pieces"
    // 6. Re-encode the info dict: encode(info_value)
    // 7. SHA-1 the re-encoded info dict → info_hash
    // 8. Fill and return TorrentFile struct
    size_t pos = 0;
    BencodeValue decoded = decode(file_content, pos);
    auto& dict = std::get<BencodeDict>(decoded.data);
    TorrentFile torrent;
    torrent.announce = std::get<BencodeString>(dict["announce"].data);
    auto& info_dict = std::get<BencodeDict>(dict["info"].data);
    torrent.name = std::get<BencodeString>(info_dict["name"].data);
    torrent.length = std::get<BencodeInt>(info_dict["length"].data);
    torrent.piece_length = std::get<BencodeInt>(info_dict["piece length"].data);
    torrent.pieces = std::get<BencodeString>(info_dict["pieces"].data);
    std::string encoded_info = encode(dict["info"]);
    torrent.info_hash = sha1_hash(encoded_info);
    return torrent;
}

// Step 2.3: Split piece hashes
// INPUT:  pieces = "aaaaaaaaaaaaaaaaaaaa bbbbbbbbbbbbbbbbbbbb" (40 bytes)
// OUTPUT: vector with 2 entries:
//         [0] = "aaaaaaaaaaaaaaaaaaaa" (20 bytes — hash of piece 0)
//         [1] = "bbbbbbbbbbbbbbbbbbbb" (20 bytes — hash of piece 1)
std::vector<std::string> split_piece_hashes(const std::string& pieces) {
    // TODO:
    // 1. Loop from i=0 to pieces.size(), step 20
    // 2. pieces.substr(i, 20) → push to vector
    // 3. Return vector
    std::vector<std::string> hashes;
    for (size_t i = 0; i < pieces.size(); i += 20) {
        hashes.push_back(pieces.substr(i, 20));
    }
    return hashes;
}
