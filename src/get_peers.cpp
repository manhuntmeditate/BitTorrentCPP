#include "get_peers.h"
#include <sstream>
#include <iomanip>
#include <curl/curl.h>

// Helper: URL-encode a binary string (like info_hash)
// INPUT:  "\xab\xcd\xef" (raw bytes)
// OUTPUT: "%AB%CD%EF"
std::string url_encode(const std::string& data) {
    // TODO:
    // For each byte in data:
    //   if it's alphanumeric or one of "-_.~" → keep as-is
    //   else → "%" + two uppercase hex digits
    // Example: byte 0xAB → "%AB"
    std::string encoded;
    const std::string unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
    for (unsigned char c : data) {
        if (unreserved.find(c) != std::string::npos) {
            encoded += c;
        } else {
            std::stringstream ss;
            ss << "%" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)c;
            encoded += ss.str();
        }
    }
}

// Helper: Generate a random 20-byte peer_id
// INPUT:  none
// OUTPUT: something like "-BT0001-aB3kF9xLm2nP" (20 bytes)
std::string generate_peer_id() {
    // TODO:
    // 1. Start with a prefix: "-BT0001-" (8 bytes)
    // 2. Fill remaining 12 bytes with random alphanumeric chars
    // 3. Return the 20-byte string
    std::string peer_id = "-BT0001-";
    const std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < 12; i++) {
        char curr = charset[rand() % charset.size()];
        peer_id += curr;
    }
    return peer_id;
}

// Step 3.1: Build tracker announce URL
// INPUT:  torrent with announce="http://tracker.com/announce", info_hash=<20 bytes>
// OUTPUT: "http://tracker.com/announce?info_hash=%XX%XX...&peer_id=-BT0001-xxxx&port=6881&uploaded=0&downloaded=0&left=92063&compact=1&event=started"
std::string build_tracker_url(const TorrentFile& torrent, int port) {
    // TODO:
    // 1. Start with torrent.announce + "?"
    // 2. Add params:
    //    info_hash  = url_encode(torrent.info_hash)  (20 raw bytes → percent-encoded)
    //    peer_id    = url_encode(generate_peer_id())
    //    port       = 6881
    //    uploaded   = 0
    //    downloaded = 0
    //    left       = torrent.length
    //    compact    = 1
    //    event      = started
    // 3. Join with "&"
    // 4. Return full URL
    std::string url = torrent.announce + "?";
    url += "info_hash=" + url_encode(torrent.info_hash);
    url += "&peer_id=" + url_encode(generate_peer_id());
    url += "&port=" + std::to_string(port);
    url += "&uploaded=0";
    url += "&downloaded=0";
    url += "&left=" + std::to_string(torrent.length);
    url += "&compact=1";
    url += "&event=started";
    return url;
}

// Step 3.2: HTTP GET request
// INPUT:  url = "http://tracker.com/announce?info_hash=..."
// OUTPUT: raw bencoded response body like "d8:intervali900e5:peers60:..."
std::string http_get(const std::string& url) {
    // Using libcurl:
    // 1. curl_easy_init()
    // 2. curl_easy_setopt(curl, CURLOPT_URL, url.c_str())
    // 3. curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback)
    // 4. curl_easy_perform(curl)
    // 5. curl_easy_cleanup(curl)
    // 6. Return response string
    CURL* curl = curl_easy_init();
    std::string response;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
            std::string* response = static_cast<std::string*>(userdata);
            response->append(ptr, size * nmemb);
            return size * nmemb;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }
    return response;
}

// Step 3.3: Parse compact peer list from tracker response
// INPUT:  response = "d8:intervali900e5:peers12:\xc0\xa8\x01\x01\x1a\xe1\xc0\xa8\x01\x02\x1a\xe2e"
//         (bencoded dict with "peers" = 12 bytes = 2 peers)
// OUTPUT: [{ip:"192.168.1.1", port:6881}, {ip:"192.168.1.2", port:6882}]
//
// How compact format works:
//   Every 6 bytes = 1 peer
//   Bytes 0-3: IP address (4 bytes, e.g., 192.168.1.1 = 0xC0 0xA8 0x01 0x01)
//   Bytes 4-5: Port (2 bytes, big-endian, e.g., 6881 = 0x1A 0xE1)
std::vector<Peer> parse_peers(const std::string& response) {
    // TODO:
    // 1. Decode the response with your bencode decoder
    // 2. Get the "peers" value as a string (compact format)
    // 3. Loop every 6 bytes:
    //    a. IP = byte[0].byte[1].byte[2].byte[3] (as decimal dotted string)
    //    b. Port = byte[4] * 256 + byte[5]  (big-endian)
    //    c. Push Peer{ip, port} to vector
    // 4. Return vector
    std::vector<Peer> peers;
    size_t pos = 0;
    BencodeValue decoded = decode(response, pos);
    auto& dict = std::get<BencodeDict>(decoded.data);
    auto& peers_str = std::get<std::string>(dict["peers"].data);
    for (size_t i = 0; i < peers_str.size(); i += 6) {
        unsigned char b1 = peers_str[i];
        unsigned char b2 = peers_str[i + 1];
        unsigned char b3 = peers_str[i + 2];
        unsigned char b4 = peers_str[i + 3];
        unsigned char p1 = peers_str[i + 4];
        unsigned char p2 = peers_str[i + 5];
        std::string ip = std::to_string(b1) + "." + std::to_string(b2) + "." + std::to_string(b3) + "." + std::to_string(b4);
        int port = p1 * 256 + p2;
        peers.push_back(Peer{ip, port});
    }
    return peers;
}

// Convenience function: full flow
std::vector<Peer> get_peers(const TorrentFile& torrent) {
    std::string url = build_tracker_url(torrent);
    std::string response = http_get(url);
    return parse_peers(response);
}
