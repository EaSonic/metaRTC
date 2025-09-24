/**
 * @file mini_sdp/mini_sdp.h
 * @brief 
 * @version 0.1
 * @date 2021-01-14
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#ifndef MINI_SDP_MINI_SDP_H_
#define MINI_SDP_MINI_SDP_H_

#include <string>
#include <vector>
#include "mini_sdp/compat.h"
#include "mini_sdp/sdp.h"

namespace mini_sdp {

constexpr uint16_t kAgentId = 0x2104;   // 2021/11/22 - 0

/**
 * @brief Return Code
 *  统一返回码
 */
enum SdpRetcode {
  kSdpRetWrongFormat    = -1,     // 格式错误
  kSdpRetSizeExceeded   = -2,     // 打包结果超过所提供的 buffer 大小
  kSdpRetUrlExceeded    = -3,     // 流 URL 过长，使得无法存入所有提供的 buffer
  kSdpRetVerifyFailed   = -4,
};

/**
 * @brief Stream Direction
 *  流类型（传输方向）
 *  - 可为拉流、推流或者默认
 */
enum StreamDirection {
  kStreamDefault = -1,
  kStreamPull = 0,    // 拉流
  kStreamPush = 1,    // 推流
};

/**
 * @brief Network Type
 *  网络类型
 *  - 可多选，按位组合
 */
enum NetworkType {
  kNetUnknown   = 0,
  kNetEthernet  = 1 << 0,   // 以太网（有线）
  kNetWifi      = 1 << 1,
  kNetCellular  = 1 << 2,   // 移动网络（4G）
  kNetVpn       = 1 << 3,
  kNetLoopback  = 1 << 4,
};

/**
 * @brief StunType
 *  通过 STUN 承载 UDP 信令，STUN 的类型 
 */
enum StunType : uint16_t {
  kStunNone = 0,
  kStunPing = 0x0001,
  kStunResp = 0x0101,
};

/**
 * @brief XorDecodeType
 *  在 STUN 中为 MiniSDP 做 XOR 编码
 */
enum XorCodeType : uint32_t {
  kXorCodeNone    = 0x0,
  kXorCodeRandom  = 0xFFFFFFFF
};

/**
 * @brief Origin SDP Attribute
 *  原始 SDP 属性，原始 SDP 与 mini sdp 互转的参数结构
 */
struct OriginSdpAttr {
  // SDP Type
  // - SDP 类型，offer 或者 answer
  // - SdpType的定义见 mini_sdp/sdp.h
  SdpType             sdp_type;

  // Origin SDP
  // - 原始SDP
  std::string         origin_sdp;

  // Stream Url
  // - 流URL，格式如：webrtc://<domain>/[<path>/]<stream id>
  std::string         stream_url;

  // Server Signature
  // - 服务端标识，唯一标识 Session
  // - 停流请求需要设置该值
  // - offer 不需要该值
  // - 格式: <ip>:<ice-ufrag in answer>:<ice-ufrag in offer>
  // * 服务端需要保存 answer （响应UDP）中的 svrsig
  std::string         svrsig;

  // Status Code
  // - 响应状态码，仅在 answer 中为有效值
  int                 status_code = 0;

  // Sequence
  // - 请求序号
  // - UDP 可能会有丢包的情况，为保证到达服务端，需要有一定的重试策略，
  //   相同请求的 seq 需要保持一致，并且需要确保同一客户端不同请求的 seq 是不一致，
  //   建议客户端本地对 seq 递增处理。
  // - 服务端可以根据 seq 来过滤重复 UDP 请求
  // * 客户端需要保存当前请求的 seq
  uint16_t            seq = 0;

  // Flag: Immediately Sending
  // - 立即发送标志位，即 0-RTT
  // - 若开启，表示客户端有能力直接立即接收媒体数据,
  //   服务端可以向该 UDP 请求的源地址发送媒体数据
  bool                is_imm_send = false;

  // Flag: Aac Fmtp Supported
  // - Aac 参数标志位
  // - 表示是否保留 ADTS 和 LATM 音频的 fmtp属性，默认开启
  // - 这是用于 v0 版本兼容不同子版本的标志位，v1 版本将不再需要
  bool                is_support_aac_fmtp = true;

  // Flag: Stream Direction
  // - 流类型标志位，指示拉流或者推流
  // - 默认表示依据原始 SDP 的描述
  StreamDirection     is_push = kStreamDefault;   // -1 not have field , 0 false, 1 true

  // Flag: MiniSdp CRC verify
  // - 编码 MiniSdp 时，生成校验码
  // - 解码 MiniSdp 时，校验内容，失败返回 kSdpRetVerifyFailed
  bool                is_crc = false;

  // over STUN Type
  // - 编码 MiniSdp 时， 配置按 STUN PING 或 STUN RES 生成信令
  // - 需要通过 STUN 发 UDP 信令时，应该选择 StunType::kStunPing
  // - 默认值 StunType::kStunNone 表示不需要用 STUN 承载信令
  StunType            stun_type = StunType::kStunNone;

