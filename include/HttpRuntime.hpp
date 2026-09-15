#pragma once

#include <string>

namespace nexa {

// OS-API HTTP client (no third-party libs bundled).
// Windows: WinHTTP (HTTP + HTTPS via Schannel)
// macOS:   CFNetwork (HTTP + HTTPS)
// Linux:   POSIX sockets; HTTPS via system libssl.so (dlopen, not linked)
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
// `needSimple` and `needResponse` gate the two halves above that primitive, so
// a program carries only the one its calls can reach -- same needs-driven
// emission as std/gfx and std/crypto.
inline std::string httpRuntimeCpp(bool needSimple = true, bool needResponse = true) {
    std::string out = R"NEXA_HTTP(
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

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
[[maybe_unused]] static bool __nexa_http_contains_ci(const std::string& hay, const char* needle) {
  size_t n = std::strlen(needle);
  if (n == 0 || hay.size() < n) return false;
  for (size_t i = 0; i + n <= hay.size(); i++) {
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
[[maybe_unused]] static std::string __nexa_http_header_value(const std::vector<std::string>& hs, const char* name) {
  for (size_t i = 0; i < hs.size(); i++) {
    if (!__nexa_http_header_named(hs[i], name)) continue;
    std::string n, v;
    if (__nexa_http_split_header(hs[i], n, v)) return v;
  }
  return std::string();
}
// Split a raw CRLF header block into lines, dropping the status line and the
// blank terminator. Continuation lines (leading space) fold onto the previous.
[[maybe_unused]] static std::vector<std::string> __nexa_http_split_headers(const std::string& blob) {
  std::vector<std::string> outv;
  size_t i = 0;
  while (i < blob.size()) {
    size_t e = blob.find('\n', i);
    std::string line = (e == std::string::npos) ? blob.substr(i) : blob.substr(i, e - i);
    i = (e == std::string::npos) ? blob.size() : e + 1;
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (line.empty()) continue;
    if (line.size() >= 5 && line.compare(0, 5, "HTTP/") == 0) continue;
    if ((line[0] == ' ' || line[0] == '\t') && !outv.empty()) {
      size_t s = 0;
      while (s < line.size() && (line[s] == ' ' || line[s] == '\t')) s++;
      outv.back() += " ";
      outv.back() += line.substr(s);
      continue;
    }
    if (line.find(':') == std::string::npos) continue;
    outv.push_back(line);
  }
  return outv;
}
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
static std::wstring __nexa_http_widen(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring w((size_t)n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
  return w;
}
static std::string __nexa_http_narrow(const std::wstring& w) {
  if (w.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s((size_t)n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
  return s;
}
static std::string __nexa_http_perform(const std::string& method, const std::string& url,
                                       const std::string& body, const std::vector<std::string>& headers,
                                       int* status, std::string* respBody,
                                       std::vector<std::string>* respHeaders) {
  std::wstring wurl = __nexa_http_widen(url);
  URL_COMPONENTS uc;
  memset(&uc, 0, sizeof(uc));
  uc.dwStructSize = sizeof(uc);
  wchar_t host[256]; wchar_t path[2048]; wchar_t extra[2048];
  uc.lpszHostName = host; uc.dwHostNameLength = 256;
  uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
  uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 2048;
  if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) return "invalid URL";
  std::wstring wpath = std::wstring(path, uc.dwUrlPathLength) + std::wstring(extra, uc.dwExtraInfoLength);
  if (wpath.empty()) wpath = L"/";
  HINTERNET hSession = WinHttpOpen(L"NexaHTTP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) return "could not open HTTP session";
  INTERNET_PORT port = uc.nPort ? uc.nPort : (uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);
  HINTERNET hConnect = WinHttpConnect(hSession, std::wstring(host, uc.dwHostNameLength).c_str(), port, 0);
  if (!hConnect) { WinHttpCloseHandle(hSession); return "could not connect"; }
  DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
  std::wstring wmethod = __nexa_http_widen(method);
  HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), wpath.c_str(), nullptr,
    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return "could not open request"; }
  std::wstring extraHdrs;
  for (size_t i = 0; i < headers.size(); i++) {
    if (headers[i].empty()) continue;
    extraHdrs += __nexa_http_widen(headers[i]);
    extraHdrs += L"\r\n";
  }
  BOOL ok = WinHttpSendRequest(hRequest,
    extraHdrs.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extraHdrs.c_str(),
    extraHdrs.empty() ? 0 : (DWORD)-1L,
    body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
    (DWORD)body.size(), (DWORD)body.size(), 0);
  if (!ok || !WinHttpReceiveResponse(hRequest, nullptr)) {
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
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
    std::wstring raw(rawLen / sizeof(wchar_t) + 1, L'\0');
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
          &raw[0], &rawLen, WINHTTP_NO_HEADER_INDEX)) {
      *respHeaders = __nexa_http_split_headers(__nexa_http_narrow(raw.c_str()));
    }
  }
  for (;;) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &avail)) break;
    if (avail == 0) break;
    std::string chunk(avail, '\0');
    DWORD read = 0;
    if (!WinHttpReadData(hRequest, &chunk[0], avail, &read)) break;
    chunk.resize(read);
    *respBody += chunk;
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
#include <cstring>
static std::string __nexa_http_perform(const std::string& method, const std::string& url,
                                       const std::string& body, const std::vector<std::string>& headers,
                                       int* status, std::string* respBody,
                                       std::vector<std::string>* respHeaders) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  std::memset(attr.requestMethod, 0, sizeof(attr.requestMethod));
  std::strncpy(attr.requestMethod, method.c_str(), sizeof(attr.requestMethod) - 1);
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
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
  *status = (int)fetch->status;
  if (fetch->data && fetch->numBytes) respBody->assign(fetch->data, fetch->numBytes);
  size_t hdrLen = emscripten_fetch_get_response_headers_length(fetch);
  if (hdrLen > 0) {
    std::string blob(hdrLen + 1, '\0');
    emscripten_fetch_get_response_headers(fetch, &blob[0], hdrLen + 1);
    blob.resize(std::strlen(blob.c_str()));
    *respHeaders = __nexa_http_split_headers(blob);
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
static int __nexa_ssl_sym(void* lib, void** out, const char* name) {
  *out = dlsym(lib, name);
  return *out != nullptr;
}
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
  void* p = nullptr;
  if (__nexa_ssl_sym(a.lib, &p, "OPENSSL_init_ssl")) a.OPENSSL_init_ssl = (int (*)(unsigned long, const void*))p;
  if (__nexa_ssl_sym(a.lib, &p, "SSL_library_init")) a.SSL_library_init = (int (*)())p;
  if (!__nexa_ssl_sym(a.lib, &p, "TLS_client_method")) {
    if (!__nexa_ssl_sym(a.lib, &p, "SSLv23_client_method")) { dlclose(a.lib); a.lib = nullptr; return nullptr; }
  }
  a.TLS_client_method = (const void* (*)())p;
  if (!__nexa_ssl_sym(a.lib, &p, "SSL_CTX_new")) { dlclose(a.lib); a.lib = nullptr; return nullptr; }
  a.SSL_CTX_new = (void* (*)(const void*))p;
  if (!__nexa_ssl_sym(a.lib, &p, "SSL_CTX_free")) { dlclose(a.lib); a.lib = nullptr; return nullptr; }
  a.SSL_CTX_free = (void (*)(void*))p;
  if (__nexa_ssl_sym(a.lib, &p, "SSL_CTX_set_default_verify_paths"))
    a.SSL_CTX_set_default_verify_paths = (int (*)(void*))p;
  if (!__nexa_ssl_sym(a.lib, &p, "SSL_new")) { dlclose(a.lib); a.lib = nullptr; return nullptr; }
  a.SSL_new = (void* (*)(void*))p;
  if (!__nexa_ssl_sym(a.lib, &p, "SSL_set_fd")) { dlclose(a.lib); a.lib = nullptr; return nullptr; }
  a.SSL_set_fd = (int (*)(void*, int))p;
  if (!__nexa_ssl_sym(a.lib, &p, "SSL_connect")) { dlclose(a.lib); a.lib = nullptr; return nullptr; }
  a.SSL_connect = (int (*)(void*))p;
  if (!__nexa_ssl_sym(a.lib, &p, "SSL_write")) { dlclose(a.lib); a.lib = nullptr; return nullptr; }
  a.SSL_write = (int (*)(void*, const void*, int))p;
  if (!__nexa_ssl_sym(a.lib, &p, "SSL_read")) { dlclose(a.lib); a.lib = nullptr; return nullptr; }
  a.SSL_read = (int (*)(void*, void*, int))p;
  if (__nexa_ssl_sym(a.lib, &p, "SSL_shutdown")) a.SSL_shutdown = (int (*)(void*))p;
  if (!__nexa_ssl_sym(a.lib, &p, "SSL_free")) { dlclose(a.lib); a.lib = nullptr; return nullptr; }
  a.SSL_free = (void (*)(void*))p;
  if (__nexa_ssl_sym(a.lib, &p, "SSL_set_verify")) a.SSL_set_verify = (void (*)(void*, int, void*))p;
  if (__nexa_ssl_sym(a.lib, &p, "SSL_get_verify_result")) a.SSL_get_verify_result = (long (*)(const void*))p;
  if (__nexa_ssl_sym(a.lib, &p, "SSL_ctrl")) a.SSL_ctrl = (long (*)(void*, int, long, void*))p;
  if (__nexa_ssl_sym(a.lib, &p, "SSL_set1_host")) a.SSL_set1_host = (int (*)(void*, const char*))p;
  if (a.OPENSSL_init_ssl) a.OPENSSL_init_ssl(0, nullptr);
  else if (a.SSL_library_init) a.SSL_library_init();
  return &a;
}
static bool __nexa_http_parse_url(const std::string& url, std::string& scheme, std::string& host, int& port, std::string& path) {
  scheme.clear(); host.clear(); path = "/"; port = 80;
  size_t sp = url.find("://");
  if (sp == std::string::npos) return false;
  scheme = url.substr(0, sp);
  size_t start = sp + 3;
  size_t slash = url.find('/', start);
  std::string hostport = (slash == std::string::npos) ? url.substr(start) : url.substr(start, slash - start);
  if (slash != std::string::npos) path = url.substr(slash);
  size_t colon = hostport.rfind(':');
  size_t bracket = hostport.find(']');
  if (colon != std::string::npos && (hostport[0] != '[' || (bracket != std::string::npos && colon > bracket))) {
    host = hostport.substr(0, colon);
    port = std::atoi(hostport.c_str() + colon + 1);
  } else {
    host = hostport;
    port = (scheme == "https") ? 443 : 80;
  }
  if (host.size() >= 2 && host[0] == '[' && host.back() == ']') host = host.substr(1, host.size() - 2);
  return !host.empty();
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
// Strip `Transfer-Encoding: chunked` framing. WinHTTP and CFNetwork do this
// inside the stack; on raw sockets it is ours to do, or the hex chunk sizes
// leak into the body the program sees.
static std::string __nexa_http_dechunk(const std::string& in) {
  std::string out;
  size_t i = 0;
  while (i < in.size()) {
    size_t eol = in.find("\r\n", i);
    if (eol == std::string::npos) break;
    std::string sizeLine = in.substr(i, eol - i);
    size_t semi = sizeLine.find(';');          // chunk extensions
    if (semi != std::string::npos) sizeLine = sizeLine.substr(0, semi);
    unsigned long n = std::strtoul(sizeLine.c_str(), nullptr, 16);
    i = eol + 2;
    if (n == 0) break;                          // last chunk; trailers ignored
    size_t avail = in.size() - i;
    size_t take = ((size_t)n < avail) ? (size_t)n : avail;   // truncated response
    out.append(in, i, take);
    i += take + 2;                              // chunk data is CRLF-terminated
  }
  return out;
}
// Turn a Location value into an absolute URL against the request it answered.
static std::string __nexa_http_resolve_url(const std::string& base, const std::string& loc) {
  if (loc.empty()) return std::string();
  if (loc.find("://") != std::string::npos) return loc;
  std::string scheme, host, path; int port = 80;
  if (!__nexa_http_parse_url(base, scheme, host, port, path)) return std::string();
  std::string origin = scheme + "://" + host;
  if (!((scheme == "http" && port == 80) || (scheme == "https" && port == 443))) {
    origin += ":" + std::to_string(port);
  }
  if (loc[0] == '/') return origin + loc;
  size_t q = path.find_first_of("?#");
  if (q != std::string::npos) path = path.substr(0, q);
  size_t slash = path.rfind('/');
  std::string dir = (slash == std::string::npos) ? std::string("/") : path.substr(0, slash + 1);
  return origin + dir + loc;
}
// One request/response over one connection. Returns "" when a response came
// back, an error message when the exchange never got that far.
static std::string __nexa_http_once(const std::string& method, const std::string& url,
                                    const std::string& body, const std::vector<std::string>& headers,
                                    int* status, std::string* respBody,
                                    std::vector<std::string>* respHeaders) {
  std::string scheme, host, path; int port = 80;
  if (!__nexa_http_parse_url(url, scheme, host, port, path)) return "invalid URL";
  int tls = 0;
  if (scheme == "https") tls = 1;
  else if (scheme != "http") return "unsupported URL scheme";
  const __nexa_SslApi* api = nullptr;
  if (tls) {
    api = __nexa_ssl_api();
    if (!api) return "HTTPS requires libssl";
  }
  addrinfo hints; memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  std::string portStr = std::to_string(port);
  if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
    return "could not resolve host";
  }
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
  if (tls) {
    const void* meth = api->TLS_client_method();
    ctx = meth ? api->SSL_CTX_new(meth) : nullptr;
    if (!ctx) { close(fd); return "TLS setup failed"; }
    if (api->SSL_CTX_set_default_verify_paths) api->SSL_CTX_set_default_verify_paths(ctx);
    ssl = api->SSL_new(ctx);
    if (!ssl) { api->SSL_CTX_free(ctx); close(fd); return "TLS setup failed"; }
    if (api->SSL_set_verify) api->SSL_set_verify(ssl, 1, nullptr);
    if (api->SSL_set1_host) api->SSL_set1_host(ssl, host.c_str());
    if (api->SSL_ctrl) api->SSL_ctrl(ssl, 55, 0, (void*)host.c_str());
    if (!api->SSL_set_fd(ssl, fd) || api->SSL_connect(ssl) != 1) {
      api->SSL_free(ssl);
      api->SSL_CTX_free(ctx);
      close(fd);
      return "TLS handshake failed";
    }
    if (api->SSL_get_verify_result && api->SSL_get_verify_result(ssl) != 0) {
      if (api->SSL_shutdown) api->SSL_shutdown(ssl);
      api->SSL_free(ssl);
      api->SSL_CTX_free(ctx);
      close(fd);
      return "TLS certificate verify failed";
    }
  }
  // Content-Length and Connection belong to this transport, so a caller's copy
  // is ignored; Host and Content-Type are the caller's to override.
  bool userHost = false, userType = false;
  for (size_t i = 0; i < headers.size(); i++) {
    if (__nexa_http_header_named(headers[i], "Host")) userHost = true;
    else if (__nexa_http_header_named(headers[i], "Content-Type")) userType = true;
  }
  std::string req = method + " " + path + " HTTP/1.1\r\n";
  if (!userHost) {
    std::string hostHdr = host;
    if (!((scheme == "http" && port == 80) || (scheme == "https" && port == 443))) {
      hostHdr += ":";
      hostHdr += std::to_string(port);
    }
    req += "Host: " + hostHdr + "\r\n";
  }
  req += "Connection: close\r\n";
  for (size_t i = 0; i < headers.size(); i++) {
    if (headers[i].empty()) continue;
    if (__nexa_http_header_named(headers[i], "Content-Length")) continue;
    if (__nexa_http_header_named(headers[i], "Connection")) continue;
    req += headers[i];
    req += "\r\n";
  }
  if (!body.empty()) {
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    if (!userType) req += "Content-Type: application/octet-stream\r\n";
  }
  req += "\r\n";
  req += body;
  if (!__nexa_http_io_write(fd, ssl, api, req.data(), req.size())) {
    if (ssl) {
      if (api->SSL_shutdown) api->SSL_shutdown(ssl);
      api->SSL_free(ssl);
      api->SSL_CTX_free(ctx);
    }
    close(fd);
    return "request failed";
  }
  std::string raw;
  char buf[4096];
  for (;;) {
    int n = ssl ? api->SSL_read(ssl, buf, (int)sizeof(buf)) : (int)recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) break;
    raw.append(buf, (size_t)n);
  }
  if (ssl) {
    if (api->SSL_shutdown) api->SSL_shutdown(ssl);
    api->SSL_free(ssl);
    api->SSL_CTX_free(ctx);
  }
  close(fd);
  if (raw.empty()) return "no response";
  size_t hdrEnd = raw.find("\r\n\r\n");
  if (hdrEnd == std::string::npos) {
    *respBody = raw;
    return std::string();
  }
  std::string blob = raw.substr(0, hdrEnd + 2);
  *respHeaders = __nexa_http_split_headers(blob);
  size_t sp1 = blob.find(' ');
  if (blob.compare(0, 5, "HTTP/") == 0 && sp1 != std::string::npos) {
    *status = std::atoi(blob.c_str() + sp1 + 1);
  }
  std::string payload = raw.substr(hdrEnd + 4);
  std::string te = __nexa_http_header_value(*respHeaders, "Transfer-Encoding");
  if (__nexa_http_contains_ci(te, "chunked")) {
    payload = __nexa_http_dechunk(payload);
  } else {
    std::string cl = __nexa_http_header_value(*respHeaders, "Content-Length");
    if (!cl.empty()) {
      unsigned long want = std::strtoul(cl.c_str(), nullptr, 10);
      if (want < payload.size()) payload.resize((size_t)want);
    }
  }
  *respBody = payload;
  return std::string();
}
static std::string __nexa_http_perform(const std::string& method, const std::string& url,
                                       const std::string& body, const std::vector<std::string>& headers,
                                       int* status, std::string* respBody,
                                       std::vector<std::string>* respHeaders) {
  // WinHTTP and CFNetwork follow redirects for us; do the same here, bounded,
  // and demote to GET the way both of those stacks do on 301/302/303.
  std::string target = url;
  std::string verb = method;
  std::string payload = body;
  for (int hop = 0; hop < 5; hop++) {
    *status = 0;
    respBody->clear();
    respHeaders->clear();
    std::string err = __nexa_http_once(verb, target, payload, headers, status, respBody, respHeaders);
    if (!err.empty()) return err;
    if (*status != 301 && *status != 302 && *status != 303 && *status != 307 && *status != 308) {
      return std::string();
    }
    std::string loc = __nexa_http_resolve_url(target, __nexa_http_header_value(*respHeaders, "Location"));
    if (loc.empty()) return std::string();
    if (*status != 307 && *status != 308 && verb != "GET" && verb != "HEAD") {
      verb = "GET";
      payload.clear();
    }
    target = loc;
  }
  return "too many redirects";
}
#endif
)NEXA_HTTP";
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
    return __nexa_result<std::string>::make_err("HTTP " + std::to_string(status));
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
    return out;
}

}  // namespace nexa
