/**
 * @file udpsig.cc
 * @brief 
 * @version 0.1
 * @date 2021-05-25
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#include "mini_sdp/udpsig.h"
#include <cstring>
#include "mini_sdp/mini_sdp.h"
#include "mini_sdp/mini_sdp_impl.h"
#include "mini_sdp/util.h"

namespace mini_sdp {

static ssize_t DoUdpSigEncode(char* buff, size_t len, const UdpSigAttrDetect& attr) {
  size_t total_size = sizeof(usig_header_t) + sizeof(uint32_t) + attr.token.size();
  if (len < total_size) return kSdpRetSizeExceeded;

  usig_header_t* phdr = reinterpret_cast<usig_header_t*>(buff);
  phdr->packet_type = kUdpSigPacketType;
  phdr->command = kCmdUdpDetect;
  phdr->length = htons(sizeof(uint32_t) + attr.token.size());

  uint32_t* pts = reinterpret_cast<uint32_t*>(buff + sizeof(usig_header_t));
  *pts = htonl(attr.stime);

  char* ptoken = buff + sizeof(usig_header_t) + sizeof(uint32_t);
  memcpy(ptoken, attr.token.c_str(), attr.token.size());

  return total_size;
}

ssize_t UdpSigEncode(char* buff, size_t len, const UdpSigAttrDetect& attr) {
  if (attr.stun_type == StunType::kStunNone) {
    return DoUdpSigEncode(buff, len, attr);
  }

  if (len < kStunPrefixLen) {
    return kSdpRetSizeExceeded;
  }

  auto r = DoUdpSigEncode(buff + kStunPrefixLen, len - kStunPrefixLen, attr);
  if (r < 0) {
    return r;
  }

  return EncodeStunWithFixedPrefix(attr.stun_type, kSigOverStunAttributeType, r, buff, len);
}

static ssize_t DoUdpSigDecode(const char* buff, size_t len, UdpSigAttrDetect& attr) {
  if (len < sizeof(usig_header_t)) return kSdpRetWrongFormat;

  const usig_header_t* phdr = reinterpret_cast<const usig_header_t*>(buff);
  uint16_t length = ntohs(phdr->length);
  if (phdr->packet_type != kUdpSigPacketType
      || phdr->command != kCmdUdpDetect) {
    return kSdpRetWrongFormat;
  }

  const uint32_t* pts = reinterpret_cast<const uint32_t*>(buff + sizeof(usig_header_t));
  attr.stime = ntohl(*pts);

  const char* ptoken = buff + sizeof(usig_header_t) + sizeof(uint32_t);
  attr.token.assign(ptoken, length - sizeof(uint32_t));

  return len;
}

ssize_t UdpSigDecode(const char* buff, size_t len, UdpSigAttrDetect& attr) {
  uint16_t attr_type;
  if (CheckStunPrefix(buff, len, &attr_type) != StunType::kStunNone
      && attr_type == kSigOverStunAttributeType) {
    return DoUdpSigDecode(buff + kStunPrefixLen, len - kStunPrefixLen, attr);
  }

  return DoUdpSigDecode(buff, len, attr);
}

}  // namespace mini_sdp
