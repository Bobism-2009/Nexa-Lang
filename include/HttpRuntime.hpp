#pragma once

#include <string>

namespace nexa {

// OS-API HTTP client (no third-party libs).
// Windows: WinHTTP (HTTP + HTTPS via Schannel)
// macOS:   CFNetwork (HTTP + HTTPS)
// Linux:   POSIX sockets (HTTP only; HTTPS returns an error string)
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
static std::string __nexa_http_request(const std::string& method, const std::string& url, const std::string& body) {
  std::wstring wurl = __nexa_http_widen(url);
  URL_COMPONENTS uc;
  memset(&uc, 0, sizeof(uc));
  uc.dwStructSize = sizeof(uc);
  wchar_t host[256]; wchar_t path[2048]; wchar_t extra[2048];
  uc.lpszHostName = host; uc.dwHostNameLength = 256;
  uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
  uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 2048;
  if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) return std::string();
  std::wstring wpath = std::wstring(path, uc.dwUrlPathLength) + std::wstring(extra, uc.dwExtraInfoLength);
  if (wpath.empty()) wpath = L"/";
  HINTERNET hSession = WinHttpOpen(L"NexaHTTP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) return std::string();
  INTERNET_PORT port = uc.nPort ? uc.nPort : (uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT);
  HINTERNET hConnect = WinHttpConnect(hSession, std::wstring(host, uc.dwHostNameLength).c_str(), port, 0);
  if (!hConnect) { WinHttpCloseHandle(hSession); return std::string(); }
  DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
  std::wstring wmethod = __nexa_http_widen(method);
  HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), wpath.c_str(), nullptr,
    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return std::string(); }
  BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
    body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
    (DWORD)body.size(), (DWORD)body.size(), 0);
  if (!ok || !WinHttpReceiveResponse(hRequest, nullptr)) {
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return std::string();
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
  return out;
}
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CFNetwork/CFNetwork.h>
static std::string __nexa_http_request(const std::string& method, const std::string& url, const std::string& body) {
  CFStringRef cfUrl = CFStringCreateWithCString(kCFAllocatorDefault, url.c_str(), kCFStringEncodingUTF8);
  CFStringRef cfMethod = CFStringCreateWithCString(kCFAllocatorDefault, method.c_str(), kCFStringEncodingUTF8);
  CFURLRef urlRef = CFURLCreateWithString(kCFAllocatorDefault, cfUrl, nullptr);
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
    return std::string();
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
  return out;
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
static bool __nexa_http_parse_url(const std::string& url, std::string& scheme, std::string& host, int& port, std::string& path) {
  scheme.clear(); host.clear(); path = "/"; port = 80;
  size_t sp = url.find("://");
  if (sp == std::string::npos) return false;
  scheme = url.substr(0, sp);
  size_t start = sp + 3;
  size_t slash = url.find('/', start);
  std::string hostport = (slash == std::string::npos) ? url.substr(start) : url.substr(start, slash - start);
  if (slash != std::string::npos) path = url.substr(slash);
  size_t colon = hostport.find(':');
  if (colon == std::string::npos) {
    host = hostport;
    port = (scheme == "https") ? 443 : 80;
  } else {
    host = hostport.substr(0, colon);
    port = std::atoi(hostport.c_str() + colon + 1);
  }
  return !host.empty();
}
static std::string __nexa_http_request(const std::string& method, const std::string& url, const std::string& body) {
  std::string scheme, host, path; int port = 80;
  if (!__nexa_http_parse_url(url, scheme, host, port, path)) return std::string();
  if (scheme == "https") {
    return std::string("HTTPS requires OS TLS (available on Windows/macOS); use http:// on Linux or switch platforms");
  }
  if (scheme != "http") return std::string();
  addrinfo hints; memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  std::string portStr = std::to_string(port);
  if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) return std::string();
  int fd = -1;
  for (addrinfo* p = res; p; p = p->ai_next) {
    fd = (int)socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0) continue;
    if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
    close(fd); fd = -1;
  }
  freeaddrinfo(res);
  if (fd < 0) return std::string();
  std::string req = method + " " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n";
  if (!body.empty()) {
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    req += "Content-Type: application/octet-stream\r\n";
  }
  req += "\r\n";
  req += body;
  size_t sent = 0;
  while (sent < req.size()) {
    ssize_t n = send(fd, req.data() + sent, req.size() - sent, 0);
    if (n <= 0) { close(fd); return std::string(); }
    sent += (size_t)n;
  }
  std::string raw;
  char buf[4096];
  for (;;) {
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) break;
    raw.append(buf, (size_t)n);
  }
  close(fd);
  size_t hdr = raw.find("\r\n\r\n");
  if (hdr == std::string::npos) return raw;
  return raw.substr(hdr + 4);
}
#endif
static std::string __nexa_http_get(const std::string& url) {
  return __nexa_http_request("GET", url, "");
}
static std::string __nexa_http_post(const std::string& url, const std::string& body) {
  return __nexa_http_request("POST", url, body);
}
)NEXA_HTTP";
}

}  // namespace nexa
