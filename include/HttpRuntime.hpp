#pragma once

#include <string>

namespace nexa {

// OS-API HTTP, both ends (no third-party libs bundled).
//
// Client. Windows: WinHTTP (HTTP + HTTPS via Schannel)
//         macOS:   CFNetwork (HTTP + HTTPS)
//         Linux:   POSIX sockets; HTTPS via system libssl.so (dlopen, not linked)
//
// Every backend funnels through one primitive:
//
//   __nexa_http_perform(method, url, body, headers,
//                       &status, &respBody, &respHeaders) -> error string
//
// which returns "" when the exchange reached a server and a message when it
// did not. Everything above it is portable: __nexa_http_simple turns a non-2xx
// status into "HTTP <status>" (get/post/put/patch/delete), __nexa_http_request
// hands back status, body and raw header lines untouched.
//
// Server. http.localhost() and the calls around it, on the OS's own sockets:
// Winsock on Windows, POSIX sockets everywhere else. There is no third stack
// here because there is no reason for one -- listen/accept/recv/send is the
// same call sequence on both, so the split is a handful of typedefs rather
// than a second implementation.
//
// `needSimple`, `needResponse` and `needServer` gate the halves above the
// transports, and the client transport itself is emitted only when something
// above it calls -- so a program carries only what its calls can reach, the
// same needs-driven emission as std/gfx and std/crypto.
//
// Size. Requests and responses are put together and taken apart in one growable
// byte buffer (__nexa_hb) and by pointer, not as chains of std::string + and
// substr: each of those inlined its own allocation and copy, which made the Linux
// client 34KB and the Windows one 15KB. A std::string is made only for what the
// program gets back -- the body and the header lines.
inline std::string httpRuntimeCpp(bool needSimple = true, bool needResponse = true,
                                  bool needServer = false) {
    // The simple verbs and http.request are the only callers of the client
    // transport; a server-only program leaves all of it, libssl included, out.
    const bool needClient = needSimple || needResponse;
    std::string out = R"NEXA_HTTP(
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>

// A growable run of bytes, freed when it goes out of scope. `bad` is set when
// memory ran out; what was put before then is still there.
struct __nexa_hb {
  char* p = nullptr;
  size_t n = 0, cap = 0;
  bool bad = false;
  __nexa_hb() {}
  __nexa_hb(const __nexa_hb&) = delete;
  __nexa_hb& operator=(const __nexa_hb&) = delete;
  ~__nexa_hb() { std::free(p); }
  // Room for k more bytes at p + n, or nullptr.
  char* room(size_t k) {
    if (n + k > cap) {
      size_t c = cap ? cap * 2 : 256;
      while (c < n + k) c *= 2;
      char* q = (char*)std::realloc(p, c);
      if (!q) { bad = true; return nullptr; }
      p = q;
      cap = c;
    }
    return p + n;
  }
  void put(const char* s, size_t k) {
    char* d = room(k);
    if (d && k) { std::memcpy(d, s, k); n += k; }
  }
  void str(const char* s) { put(s, std::strlen(s)); }
  void num(long long v) {
    char b[24];
    int k = std::snprintf(b, sizeof b, "%lld", v);
    if (k > 0) put(b, (size_t)k);
  }
  void swap(__nexa_hb& o) {
    char* tp = p; p = o.p; o.p = tp;
    size_t t = n; n = o.n; o.n = t;
    t = cap; cap = o.cap; o.cap = t;
  }
};
// Where "\r\n\r\n" starts in p[0..n), or (size_t)-1.
[[maybe_unused]] static size_t __nexa_http_blank_line(const char* p, size_t n) {
  for (size_t i = 0; i + 4 <= n; i++) {
    if (p[i] == '\r' && p[i + 1] == '\n' && p[i + 2] == '\r' && p[i + 3] == '\n') return i;
  }
  return (size_t)-1;
}
// "Content-Type: text/html" -> name "Content-Type", value "text/html".
// A line with no ':' is not a header and is dropped.
[[maybe_unused]] static bool __nexa_http_split_header(const std::string& line, std::string& name, std::string& value) {
  size_t c = line.find(':');
  if (c == 0 || c == std::string::npos) return false;
  name = line.substr(0, c);
  while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
  size_t v = c + 1;
  while (v < line.size() && (line[v] == ' ' || line[v] == '\t')) v++;
  value = line.substr(v);
  while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) value.pop_back();
  return !name.empty();
}
// Header names are case-insensitive (RFC 9110); compare the part before ':'.
[[maybe_unused]] static bool __nexa_http_header_named(const std::string& line, const char* name) {
  size_t n = std::strlen(name);
  if (line.size() < n + 1 || line[n] != ':') return false;
  for (size_t i = 0; i < n; i++) {
    char a = line[i], b = name[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (a != b) return false;
  }
  return true;
}
[[maybe_unused]] static bool __nexa_http_contains_ci(const char* hay, size_t hn, const char* needle) {
  size_t n = std::strlen(needle);
  if (n == 0 || hn < n) return false;
  for (size_t i = 0; i + n <= hn; i++) {
    size_t k = 0;
    while (k < n) {
      char a = hay[i + k], b = needle[k];
      if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
      if (a != b) break;
      k++;
    }
    if (k == n) return true;
  }
  return false;
}
// The value of the first header line named `name`: a pointer into that line and
// its length, leading blanks and the line's end left off. nullptr when no line
// has the name.
[[maybe_unused]] static const char* __nexa_http_hval(const std::vector<std::string>& hs, const char* name, size_t* len) {
  const size_t nl = std::strlen(name);
  for (size_t i = 0; i < hs.size(); i++) {
    if (!__nexa_http_header_named(hs[i], name)) continue;
    const char* p = hs[i].data() + nl + 1;
    const char* e = hs[i].data() + hs[i].size();
    while (p < e && (*p == ' ' || *p == '\t')) p++;
    while (e > p && (e[-1] == '\r' || e[-1] == '\n')) e--;
    *len = (size_t)(e - p);
    return p;
  }
  *len = 0;
  return nullptr;
}
// Split a raw CRLF header block into lines, dropping the status line and the
// blank terminator. Continuation lines (leading space) fold onto the previous.
[[maybe_unused]] static void __nexa_http_split_headers(const char* p, size_t n, std::vector<std::string>& outv) {
  const char* e = p + n;
  while (p < e) {
    const char* nl = (const char*)std::memchr(p, '\n', (size_t)(e - p));
    const char* le = nl ? nl : e;
    const char* next = nl ? nl + 1 : e;
    while (le > p && (le[-1] == '\r' || le[-1] == '\n')) le--;
    const size_t k = (size_t)(le - p);
    if (k == 0 || (k >= 5 && std::memcmp(p, "HTTP/", 5) == 0)) { p = next; continue; }
    if ((*p == ' ' || *p == '\t') && !outv.empty()) {
      while (p < le && (*p == ' ' || *p == '\t')) p++;
      outv.back().push_back(' ');
      outv.back().append(p, (size_t)(le - p));
    } else if (std::memchr(p, ':', k)) {
      outv.emplace_back(p, k);
    }
    p = next;
  }
}
)NEXA_HTTP";
    if (needClient) {
        out += R"NEXA_HTTP_CLIENT(
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
// UTF-8 -> a NUL-terminated wide string for WinHTTP, from malloc: free() it.
static wchar_t* __nexa_http_wide(const char* s, size_t n) {
  int k = n ? MultiByteToWideChar(CP_UTF8, 0, s, (int)n, nullptr, 0) : 0;
  wchar_t* w = (wchar_t*)std::malloc(((size_t)k + 1) * sizeof(wchar_t));
  if (!w) return nullptr;
  if (k) MultiByteToWideChar(CP_UTF8, 0, s, (int)n, w, k);
  w[k] = 0;
  return w;
}
static std::string __nexa_http_perform(const std::string& method, const std::string& url,
                                       const std::string& body, const std::vector<std::string>& headers,
                                       int* status, std::string* respBody,
                                       std::vector<std::string>* respHeaders) {
  URL_COMPONENTS uc;
  memset(&uc, 0, sizeof(uc));
  uc.dwStructSize = sizeof(uc);
  wchar_t host[256]; wchar_t path[2048]; wchar_t extra[2048];
  uc.lpszHostName = host; uc.dwHostNameLength = 256;
  uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
  uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 2048;
  wchar_t* wurl = __nexa_http_wide(url.data(), url.size());
  BOOL cracked = wurl && WinHttpCrackUrl(wurl, 0, 0, &uc);
  std::free(wurl);
  if (!cracked || uc.dwHostNameLength >= 256) return "invalid URL";
  host[uc.dwHostNameLength] = 0;
  // The path and the query after it, as one; "/" when the URL has neither.
  wchar_t wpath[4097];
  const size_t pl = uc.dwUrlPathLength, el = uc.dwExtraInfoLength;
  if (pl + el > 4096) return "invalid URL";
  memcpy(wpath, path, pl * sizeof(wchar_t));
  memcpy(wpath + pl, extra, el * sizeof(wchar_t));
  wpath[pl + el] = 0;
  if (pl + el == 0) { wpath[0] = L'/'; wpath[1] = 0; }
  HINTERNET hSession = WinHttpOpen(L"NexaHTTP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) return "could not open HTTP session";
  INTERNET_PORT port = uc.nPort ? uc.nPort : (uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);
  HINTERNET hConnect = WinHttpConnect(hSession, host, port, 0);
  if (!hConnect) { WinHttpCloseHandle(hSession); return "could not connect"; }
  DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
  wchar_t* wmethod = __nexa_http_wide(method.data(), method.size());
  HINTERNET hRequest = wmethod ? WinHttpOpenRequest(hConnect, wmethod, wpath, nullptr,
    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags) : nullptr;
  std::free(wmethod);
  if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return "could not open request"; }
  // The caller's header lines, CRLF after each, made wide once.
  __nexa_hb hdr;
  bool userType = false;
  for (size_t i = 0; i < headers.size(); i++) {
    if (headers[i].empty()) continue;
    if (__nexa_http_header_named(headers[i], "Content-Type")) userType = true;
    hdr.put(headers[i].data(), headers[i].size());
    hdr.put("\r\n", 2);
  }
  // The default the POSIX path sends, and the one the reference promises:
  // "a Content-Type you supply replaces the default application/octet-stream".
  // WinHTTP adds none of its own, so a PUT or PATCH from Windows went out
  // without one while the same program on Linux sent it.
  if (!body.empty() && !userType) hdr.str("Content-Type: application/octet-stream\r\n");
  wchar_t* whdr = hdr.n ? __nexa_http_wide(hdr.p, hdr.n) : nullptr;
  BOOL ok = WinHttpSendRequest(hRequest,
    whdr ? whdr : WINHTTP_NO_ADDITIONAL_HEADERS,
    whdr ? (DWORD)-1L : 0,
    body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
    (DWORD)body.size(), (DWORD)body.size(), 0);
  std::free(whdr);
  if (!ok || !WinHttpReceiveResponse(hRequest, nullptr)) {
    // WinHTTP follows redirects itself and gives up after its own limit.
    // That is the condition the POSIX path reports as "too many redirects",
    // and answering "request failed" for it said nothing about why.
    const DWORD err = GetLastError();
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    // WinHttpConnect only makes a handle, so everything that can go wrong
    // between here and a reply comes back from this one call. Reported with
    // the same words the POSIX path uses, so a program reads the same reason
    // whichever platform it runs on rather than a flat "request failed".
    if (err == ERROR_WINHTTP_REDIRECT_FAILED) return "too many redirects";
    if (err == ERROR_WINHTTP_NAME_NOT_RESOLVED) return "could not resolve host";
    if (err == ERROR_WINHTTP_CANNOT_CONNECT) return "could not connect";
    if (err == ERROR_WINHTTP_CONNECTION_ERROR) return "could not connect";
    if (err == ERROR_WINHTTP_TIMEOUT) return "timed out";
    if (err == ERROR_WINHTTP_SECURE_FAILURE) return "TLS handshake failed";
    return "request failed";
  }
  DWORD code = 0, codeLen = sizeof(code);
  if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &code, &codeLen, WINHTTP_NO_HEADER_INDEX)) {
    *status = (int)code;
  }
  DWORD rawLen = 0;
  WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
    WINHTTP_NO_OUTPUT_BUFFER, &rawLen, WINHTTP_NO_HEADER_INDEX);
  if (rawLen > 0) {
    wchar_t* raw = (wchar_t*)std::malloc(rawLen + sizeof(wchar_t));
    if (raw && WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
          raw, &rawLen, WINHTTP_NO_HEADER_INDEX)) {
      raw[rawLen / sizeof(wchar_t)] = 0;
      int k = WideCharToMultiByte(CP_UTF8, 0, raw, -1, nullptr, 0, nullptr, nullptr);
      char* a = k > 0 ? (char*)std::malloc((size_t)k) : nullptr;
      if (a) {
        WideCharToMultiByte(CP_UTF8, 0, raw, -1, a, k, nullptr, nullptr);
        __nexa_http_split_headers(a, std::strlen(a), *respHeaders);
        std::free(a);
      }
    }
    std::free(raw);
  }
  char buf[8192];
  for (;;) {
    DWORD read = 0;
    if (!WinHttpReadData(hRequest, buf, (DWORD)sizeof(buf), &read) || read == 0) break;
    respBody->append(buf, (size_t)read);
  }
  WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
  return std::string();
}
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CFNetwork/CFNetwork.h>
static std::string __nexa_http_cfstr(CFStringRef s) {
  if (!s) return std::string();
  CFIndex len = CFStringGetLength(s);
  CFIndex max = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
  std::string out((size_t)max, '\0');
  if (!CFStringGetCString(s, &out[0], max, kCFStringEncodingUTF8)) return std::string();
  out.resize(std::strlen(out.c_str()));
  return out;
}
static std::string __nexa_http_perform(const std::string& method, const std::string& url,
                                       const std::string& body, const std::vector<std::string>& headers,
                                       int* status, std::string* respBody,
                                       std::vector<std::string>* respHeaders) {
  CFStringRef cfUrl = CFStringCreateWithCString(kCFAllocatorDefault, url.c_str(), kCFStringEncodingUTF8);
  CFStringRef cfMethod = CFStringCreateWithCString(kCFAllocatorDefault, method.c_str(), kCFStringEncodingUTF8);
  CFURLRef urlRef = CFURLCreateWithString(kCFAllocatorDefault, cfUrl, nullptr);
  if (!urlRef) {
    CFRelease(cfMethod); CFRelease(cfUrl);
    return "invalid URL";
  }
  CFHTTPMessageRef req = CFHTTPMessageCreateRequest(kCFAllocatorDefault, cfMethod, urlRef, kCFHTTPVersion1_1);
  for (size_t i = 0; i < headers.size(); i++) {
    std::string name, value;
    if (!__nexa_http_split_header(headers[i], name, value)) continue;
    CFStringRef n = CFStringCreateWithCString(kCFAllocatorDefault, name.c_str(), kCFStringEncodingUTF8);
    CFStringRef v = CFStringCreateWithCString(kCFAllocatorDefault, value.c_str(), kCFStringEncodingUTF8);
    CFHTTPMessageSetHeaderFieldValue(req, n, v);
    CFRelease(v); CFRelease(n);
  }
  if (!body.empty()) {
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8*)body.data(), (CFIndex)body.size());
    CFHTTPMessageSetBody(req, data);
    CFRelease(data);
  }
  CFReadStreamRef stream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, req);
  CFReadStreamSetProperty(stream, kCFStreamPropertyHTTPShouldAutoredirect, kCFBooleanTrue);
  if (!CFReadStreamOpen(stream)) {
    CFRelease(stream); CFRelease(req); CFRelease(urlRef); CFRelease(cfMethod); CFRelease(cfUrl);
    return "could not open request";
  }
  UInt8 buf[4096];
  for (;;) {
    CFIndex n = CFReadStreamRead(stream, buf, sizeof(buf));
    if (n <= 0) break;
    respBody->append((const char*)buf, (size_t)n);
  }
  // The response message is only complete once the body has been drained.
  CFHTTPMessageRef resp = (CFHTTPMessageRef)CFReadStreamCopyProperty(stream, kCFStreamPropertyHTTPResponseHeader);
  if (resp) {
    *status = (int)CFHTTPMessageGetResponseStatusCode(resp);
    CFDictionaryRef fields = CFHTTPMessageCopyAllHeaderFields(resp);
    if (fields) {
      CFIndex count = CFDictionaryGetCount(fields);
      if (count > 0) {
        std::vector<const void*> keys((size_t)count), vals((size_t)count);
        CFDictionaryGetKeysAndValues(fields, keys.data(), vals.data());
        for (CFIndex i = 0; i < count; i++) {
          std::string n = __nexa_http_cfstr((CFStringRef)keys[(size_t)i]);
          std::string v = __nexa_http_cfstr((CFStringRef)vals[(size_t)i]);
          if (!n.empty()) respHeaders->push_back(n + ": " + v);
        }
      }
      CFRelease(fields);
    }
    CFRelease(resp);
  }
  CFReadStreamClose(stream);
  CFRelease(stream); CFRelease(req); CFRelease(urlRef); CFRelease(cfMethod); CFRelease(cfUrl);
  return std::string();
}
#elif defined(__EMSCRIPTEN__)
#include <emscripten/fetch.h>
#include <emscripten.h>
#ifdef __EMSCRIPTEN_PTHREADS__
#include <emscripten/threading.h>
#endif
#include <cstring>
static std::string __nexa_http_perform(const std::string& method, const std::string& url,
                                       const std::string& body, const std::vector<std::string>& headers,
                                       int* status, std::string* respBody,
                                       std::vector<std::string>* respHeaders) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  std::memset(attr.requestMethod, 0, sizeof(attr.requestMethod));
  std::strncpy(attr.requestMethod, method.c_str(), sizeof(attr.requestMethod) - 1);
  // A synchronous fetch is only had on a worker: on the page's thread emscripten_fetch
  // refuses it and every request failed. There the request runs asynchronously and the
  // call waits with emscripten_sleep (the program is built with ASYNCIFY), so the page
  // goes on while it does.
  bool onWorker = false;