  // Network Set
  // - 类型参考 enum NetworkType
  // - 可以设置多种类型，例如：netset = kNetWifi | kNetCellular;
  // - 判断类型是否存在：netset & kNetWifi
  uint8_t             netset = kNetUnknown;

  // Agent ID
  // - 标识端的版本 ID
  // - 建议值：
  //     0      3|       7|    10|        15|
  //    +--------+--------+----------+------+
  //    |  year  |  month | seq  |   day    |
  uint16_t            agentid = kAgentId;

  // Client Type
  // - 标识客户端（SDK）的名称
  // - 限制最大为 8 Char
  // - 超过最大长度会截断到最大长度
  std::string         client_type;

  // Client Version
  // - 标识客户端（SDK）版本号
  uint32_t            client_version = 0;

  // XOR Code Key
  // - 对 MiniSDP 做 XOR 编码
  // - kXorCodeNone 表示关闭功能，kXorCodeRandom 表示随机生成 key，其他值则为指定 key
  // - 只在 MiniSDP over STUN 有效
  uint32_t            xor_code_key = XorCodeType::kXorCodeNone;
};  // struct OriginSdpAttr

/**
 * @brief Status Code
 *  响应码，与 http 接口保持一致
 */
enum StatusCode {
  kStatCodeSuccess      = 0,
  kStatCodeFormatError  = 100,  // sdp format error
  kStatCodeParamError   = 101,  // parameters of request error
  kStatCodeInfoError    = 102,  // stream info error
  kStatCodeAuthError    = 103,  // auth error
  kStatCodeNotFound     = 104   // stream not existed
};

/**
 * @brief Check Request Packet
 *  检查 UDP 包是否为 mini sdp 请求
 * @param data 
 * @param len 
 * @return true 
 * @return false 
 */
bool IsMiniSdpReqPack(const char* data, size_t len);

/**
 * @brief Check Request Packet
 *  检查 UDP 包是否为 mini sdp 请求
 * @param data 
 * @param len 
 * @return true 
 * @return false 
 */
bool IsMiniSdpOverStun(const char* data, size_t len);

/**
 * @brief Load mini_sdp to origin_sdp
 *  将 mini sdp 转换成原始 SDP
 * @param buff mini_sdp
 * @param len mini_sdp
 * @param attr result
 * @return int SdpRetCode or size of mini_sdp
 */
ssize_t LoadMiniSdpToOriginSdp(const char* buff, size_t len, OriginSdpAttr& attr);

/**
 * @brief Parse origin_sdp to mini_sdp
 *  将原始 SDP 转换成 mini sdp
 * @param attr origin sdp
 * @param buff mini_sdp
 * @param len mini_sdp
 * @return int SdpRetCode or size of mini_sdp
 */
ssize_t ParseOriginSdpToMiniSdp(const OriginSdpAttr& attr, char* buff, size_t len);

/**
 * @brief Stop Stream Attribute
 *  停流参数
 */
struct StopStreamAttr {
  // Server Signature
  // - 服务端标识
  // * 与 answer （响应 UDP）的 svrsig 保持一致
  std::string svrsig;

  // Status Code
  // - 响应状态码，仅在响应中为有效值
  uint16_t    status = 0;

  // Sequence
  // - 请求序号
  // * 与请求的 seq 保持一致
  uint16_t    seq = 0;

  // over STUN Type
  // - 编码 MiniSdp 时， 配置按 STUN PING 或 STUN RES 生成信令
  // - 需要通过 STUN 发 UDP 信令时，应该选择 StunType::kStunPing
  // - 默认值 StunType::kStunNone 表示不需要用 STUN 承载信令
  StunType    stun_type = StunType::kStunNone;
};  // struct StopStreamAttr

/**
 * @brief Check Stop Packet
 *  检查 UDP 包是否为 mini sdp 停流包
 * @param data 
 * @param len 
 * @return true 
 * @return false 
 */
bool IsMiniSdpStopPack(const char* data, size_t len);

/**
 * @brief Build packet for stop stream
 *  构建 mini sdp 停流 UDP 包
 * @param buff packet
 * @param len packet
 * @param attr
 * @return ssize_t 
 */
ssize_t BuildStopStreamPacket(char* buff, size_t len, const StopStreamAttr& attr);

/**
 * @brief Load Response of Stop Stream Packet
 *  解析 mini sdp 停流 UDP 包
 * @param buff 
 * @param len 
 * @param attr
 * @return ssize_t 
 */
ssize_t LoadStopStreamPacket(const char* buff, size_t len, StopStreamAttr& attr);

/**
 * @brief UDP Detect
 *  UDP 探测，服务端响应与请求内容一致
 */
struct UdpSigAttrDetect {
  uint32_t    stime;      // send time, 发送时的时间戳
  std::string token;      // 业务特有 token，需要与服务侧协商
  StunType    stun_type = StunType::kStunNone;
};

ssize_t UdpSigEncode(char* buff, size_t len, const UdpSigAttrDetect& attr);

ssize_t UdpSigDecode(const char* buff, size_t len, UdpSigAttrDetect& attr);

}  // namespace mini_sdp

#endif  // MINI_SDP_MINI_SDP_H_
