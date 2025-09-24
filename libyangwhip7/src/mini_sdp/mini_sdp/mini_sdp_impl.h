/**
 * @file mini_sdp/mini_sdp_impl.h
 * @brief 
 * @version 0.1
 * @date 2021-01-11
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#ifndef MINI_SDP_MINI_SDP_IMPL_H_
#define MINI_SDP_MINI_SDP_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "mini_sdp/sdp_parser.h"
#include "mini_sdp/mini_sdp.h"

namespace mini_sdp {

constexpr uint8_t kMiniSdpPacketType = 0xFF;
constexpr uint8_t kMiniSdpAuthLength = 16;

constexpr char kMiniSdpMagic[] = "SDP";
constexpr char kMiniSdpEncryptKey[] = "zDAtJsmOyhljoSu4";
constexpr char kMiniSdpUrlPrefix[] = "webrtc://";
constexpr size_t kMiniSdpUrlMaxLen = 1200;
constexpr size_t kMiniSdpMaxLen = 1400;
constexpr int kMaxExtCandidatePortCount = 4;
constexpr uint16_t kMiniSdpOverStunAttributeType = 0x8F03;
constexpr uint16_t kStopStreamOverStunAttributeType = 0x8F04;
constexpr uint16_t kSigOverStunAttributeType = 0x8F05;
constexpr uint16_t kStunXorCodeAttributeType = 0x8F07;
constexpr uint16_t kStunXorTypeKey = 0x0001;
constexpr char kMiniSdpStunTransactionID[] = "MINISDPOSTUN";
constexpr uint32_t kStunMagicCookie = 0x2112A442;
constexpr size_t kStunTransactionIdLength = 12;
constexpr size_t kStunAttributeHeaderLen = 4;
constexpr size_t kStunHeaderLen = 20;
constexpr size_t kStunPrefixLen = kStunHeaderLen + kStunAttributeHeaderLen;
constexpr size_t kStunAttrFingerPrintLen = kStunAttributeHeaderLen + 4;
constexpr size_t kMiniSdpHeaderLen = 29;
constexpr size_t kMiniSdpCodecDescLen = 4;
constexpr size_t kMiniFooterLen = 16;

constexpr size_t kCrcOffsetInAuth = 12;

PACK(struct MiniSdpHdr {
  uint8_t   packet_type;
  char      magic_word[3];  // "SDP"

  uint8_t   version;

  uint16_t  status_code;

  uint8_t   not_seq_align           : 1;
  uint8_t   not_support_aac_fmtp    : 1;
  uint8_t   is_string_bundle        : 1;
  uint8_t   role                    : 2;
  uint8_t   has_candidate           : 1;  // sdp_type为offer时，可无candidate，此时candidate相关置0
  uint8_t   encrypt_switch          : 1;  // encrypt_switch为0表示不加密，1表示通过encrypt_key加密
  uint8_t   ip_type                 : 1;  // 0-ipv4，1-ipv6

  uint16_t  candidate_port;

  uint32_t  canditate_ip[4];

  uint16_t  seq;  // 用于服务端去重

  uint8_t   not_imm_send            : 1;
  uint8_t   video_audio_data_flag   : 3;  // 对应3位表示是否有video，audio，datachannel描述
  uint8_t   direction               : 2;
  uint8_t   sdp_type                : 2;
});

PACK(struct MiniMediaHdr {
  uint32_t  ssrc1       : 32;
  uint32_t  ssrc2       : 32;
  uint8_t   media_type  :  2;
  uint8_t   codec_num   :  6;  // codec_num表示以下有多少个uint32_t的codec描述
});

PACK(struct MiniAacConfig {
  uint8_t     object;
  uint8_t     config_len;
  uint16_t    flag;
  char        config_data[];
});

constexpr uint16_t kMiniAacFlagPs       = 0x1;
constexpr uint16_t kMiniAacFlagSbr      = 0x2;
constexpr uint16_t kMiniAacFlagStereo   = 0x4;
constexpr uint16_t kMiniAacFlagCPresent = 0x8;

PACK(struct MiniCodecDesc {
  uint32_t frequency    :  4;
  uint32_t codec        :  4;

  uint32_t mark_a       :  1;
  uint32_t payload_type :  7;

  uint32_t mark_b       :  3;
  uint32_t bfame_enable :  1;
  uint32_t goog_remb    :  1;
  uint32_t transport_cc :  1;
  uint32_t flex_fec     :  1;
  uint32_t nack         :  1;

  uint32_t reversed     :  5;
  uint32_t rrtr         :  1;
  uint32_t channels     :  2;
});

PACK(struct MiniExtDesc {
  uint16_t id   : 8;
  uint16_t uri  : 8;
});


PACK(struct MiniFooter {
  uint16_t    ports[4];   // 8-Bytes
  uint8_t     netset;     // 1-Bytes
  uint8_t     ext_flag = 0;   // 1-Bytes
  uint16_t    agentid;    // 2-Bytes
  uint32_t    crc;        // 4-Bytes
});

PACK(struct StopStreamSignalHeader {
  uint8_t     pack_type;
  char        magic_word[3];
  uint8_t     version;
  uint16_t    status;
  uint16_t    seq;
  uint16_t    svrsig_len;
});

class MiniSdp {
 public:
  MiniSdp();

  void HdrNtoh();

  void HdrHton();

  bool containVideo() { return ((mini_sdp_hdr.video_audio_data_flag & 4) >> 2 == 1); }
  bool containAudio() { return ((mini_sdp_hdr.video_audio_data_flag & 2) >> 1 == 1);  }
  bool containData()  { return ((mini_sdp_hdr.video_audio_data_flag & 1) >> 0 == 1); }

 public:
  MiniSdpHdr mini_sdp_hdr;

  uint16_t ufrag_len;
  const char* ufrag;

  uint16_t pwd_len;
  const char* pwd;

  uint32_t stream_url_len;
  const char* stream_url;

  uint16_t key_len;
  const char* encrypt_key;

  // 前 8 Bytes 用于扩展多端口
  // 中 4 Bytes 预留
  // 后 4 Bytes 用于 CRC 校验
  char auth[16];
};  // class MiniSdp

struct KLVData {
  uint8_t ext_id = 0;
  std::vector<uint8_t>  ext_value;

  size_t GetLength() const {
    return 2 + ext_value.size();
  }

  std::vector<uint8_t> Bytes() {
    std::vector<uint8_t> buffer;
    buffer.push_back(GetLength());
    buffer.push_back(ext_id);
    buffer.insert(buffer.end(), std::make_move_iterator(ext_value.begin()), std::make_move_iterator(ext_value.end()));
    return buffer;
  }
};

enum VideoConfigType : uint8_t {
  kVideoConfigTypeH264 = 0,
  kVideoConfigTypeH265 = 1
};

enum ExtId : uint8_t {
  kExtIdRtx                     = 0,
  kExtTcpTransportType          = 1,
  kExtVideoConfig               = 4,
  kExtVideoResolution           = 5,
  kExtUdpTransportType          = 6,
  kExtClientInfo                = 7,
  kExtIdLens
};

struct Ext {
  std::vector<uint8_t> bit_maps;
  std::vector<KLVData> KVLS;

  size_t GetLength() {
    size_t lengths = 2 + bit_maps.size();
    for (const auto &ext : KVLS) {
      lengths += ext.GetLength();
    }
    return lengths;
  }

  std::vector<uint8_t> Bytes() {
    std::vector<uint8_t> ext_bytes;
    ext_bytes.push_back(KVLS.size());
    ext_bytes.push_back(bit_maps.size());
    if (bit_maps.size()) {
      ext_bytes.insert(ext_bytes.end(),
                       std::make_move_iterator(bit_maps.begin()),
                       std::make_move_iterator(bit_maps.end()));
    }
    for (auto& klv : KVLS) {
      auto klv_bytes = klv.Bytes();
      ext_bytes.insert(ext_bytes.end(),
                       std::make_move_iterator(klv_bytes.begin()),
                       std::make_move_iterator(klv_bytes.end()));
    }
    return ext_bytes;
  }
};

struct TransportInfo {
  std::string ip_addr;
  std::vector<uint16_t> prots;
  TransportLayerProtocolType transport_type;
};

class MiniSdpPacker {
 public:
  MiniSdpPacker(const OriginSdpAttr& a) : attr(a) {}

  /**
   * @brief packer PullReq and original sdp to dst buffer
   * 
   * @param data buffer to store dst data
   * @param sdp need to be packed
   * @param sdp_type offer/answer/none
   * @param stream_url pull stream url
   * @param status_code only sdp_type=none need
   * 
   * @return >0 buffer size 
   * @return =0 pack error
   */
  int PackToDstMem(char *data, size_t len);

 private:
  int copyStr16(uint16_t len, char* str, char* data, uint32_t& offset, size_t buffer_total_len);

  int copyStr32(uint32_t len, char* str, char* data, uint32_t& offset, size_t buffer_total_len);

  std::string encrypt_key;

  std::string ip_addr;

  static uint16_t sdp_seq;

  const OriginSdpAttr& attr;

 private:
  void setRtxInfoToKlvData(const std::map<uint8_t, std::pair<uint32_t, std::map<uint8_t, uint8_t>>> &rtx_info,
                           KLVData &klv_data);

  void setTransportTypeToKlvData(KLVData &klv_data, const SdpAddrType addr_type,
                                 const std::pair<std::string, std::set<uint16_t>> &transport_attr);

  void setVideoConfigInfoToKlvData(const std::pair<std::string, std::vector<std::string>> &video_config_entry,
                                   KLVData &klv_data);

  void setVideoResolutionToKlvData(KLVData &klv_data);

  void setClientInfoToKlvData(const mini_sdp::OriginSdpAttr &attr, KLVData &klv_data);

  // function for write custom ext
  void writeBigEndian32(std::vector<uint8_t> &dst_vector, const uint32_t value);

  void writeLittleEndian32(std::vector<uint8_t> &dst_vector, const uint32_t value);


  std::map<std::string, std::set<uint16_t>> transport_tcp_attrs;

  std::map<std::string, std::set<uint16_t>> transport_udp_attrs;

  std::map<std::string, std::vector<std::string>> video_config_entries;

  std::pair<std::pair<uint32_t, uint32_t>, bool> video_resolution;
};  // class MiniSdpPacker