#ifdef __EMSCRIPTEN_PTHREADS__
  onWorker = !emscripten_is_main_browser_thread();
#endif
  volatile int done = 0;
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | (onWorker ? EMSCRIPTEN_FETCH_SYNCHRONOUS : 0);
  if (!onWorker) {
    attr.userData = (void*)&done;
    attr.onsuccess = [](emscripten_fetch_t* f) { *(volatile int*)f->userData = 1; };
    attr.onerror = [](emscripten_fetch_t* f) { *(volatile int*)f->userData = 1; };
  }
  // requestHeaders is a null-terminated name,value,name,value,... array that
  // has to outlive the fetch, so the split pieces are kept alive alongside it.
  std::vector<std::string> parts;
  std::vector<const char*> hdrPtrs;
  for (size_t i = 0; i < headers.size(); i++) {
    std::string name, value;
    if (!__nexa_http_split_header(headers[i], name, value)) continue;
    parts.push_back(name);
    parts.push_back(value);
  }
  if (!parts.empty()) {
    hdrPtrs.reserve(parts.size() + 1);
    for (size_t i = 0; i < parts.size(); i++) hdrPtrs.push_back(parts[i].c_str());
    hdrPtrs.push_back(nullptr);
    attr.requestHeaders = hdrPtrs.data();
  }
  if (!body.empty()) {
    attr.requestData = body.data();
    attr.requestDataSize = body.size();
  }
  emscripten_fetch_t* fetch = emscripten_fetch(&attr, url.c_str());
  if (!fetch) return "request failed";
  while (!onWorker && !done) emscripten_sleep(1);
  // A request the browser would not make (no server, CORS) ends with status 0.
  if (fetch->status == 0) {
    emscripten_fetch_close(fetch);
    return "request failed";
  }
  *status = (int)fetch->status;
  if (fetch->data && fetch->numBytes) respBody->assign(fetch->data, fetch->numBytes);
  size_t hdrLen = emscripten_fetch_get_response_headers_length(fetch);
  if (hdrLen > 0) {
    char* blob = (char*)std::malloc(hdrLen + 1);
    if (blob) {
      emscripten_fetch_get_response_headers(fetch, blob, hdrLen + 1);
      __nexa_http_split_headers(blob, std::strlen(blob), *respHeaders);
      std::free(blob);
    }
  }
  emscripten_fetch_close(fetch);
  return std::string();
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <dlfcn.h>
struct __nexa_SslApi {
  void* lib;
  int (*OPENSSL_init_ssl)(unsigned long, const void*);
  int (*SSL_library_init)();
  const void* (*TLS_client_method)();
  void* (*SSL_CTX_new)(const void*);
  void (*SSL_CTX_free)(void*);
  int (*SSL_CTX_set_default_verify_paths)(void*);
  void* (*SSL_new)(void*);
  int (*SSL_set_fd)(void*, int);
  int (*SSL_connect)(void*);
  int (*SSL_write)(void*, const void*, int);
  int (*SSL_read)(void*, void*, int);
  int (*SSL_shutdown)(void*);
  void (*SSL_free)(void*);
  void (*SSL_set_verify)(void*, int, void*);
  long (*SSL_get_verify_result)(const void*);
  long (*SSL_ctrl)(void*, int, long, void*);
  int (*SSL_set1_host)(void*, const char*);
};
// libssl's functions by name, in __nexa_SslApi's order after `lib`. A name with
// '?' in front may be missing; the rest must be there. TLS_client_method falls
// back to its OpenSSL 1.0 name.
static const __nexa_SslApi* __nexa_ssl_api() {
  static __nexa_SslApi a;
  static int once = 0;
  if (once) return a.lib ? &a : nullptr;
  once = 1;
  const char* crypto[] = { "libcrypto.so.3", "libcrypto.so.1.1", "libcrypto.so", nullptr };
  for (int i = 0; crypto[i]; i++) {
    if (dlopen(crypto[i], RTLD_LAZY | RTLD_GLOBAL)) break;
  }
  const char* ssl[] = { "libssl.so.3", "libssl.so.1.1", "libssl.so", nullptr };
  for (int i = 0; ssl[i]; i++) {
    a.lib = dlopen(ssl[i], RTLD_LAZY | RTLD_LOCAL);
    if (a.lib) break;
  }
  if (!a.lib) return nullptr;
  static const char* const names[] = {
    "?OPENSSL_init_ssl", "?SSL_library_init", "TLS_client_method", "SSL_CTX_new", "SSL_CTX_free",
    "?SSL_CTX_set_default_verify_paths", "SSL_new", "SSL_set_fd", "SSL_connect", "SSL_write",
    "SSL_read", "?SSL_shutdown", "SSL_free", "?SSL_set_verify", "?SSL_get_verify_result",
    "?SSL_ctrl", "?SSL_set1_host" };
  void** slot = (void**)&a.OPENSSL_init_ssl;
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    const bool optional = names[i][0] == '?';
    void* p = dlsym(a.lib, names[i] + (optional ? 1 : 0));
    if (!p && i == 2) p = dlsym(a.lib, "SSLv23_client_method");
    if (!p && !optional) { dlclose(a.lib); a.lib = nullptr; return nullptr; }
    slot[i] = p;
  }
  if (a.OPENSSL_init_ssl) a.OPENSSL_init_ssl(0, nullptr);
  else if (a.SSL_library_init) a.SSL_library_init();
  return &a;
}
// http[s]://host[:port]/path?query, taken apart in place: the host is copied out
// (brackets off an IPv6 one), the path points into the URL. tls is -1 for a
// scheme that is neither. false when there is no "://" or no host.
struct __nexa_http_url { int tls; int port; char host[256]; const char* path; size_t pathLen; };
static bool __nexa_http_parse_url(const char* u, size_t n, __nexa_http_url& o) {
  const char* e = u + n;
  const char* s = nullptr;
  for (const char* q = u; q + 3 <= e; q++) {
    if (q[0] == ':' && q[1] == '/' && q[2] == '/') { s = q; break; }
  }
  if (!s) return false;
  const size_t sl = (size_t)(s - u);
  o.tls = (sl == 5 && std::memcmp(u, "https", 5) == 0) ? 1 : (sl == 4 && std::memcmp(u, "http", 4) == 0) ? 0 : -1;
  const char* h = s + 3;
  const char* slash = (const char*)std::memchr(h, '/', (size_t)(e - h));
  const char* he = slash ? slash : e;
  o.path = slash ? slash : "/";
  o.pathLen = slash ? (size_t)(e - slash) : 1;
  const char* colon = nullptr;
  for (const char* q = he; q > h;) {
    if (*--q == ':') { colon = q; break; }
  }
  const char* br = (const char*)std::memchr(h, ']', (size_t)(he - h));
  const char* hs = h;
  const char* hostEnd = he;
  if (colon && (*h != '[' || (br && colon > br))) {
    hostEnd = colon;
    o.port = std::atoi(colon + 1);
  } else {
    o.port = o.tls == 1 ? 443 : 80;
  }
  if (hostEnd - hs >= 2 && *hs == '[' && hostEnd[-1] == ']') { hs++; hostEnd--; }
  const size_t hl = (size_t)(hostEnd - hs);
  if (hl == 0 || hl >= sizeof o.host) return false;
  std::memcpy(o.host, hs, hl);
  o.host[hl] = 0;
  return true;
}
static int __nexa_http_io_write(int fd, void* ssl, const __nexa_SslApi* api, const char* p, size_t n) {
  while (n > 0) {
    int chunk = n > 0x7fffffff ? 0x7fffffff : (int)n;
    int k = ssl ? api->SSL_write(ssl, p, chunk) : (int)send(fd, p, (size_t)chunk, 0);
    if (k <= 0) return 0;
    p += k;
    n -= (size_t)k;
  }
  return 1;
}
// Strip `Transfer-Encoding: chunked` framing, in place; the length left. WinHTTP
// and CFNetwork do this inside the stack; on raw sockets it is ours to do, or the
// hex chunk sizes leak into the body the program sees.
static size_t __nexa_http_dechunk(char* p, size_t n) {
  size_t i = 0, o = 0;
  while (i < n) {
    size_t eol = i;
    while (eol + 1 < n && !(p[eol] == '\r' && p[eol + 1] == '\n')) eol++;
    if (eol + 1 >= n) break;
    // The size, in hex; a ';' and chunk extensions after it are left alone.
    size_t j = i;
    while (j < eol && (p[j] == ' ' || p[j] == '\t')) j++;
    unsigned long k = 0;
    for (; j < eol; j++) {
      const char c = p[j];
      int d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 :
              (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
      if (d < 0) break;
      k = k * 16 + (unsigned long)d;
    }
    i = eol + 2;
    if (k == 0) break;                          // last chunk; trailers ignored
    const size_t avail = n - i;
    const size_t take = ((size_t)k < avail) ? (size_t)k : avail;   // truncated response
    std::memmove(p + o, p + i, take);
    o += take;
    i += take + 2;                              // chunk data is CRLF-terminated
  }
  return o;
}
// A Location, made absolute against the URL it answered, into `out`.
static bool __nexa_http_resolve(const char* base, size_t baseLen, const char* loc, size_t locLen, __nexa_hb& out) {
  if (!locLen) return false;
  for (size_t i = 0; i + 3 <= locLen; i++) {
    if (loc[i] == ':' && loc[i + 1] == '/' && loc[i + 2] == '/') { out.put(loc, locLen); return true; }
  }
  __nexa_http_url u;
  if (!__nexa_http_parse_url(base, baseLen, u)) return false;
  out.str(u.tls == 1 ? "https://" : "http://");
  out.str(u.host);
  if (u.port != (u.tls == 1 ? 443 : 80)) { out.put(":", 1); out.num(u.port); }
  if (loc[0] != '/') {
    // The directory of the request's path, its query and fragment left off.
    size_t pl = u.pathLen;
    for (size_t i = 0; i < pl; i++) {
      if (u.path[i] == '?' || u.path[i] == '#') { pl = i; break; }
    }
    while (pl > 0 && u.path[pl - 1] != '/') pl--;
    if (pl == 0) out.put("/", 1);
    else out.put(u.path, pl);
  }
  out.put(loc, locLen);
  return !out.bad;
}
// One request/response over one connection: nullptr when a response came back,
// what went wrong when the exchange never got that far.
static const char* __nexa_http_once(const char* method, const char* url, size_t urlLen,
                                    const char* body, size_t bodyLen, const std::vector<std::string>& headers,
                                    int* status, std::string* respBody,
                                    std::vector<std::string>* respHeaders) {
  __nexa_http_url u;
  if (!__nexa_http_parse_url(url, urlLen, u)) return "invalid URL";
  if (u.tls < 0) return "unsupported URL scheme";
  const __nexa_SslApi* api = nullptr;
  if (u.tls) {
    api = __nexa_ssl_api();
    if (!api) return "HTTPS requires libssl";
  }
  addrinfo hints; memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  char portStr[8];
  std::snprintf(portStr, sizeof portStr, "%d", u.port);
  if (getaddrinfo(u.host, portStr, &hints, &res) != 0 || !res) return "could not resolve host";
  int fd = -1;
  for (addrinfo* p = res; p; p = p->ai_next) {
    fd = (int)socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0) continue;
    if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
    close(fd); fd = -1;
  }
  freeaddrinfo(res);
  if (fd < 0) return "could not connect";
  void* ctx = nullptr;
  void* ssl = nullptr;
  const char* err = nullptr;
  if (u.tls) {
    const void* meth = api->TLS_client_method();
    ctx = meth ? api->SSL_CTX_new(meth) : nullptr;
    if (!ctx) { close(fd); return "TLS setup failed"; }
    if (api->SSL_CTX_set_default_verify_paths) api->SSL_CTX_set_default_verify_paths(ctx);
    ssl = api->SSL_new(ctx);
    if (!ssl) { api->SSL_CTX_free(ctx); close(fd); return "TLS setup failed"; }
    if (api->SSL_set_verify) api->SSL_set_verify(ssl, 1, nullptr);
    if (api->SSL_set1_host) api->SSL_set1_host(ssl, u.host);
    if (api->SSL_ctrl) api->SSL_ctrl(ssl, 55, 0, (void*)u.host);
    if (!api->SSL_set_fd(ssl, fd) || api->SSL_connect(ssl) != 1) {
      api->SSL_free(ssl);
      api->SSL_CTX_free(ctx);
      close(fd);
      return "TLS handshake failed";
    }
    if (api->SSL_get_verify_result && api->SSL_get_verify_result(ssl) != 0) err = "TLS certificate verify failed";
  }
  __nexa_hb raw;
  if (!err) {
    // Content-Length and Connection belong to this transport, so a caller's copy
    // is ignored; Host and Content-Type are the caller's to override.
    bool userHost = false, userType = false;
    for (size_t i = 0; i < headers.size(); i++) {
      if (__nexa_http_header_named(headers[i], "Host")) userHost = true;
      else if (__nexa_http_header_named(headers[i], "Content-Type")) userType = true;
    }
    __nexa_hb req;
    req.str(method);
    req.put(" ", 1);
    req.put(u.path, u.pathLen);
    req.str(" HTTP/1.1\r\n");
    if (!userHost) {
      req.str("Host: ");
      req.str(u.host);
      if (u.port != (u.tls == 1 ? 443 : 80)) { req.put(":", 1); req.num(u.port); }
      req.put("\r\n", 2);
    }
    req.str("Connection: close\r\n");
    for (size_t i = 0; i < headers.size(); i++) {
      if (headers[i].empty()) continue;
      if (__nexa_http_header_named(headers[i], "Content-Length")) continue;
      if (__nexa_http_header_named(headers[i], "Connection")) continue;
      req.put(headers[i].data(), headers[i].size());
      req.put("\r\n", 2);
    }
    if (bodyLen) {
      req.str("Content-Length: ");
      req.num((long long)bodyLen);
      req.put("\r\n", 2);
      if (!userType) req.str("Content-Type: application/octet-stream\r\n");
    }
    req.put("\r\n", 2);
    req.put(body, bodyLen);
    if (req.bad || !__nexa_http_io_write(fd, ssl, api, req.p, req.n)) err = "request failed";
  }
  if (!err) {
    for (;;) {
      char* d = raw.room(16384);
      if (!d) break;
      int n = ssl ? api->SSL_read(ssl, d, 16384) : (int)recv(fd, d, 16384, 0);
      if (n <= 0) break;
      raw.n += (size_t)n;
    }
  }
  if (ssl) {
    if (api->SSL_shutdown) api->SSL_shutdown(ssl);
    api->SSL_free(ssl);
    api->SSL_CTX_free(ctx);
  }
  close(fd);
  if (err) return err;
  if (raw.n == 0) return "no response";
  const size_t hdrEnd = __nexa_http_blank_line(raw.p, raw.n);
  if (hdrEnd == (size_t)-1) {
    respBody->assign(raw.p, raw.n);
    return nullptr;
  }
  __nexa_http_split_headers(raw.p, hdrEnd + 2, *respHeaders);
  if (hdrEnd >= 5 && std::memcmp(raw.p, "HTTP/", 5) == 0) {
    const char* sp = (const char*)std::memchr(raw.p, ' ', hdrEnd + 2);
    if (sp) *status = std::atoi(sp + 1);
  }
  char* payload = raw.p + hdrEnd + 4;
  size_t plen = raw.n - (hdrEnd + 4);
  size_t vl = 0;
  const char* te = __nexa_http_hval(*respHeaders, "Transfer-Encoding", &vl);
  if (te && __nexa_http_contains_ci(te, vl, "chunked")) {
    plen = __nexa_http_dechunk(payload, plen);
  } else {
    const char* cl = __nexa_http_hval(*respHeaders, "Content-Length", &vl);
    if (cl && vl) {
      unsigned long want = std::strtoul(cl, nullptr, 10);
      if (want < plen) plen = (size_t)want;
    }
  }
  respBody->assign(payload, plen);
  return nullptr;
}
static std::string __nexa_http_perform(const std::string& method, const std::string& url,
                                       const std::string& body, const std::vector<std::string>& headers,
                                       int* status, std::string* respBody,
                                       std::vector<std::string>* respHeaders) {
  // WinHTTP and CFNetwork follow redirects for us; do the same here, bounded,
  // and demote to GET the way both of those stacks do on 301/302/303.
  __nexa_hb target;
  target.put(url.data(), url.size());
  const char* verb = method.c_str();
  size_t bodyLen = body.size();
  for (int hop = 0; hop < 5; hop++) {
    *status = 0;
    respBody->clear();
    respHeaders->clear();
    const char* err = __nexa_http_once(verb, target.p ? target.p : "", target.n, body.data(), bodyLen,
                                       headers, status, respBody, respHeaders);
    if (err) return err;
    const int s = *status;
    if (s != 301 && s != 302 && s != 303 && s != 307 && s != 308) return std::string();
    size_t ll = 0;
    const char* loc = __nexa_http_hval(*respHeaders, "Location", &ll);
    __nexa_hb next;
    if (!loc || !__nexa_http_resolve(target.p, target.n, loc, ll, next)) return std::string();
    if (s != 307 && s != 308 && std::strcmp(verb, "GET") != 0 && std::strcmp(verb, "HEAD") != 0) {
      verb = "GET";
      bodyLen = 0;
    }
    target.swap(next);
  }
  return "too many redirects";
}
#endif
)NEXA_HTTP_CLIENT";
    }
    if (needSimple) {
        out += R"NEXA_HTTP_SIMPLE(
// get/post/put/patch/delete: the body on success, "HTTP <status>" on any
// non-2xx. Identical on all four backends -- http.request is where a program
// goes when it wants the status itself.
static __nexa_result<std::string> __nexa_http_simple(const std::string& method, const std::string& url,
                                                     const std::string& body,
                                                     const std::vector<std::string>& headers) {
  int status = 0;
  std::string respBody;
  std::vector<std::string> respHeaders;
  std::string err = __nexa_http_perform(method, url, body, headers, &status, &respBody, &respHeaders);
  if (!err.empty()) return __nexa_result<std::string>::make_err(err);
  if (status < 200 || status >= 300) {
    char b[24];
    std::snprintf(b, sizeof b, "HTTP %d", status);
    return __nexa_result<std::string>::make_err(b);
  }
  return __nexa_result<std::string>::make_ok(respBody);
}
)NEXA_HTTP_SIMPLE";
    }
    if (needResponse) {
        out += R"NEXA_HTTP_RESP(
struct __nexa_http_response {
    int status;
    std::string body;
    std::vector<std::string> headers;
    bool operator==(const __nexa_http_response& __o) const {
        return status == __o.status && body == __o.body && headers == __o.headers;
    }
    bool operator!=(const __nexa_http_response& __o) const { return !(*this == __o); }
};
// http.request: any status the server answered with is a success. Only a
// transport failure -- no route, no TLS, no response -- is an error.
static __nexa_result<__nexa_http_response> __nexa_http_request(const std::string& method, const std::string& url,
                                                               const std::string& body,
                                                               const std::vector<std::string>& headers) {
  __nexa_http_response r;
  r.status = 0;
  std::string err = __nexa_http_perform(method, url, body, headers, &r.status, &r.body, &r.headers);
  if (!err.empty()) return __nexa_result<__nexa_http_response>::make_err(err);
  return __nexa_result<__nexa_http_response>::make_ok(r);
}
)NEXA_HTTP_RESP";
    }
    if (needServer) {
        out += R"NEXA_HTTP_SERVER(
// http.localhost(): the other end of the same wire. A listening socket bound
// to 127.0.0.1 and nothing else -- the loopback address is not a default the
// program can talk out of, it is the whole of what this server binds.
//
// One request per connection: accept reads a whole request, reply or raw
// answers it and closes. That is the entire lifetime, so there is no keep-alive
// state to get wrong and no connection a program has to remember to close.
struct __nexa_http_server {
    int port;
    int socket;
    bool operator==(const __nexa_http_server& __o) const {
        return port == __o.port && socket == __o.socket;
    }
    bool operator!=(const __nexa_http_server& __o) const { return !(*this == __o); }
};
// The request a server took off the wire. Named "incoming" rather than
// "request" because http.request -- the client call -- already owns that name
// in the emitted C++, and a function name hides a struct of the same name.
struct __nexa_http_incoming {
    std::string method;
    std::string path;
    std::string body;
    std::vector<std::string> headers;
    int socket;
    bool operator==(const __nexa_http_incoming& __o) const {
        return method == __o.method && path == __o.path && body == __o.body &&
               headers == __o.headers && socket == __o.socket;
    }
    bool operator!=(const __nexa_http_incoming& __o) const { return !(*this == __o); }
};
// Reason phrases are informational -- no client routes on them -- so this is
// the set worth naming and everything else says "Status".
[[maybe_unused]] static const char* __nexa_http_reason(int status) {
  switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    default: break;
  }
  return "Status";
}
#if defined(__EMSCRIPTEN__)
// A page cannot listen. The calls exist so a program still builds for wasm;
// they say so rather than pretending to bind.
[[maybe_unused]] static __nexa_result<__nexa_http_server> __nexa_http_localhost(int port) {
  (void)port;
  return __nexa_result<__nexa_http_server>::make_err("HTTP server is not available on wasm");
}
[[maybe_unused]] static __nexa_result<__nexa_http_incoming> __nexa_http_accept(const __nexa_http_server& srv) {
  (void)srv;
  return __nexa_result<__nexa_http_incoming>::make_err("HTTP server is not available on wasm");
}
[[maybe_unused]] static int __nexa_http_reply(const __nexa_http_incoming& req, int status, const std::string& body,
                             const std::vector<std::string>& headers) {
  (void)req; (void)status; (void)body; (void)headers;
  return 0;
}
[[maybe_unused]] static int __nexa_http_raw(const __nexa_http_incoming& req, const std::string& bytes) {
  (void)req; (void)bytes;
  return 0;
}
[[maybe_unused]] static int __nexa_http_close(const __nexa_http_server& srv) {
  (void)srv;
  return 0;
}
#else
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET __nexa_sock_t;
#define __NEXA_SOCK_BAD INVALID_SOCKET
#define __NEXA_SEND_FLAGS 0
static void __nexa_sock_close(__nexa_sock_t s) { closesocket(s); }
// Winsock wants to be woken before its first call; every other platform does
// not have an equivalent, so this is the one thing the split is really for.
static int __nexa_sock_start() {
  static int started = 0;
  if (started) return 1;
  WSADATA w;
  if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return 0;
  started = 1;
  return 1;
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
typedef int __nexa_sock_t;
#define __NEXA_SOCK_BAD (-1)
// A client that hangs up mid-answer must not take the server's process with
// it: Linux says so per-send, macOS per-socket (SO_NOSIGPIPE, below).
#ifdef MSG_NOSIGNAL
#define __NEXA_SEND_FLAGS MSG_NOSIGNAL
#else
#define __NEXA_SEND_FLAGS 0
#endif
static void __nexa_sock_close(__nexa_sock_t s) { close(s); }
static int __nexa_sock_start() { return 1; }
#endif
static int __nexa_http_send_all(__nexa_sock_t s, const char* p, size_t n) {
  while (n > 0) {
    int chunk = n > 0x7fffffff ? 0x7fffffff : (int)n;
    int k = (int)send(s, p, chunk, __NEXA_SEND_FLAGS);
    if (k <= 0) return 0;
    p += k;
    n -= (size_t)k;
  }
  return 1;
}
[[maybe_unused]] static __nexa_result<__nexa_http_server> __nexa_http_localhost(int port) {
  __nexa_http_server srv;
  srv.port = 0;
  srv.socket = -1;
  const char* err = nullptr;
  __nexa_sock_t fd = __NEXA_SOCK_BAD;
  if (!__nexa_sock_start()) err = "sockets unavailable";
  else if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == __NEXA_SOCK_BAD) err = "could not open socket";
  if (!err) {
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, (socklen_t)sizeof(one));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(0x7f000001u);          // 127.0.0.1, always
    socklen_t blen = (socklen_t)sizeof(addr);
    if (bind(fd, (struct sockaddr*)&addr, (socklen_t)sizeof(addr)) != 0) err = "could not bind 127.0.0.1";
    else if (listen(fd, 16) != 0) err = "could not listen";
    // Port 0 asked the OS to pick; read back which one it picked, so a caller
    // never has to guess a free port.
    else if (getsockname(fd, (struct sockaddr*)&addr, &blen) != 0) err = "could not read the bound port";
    if (err) __nexa_sock_close(fd);
    srv.port = (int)ntohs(addr.sin_port);
  }
  if (err) return __nexa_result<__nexa_http_server>::make_err(err);
  srv.socket = (int)fd;
  return __nexa_result<__nexa_http_server>::make_ok(srv);
}
[[maybe_unused]] static __nexa_result<__nexa_http_incoming> __nexa_http_accept(const __nexa_http_server& srv) {
  __nexa_http_incoming req;
  req.socket = -1;
  __nexa_sock_t fd = accept((__nexa_sock_t)srv.socket, nullptr, nullptr);
  if (fd == __NEXA_SOCK_BAD) return __nexa_result<__nexa_http_incoming>::make_err("accept failed");
#ifdef SO_NOSIGPIPE
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (const char*)&one, (socklen_t)sizeof(one));
#endif
  // Read until the blank line that ends the headers, then as much body as
  // Content-Length says, all into one buffer.
  __nexa_hb data;
  size_t hdrEnd = (size_t)-1;
  size_t from = 0;  // where the search for the blank line goes on from
  for (;;) {
    size_t at = __nexa_http_blank_line(data.p + from, data.n - from);
    if (at != (size_t)-1) { hdrEnd = from + at; break; }
    from = data.n > 3 ? data.n - 3 : 0;
    char* d = data.room(4096);
    if (!d) break;
    int n = (int)recv(fd, d, 4096, 0);
    if (n <= 0) break;
    data.n += (size_t)n;
  }
  if (hdrEnd == (size_t)-1) {
    __nexa_sock_close(fd);
    return __nexa_result<__nexa_http_incoming>::make_err("malformed request");
  }
  // "GET /path HTTP/1.1" is not a header line, so it is split off before the
  // rest goes through the same header parser the client's responses use.
  size_t eol = 0;
  while (!(data.p[eol] == '\r' && data.p[eol + 1] == '\n')) eol++;
  __nexa_http_split_headers(data.p + eol + 2, hdrEnd - eol, req.headers);
  const char* line = data.p;
  const char* sp1 = (const char*)std::memchr(line, ' ', eol);
  if (sp1) {
    req.method.assign(line, (size_t)(sp1 - line));
    const char* rest = sp1 + 1;
    const char* le = line + eol;
    const char* sp2 = (const char*)std::memchr(rest, ' ', (size_t)(le - rest));
    req.path.assign(rest, (size_t)((sp2 ? sp2 : le) - rest));
  }
  size_t vl = 0;
  const char* cl = __nexa_http_hval(req.headers, "Content-Length", &vl);
  const size_t want = (cl && vl) ? (size_t)std::strtoul(cl, nullptr, 10) : (size_t)0;
  const size_t start = hdrEnd + 4;
  while (data.n - start < want) {
    char* d = data.room(4096);
    if (!d) break;
    int n = (int)recv(fd, d, 4096, 0);
    if (n <= 0) break;
    data.n += (size_t)n;
  }
  const size_t have = data.n - start;
  req.body.assign(data.p + start, have < want ? have : want);
  req.socket = (int)fd;
  return __nexa_result<__nexa_http_incoming>::make_ok(req);
}
// Content-Length and Connection are this transport's to write, the same way
// they are on the client side, so a caller's copy of either is dropped. A
// program that wants to frame a response itself reaches for http.raw.
[[maybe_unused]] static int __nexa_http_reply(const __nexa_http_incoming& req, int status, const std::string& body,
                             const std::vector<std::string>& headers) {
  __nexa_hb out;
  out.str("HTTP/1.1 ");
  out.num(status);
  out.put(" ", 1);
  out.str(__nexa_http_reason(status));
  out.put("\r\n", 2);
  for (size_t i = 0; i < headers.size(); i++) {
    if (headers[i].empty()) continue;
    if (__nexa_http_header_named(headers[i], "Content-Length")) continue;
    if (__nexa_http_header_named(headers[i], "Connection")) continue;
    out.put(headers[i].data(), headers[i].size());
    out.put("\r\n", 2);
  }
  out.str("Content-Length: ");
  out.num((long long)body.size());
  out.str("\r\nConnection: close\r\n\r\n");
  out.put(body.data(), body.size());
  __nexa_sock_t fd = (__nexa_sock_t)req.socket;
  int ok = !out.bad && __nexa_http_send_all(fd, out.p, out.n);
  __nexa_sock_close(fd);
  return ok;
}
[[maybe_unused]] static int __nexa_http_raw(const __nexa_http_incoming& req, const std::string& bytes) {
  __nexa_sock_t fd = (__nexa_sock_t)req.socket;
  int ok = __nexa_http_send_all(fd, bytes.data(), bytes.size());
  __nexa_sock_close(fd);
  return ok;
}
[[maybe_unused]] static int __nexa_http_close(const __nexa_http_server& srv) {
  __nexa_sock_close((__nexa_sock_t)srv.socket);
  return 1;
}
#endif
)NEXA_HTTP_SERVER";
    }
    return out;
}

}  // namespace nexa
