#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <stdexcept>

// Forward declaration (needed because BencodeValue is recursive)
struct BencodeValue;

// Type aliases for clarity
using BencodeInt = long long;
using BencodeString = std::string;
using BencodeList = std::vector<BencodeValue>;
using BencodeDict = std::map<std::string, BencodeValue>;

// A BencodeValue can be one of these 4 types
struct BencodeValue {
    std::variant<BencodeInt, BencodeString, BencodeList, BencodeDict> data;
};

// ========== DECODING ==========
// Takes a bencoded string and a position reference.
// Returns a BencodeValue and advances pos past what was consumed.

// Step 1.1: Decode string like "4:spam" → "spam"
// Format: <length>:<content>
BencodeValue decode_string(const std::string& encoded, size_t& pos);

// Step 1.2: Decode integer like "i42e" → 42
// Format: i<number>e
BencodeValue decode_integer(const std::string& encoded, size_t& pos);

// Step 1.3: Decode list like "l4:spami42ee" → ["spam", 42]
// Format: l<items>e
BencodeValue decode_list(const std::string& encoded, size_t& pos);

// Step 1.4: Decode dict like "d3:foo3:bare" → {"foo": "bar"}
// Format: d<key><value><key><value>...e  (keys are always strings, sorted)
BencodeValue decode_dict(const std::string& encoded, size_t& pos);

// Master decode function — looks at first char and dispatches:
//   digit → decode_string
//   'i'   → decode_integer
//   'l'   → decode_list
//   'd'   → decode_dict
BencodeValue decode(const std::string& encoded, size_t& pos);

// Convenience overload: decode from beginning
BencodeValue decode(const std::string& encoded);

// ========== ENCODING ==========
// Step 1.5: Convert BencodeValue back to bencoded string
// (You'll need this later to compute info_hash)
std::string encode(const BencodeValue& value);