class MiniSdpLoader {
 public:
  MiniSdpLoader(OriginSdpAttr& a) : attr(a) {}

  /**
   * @brief Parse raw data to origin sdp
   * 
   * @param data recv packet buffer
   * @param data_len recv packet buffer len
   * 
   * @param seq return seq
   * @param dst_sdp return origin sdp
   * @param dst_stream_url return stream_url
   * 
   * @return >0 buffer size 
   * @return =0 parse error
   */
  int ParseToString(char *data, uint32_t len);

 private:
  MediaDescriptionPtr parseMedia(char* data, uint32_t& offset, MiniSdpHdr* mini_sdp_hdr, size_t len);

  int readStr16(std::string& dst, char* data, uint32_t& offset, size_t total_len);

  int readStr32(std::string& dst, char* data, uint32_t& offset, size_t total_len);

  std::string encrypt_key;

  SdpAddrType addr_type = SdpAddrType::kIPv4;
  std::string ip_addr;
  std::vector<uint16_t> ports;
  std::vector<TransportInfo> transport_infos;
  std::map<std::string, std::vector<uint16_t>> transport_tcp_attrs;
  std::map<std::string, std::vector<uint16_t>> transport_udp_attrs;
  std::map<std::string, std::string> video_config_entries;
  std::pair<std::pair<uint32_t, uint32_t>, bool> video_resolution;

