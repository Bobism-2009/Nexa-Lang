#pragma once

#include <string>

namespace nexa {

// Which crypto helpers to emit. Hex-only must not pull <random> / SHA / base64 —
// those headers and objects are what blow a statically-linked exe from ~14KB to ~200KB.
struct CryptoEmit {
    bool hex = false;
    bool xorv = false;
    bool base64 = false;
    bool sha256 = false;
    bool sha1 = false;
    bool hmac = false;
    bool random = false;
};

inline std::string cryptoRuntimeCpp(const CryptoEmit& need) {
    std::string out;
    if (need.hex || need.xorv || need.base64 || need.sha256 || need.sha1 || need.hmac || need.random) {
        out += "#include <string>\n";
    }
    if (need.xorv) {
        out += "#include <vector>\n";
    }
    if (need.sha256 || need.sha1 || need.hmac) {
        out += "#include <cstdint>\n";
    }
    if (need.random) {
        out += "#include <random>\n";
    }

    if (need.xorv) {
        out += R"NEXA_CRYPTO(
static std::string __nexa_crypto_xor(const std::string& __s, const std::vector<int>& __keys) {
  if (__keys.empty()) return __s;
  std::string __out = __s;
  const size_t __n = __keys.size();
  for (size_t __i = 0; __i < __out.size(); ++__i) {
    __out[__i] = (char)((unsigned char)__out[__i] ^ (__keys[__i % __n] & 0xFF));
  }
  return __out;
}

static std::string __nexa_crypto_xor_key(const std::string& __s, const std::string& __key) {
  if (__key.empty()) return __s;
  std::string __out = __s;
  const size_t __n = __key.size();
  for (size_t __i = 0; __i < __out.size(); ++__i) {
    __out[__i] = (char)((unsigned char)__out[__i] ^ (unsigned char)__key[__i % __n]);
  }
  return __out;
}
)NEXA_CRYPTO";
    }

