#pragma once

#include <string>

namespace nexa {

// OS-API HTTP client (no third-party libs bundled).
// Windows: WinHTTP (HTTP + HTTPS via Schannel)
// macOS:   CFNetwork (HTTP + HTTPS)
// Linux:   POSIX sockets; HTTPS via system libssl.so (dlopen, not linked)
inline std::string httpRuntimeCpp() {
    return R"NEXA_HTTP(
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
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
static __nexa_result<std::string> __nexa_http_request(const std::string& method, const std::string& url, const std::string& body) {
  std::wstring wurl = __nexa_http_widen(url);
  URL_COMPONENTS uc;
  memset(&uc, 0, sizeof(uc));
  uc.dwStructSize = sizeof(uc);
  wchar_t host[256]; wchar_t path[2048]; wchar_t extra[2048];
  uc.lpszHostName = host; uc.dwHostNameLength = 256;
  uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
  uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 2048;
  if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) {
    return __nexa_result<std::string>::make_err("invalid URL");
  }
  std::wstring wpath = std::wstring(path, uc.dwUrlPathLength) + std::wstring(extra, uc.dwExtraInfoLength);
  if (wpath.empty()) wpath = L"/";
  HINTERNET hSession = WinHttpOpen(L"NexaHTTP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) return __nexa_result<std::string>::make_err("could not open HTTP session");
  INTERNET_PORT port = uc.nPort ? uc.nPort : (uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);
  HINTERNET hConnect = WinHttpConnect(hSession, std::wstring(host, uc.dwHostNameLength).c_str(), port, 0);
  if (!hConnect) { WinHttpCloseHandle(hSession); return __nexa_result<std::string>::make_err("could not connect"); }
  DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
  std::wstring wmethod = __nexa_http_widen(method);
  HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), wpath.c_str(), nullptr,
    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return __nexa_result<std::string>::make_err("could not open request"); }
  BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
    body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
    (DWORD)body.size(), (DWORD)body.size(), 0);
  if (!ok || !WinHttpReceiveResponse(hRequest, nullptr)) {
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return __nexa_result<std::string>::make_err("request failed");
  }
  std::string out;
  for (;;) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(hRequest, &avail)) break;
    if (avail == 0) break;
    std::string chunk(avail, '\0');
    DWORD read = 0;
    if (!WinHttpReadData(hRequest, &chunk[0], avail, &read)) break;
    chunk.resize(read);
    out += chunk;
  }
  WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
  return __nexa_result<std::string>::make_ok(out);
}
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CFNetwork/CFNetwork.h>
static __nexa_result<std::string> __nexa_http_request(const std::string& method, const std::string& url, const std::string& body) {
  CFStringRef cfUrl = CFStringCreateWithCString(kCFAllocatorDefault, url.c_str(), kCFStringEncodingUTF8);
  CFStringRef cfMethod = CFStringCreateWithCString(kCFAllocatorDefault, method.c_str(), kCFStringEncodingUTF8);
  CFURLRef urlRef = CFURLCreateWithString(kCFAllocatorDefault, cfUrl, nullptr);
  if (!urlRef) {
    CFRelease(cfMethod); CFRelease(cfUrl);
    return __nexa_result<std::string>::make_err("invalid URL");
  }
  CFHTTPMessageRef req = CFHTTPMessageCreateRequest(kCFAllocatorDefault, cfMethod, urlRef, kCFHTTPVersion1_1);
  if (!body.empty()) {
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8*)body.data(), (CFIndex)body.size());
    CFHTTPMessageSetBody(req, data);
    CFRelease(data);
  }
  CFReadStreamRef stream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, req);
  CFReadStreamSetProperty(stream, kCFStreamPropertyHTTPShouldAutoredirect, kCFBooleanTrue);
  if (!CFReadStreamOpen(stream)) {
    CFRelease(stream); CFRelease(req); CFRelease(urlRef); CFRelease(cfMethod); CFRelease(cfUrl);
    return __nexa_result<std::string>::make_err("could not open request");
  }
  std::string out;
  UInt8 buf[4096];
  for (;;) {
    CFIndex n = CFReadStreamRead(stream, buf, sizeof(buf));
    if (n <= 0) break;
    out.append((const char*)buf, (size_t)n);
  }
  CFReadStreamClose(stream);
  CFRelease(stream); CFRelease(req); CFRelease(urlRef); CFRelease(cfMethod); CFRelease(cfUrl);
  return __nexa_result<std::string>::make_ok(out);
}
#elif defined(__EMSCRIPTEN__)
#include <emscripten/fetch.h>
#include <cstring>
static __nexa_result<std::string> __nexa_http_request(const std::string& method, const std::string& url, const std::string& body) {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  std::memset(attr.requestMethod, 0, sizeof(attr.requestMethod));
  std::strncpy(attr.requestMethod, method.c_str(), sizeof(attr.requestMethod) - 1);
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
  if (!body.empty()) {
    attr.requestData = body.data();
    attr.requestDataSize = body.size();
  }
  emscripten_fetch_t* fetch = emscripten_fetch(&attr, url.c_str());
  if (!fetch) return __nexa_result<std::string>::make_err("request failed");
  if (fetch->status < 200 || fetch->status >= 300) {
    std::string err = "HTTP " + std::to_string((int)fetch->status);
    emscripten_fetch_close(fetch);
    return __nexa_result<std::string>::make_err(err);
  }
  std::string out;
  if (fetch->data && fetch->numBytes) out.assign(fetch->data, fetch->numBytes);
  emscripten_fetch_close(fetch);
  return __nexa_result<std::string>::make_ok(out);
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
static __nexa_result<std::string> __nexa_http_request(const std::string& method, const std::string& url, const std::string& body) {
  std::string scheme, host, path; int port = 80;
  if (!__nexa_http_parse_url(url, scheme, host, port, path)) {
    return __nexa_result<std::string>::make_err("invalid URL");
  }
  int tls = 0;
  if (scheme == "https") tls = 1;
  else if (scheme != "http") return __nexa_result<std::string>::make_err("unsupported URL scheme");
  const __nexa_SslApi* api = nullptr;
  if (tls) {
    api = __nexa_ssl_api();
    if (!api) return __nexa_result<std::string>::make_err("HTTPS requires libssl");
  }
  addrinfo hints; memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  std::string portStr = std::to_string(port);
  if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
    return __nexa_result<std::string>::make_err("could not resolve host");
  }
  int fd = -1;
  for (addrinfo* p = res; p; p = p->ai_next) {
    fd = (int)socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0) continue;
    if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
    close(fd); fd = -1;
  }
  freeaddrinfo(res);
  if (fd < 0) return __nexa_result<std::string>::make_err("could not connect");
  void* ctx = nullptr;
  void* ssl = nullptr;
  if (tls) {
    const void* meth = api->TLS_client_method();
    ctx = meth ? api->SSL_CTX_new(meth) : nullptr;
    if (!ctx) { close(fd); return __nexa_result<std::string>::make_err("TLS setup failed"); }
    if (api->SSL_CTX_set_default_verify_paths) api->SSL_CTX_set_default_verify_paths(ctx);
    ssl = api->SSL_new(ctx);
    if (!ssl) { api->SSL_CTX_free(ctx); close(fd); return __nexa_result<std::string>::make_err("TLS setup failed"); }
    if (api->SSL_set_verify) api->SSL_set_verify(ssl, 1, nullptr);
    if (api->SSL_set1_host) api->SSL_set1_host(ssl, host.c_str());
    if (api->SSL_ctrl) api->SSL_ctrl(ssl, 55, 0, (void*)host.c_str());
    if (!api->SSL_set_fd(ssl, fd) || api->SSL_connect(ssl) != 1) {
      api->SSL_free(ssl);
      api->SSL_CTX_free(ctx);
      close(fd);
      return __nexa_result<std::string>::make_err("TLS handshake failed");
    }
    if (api->SSL_get_verify_result && api->SSL_get_verify_result(ssl) != 0) {
      if (api->SSL_shutdown) api->SSL_shutdown(ssl);
      api->SSL_free(ssl);
      api->SSL_CTX_free(ctx);
      close(fd);
      return __nexa_result<std::string>::make_err("TLS certificate verify failed");
    }
  }
  std::string hostHdr = host;
  if (!((scheme == "http" && port == 80) || (scheme == "https" && port == 443))) {
    hostHdr += ":";
    hostHdr += std::to_string(port);
  }
  std::string req = method + " " + path + " HTTP/1.1\r\nHost: " + hostHdr + "\r\nConnection: close\r\n";
  if (!body.empty()) {
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    req += "Content-Type: application/octet-stream\r\n";
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
    return __nexa_result<std::string>::make_err("request failed");
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
  size_t hdr = raw.find("\r\n\r\n");
  if (hdr == std::string::npos) return __nexa_result<std::string>::make_ok(raw);
  return __nexa_result<std::string>::make_ok(raw.substr(hdr + 4));
}
#endif
static __nexa_result<std::string> __nexa_http_get(const std::string& url) {
  return __nexa_http_request("GET", url, "");
}
static __nexa_result<std::string> __nexa_http_post(const std::string& url, const std::string& body) {
  return __nexa_http_request("POST", url, body);
}
)NEXA_HTTP";
}

}  // namespace nexa
