#include "bencode.h"

// Step 1.1: Decode bencoded string
// Example: "4:spam" → pos is at '4', read length, skip ':', read that many chars
// INPUT:  encoded = "4:spam", pos = 0
// OUTPUT: BencodeValue{"spam"}, pos = 6
BencodeValue decode_string(const std::string& encoded, size_t& pos) {
    // TODO: 
    // 1. Find the ':' starting from pos
    // 2. Extract the length (substring from pos to colon)
    // 3. Convert length string to integer
    // 4. Extract that many characters after the ':'
    // 5. Advance pos past the content
    // 6. Return BencodeValue with the string
    std::string result;
    size_t colon = encoded.find(':', pos);
    int counter = std::stoi(encoded.substr(pos, colon - pos));  // handles "12" etc.
    int start = colon + 1;
    for (int i = 0; i < counter; i++) {
        result += encoded[start + i];
    }
    pos = start + counter;
    return BencodeValue{result};
}

// Step 1.2: Decode bencoded integer
// Example: "i42e" → pos is at 'i', read until 'e'
// INPUT:  encoded = "i42e", pos = 0
// OUTPUT: BencodeValue{42}, pos = 4
BencodeValue decode_integer(const std::string& encoded, size_t& pos) {
    // TODO:
    // 1. Skip the 'i' (pos++)
    // 2. Find the 'e'
    // 3. Extract the number string between them
    // 4. Convert to long long (std::stoll)
    // 5. Advance pos past the 'e'
    // 6. Return BencodeValue with the integer
    pos++;
    size_t end = encoded.find('e', pos);
    std::string number_str = encoded.substr(pos, end - pos);
    long long result = 0;
    bool negative = false;
    for (char c : number_str) {
        if (c == '-') {
            negative = true;
            continue;
        }
        result = result * 10 + (c - '0');
    }
    if (negative) {
        result = -result;
    }
    pos = end + 1;
    return BencodeValue{result};

}

// Step 1.3: Decode bencoded list
// Example: "l4:spami42ee" → pos is at 'l'
// INPUT:  encoded = "l4:spami42ee", pos = 0
// OUTPUT: BencodeValue{vector: ["spam", 42]}, pos = 12
BencodeValue decode_list(const std::string& encoded, size_t& pos) {
    // TODO:
    // 1. Skip the 'l' (pos++)
    // 2. Loop: while encoded[pos] != 'e', call decode() and push to vector
    // 3. Skip the 'e' (pos++)
    // 4. Return BencodeValue with the list
    pos++;
    std::vector<BencodeValue> result;
    while (encoded[pos] != 'e') {
        result.push_back(decode(encoded, pos));
    }
    pos++;
    return BencodeValue{result};
}

// Step 1.4: Decode bencoded dictionary
// Example: "d3:foo3:bare" → pos is at 'd'
// INPUT:  encoded = "d3:foo3:bare", pos = 0
// OUTPUT: BencodeValue{map: {"foo": "bar"}}, pos = 12
BencodeValue decode_dict(const std::string& encoded, size_t& pos) {
    // TODO:
    // 1. Skip the 'd' (pos++)
    // 2. Loop: while encoded[pos] != 'e':
    //    a. Decode a string (the key) — keys are ALWAYS strings
    //    b. Decode any value (the value)
    //    c. Insert into map
    // 3. Skip the 'e' (pos++)
    // 4. Return BencodeValue with the dict
    pos++;
    std::map<std::string, BencodeValue> result;
    while (encoded[pos] != 'e') {
        BencodeValue key = decode_string(encoded, pos);
        BencodeValue value = decode(encoded, pos);
        result[std::get<BencodeString>(key.data)] = value;
    }
    pos++;
    return BencodeValue{result};
}

// Master decode: dispatch based on first character
BencodeValue decode(const std::string& encoded, size_t& pos) {
    // TODO:
    // Look at encoded[pos]:
    //   if digit (0-9) → decode_string
    //   if 'i'         → decode_integer
    //   if 'l'         → decode_list
    //   if 'd'         → decode_dict
    //   else           → throw std::runtime_error("Invalid bencode")
    char c = encoded[pos];
    if (c >= '0' && c <= '9') {
        return decode_string(encoded, pos);
    } else if (c == 'i') {
        return decode_integer(encoded, pos);
    } else if (c == 'l') {
        return decode_list(encoded, pos);
    } else if (c == 'd') {
        return decode_dict(encoded, pos);
    } else {
        throw std::runtime_error("Invalid bencode");
    }
}

// Convenience overload
BencodeValue decode(const std::string& encoded) {
    size_t pos = 0;
    return decode(encoded, pos);
}

// Step 1.5: Encode BencodeValue back to bencoded string
// INPUT:  BencodeValue{"spam"}
// OUTPUT: "4:spam"
// INPUT:  BencodeValue{42}
// OUTPUT: "i42e"
std::string encode(const BencodeValue& value) {
    if (std::holds_alternative<BencodeString>(value.data)) {
        auto& s = std::get<BencodeString>(value.data);
        return std::to_string(s.size()) + ":" + s;
    } else if (std::holds_alternative<BencodeInt>(value.data)) {
        auto i = std::get<BencodeInt>(value.data);
        return "i" + std::to_string(i) + "e";
    } else if (std::holds_alternative<BencodeList>(value.data)) {
        auto& l = std::get<BencodeList>(value.data);
        std::string result = "l";
        for (const auto& item : l) result += encode(item);
        return result + "e";
    } else {
        auto& d = std::get<BencodeDict>(value.data);
        std::string result = "d";
        for (const auto& [k, v] : d) {
            result += std::to_string(k.size()) + ":" + k + encode(v);
        }
        return result + "e";
    }
}
