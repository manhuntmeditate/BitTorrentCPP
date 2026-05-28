#pragma once

#include <string>
#include <vector>
#include "calc_hash.h"

// A peer is just an IP + port
struct Peer {
    std::string ip;     // e.g., "192.168.1.1"
    int port;           // e.g., 6881
};

// Step 3.1: Build the tracker announce URL with all required params
// INPUT:  torrent = parsed TorrentFile, port = 6881
// OUTPUT: full URL like "http://tracker.com/announce?info_hash=%ab%cd...&peer_id=...&port=6881&..."
std::string build_tracker_url(const TorrentFile& torrent, int port = 6881);

// Step 3.2: Make HTTP GET request to a URL (uses libcurl)
// INPUT:  url = the full tracker URL
// OUTPUT: raw response body (bencoded)
std::string http_get(const std::string& url);

// Step 3.3: Parse the tracker response to extract peers
// INPUT:  response = bencoded tracker response (contains "peers" key in compact format)
// OUTPUT: vector of Peer structs with ip and port
// Compact format: every 6 bytes = 4 bytes IP + 2 bytes port (big-endian)
std::vector<Peer> parse_peers(const std::string& response);

// Convenience: do the full flow (build URL → HTTP GET → parse response)
// INPUT:  torrent = parsed TorrentFile
// OUTPUT: list of peers
std::vector<Peer> get_peers(const TorrentFile& torrent);
