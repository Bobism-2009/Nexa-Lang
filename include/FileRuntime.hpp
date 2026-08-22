#pragma once

#include <string>

namespace nexa {

// Extra std/file helpers emitted into generated C++.
inline std::string fileRuntimeCpp() {
    return R"NEXA_FILE(
#include <vector>
static std::vector<std::string> __nexa_file_list(const std::string& __path) {
  std::vector<std::string> __out;
  std::error_code __ec;
  if (!std::filesystem::exists(__path, __ec)) return __out;
  for (const auto& __entry : std::filesystem::directory_iterator(__path, __ec)) {
    if (__ec) break;
    __out.push_back(__entry.path().filename().string());
  }
  return __out;
}
static int __nexa_file_remove(const std::string& __path) {
  std::error_code __ec;
  return std::filesystem::remove(__path, __ec) ? 1 : 0;
}
static int __nexa_file_remove_all(const std::string& __path) {
  std::error_code __ec;
  auto __n = std::filesystem::remove_all(__path, __ec);
  return (__ec || __n == static_cast<uintmax_t>(-1)) ? 0 : 1;
}
static int __nexa_file_rename(const std::string& __from, const std::string& __to) {
  std::error_code __ec;
  std::filesystem::rename(__from, __to, __ec);
  return __ec ? 0 : 1;
}
static int __nexa_file_copy(const std::string& __from, const std::string& __to) {
  std::error_code __ec;
  std::filesystem::copy(__from, __to, std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive, __ec);
  return __ec ? 0 : 1;
}
static int __nexa_file_isdir(const std::string& __path) {
  std::error_code __ec;
  return std::filesystem::is_directory(__path, __ec) ? 1 : 0;
}
static int __nexa_file_isfile(const std::string& __path) {
  std::error_code __ec;
  return std::filesystem::is_regular_file(__path, __ec) ? 1 : 0;
}
static int __nexa_file_size(const std::string& __path) {
  std::error_code __ec;
  auto __n = std::filesystem::file_size(__path, __ec);
  if (__ec) return -1;
  if (__n > static_cast<uintmax_t>(0x7FFFFFFF)) return 0x7FFFFFFF;
  return static_cast<int>(__n);
}
static std::string __nexa_file_cwd() {
  std::error_code __ec;
  return std::filesystem::current_path(__ec).string();
}
static int __nexa_file_chdir(const std::string& __path) {
  std::error_code __ec;
  std::filesystem::current_path(__path, __ec);
  return __ec ? 0 : 1;
}
static std::string __nexa_file_abspath(const std::string& __path) {
  std::error_code __ec;
  return std::filesystem::absolute(__path, __ec).string();
}
static std::string __nexa_file_join(const std::string& __a, const std::string& __b) {
  return (std::filesystem::path(__a) / std::filesystem::path(__b)).string();
}
static std::string __nexa_file_dirname(const std::string& __path) {
  return std::filesystem::path(__path).parent_path().string();
}
static std::string __nexa_file_basename(const std::string& __path) {
  return std::filesystem::path(__path).filename().string();
}
static std::string __nexa_file_extension(const std::string& __path) {
  return std::filesystem::path(__path).extension().string();
}
)NEXA_FILE";
}

}  // namespace nexa