    if (need.hex) {
        out += R"NEXA_CRYPTO(
static std::string __nexa_crypto_hex_encode(const std::string& __s) {
  static const char* __hex = "0123456789abcdef";
  std::string __out;
  __out.resize(__s.size() * 2);
  for (size_t __i = 0; __i < __s.size(); ++__i) {
    unsigned char __c = (unsigned char)__s[__i];
    __out[__i * 2] = __hex[__c >> 4];
    __out[__i * 2 + 1] = __hex[__c & 0xF];
  }
  return __out;
}

static int __nexa_crypto_hex_nibble(char __c) {
  if (__c >= '0' && __c <= '9') return __c - '0';
  if (__c >= 'a' && __c <= 'f') return __c - 'a' + 10;
  if (__c >= 'A' && __c <= 'F') return __c - 'A' + 10;
  return -1;
}

static std::string __nexa_crypto_hex_decode(const std::string& __hex) {
  if (__hex.size() % 2 != 0) return std::string();
  std::string __out;
  __out.resize(__hex.size() / 2);
  for (size_t __i = 0; __i < __out.size(); ++__i) {
    int __hi = __nexa_crypto_hex_nibble(__hex[__i * 2]);
    int __lo = __nexa_crypto_hex_nibble(__hex[__i * 2 + 1]);
    if (__hi < 0 || __lo < 0) return std::string();
    __out[__i] = (char)((__hi << 4) | __lo);
  }
  return __out;
}
)NEXA_CRYPTO";
    }

    if (need.base64) {
        out += R"NEXA_CRYPTO(
static std::string __nexa_crypto_base64_encode(const std::string& __s) {
  static const char* __tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string __out;
  __out.reserve(((__s.size() + 2) / 3) * 4);
  size_t __i = 0;
  while (__i + 2 < __s.size()) {
    unsigned int __n = ((unsigned char)__s[__i] << 16) | ((unsigned char)__s[__i + 1] << 8) | (unsigned char)__s[__i + 2];
    __out.push_back(__tbl[(__n >> 18) & 63]);
    __out.push_back(__tbl[(__n >> 12) & 63]);
    __out.push_back(__tbl[(__n >> 6) & 63]);
    __out.push_back(__tbl[__n & 63]);
    __i += 3;
  }
  if (__i < __s.size()) {
    unsigned int __n = (unsigned char)__s[__i] << 16;
    __out.push_back(__tbl[(__n >> 18) & 63]);
    if (__i + 1 < __s.size()) {
      __n |= (unsigned char)__s[__i + 1] << 8;
      __out.push_back(__tbl[(__n >> 12) & 63]);
      __out.push_back(__tbl[(__n >> 6) & 63]);
      __out.push_back('=');
    } else {
      __out.push_back(__tbl[(__n >> 12) & 63]);
      __out.push_back('=');
      __out.push_back('=');
    }
  }
  return __out;
}

static int __nexa_crypto_b64_val(char __c) {
  if (__c >= 'A' && __c <= 'Z') return __c - 'A';
  if (__c >= 'a' && __c <= 'z') return __c - 'a' + 26;
  if (__c >= '0' && __c <= '9') return __c - '0' + 52;
  if (__c == '+') return 62;
  if (__c == '/') return 63;
  return -1;
}

static std::string __nexa_crypto_base64_decode(const std::string& __b64) {
  std::string __clean;
  __clean.reserve(__b64.size());
  for (char __c : __b64) {
    if (__c == '=' || __nexa_crypto_b64_val(__c) >= 0) __clean.push_back(__c);
  }
  if (__clean.size() % 4 != 0) return std::string();
  std::string __out;
  __out.reserve((__clean.size() / 4) * 3);
  for (size_t __i = 0; __i < __clean.size(); __i += 4) {
    int __a = __nexa_crypto_b64_val(__clean[__i]);
    int __b = __nexa_crypto_b64_val(__clean[__i + 1]);
    int __c = __clean[__i + 2] == '=' ? 0 : __nexa_crypto_b64_val(__clean[__i + 2]);
    int __d = __clean[__i + 3] == '=' ? 0 : __nexa_crypto_b64_val(__clean[__i + 3]);
    if (__a < 0 || __b < 0 || (__clean[__i + 2] != '=' && __c < 0) || (__clean[__i + 3] != '=' && __d < 0)) return std::string();
    unsigned int __n = (__a << 18) | (__b << 12) | (__c << 6) | __d;
    __out.push_back((char)((__n >> 16) & 0xFF));
    if (__clean[__i + 2] != '=') __out.push_back((char)((__n >> 8) & 0xFF));
    if (__clean[__i + 3] != '=') __out.push_back((char)(__n & 0xFF));
  }
  return __out;
}
)NEXA_CRYPTO";
    }

    if (need.random) {
        out += R"NEXA_CRYPTO(
static std::string __nexa_crypto_random_bytes(int __n) {
  if (__n <= 0) return std::string();
  std::string __out;
  __out.resize((size_t)__n);
  std::random_device __rd;
  for (int __i = 0; __i < __n; ++__i) {
    __out[(size_t)__i] = (char)(__rd() & 0xFF);
  }
  return __out;
}
)NEXA_CRYPTO";
    }

    if (need.sha256 || need.hmac) {
        out += R"NEXA_CRYPTO(
static uint32_t __nexa_rotr32(uint32_t __x, uint32_t __n) { return (__x >> __n) | (__x << (32 - __n)); }

static std::string __nexa_crypto_sha256_raw(const std::string& __msg) {
  static const uint32_t __K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
  };
  uint32_t __h0=0x6a09e667u,__h1=0xbb67ae85u,__h2=0x3c6ef372u,__h3=0xa54ff53au;
  uint32_t __h4=0x510e527fu,__h5=0x9b05688cu,__h6=0x1f83d9abu,__h7=0x5be0cd19u;
  uint64_t __bitlen = (uint64_t)__msg.size() * 8ull;
  std::string __data = __msg;
  __data.push_back((char)0x80);
  while ((__data.size() % 64) != 56) __data.push_back((char)0);
  for (int __i = 7; __i >= 0; --__i) __data.push_back((char)((__bitlen >> (__i * 8)) & 0xFF));
  for (size_t __off = 0; __off < __data.size(); __off += 64) {
    uint32_t __w[64];
    for (int __i = 0; __i < 16; ++__i) {
      __w[__i] = ((uint32_t)(unsigned char)__data[__off + __i * 4] << 24) |
                 ((uint32_t)(unsigned char)__data[__off + __i * 4 + 1] << 16) |
                 ((uint32_t)(unsigned char)__data[__off + __i * 4 + 2] << 8) |
                 ((uint32_t)(unsigned char)__data[__off + __i * 4 + 3]);
    }
    for (int __i = 16; __i < 64; ++__i) {
      uint32_t __s0 = __nexa_rotr32(__w[__i-15], 7) ^ __nexa_rotr32(__w[__i-15], 18) ^ (__w[__i-15] >> 3);
      uint32_t __s1 = __nexa_rotr32(__w[__i-2], 17) ^ __nexa_rotr32(__w[__i-2], 19) ^ (__w[__i-2] >> 10);
      __w[__i] = __w[__i-16] + __s0 + __w[__i-7] + __s1;
    }
    uint32_t __a=__h0,__b=__h1,__c=__h2,__d=__h3,__e=__h4,__f=__h5,__g=__h6,__h=__h7;
    for (int __i = 0; __i < 64; ++__i) {
      uint32_t __S1 = __nexa_rotr32(__e, 6) ^ __nexa_rotr32(__e, 11) ^ __nexa_rotr32(__e, 25);
      uint32_t __ch = (__e & __f) ^ ((~__e) & __g);
      uint32_t __t1 = __h + __S1 + __ch + __K[__i] + __w[__i];
      uint32_t __S0 = __nexa_rotr32(__a, 2) ^ __nexa_rotr32(__a, 13) ^ __nexa_rotr32(__a, 22);
      uint32_t __maj = (__a & __b) ^ (__a & __c) ^ (__b & __c);
      uint32_t __t2 = __S0 + __maj;
      __h=__g; __g=__f; __f=__e; __e=__d+__t1; __d=__c; __c=__b; __b=__a; __a=__t1+__t2;
    }
    __h0+=__a; __h1+=__b; __h2+=__c; __h3+=__d; __h4+=__e; __h5+=__f; __h6+=__g; __h7+=__h;
  }
  std::string __digest(32, '\0');
  uint32_t __hs[8] = {__h0,__h1,__h2,__h3,__h4,__h5,__h6,__h7};
  for (int __i = 0; __i < 8; ++__i) {
    __digest[__i*4] = (char)((__hs[__i] >> 24) & 0xFF);
    __digest[__i*4+1] = (char)((__hs[__i] >> 16) & 0xFF);
    __digest[__i*4+2] = (char)((__hs[__i] >> 8) & 0xFF);
    __digest[__i*4+3] = (char)(__hs[__i] & 0xFF);
  }
  return __digest;
}

