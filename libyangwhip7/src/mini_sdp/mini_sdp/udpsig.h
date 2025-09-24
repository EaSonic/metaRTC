/**
 * @file mini_sdp/udpsig.h
 * @brief UDP Signal
 * @version 0.1
 * @date 2021-05-25
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#ifndef MINI_SDP_UDPSIG_H_
#define MINI_SDP_UDPSIG_H_

#include <cstdint>

namespace mini_sdp {

constexpr uint8_t kUdpSigPacketType = 0xFF;

struct usig_header_t {
  uint8_t   packet_type = kUdpSigPacketType;
  uint8_t   command     = 0;
  uint16_t  length      = 0;
} __attribute__((packed));

enum CmdTypes : uint8_t {
  kCmdUdpDetect = 0x01
};

}  // namespace mini_sdp

#endif  // MINI_SDP_UDPSIG_H_
