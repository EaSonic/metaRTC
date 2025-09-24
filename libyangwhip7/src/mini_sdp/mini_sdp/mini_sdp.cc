/**
 * @file mini_sdp/mini_sdp.cc
 * @brief 
 * @version 0.1
 * @date 2021-01-14
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#include "mini_sdp/mini_sdp.h"
#include <cstring>
#include <limits>
#include "mini_sdp/mini_sdp_impl.h"
#include "mini_sdp/util.h"

namespace mini_sdp {

bool IsMiniSdpOverStun(const char* data, size_t len) {
  return StunType::kStunNone != CheckStunPrefix(data, len, nullptr);
}

static inline bool IsOriginMiniSdpReqPack(const char* data, size_t len) {
  return len >= 4 && (uint8_t)data[0] == kMiniSdpPacketType &&
      data[1] == 'S' && data[2] == 'D' && data[3] == 'P';
}

bool IsMiniSdpReqPack(const char* data, size_t len) {
  uint16_t attr_type = 0;
  return (StunType::kStunNone != CheckStunPrefix(data, len, &attr_type)
            && attr_type == kMiniSdpOverStunAttributeType)
            || IsOriginMiniSdpReqPack(data, len);
}

static ssize_t DoParseOriginSdpToMiniSdp(const OriginSdpAttr& attr, char* buff, size_t len) {
  if (attr.stream_url.size() > kMiniSdpUrlMaxLen) {
    return kSdpRetUrlExceeded;
  }
  MiniSdpPacker packer(attr);
  int pack_size = packer.PackToDstMem(buff, len);
  if (pack_size == 0) {
    return kSdpRetWrongFormat;
  }
  if (static_cast<size_t>(pack_size) > kMiniSdpMaxLen ||
      static_cast<size_t>(pack_size) > len) {
    return kSdpRetSizeExceeded;
  }
  return pack_size;
}

ssize_t ParseOriginSdpToMiniSdp(const OriginSdpAttr& attr, char* buff, size_t len) {
  if (attr.stun_type == StunType::kStunNone) {
    return DoParseOriginSdpToMiniSdp(attr, buff, len);
  }

  if (len < kStunPrefixLen) {
    return kSdpRetSizeExceeded;
  }

  char msdp_buff[kMiniSdpMaxLen];
  auto r = DoParseOriginSdpToMiniSdp(attr, msdp_buff, kMiniSdpMaxLen);
  if (r < 0) {
    return r;
  }

  MsdpOverStunOption option;
  option.stun_type = attr.stun_type;
  option.attr_type = kMiniSdpOverStunAttributeType;
  option.xor_code_key = attr.xor_code_key;

  return EncodeMsdpInStun(msdp_buff, r, buff, len, option);
}

static ssize_t DoLoadMiniSdpToOriginSdp(const char* buff, size_t len, OriginSdpAttr& attr) {
  if (len < kMiniSdpHeaderLen) return kSdpRetWrongFormat;
  MiniSdpLoader loader(attr);
  int parse_size = loader.ParseToString(const_cast<char *>(buff), len);
  return parse_size;
}

ssize_t LoadMiniSdpToOriginSdp(const char* buff, size_t len, OriginSdpAttr& attr) {
  MsdpOverStunOption option;
  char msdp_data[kMiniSdpMaxLen];
  int32_t r = DecodeMsdpInStun(msdp_data, kMiniSdpMaxLen, buff, len, &option);
  if (r > 0 && option.attr_type == kMiniSdpOverStunAttributeType) {
    attr.xor_code_key = option.xor_code_key;
    return DoLoadMiniSdpToOriginSdp(msdp_data, r, attr);
  }

  return DoLoadMiniSdpToOriginSdp(buff, len, attr);
}

static inline bool IsOriginMiniSdpStopPack(const char* data, size_t len) {
  return len >= 4 && (uint8_t)data[0] == kMiniSdpPacketType && data[1] == 'S' && data[2] == 'T' && data[3] == 'P';
}

bool IsMiniSdpStopPack(const char* data, size_t len) {
  uint16_t attr_type = 0;
  return (StunType::kStunNone != CheckStunPrefix(data, len, &attr_type)
            && attr_type == kStopStreamOverStunAttributeType)
            || IsOriginMiniSdpStopPack(data, len);
}

static ssize_t DoBuildStopStreamPacket(char* buff, size_t len, const StopStreamAttr& attr) {
  size_t total_bytes = sizeof(StopStreamSignalHeader) + attr.svrsig.size() + kMiniSdpAuthLength;
  if (total_bytes > len || attr.svrsig.size() > std::numeric_limits<uint16_t>::max()) {
    return kSdpRetSizeExceeded;
  }

  StopStreamSignalHeader* hdr = reinterpret_cast<StopStreamSignalHeader*>(buff);
  hdr->pack_type = kMiniSdpPacketType;
  memcpy(hdr->magic_word, "STP", 3);
  hdr->version = 0;
  hdr->status = htons(attr.status);
  hdr->seq = htons(attr.seq);
  hdr->svrsig_len = htons((uint16_t)attr.svrsig.size());

  buff += sizeof(StopStreamSignalHeader);
  memcpy(buff, attr.svrsig.c_str(), attr.svrsig.size());

  buff += attr.svrsig.size();
  memset(buff, 0, kMiniSdpAuthLength);

  return total_bytes;
}

ssize_t BuildStopStreamPacket(char* buff, size_t len, const StopStreamAttr& attr) {
  if (attr.stun_type == StunType::kStunNone) {
    return DoBuildStopStreamPacket(buff, len, attr);
  }

  if (len < kStunPrefixLen) {
    return kSdpRetSizeExceeded;
  }

  auto r = DoBuildStopStreamPacket(buff + kStunPrefixLen, len - kStunPrefixLen, attr);
  if (r < 0) {
    return r;
  }

  return EncodeStunWithFixedPrefix(attr.stun_type, kStopStreamOverStunAttributeType, r, buff, len);
}

static ssize_t DoLoadStopStreamPacket(const char* buff, size_t len, StopStreamAttr& attr) {
  if (len < sizeof(StopStreamSignalHeader) + kMiniSdpAuthLength) return kSdpRetSizeExceeded;
  if ((uint8_t)*buff != kMiniSdpPacketType || buff[1] != 'S' || buff[2] != 'T' || buff[3] != 'P') {
    return kSdpRetWrongFormat;
  }

  const StopStreamSignalHeader* hdr = (const StopStreamSignalHeader*)buff;
  if (hdr->version != 0) return kSdpRetWrongFormat;

  attr.status = ntohs(hdr->status);
  attr.seq = ntohs(hdr->seq);
  uint16_t length = ntohs(hdr->svrsig_len);

  if (sizeof(StopStreamSignalHeader) + length + kMiniSdpAuthLength > len) {
    return kSdpRetSizeExceeded;
  }

  attr.svrsig.assign(buff + sizeof(StopStreamSignalHeader), length);
  return sizeof(StopStreamSignalHeader) + kMiniSdpAuthLength + length;
}

ssize_t LoadStopStreamPacket(const char* buff, size_t len, StopStreamAttr& attr) {
  uint16_t attr_type;
  if (CheckStunPrefix(buff, len, &attr_type) != StunType::kStunNone
      && attr_type == kStopStreamOverStunAttributeType) {
    return DoLoadStopStreamPacket(buff + kStunPrefixLen, len - kStunPrefixLen, attr);
  }

  return DoLoadStopStreamPacket(buff, len, attr);
}

}  // namespace mini_sdp