  OriginSdpAttr& attr;

public:
  // for custom ext

  struct MiniSdpLoaderContext {
    explicit MiniSdpLoaderContext(MiniSdpLoader *loader) : mini_sdp_loader_ptr(loader) {}
    std::map<uint8_t, std::pair<uint32_t, std::map<uint8_t, uint8_t>>> rtx_info;
    SessionDescriptionPtr sdp_info;
    MiniSdpLoader* mini_sdp_loader_ptr;
  };

  using MiniSdpLoaderContextPtr = std::shared_ptr<MiniSdpLoaderContext>;

  MiniSdpLoaderContextPtr MakeMiniSdpLoaderCtx(MiniSdpLoader *loader) {
    return std::make_shared<MiniSdpLoaderContext>(loader);
  }
  MiniSdpLoaderContextPtr MiniSdpLoaderCtx;
  using CustomExtParseHandle =
    std::function<bool(MiniSdpLoaderContextPtr, char*, size_t)>;

public:
  // aux function
  static bool parseCandidateTransportType(MiniSdpLoaderContextPtr mini_sdp_loader_ctx,
                                          char* data, size_t lens,
                                          TransportLayerProtocolType transport_type);

  // function to parse custom ext
  static bool parseRtxExt(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char* data, size_t lens);

  static bool parseVideoConfig(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char* data, size_t lens);

  static bool parseVideoResolution(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char* data, size_t lens);

  static bool parseTcpTransportType(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char* data, size_t lens);

  static bool parseUdpTransportType(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char* data, size_t lens);

  static bool parseClientInfo(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char* data, size_t lens);
};  // class MiniSdpLoader


PACK(struct StunHeader {
  uint16_t type;
  uint16_t length;
  uint32_t cookie;
  char     transaction_id[kStunTransactionIdLength];
});

PACK(struct StunAttribute {
  uint16_t type;
  uint16_t length;
  char     value[];
});

PACK(struct StunAttrDecodeKeyData {
  uint16_t target_attr_id;
  uint16_t decode_type;
  uint32_t xkey;
});

StunType CheckStunPrefix(const char* buff, size_t len, uint16_t* attr_type);

ssize_t EncodeStunWithFixedPrefix(uint16_t stun_type, uint16_t attr_type, uint16_t attr_len, char* buff, size_t len);

struct MsdpOverStunOption {
  uint16_t stun_type = StunType::kStunNone;
  uint16_t attr_type = 0;
  uint32_t xor_code_key = XorCodeType::kXorCodeNone;
};

int32_t EncodeMsdpInStun(char* msdp_data, uint16_t msdp_len,
                         char* buff, uint32_t len,
                         const MsdpOverStunOption& option);

int32_t DecodeMsdpInStun(char* msdp_data, uint16_t msdp_len,
                         const char* buff, uint32_t len,
                         MsdpOverStunOption* option);

}  // namespace mini_sdp

#endif  // MINI_SDP_MINI_SDP_IMPL_H_