static std::string __nexa_crypto_sha256(const std::string& __s) {
  return __nexa_crypto_hex_encode(__nexa_crypto_sha256_raw(__s));
}
)NEXA_CRYPTO";
    }

    if (need.sha1) {
        out += R"NEXA_CRYPTO(
static uint32_t __nexa_rotl32(uint32_t __x, uint32_t __n) { return (__x << __n) | (__x >> (32 - __n)); }

static std::string __nexa_crypto_sha1_raw(const std::string& __msg) {
  uint32_t __h0=0x67452301u,__h1=0xEFCDAB89u,__h2=0x98BADCFEu,__h3=0x10325476u,__h4=0xC3D2E1F0u;
  uint64_t __bitlen = (uint64_t)__msg.size() * 8ull;
  std::string __data = __msg;
  __data.push_back((char)0x80);
  while ((__data.size() % 64) != 56) __data.push_back((char)0);
  for (int __i = 7; __i >= 0; --__i) __data.push_back((char)((__bitlen >> (__i * 8)) & 0xFF));
  for (size_t __off = 0; __off < __data.size(); __off += 64) {
    uint32_t __w[80];
    for (int __i = 0; __i < 16; ++__i) {
      __w[__i] = ((uint32_t)(unsigned char)__data[__off + __i * 4] << 24) |
                 ((uint32_t)(unsigned char)__data[__off + __i * 4 + 1] << 16) |
                 ((uint32_t)(unsigned char)__data[__off + __i * 4 + 2] << 8) |
                 ((uint32_t)(unsigned char)__data[__off + __i * 4 + 3]);
    }
    for (int __i = 16; __i < 80; ++__i) __w[__i] = __nexa_rotl32(__w[__i-3] ^ __w[__i-8] ^ __w[__i-14] ^ __w[__i-16], 1);
    uint32_t __a=__h0,__b=__h1,__c=__h2,__d=__h3,__e=__h4;
    for (int __i = 0; __i < 80; ++__i) {
      uint32_t __f, __k;
      if (__i < 20) { __f = (__b & __c) | ((~__b) & __d); __k = 0x5A827999u; }
      else if (__i < 40) { __f = __b ^ __c ^ __d; __k = 0x6ED9EBA1u; }
      else if (__i < 60) { __f = (__b & __c) | (__b & __d) | (__c & __d); __k = 0x8F1BBCDCu; }
      else { __f = __b ^ __c ^ __d; __k = 0xCA62C1D6u; }
      uint32_t __temp = __nexa_rotl32(__a, 5) + __f + __e + __k + __w[__i];
      __e=__d; __d=__c; __c=__nexa_rotl32(__b, 30); __b=__a; __a=__temp;
    }
    __h0+=__a; __h1+=__b; __h2+=__c; __h3+=__d; __h4+=__e;
  }
  std::string __digest(20, '\0');
  uint32_t __hs[5] = {__h0,__h1,__h2,__h3,__h4};
  for (int __i = 0; __i < 5; ++__i) {
    __digest[__i*4] = (char)((__hs[__i] >> 24) & 0xFF);
    __digest[__i*4+1] = (char)((__hs[__i] >> 16) & 0xFF);
    __digest[__i*4+2] = (char)((__hs[__i] >> 8) & 0xFF);
    __digest[__i*4+3] = (char)(__hs[__i] & 0xFF);
  }
  return __digest;
}

static std::string __nexa_crypto_sha1(const std::string& __s) {
  return __nexa_crypto_hex_encode(__nexa_crypto_sha1_raw(__s));
}
)NEXA_CRYPTO";
    }

    if (need.hmac) {
        out += R"NEXA_CRYPTO(
static std::string __nexa_crypto_hmac_sha256(const std::string& __key, const std::string& __data) {
  std::string __k = __key;
  if (__k.size() > 64) __k = __nexa_crypto_sha256_raw(__k);
  if (__k.size() < 64) __k.append(64 - __k.size(), '\0');
  std::string __ipad(64, '\0'), __opad(64, '\0');
  for (size_t __i = 0; __i < 64; ++__i) {
    __ipad[__i] = (char)((unsigned char)__k[__i] ^ 0x36);
    __opad[__i] = (char)((unsigned char)__k[__i] ^ 0x5c);
  }
  return __nexa_crypto_hex_encode(__nexa_crypto_sha256_raw(__opad + __nexa_crypto_sha256_raw(__ipad + __data)));
}
)NEXA_CRYPTO";
    }

    return out;
}

}  // namespace nexa
