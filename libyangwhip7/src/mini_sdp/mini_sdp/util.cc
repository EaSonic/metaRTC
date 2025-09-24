/**
 * @file mini_sdp/util.cc
 * @brief 
 * @version 0.1
 * @date 2021-01-07
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#include "mini_sdp/util.h"
#include <cstring>

namespace mini_sdp {

std::vector<StrSlice> StrSplit(const char* data, size_t len, char chr, bool is_remove_space) {
  std::vector<StrSlice> slices;
  const char* ppos = nullptr;

  StrSlice slice;
  while (len > 0) {
    ppos = (const char*)memchr(data, chr, len);
    slice.ptr = data;
    if (ppos) {
      slice.len = ppos - data;
      len -= slice.len + 1;
      data = ppos + 1;
      if (is_remove_space) {
        while (len > 0 && *data == chr) {
          data++;
          len--;
        }
      }
    } else {
      slice.len = len;
      len = 0;
    }
    slices.push_back(slice);
  }
  return slices;
}

std::pair<std::string, const char*> StrGetFirstSplit(const char* data, size_t len, char chr) {
  const char* pos = (const char*)memchr(data, chr, len);
  if (pos == nullptr) {
    return std::make_pair(std::string(data, len), nullptr);
  }
  return std::make_pair(std::string(data, pos - data), pos + 1);
}

void Trim(std::string& str) {
  if (str.empty()) {
    return;
  }

  str.erase(0, str.find_first_not_of("\r\t"));
  str.erase(str.find_last_not_of("\r\t") + 1);
}

uint32_t GenerateCrc(const char* buff, size_t len) {
  uint32_t crc = 0;
  size_t u32len = len >> 2;   // 超出部分不会计算
  const uint32_t* u32arr = (const uint32_t*)buff;
  for (size_t i = 0; i < u32len; i++) {
    crc ^= u32arr[i];
  }
  return crc;
}

}  // namespace mini_sdp
