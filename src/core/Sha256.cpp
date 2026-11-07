#include "core/Sha256.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace lwf_sha256 {

namespace {

constexpr std::uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
    0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
    0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2,
};

std::uint32_t Rotr(std::uint32_t value, int bits) {
    return (value >> bits) | (value << (32 - bits));
}

struct Context {
    std::array<std::uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<std::uint8_t, 64> buffer{};
    std::size_t buffer_size = 0;
    std::uint64_t total_bytes = 0;
};

void Compress(Context& context, const std::uint8_t* block) {
    std::array<std::uint32_t, 64> w{};
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
               (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<std::uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = context.state[0];
    std::uint32_t b = context.state[1];
    std::uint32_t c = context.state[2];
    std::uint32_t d = context.state[3];
    std::uint32_t e = context.state[4];
    std::uint32_t f = context.state[5];
    std::uint32_t g = context.state[6];
    std::uint32_t h = context.state[7];

    for (int i = 0; i < 64; ++i) {
        const std::uint32_t s1 = Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = h + s1 + ch + kK[i] + w[i];
        const std::uint32_t s0 = Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    context.state[0] += a;
    context.state[1] += b;
    context.state[2] += c;
    context.state[3] += d;
    context.state[4] += e;
    context.state[5] += f;
    context.state[6] += g;
    context.state[7] += h;
}

void Update(Context& context, const std::string& data) {
    context.total_bytes += data.size();
    std::size_t offset = 0;
    if (context.buffer_size > 0) {
        const std::size_t needed = 64 - context.buffer_size;
        const std::size_t take = std::min(needed, data.size());
        std::memcpy(context.buffer.data() + context.buffer_size, data.data(), take);
        context.buffer_size += take;
        offset = take;
        if (context.buffer_size == 64) {
            Compress(context, context.buffer.data());
            context.buffer_size = 0;
        }
    }
    while (offset + 64 <= data.size()) {
        Compress(context, reinterpret_cast<const std::uint8_t*>(data.data()) + offset);
        offset += 64;
    }
    if (offset < data.size()) {
        const std::size_t rest = data.size() - offset;
        std::memcpy(context.buffer.data(), data.data() + offset, rest);
        context.buffer_size = rest;
    }
}

std::string Finish(Context& context) {
    const std::uint64_t bit_length = context.total_bytes * 8;
    std::string padding(1, static_cast<char>(0x80));
    std::size_t zeros = 55 - context.buffer_size;
    if (static_cast<std::int64_t>(zeros) < 0) {
        zeros += 64;
    }
    padding.append(zeros, static_cast<char>(0));
    for (int i = 7; i >= 0; --i) {
        padding.push_back(static_cast<char>((bit_length >> (i * 8)) & 0xFF));
    }
    Update(context, padding);

    std::string digest;
    digest.reserve(64);
    for (const std::uint32_t word : context.state) {
        for (int i = 3; i >= 0; --i) {
            char hex[3];
            std::snprintf(hex, sizeof(hex), "%02x", (word >> (i * 8)) & 0xFF);
            digest += hex;
        }
    }
    return digest;
}

}

std::string HexDigest(const std::string& input) {
    Context context;
    Update(context, input);
    return Finish(context);
}

std::string HexDigestOfFile(const std::string& path, bool& ok) {
    ok = false;
    std::error_code rc_ec;
    if (!std::filesystem::is_regular_file(path, rc_ec) || rc_ec) {
        return {};
    }
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    Context context;
    std::string chunk(65536, '\0');
    while (file.read(&chunk[0], static_cast<std::streamsize>(chunk.size())) ||
           file.gcount() > 0) {
        Update(context, chunk.substr(0, static_cast<std::size_t>(file.gcount())));
    }
    ok = true;
    return Finish(context);
}

}
