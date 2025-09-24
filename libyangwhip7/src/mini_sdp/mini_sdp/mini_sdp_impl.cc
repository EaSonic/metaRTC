/**
 * @file mini_sdp/mini_sdp_impl.cc
 * @brief 
 * @version 0.1
 * @date 2021-01-11
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#include "mini_sdp/mini_sdp_impl.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <sstream>
#include "mini_sdp/util.h"

namespace mini_sdp {

uint16_t MiniSdpPacker::sdp_seq = 0;

static std::unordered_map<std::string, uint8_t> mini_sdp_codec_name_map = {
  {kSdpCodecOpus, 0},
  {kSdpCodecLatm, 1},
  {kSdpCodecAdts, 2},
  {kSdpCodecH264, 3},
  {kSdpCodecH265, 4},
  {kSdpCodecFlexFec, 5},
  {kSdpCodecByteVC1, 6},
  {kSdpCodecRtx, 7},
  {kSdpCodecAV1, 8}
};

static std::vector<std::string> mini_sdp_codec_name_vec = {
  kSdpCodecOpus, kSdpCodecLatm, kSdpCodecAdts,
  kSdpCodecH264, kSdpCodecH265, kSdpCodecFlexFec,
  kSdpCodecByteVC1, kSdpCodecRtx, kSdpCodecAV1
};

static std::unordered_map<uint32_t, uint8_t> mini_sdp_frequency_map = {
  {96000, 0},  {88200, 1}, {64000, 2}, {48000, 3}, {44100, 4},
  {32000, 5},  {24000, 6}, {22050, 7}, {16000, 8}, {12000, 9},
  {11025, 10}, {8000, 11}, {7350, 12}, {0, 13},    {90000, 15},
};

static std::vector<uint32_t> mini_sdp_frequency_vec = {
  96000, 88200, 64000, 48000,
  44100, 32000, 24000, 22050,
  16000, 12000, 11025,  8000,
  7350,     0,     0,  90000,
};

static std::unordered_map<uint8_t, uint8_t>  mini_sdp_media_type_map = {
  {uint8_t(SdpMediaType::kVideo), 0x1 << 2},
  {uint8_t(SdpMediaType::kAudio), 0x1 << 1},
  {uint8_t(SdpMediaType::kData),  0x1     },
};

static std::unordered_map<uint8_t, uint8_t>  mini_sdp_trans_type_map = {
  {uint8_t(SdpTransType::kSendOnly), 0},
  {uint8_t(SdpTransType::kRecvOnly), 1},
  {uint8_t(SdpTransType::kSendRecv), 2},
};

static std::vector<uint8_t> mini_sdp_trans_type_vec = {
  uint8_t(SdpTransType::kSendOnly),
  uint8_t(SdpTransType::kRecvOnly),
  uint8_t(SdpTransType::kSendRecv),
};

static std::unordered_map<uint8_t, uint8_t>  mini_sdp_role_type_map = {
  {uint8_t(SdpRoleType::kActpass), 0},
  {uint8_t(SdpRoleType::kActive), 1},
  {uint8_t(SdpRoleType::kPassive), 2},
};

static std::vector<uint8_t> mini_sdp_role_type_vec = {
  uint8_t(SdpRoleType::kActpass),
  uint8_t(SdpRoleType::kActive),
  uint8_t(SdpRoleType::kPassive),
};

static std::unordered_map<std::string, uint8_t> mini_sdp_ext_map = {
  {kSdpExtAbsSendTime, 0},
  {kSdpExtPayloutDelay, 1},
  {kSdpExtTransportCc, 2},
  {kSdpExtMetaData01, 3},
  {kSdpExtMetaData02, 4},
  {kSdpExtMetaData03, 5},
  {kSdpExtDts, 6},
  {kSdpExtCts, 7},
  {kSdpExtVideoFrameType, 8},
  {kSdpExtCts2, 9},
  {kSdpExtOrderInStream, 10},
  {kSdpExtRefFrameTimestamp, 11},
  {kSdpExtSeqRange, 12},
  {kSdpExtVideoFrameType2, 13},
  {kSdpExtAacConfig, 14}
};

static std::unordered_map<std::string, uint8_t> mini_sdp_video_config_map = {
  {kSdpCodecH264, kVideoConfigTypeH264},
  {kSdpCodecH265, kVideoConfigTypeH265},
};

static std::vector<std::string> mini_sdp_ext_vec = {
  kSdpExtAbsSendTime, kSdpExtPayloutDelay, kSdpExtTransportCc,
  kSdpExtMetaData01,  kSdpExtMetaData02,   kSdpExtMetaData03,
  kSdpExtDts,         kSdpExtCts,          kSdpExtVideoFrameType,
  kSdpExtCts2,        kSdpExtOrderInStream, kSdpExtRefFrameTimestamp,
  kSdpExtSeqRange,    kSdpExtVideoFrameType2, kSdpExtAacConfig
};

static const std::vector<std::string> SdpMediaTypeString = {
  "audio",
  "video",
  "data"
};

static const std::unordered_map<uint8_t, MiniSdpLoader::CustomExtParseHandle>& GetExtParseHandleMap() {
  static NoDestructor<std::unordered_map<uint8_t, MiniSdpLoader::CustomExtParseHandle>> instance({
    {ExtId::kExtIdRtx,                  MiniSdpLoader::parseRtxExt},
    {ExtId::kExtTcpTransportType,       MiniSdpLoader::parseTcpTransportType},
    {ExtId::kExtVideoConfig,            MiniSdpLoader::parseVideoConfig},
    {ExtId::kExtVideoResolution,        MiniSdpLoader::parseVideoResolution},
    {ExtId::kExtUdpTransportType,       MiniSdpLoader::parseUdpTransportType},
    {ExtId::kExtClientInfo,             MiniSdpLoader::parseClientInfo},
  });
  return *instance;
}

MiniSdp::MiniSdp() {
  mini_sdp_hdr.packet_type = kMiniSdpPacketType;
  memcpy(mini_sdp_hdr.magic_word, kMiniSdpMagic, 3 * sizeof(char));
  mini_sdp_hdr.version = 0;
  mini_sdp_hdr.status_code = 0;
  mini_sdp_hdr.ip_type = 0;
  mini_sdp_hdr.encrypt_switch = 0;
  mini_sdp_hdr.has_candidate = 0;
  mini_sdp_hdr.role = 0;
  mini_sdp_hdr.is_string_bundle = 0;
  mini_sdp_hdr.not_support_aac_fmtp = 1;
  mini_sdp_hdr.not_seq_align = 1;
  mini_sdp_hdr.candidate_port = 0;
  memset(mini_sdp_hdr.canditate_ip, 0, 4 * sizeof(uint32_t));
  mini_sdp_hdr.seq = 0;
  mini_sdp_hdr.sdp_type = 0;
  mini_sdp_hdr.direction = 0;
  mini_sdp_hdr.video_audio_data_flag = 0;
  mini_sdp_hdr.not_imm_send = 1u;
  ufrag_len = 0;
  ufrag = nullptr;
  pwd_len = 0;
  pwd = nullptr;
  stream_url_len = 0;
  stream_url = nullptr;
  key_len = 0;
  encrypt_key = nullptr;
  memset(auth, 0, 16 * sizeof(char));
}

void MiniSdp::HdrHton() {
  mini_sdp_hdr.candidate_port = htons(mini_sdp_hdr.candidate_port);
  for (int i = 0; i < 4; i++) {
    mini_sdp_hdr.canditate_ip[0] = htonl(mini_sdp_hdr.canditate_ip[0]);
  }
  mini_sdp_hdr.seq = htons(mini_sdp_hdr.seq);
  mini_sdp_hdr.status_code = htons(mini_sdp_hdr.status_code);
}

void MiniSdp::HdrNtoh() {
  mini_sdp_hdr.candidate_port = ntohs(mini_sdp_hdr.candidate_port);
  for (int i = 0; i < 4; i++) {
    mini_sdp_hdr.canditate_ip[0] = ntohs(mini_sdp_hdr.canditate_ip[0]);
  }
  mini_sdp_hdr.seq = ntohs(mini_sdp_hdr.seq);
  mini_sdp_hdr.status_code = ntohs(mini_sdp_hdr.status_code);
}

// pack to mini sdp
int MiniSdpPacker::PackToDstMem(char* data, size_t len) {
  MiniSdp mini_sdp;
  uint32_t offset = 0;

  std::string mini_stream_url = (attr.stream_url.size() >= 9) ?
                                std::string(attr.stream_url.begin() + 9, attr.stream_url.end()) : "";

  mini_sdp.stream_url_len = mini_stream_url.size();
  mini_sdp.stream_url = mini_stream_url.c_str();

  // for error code
  if (attr.sdp_type == SdpType::kSdpNone) {
    mini_sdp.mini_sdp_hdr.status_code = attr.status_code;
    mini_sdp.mini_sdp_hdr.seq = attr.seq;
    mini_sdp.HdrHton();
    if (offset + sizeof(MiniSdpHdr) > len) return offset + sizeof(MiniSdpHdr);
    memcpy(data + offset, &(mini_sdp.mini_sdp_hdr), sizeof(MiniSdpHdr));
    offset += sizeof(MiniSdpHdr);
    if (offset + 16 > len) return offset + 16;
    std::string empty_str;
    copyStr16(empty_str.size(), const_cast<char *>(empty_str.c_str()), data, offset, len);
    copyStr16(empty_str.size(), const_cast<char *>(empty_str.c_str()), data, offset, len);
    copyStr32(mini_sdp.stream_url_len, const_cast<char *>(mini_sdp.stream_url), data, offset, len);
    copyStr16(empty_str.size(), const_cast<char *>(empty_str.c_str()), data, offset, len);
    copyStr16(empty_str.size(), const_cast<char *>(empty_str.c_str()), data, offset, len);
    memcpy(data+offset, mini_sdp.auth, 16);


    MiniFooter* footer = reinterpret_cast<MiniFooter*>(data + offset);
    footer->netset = attr.netset;
    footer->agentid = attr.agentid;
    if (attr.is_crc) {
      size_t crc_len = offset + kCrcOffsetInAuth;
      footer->crc = GenerateCrc(data, crc_len);
    }
    offset += 16;

    return offset;
  }

  SdpParser sdp_parser(attr.origin_sdp.c_str(), attr.origin_sdp.size());
  if (!sdp_parser.Parse()) {
    return 0;
  }

  SessionDescriptionPtr sdp_info = sdp_parser.GetSessionDescription();
  Ext custom_ext;
  mini_sdp.mini_sdp_hdr.version = (sdp_info->Version <= 0) ? 0 : sdp_info->Version;
  mini_sdp.mini_sdp_hdr.ip_type = uint8_t(sdp_info->AddrType);
  mini_sdp.mini_sdp_hdr.status_code = attr.status_code;
  mini_sdp.mini_sdp_hdr.seq = attr.seq;
  mini_sdp.mini_sdp_hdr.sdp_type = uint8_t(attr.sdp_type);
  mini_sdp.mini_sdp_hdr.not_imm_send = !attr.is_imm_send;
  mini_sdp.mini_sdp_hdr.not_support_aac_fmtp = !attr.is_support_aac_fmtp;
  mini_sdp.mini_sdp_hdr.not_seq_align = !(sdp_info->SessionId == "1");

  for (auto media_info_pair : sdp_info->Medias) {
    auto media_info = media_info_pair.second;
    if (media_info->Protos == kSdpMediaProtoEncryptDefault) {
      mini_sdp.mini_sdp_hdr.encrypt_switch = 0;
    }
    if (media_info_pair.first == "video" || media_info_pair.first == "audio") {
      mini_sdp.mini_sdp_hdr.is_string_bundle = 1;
    }

    if (!media_info->Candidates.empty()) {
      std::string udp_candidate_ip;
      uint32_t footer_udp_port_idx = 0;
      MiniFooter* footer = reinterpret_cast<MiniFooter*>(mini_sdp.auth);
      for (auto & candidate : media_info->Candidates) {
        if (candidate.TransportType == TransportLayerProtocolType::kOverUdp) {
          if (udp_candidate_ip.empty()) {
            udp_candidate_ip = candidate.Ip;
            mini_sdp.mini_sdp_hdr.candidate_port = candidate.Port;
            if (sdp_info->AddrType == SdpAddrType::kIPv4) {
              str2ipv4(candidate.Ip.c_str(),
                      static_cast<void *>(mini_sdp.mini_sdp_hdr.canditate_ip));
            } else {
              str2ipv6(candidate.Ip.c_str(),
                      static_cast<void *>(mini_sdp.mini_sdp_hdr.canditate_ip));
            }
          } else if (udp_candidate_ip == candidate.Ip && footer_udp_port_idx < kMaxExtCandidatePortCount) {
            footer->ports[footer_udp_port_idx] = htons(candidate.Port);
            footer_udp_port_idx++;
          } else {
            transport_udp_attrs[candidate.Ip].insert(htons(candidate.Port));
          }
          mini_sdp.mini_sdp_hdr.has_candidate = 1;
        } else if (candidate.TransportType == TransportLayerProtocolType::kOverTcp) {
          transport_tcp_attrs[candidate.Ip].insert(htons(candidate.Port));
        }
      }
    }

    mini_sdp.mini_sdp_hdr.video_audio_data_flag |=
        mini_sdp_media_type_map[uint8_t(media_info->MediaType)];

    mini_sdp.mini_sdp_hdr.direction = mini_sdp_trans_type_map[uint8_t(media_info->TransType)];
    mini_sdp.mini_sdp_hdr.role =  mini_sdp_role_type_map[uint8_t(media_info->RoleType)];

    mini_sdp.ufrag_len = media_info->IceUfrag.size();
    mini_sdp.ufrag = media_info->IceUfrag.c_str();
    mini_sdp.pwd_len = media_info->IcePwd.size();
    mini_sdp.pwd = media_info->IcePwd.c_str();

    std::string fingerprint = media_info->Fingerprint.first + " " + media_info->Fingerprint.second;
    if (fingerprint.size() > 1) encrypt_key = fingerprint;
  }  // sdp_hdr
  mini_sdp.key_len = encrypt_key.size();
  mini_sdp.encrypt_key = encrypt_key.c_str();

  mini_sdp.HdrHton();
  if (offset + sizeof(MiniSdpHdr) > len) return offset + sizeof(MiniSdpHdr);
  memcpy(data + offset, &(mini_sdp.mini_sdp_hdr), sizeof(MiniSdpHdr));
  offset += sizeof(MiniSdpHdr);

  KLVData rtx_ext;
  bool rtx_using = false;
  rtx_ext.ext_id = static_cast<uint8_t>(ExtId::kExtIdRtx);
  std::map<uint8_t, std::pair<uint32_t, std::map<uint8_t, uint8_t>>> rtx_info;
  for (auto media_info_pair : sdp_info->Medias) {
    auto media_info = media_info_pair.second;
    uint8_t mid = 0;
    if (media_info->MediaType == SdpMediaType::kAudio) {
      rtx_info[0] = std::make_pair(0, std::map<uint8_t, uint8_t>());
      mid = 0;
    } else if (media_info->MediaType == SdpMediaType::kVideo) {
      rtx_info[1] = std::make_pair(0, std::map<uint8_t, uint8_t>());
      mid = 1;
    } else if (media_info->MediaType == SdpMediaType::kData) {
      rtx_info[2] = std::make_pair(0, std::map<uint8_t, uint8_t>());
      mid = 2;
    }
    MiniMediaHdr mini_media_hdr;
    mini_media_hdr.media_type = uint8_t(media_info->MediaType);
    mini_media_hdr.ssrc1 = 0;
    mini_media_hdr.ssrc2 = 0;
    if (media_info->Tracks.size()) {
      rtx_info[mid].first = media_info->Tracks.front()->RtxSsrc;
      mini_media_hdr.ssrc1 = htonl(media_info->Tracks.front()->Ssrc);
      mini_media_hdr.ssrc2 = htonl(media_info->Tracks.front()->FecSsrc);
    }

    char *media_hdr_pos = data + offset;
    mini_media_hdr.codec_num = uint8_t(media_info->Codecs.size());
    offset += sizeof(MiniMediaHdr);

    for (auto it = media_info->Codecs.begin(); it != media_info->Codecs.end(); it++) {
      if (!mini_sdp_codec_name_map.count(it->second->Name) || !mini_sdp_frequency_map.count(it->second->SampleRate)) {
        mini_media_hdr.codec_num--;
        continue;
      }

      MiniCodecDesc mini_codec_desc;
      mini_codec_desc.mark_a = 0;
      mini_codec_desc.mark_b = 0;
      mini_codec_desc.reversed = 0;
      mini_codec_desc.codec = mini_sdp_codec_name_map[it->second->Name];
      mini_codec_desc.payload_type = it->second->Format;
      mini_codec_desc.channels = it->second->Channels;
      mini_codec_desc.frequency = mini_sdp_frequency_map[it->second->SampleRate];
      mini_codec_desc.nack = (it->second->Feedbacks.count(kSdpCodecNack)) ? 1u : 0u;
      mini_codec_desc.flex_fec = it->second->Name.compare(kSdpCodecFlexFec) == 0 ? 1u: 0u;
      mini_codec_desc.transport_cc = (it->second->Feedbacks.count(kSdpCodecTransportCc)) ? 1u : 0u;
      mini_codec_desc.goog_remb = (it->second->Feedbacks.count(kSdpCodecGoogleRemb)) ? 1u : 0u;
      mini_codec_desc.rrtr = (it->second->Feedbacks.count(kSdpCodecRrtr)) ? 1u : 0;
      mini_codec_desc.bfame_enable =
          static_cast<bool>(std::stol(it->second->GetFormatParam(kSdpCodecBFrameEnabled, "0"))
          || std::stol(it->second->GetFormatParam(kSdpCodecBFrameEnabled2, "0")));
      if (it->second->Name.compare(kSdpCodecRtx) == 0 && it->second->FormatParams.count(kSdpApt)) {
        rtx_info[mid].second.emplace(it->second->Format,
                                     static_cast<uint8_t>(std::stoul(it->second->FormatParams[kSdpApt])));
        rtx_using = true;
      }
      if (offset + sizeof(MiniCodecDesc) > len) return offset + sizeof(MiniCodecDesc);
      memcpy(data + offset, &mini_codec_desc, sizeof(MiniCodecDesc));
      offset += sizeof(MiniCodecDesc);
      if (attr.is_support_aac_fmtp && (it->second->Name == kSdpCodecLatm || it->second->Name == kSdpCodecAdts)) {
        auto config = it->second->GetFormatParam("config", "");
        auto aac_config_mem = std::unique_ptr<char[]>(new char[sizeof(MiniAacConfig) + config.size()]());
        auto aac_config = reinterpret_cast<MiniAacConfig *>(aac_config_mem.get());
        aac_config->object = std::stoul(it->second->GetFormatParam("object", "0"));
        aac_config->flag |= std::stoul(it->second->GetFormatParam("PS-enabled", "0")) ? kMiniAacFlagPs : 0;
        aac_config->flag |= std::stoul(it->second->GetFormatParam("SBR-enabled", "0")) ? kMiniAacFlagSbr : 0;
        aac_config->flag |= std::stoul(it->second->GetFormatParam("stereo", "0")) ? kMiniAacFlagStereo : 0;
        aac_config->flag |= std::stoul(it->second->GetFormatParam("cpresent", "0")) ? kMiniAacFlagCPresent : 0;
        aac_config->config_len = config.size();

        if (offset + sizeof(MiniAacConfig) + config.size() > len) {
          return offset + sizeof(MiniAacConfig) + config.size();
        }

        memcpy(data + offset, aac_config, sizeof(MiniAacConfig));
        offset += sizeof(MiniAacConfig);
        if (!config.empty()) {
          memcpy(data + offset, config.c_str(), config.size());
          offset += config.size();
        }
      }
      if (media_info->MediaType == SdpMediaType::kVideo) {
        auto config_iter = it->second->FormatParams.find(kSdpSpropParameterSets);
        if (config_iter != it->second->FormatParams.end()) {
          auto pos = config_iter->second.find(',');
          video_config_entries[kSdpCodecH264].push_back(config_iter->second.substr(0, pos));  // sps
          video_config_entries[kSdpCodecH264].push_back(config_iter->second.substr(pos + 1));  // pps
        }
        config_iter = it->second->FormatParams.find(kSdpSpropSps);
        if (config_iter != it->second->FormatParams.end()) {
          video_config_entries[kSdpCodecH265].push_back(config_iter->second);
        }
        config_iter = it->second->FormatParams.find(kSdpSpropPps);
        if (config_iter != it->second->FormatParams.end()) {
          video_config_entries[kSdpCodecH265].push_back(config_iter->second);
        }
        config_iter = it->second->FormatParams.find(kSdpSpropVps);
        if (config_iter != it->second->FormatParams.end()) {
          video_config_entries[kSdpCodecH265].push_back(config_iter->second);
        }

        config_iter = it->second->FormatParams.find(kSdpVideoResolutionWidth);
        if (config_iter != it->second->FormatParams.end()) {
          video_resolution.second = true;
          std::stringstream ss(config_iter->second);
          ss >> video_resolution.first.first;
        }
        config_iter = it->second->FormatParams.find(kSdpVideoResolutionHeight);
        if (config_iter != it->second->FormatParams.end()) {
          std::stringstream ss(config_iter->second);
          ss >> video_resolution.first.second;
        }
      }
    }
    if (offset + sizeof(MiniMediaHdr) > len) return offset + sizeof(MiniMediaHdr);
    memcpy(media_hdr_pos, &mini_media_hdr, sizeof(MiniMediaHdr));

    uint8_t ext_num = media_info->ExtMap.size();
    char *ext_pos = data + offset;
    offset += sizeof(uint8_t);
    for (auto it = media_info->ExtMap.begin(); it != media_info->ExtMap.end(); it++) {
      MiniExtDesc mini_ext_desc;
      Trim(it->second);
      if (!mini_sdp_ext_map.count(it->second)) {
        ext_num--;
        continue;
      }
      mini_ext_desc.id = it->first;
      mini_ext_desc.uri = mini_sdp_ext_map[it->second];
      if (offset + sizeof(MiniExtDesc) > len) return offset + sizeof(MiniExtDesc);
      memcpy(data + offset, &mini_ext_desc, sizeof(MiniExtDesc));
      offset += sizeof(MiniExtDesc);
    }

    if (offset + sizeof(uint8_t) > len) return offset + sizeof(uint8_t);
    memcpy(ext_pos, &ext_num, sizeof(uint8_t));
  }  // media descs

  if (rtx_using) {
    setRtxInfoToKlvData(rtx_info, rtx_ext);
    custom_ext.KVLS.push_back(rtx_ext);
  }

  if (!transport_tcp_attrs.empty()) {
    for (const auto &tcp_attr : transport_tcp_attrs) {
      KLVData transport_type_ext;
      transport_type_ext.ext_id = static_cast<uint8_t>(ExtId::kExtTcpTransportType);
      setTransportTypeToKlvData(transport_type_ext, sdp_info->AddrType, tcp_attr);
      custom_ext.KVLS.push_back(transport_type_ext);
    }
  }

  if (!transport_udp_attrs.empty()) {
    for (const auto &udp_attr : transport_udp_attrs) {
      KLVData transport_type_ext;
      transport_type_ext.ext_id = static_cast<uint8_t>(kExtUdpTransportType);
      setTransportTypeToKlvData(transport_type_ext, sdp_info->AddrType, udp_attr);
      custom_ext.KVLS.push_back(transport_type_ext);
    }
  }

  if (video_config_entries.size()) {
    for (const auto & pair : video_config_entries) {
      KLVData video_config_ext;
      video_config_ext.ext_id = static_cast<uint8_t>(ExtId::kExtVideoConfig);
      setVideoConfigInfoToKlvData(pair, video_config_ext);
      custom_ext.KVLS.push_back(video_config_ext);
    }
  }

  if (video_resolution.second) {
    KLVData video_resolution;
    video_resolution.ext_id = static_cast<uint8_t>(ExtId::kExtVideoResolution);
    setVideoResolutionToKlvData(video_resolution);
    custom_ext.KVLS.push_back(video_resolution);
  }

  if (!attr.client_type.empty() || attr.client_version != 0) {
    KLVData client_info;
    client_info.ext_id = kExtClientInfo;
    setClientInfoToKlvData(attr, client_info);
    custom_ext.KVLS.push_back(client_info);
  }

  size_t mem_len = offset + mini_sdp.ufrag_len + mini_sdp.pwd_len
                    + mini_sdp.stream_url_len + mini_sdp.key_len
                    + 16 + 1 + custom_ext.GetLength();

  if (mem_len > len) return mem_len;
  copyStr16(mini_sdp.ufrag_len, const_cast<char *>(mini_sdp.ufrag), data, offset, len);
  copyStr16(mini_sdp.pwd_len, const_cast<char *>(mini_sdp.pwd), data, offset, len);
  copyStr32(mini_sdp.stream_url_len, const_cast<char *>(mini_sdp.stream_url), data, offset, len);
  copyStr16(mini_sdp.key_len, const_cast<char *>(mini_sdp.encrypt_key), data, offset, len);
  copyStr16(attr.svrsig.size(), const_cast<char *>(attr.svrsig.c_str()), data, offset, len);

  memcpy(data + offset, mini_sdp.auth, 16);
  MiniFooter* footer = reinterpret_cast<MiniFooter*>(data + offset);
  footer->netset = attr.netset;
  footer->agentid = attr.agentid;
  if (custom_ext.KVLS.size()) {
    footer->ext_flag |= 0x01;
  }
  if (attr.is_push == kStreamPull || attr.is_push == kStreamPush) {
    footer->ext_flag |= 0x02;
  }
  if (attr.is_crc) {
    size_t crc_len = offset + kCrcOffsetInAuth;
    footer->crc = GenerateCrc(data, crc_len);
  }
  offset += 16;
  if (attr.is_push == kStreamPull || attr.is_push == kStreamPush) {
    uint8_t extern_byte = 0;
    extern_byte |= (attr.is_push ? 1u : 0u) << 0u;
    memcpy(data+offset, &extern_byte, 1);
    offset += 4;
  }

  if (footer->ext_flag & 0xfd) {
    auto bytes_buffer = custom_ext.Bytes();
    memcpy(data + offset, bytes_buffer.data(), bytes_buffer.size());
    offset += bytes_buffer.size();
  }
  return offset;
}

int MiniSdpPacker::copyStr16(uint16_t len, char *str, char *data, uint32_t &offset, size_t buffer_total_len) {
  constexpr size_t kU16SIZE = sizeof(uint16_t);
  if (len + kU16SIZE + offset > buffer_total_len) return -1;
  uint16_t nlen = htons(len);
  memcpy(data + offset, &(nlen), kU16SIZE);
  offset += kU16SIZE;
  memcpy(data + offset, str, len);
  offset += len;
  return 0;
}

int MiniSdpPacker::copyStr32(uint32_t len, char *str, char *data, uint32_t &offset, size_t buffer_total_len) {
  constexpr size_t kU32SIZE = sizeof(uint32_t);
  if (len + kU32SIZE + offset > buffer_total_len) return -1;
  uint32_t nlen = htonl(len);
  memcpy(data + offset, &(nlen), kU32SIZE);
  offset += kU32SIZE;
  memcpy(data + offset, str, len);
  offset += len;
  return 0;
}

void MiniSdpPacker::setRtxInfoToKlvData(
    const std::map<uint8_t, std::pair<uint32_t, std::map<uint8_t, uint8_t>>> &rtx_info,
    KLVData &klv_data) {
  for (auto &rtx_info_iter : rtx_info) {
    uint8_t mid = rtx_info_iter.first;
    uint32_t rtx_ssrc = rtx_info_iter.second.first;
    uint32_t apt_pair_cnt = rtx_info_iter.second.second.size();
    klv_data.ext_value.push_back(mid);
    writeBigEndian32(klv_data.ext_value, rtx_ssrc);
    klv_data.ext_value.push_back(static_cast<uint8_t>(apt_pair_cnt));
    for (auto &rtx_apt_pair : rtx_info_iter.second.second) {
      klv_data.ext_value.push_back(static_cast<uint8_t>(rtx_apt_pair.first));
      klv_data.ext_value.push_back(static_cast<uint8_t>(rtx_apt_pair.second));
    }
  }
}

void MiniSdpPacker::setTransportTypeToKlvData(KLVData &klv_data, const SdpAddrType addr_type,
                                              const std::pair<std::string, std::set<uint16_t>> &transport_attr) {
  klv_data.ext_value.push_back(static_cast<uint8_t>(transport_attr.second.size()));
  if (addr_type == SdpAddrType::kIPv4) {
    klv_data.ext_value.push_back(static_cast<uint8_t>(SdpAddrType::kIPv4));
    uint32_t ip_addr_v4 = 0;
    str2ipv4(transport_attr.first.c_str(), &ip_addr_v4);
    writeLittleEndian32(klv_data.ext_value, ip_addr_v4);
  } else {
    klv_data.ext_value.push_back(static_cast<uint8_t>(SdpAddrType::kIPv6));
    uint32_t ip_addr_v6[4] = {0};
    str2ipv6(transport_attr.first.c_str(), &ip_addr_v6);
    for (int i = 0; i < 4; i++) {
      writeLittleEndian32(klv_data.ext_value, ip_addr_v6[i]);
    }
  }
  for (auto transport_port : transport_attr.second) {
    klv_data.ext_value.push_back(static_cast<uint8_t>((transport_port) & 0xff));
    klv_data.ext_value.push_back(static_cast<uint8_t>((transport_port >> 8) & 0xff));
  }
}

void MiniSdpPacker::setVideoConfigInfoToKlvData(
    const std::pair<std::string, std::vector<std::string>> &video_config_entry,
    KLVData &klv_data) {
  uint8_t video_config_type = 0xff;
  if (mini_sdp_video_config_map.find(video_config_entry.first) != mini_sdp_video_config_map.end()) {
    video_config_type = mini_sdp_video_config_map[video_config_entry.first];
  }
  klv_data.ext_value.push_back(static_cast<uint8_t>(video_config_type));
  for (const auto & config : video_config_entry.second) {
    klv_data.ext_value.push_back(config.size());
    klv_data.ext_value.insert(klv_data.ext_value.end(),
                               std::make_move_iterator(config.begin()), std::make_move_iterator(config.end()));
  }
}

void MiniSdpPacker::setVideoResolutionToKlvData(KLVData &klv_data) {
  writeBigEndian32(klv_data.ext_value, video_resolution.first.first);
  writeBigEndian32(klv_data.ext_value, video_resolution.first.second);
}

void MiniSdpPacker::setClientInfoToKlvData(const mini_sdp::OriginSdpAttr &attr, KLVData &klv_data) {
  uint8_t client_type_len = std::min<uint8_t>(8, strlen(attr.client_type.c_str()));
  klv_data.ext_value.push_back(client_type_len);
  klv_data.ext_value.insert(klv_data.ext_value.end(),
                            attr.client_type.begin(), attr.client_type.begin() + client_type_len);
  writeBigEndian32(klv_data.ext_value, attr.client_version);
}

void MiniSdpPacker::writeBigEndian32(std::vector<uint8_t> &dst_vector, const uint32_t value) {
  dst_vector.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
  dst_vector.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  dst_vector.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  dst_vector.push_back(static_cast<uint8_t>((value) & 0xff));
}

void MiniSdpPacker::writeLittleEndian32(std::vector<uint8_t> &dst_vector, const uint32_t value) {
  dst_vector.push_back(static_cast<uint8_t>((value) & 0xff));
  dst_vector.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
  dst_vector.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
  dst_vector.push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

int MiniSdpLoader::ParseToString(char* data, uint32_t len) {
  uint32_t offset = 0;
  SessionDescriptionPtr sdp_info = MakeSessionDescription();
  MiniSdpLoaderCtx = MakeMiniSdpLoaderCtx(this);
  MiniSdpLoaderCtx->sdp_info = sdp_info;
  MiniSdp mini_sdp;
  MiniSdpHdr *mini_sdp_hdr = reinterpret_cast<MiniSdpHdr*>(data + offset);
  offset += sizeof(MiniSdpHdr);

  mini_sdp.mini_sdp_hdr = *mini_sdp_hdr;
  attr.sdp_type = (SdpType)mini_sdp_hdr->sdp_type;

  sdp_info->Version = mini_sdp_hdr->version;
  addr_type = SdpAddrType(mini_sdp_hdr->ip_type);
  sdp_info->AddrType = addr_type;
  if (mini_sdp_hdr->direction >= mini_sdp_trans_type_vec.size()) {
    sdp_info->TransType = SdpTransType::kSendRecv;
  } else {
    sdp_info->TransType = SdpTransType(mini_sdp_trans_type_vec[mini_sdp_hdr->direction]);
  }
  if (mini_sdp_hdr->role >= mini_sdp_role_type_vec.size()) {
    sdp_info->RoleType = SdpRoleType::kActpass;
  } else {
    sdp_info->RoleType = SdpRoleType(mini_sdp_role_type_vec[mini_sdp_hdr->role]);
  }
  if (!mini_sdp_hdr->not_seq_align) {
    sdp_info->SessionId = "1";
  }
  uint32_t ipv6[4] = {0};
  for (int i = 0; i < 4; i++) {
    ipv6[i] = (mini_sdp_hdr->canditate_ip[i]);
  }
  ip_addr = (sdp_info->AddrType == SdpAddrType::kIPv4)
              ? ip2strv4((mini_sdp_hdr->canditate_ip[0]))
              : ip2strv6(reinterpret_cast<unsigned char *>(ipv6));
  std::vector<MediaDescriptionPtr> medias;
  if (mini_sdp.containVideo() && offset < len) {
    medias.push_back(parseMedia(data, offset, mini_sdp_hdr, len));
  }
  if (mini_sdp.containAudio() && offset < len) {
    medias.push_back(parseMedia(data, offset, mini_sdp_hdr, len));
  }
  if (mini_sdp.containData() && offset < len) {
    medias.push_back(parseMedia(data, offset, mini_sdp_hdr, len));
  }
  std::string ice_ufrag;
  std::string ice_pwd;
  std::string stream_url;
  std::string encrypt_key;
  if (readStr16(ice_ufrag, data, offset, len) < 0) return kSdpRetWrongFormat;
  if (readStr16(ice_pwd, data, offset, len) < 0) return kSdpRetWrongFormat;
  if (readStr32(stream_url, data, offset, len) < 0) return kSdpRetWrongFormat;
  if (readStr16(encrypt_key, data, offset, len) < 0) return kSdpRetWrongFormat;
  if (readStr16(attr.svrsig, data, offset, len) < 0) return kSdpRetWrongFormat;
  // auth/footer
  if (offset + kMiniFooterLen > len) return kSdpRetWrongFormat;
  MiniFooter* footer = reinterpret_cast<MiniFooter*>(data + offset);
  if (attr.is_crc && footer->crc != GenerateCrc(data, offset + kCrcOffsetInAuth)) {
    return kSdpRetVerifyFailed;
  }
  if (!ip_addr.empty() && ip_addr != "0.0.0.0") {
    CandidateInfo candidate;
    candidate.Ip = ip_addr;
    candidate.TransportType = TransportLayerProtocolType::kOverUdp;
    // uint16_t* pport = (uint16_t*)(data + offset);
    for (int i = 0; i < kMaxExtCandidatePortCount; i++) {
      if (footer->ports[i] == 0) break;
      candidate.Port = ntohs(footer->ports[i]);
      for (auto& media : medias) {
        if (media) media->Candidates.push_back(candidate);
      }
    }
  }
  attr.netset = footer->netset;
  attr.agentid = footer->agentid;
  bool using_ext = footer->ext_flag & 0xfd;
  bool using_push_flag = footer->ext_flag & 0x02;
  offset += 16;

  attr.is_push = kStreamDefault;
  if (using_push_flag && offset < len) {
    uint8_t extern_byte = 0;
    extern_byte = *reinterpret_cast<uint8_t*>(data + offset);
    offset += 4;
    if (extern_byte & 1u) {
      attr.is_push = kStreamPush;
    } else {
      attr.is_push = kStreamPull;
    }
  }
  auto &rtx_info = MiniSdpLoaderCtx->rtx_info;
  if (offset < len && using_ext) {
    uint8_t ext_total_count = *reinterpret_cast<uint8_t*>(data + offset);
    offset++;
    uint8_t bit_map_lens = *reinterpret_cast<uint8_t*>(data + offset);
    offset++;
    std::vector<uint8_t> bit_maps;
    for (int32_t idx = 0; idx < static_cast<int32_t>(bit_map_lens); idx++) {
      bit_maps.push_back(*reinterpret_cast<uint8_t*>(data + offset));
      offset++;
    }
    while (ext_total_count-- && offset < len) {
      uint8_t ext_iter_lens = *reinterpret_cast<uint8_t*>(data + offset);
      offset++;
      uint8_t ext_iter_id = *reinterpret_cast<uint8_t*>(data + offset);
      offset++;
      auto it = GetExtParseHandleMap().find(ext_iter_id);
      if (it == GetExtParseHandleMap().end()) {
        offset += (ext_iter_lens - 2);
        continue;
      }
      if (!it->second(MiniSdpLoaderCtx, data + offset, ext_iter_lens - 2)) {
        return kSdpRetWrongFormat;
      }
      offset += (ext_iter_lens - 2);
    }
    if (offset > len) return kSdpRetWrongFormat;
  }

  uint32_t cur_media_id = 0;
  for (auto media : medias) {
    if (!media) continue;
    if (media->MediaType == SdpMediaType::kData) {
      media->MediaName = kSdpDatachannelName;
      media->Protos = kSdpMediaProtoSctpDefault;
      media->TransType = SdpTransType::kTransNone;
    } else {
      media->Protos = (mini_sdp_hdr->encrypt_switch ? kSdpMediaProtoEncryptDefault : kSdpMediaProtoNotEncryptDefault);
    }
    std::string mid = media->MediaId;
    if (mid.empty()) {
      if (mini_sdp_hdr->is_string_bundle) {
        if (media->MediaType == SdpMediaType::kVideo) {
          mid = "video";
        } else if (media->MediaType == SdpMediaType::kAudio) {
          mid = "audio";
        } else {
          mid = "data";
        }
      } else {
        mid = std::to_string(cur_media_id++);
      }
    }

    media->MediaId = mid;
    if (media->MediaType == SdpMediaType::kAudio && rtx_info.count(0)) {
      if (media->Tracks.size()) {
        media->Tracks.front()->RtxSsrc = rtx_info[0].first;
      }
      media->SetRtxInfo(rtx_info[0].second);
    } else if (media->MediaType == SdpMediaType::kVideo && rtx_info.count(1)) {
      if (media->Tracks.size()) {
        media->Tracks.front()->RtxSsrc = rtx_info[1].first;
      }
      media->SetRtxInfo(rtx_info[1].second);
    } else if (media->MediaType == SdpMediaType::kData && rtx_info.count(2)) {
      if (media->Tracks.size()) {
        media->Tracks.front()->RtxSsrc = rtx_info[2].first;
      }
      media->SetRtxInfo(rtx_info[2].second);
    }
    media->IceUfrag = ice_ufrag;
    media->IcePwd = ice_pwd;
    media->RoleType = sdp_info->RoleType;
    for (auto track : media->Tracks) {
      track->SetAttribute("cname", media->IceUfrag);
      std::string codec_name = track->GetAttribute("label");
      if (!codec_name.empty()) {
        track->SetAttribute("msid", media->IceUfrag + " " + media->IceUfrag + "_" + codec_name);
        track->SetAttribute("mslabel", media->IceUfrag);
        track->SetAttribute("label", media->IceUfrag + "_" + codec_name);
      }
    }

    sdp_info->GroupBundle.push_back(mid);
    sdp_info->Medias.emplace(mid, media);

    auto pos = encrypt_key.find(' ');
    if (pos != std::string::npos) {
      media->Fingerprint.first = encrypt_key.substr(0, pos);
      media->Fingerprint.second = encrypt_key.substr(pos + 1);
    }
    for (const auto &transport_info : transport_infos) {
      CandidateInfo candidate;
      candidate.Ip = transport_info.ip_addr;

      if (candidate.Ip.empty() || candidate.Ip == "0.0.0.0" || candidate.Ip == "::" || transport_info.prots.empty()) {
        continue;
      }

      for (auto port : transport_info.prots) {
        candidate.Port = ntohs(port);
        candidate.TransportType = transport_info.transport_type;
        media->Candidates.push_back(candidate);
      }
    }

    if (media->MediaType == SdpMediaType::kVideo && (video_config_entries.size() || video_resolution.second)) {
      for (auto & codec_pair : media->Codecs) {
        if (video_resolution.second) {
          // set videoresolution
          codec_pair.second->FormatParams[kSdpVideoResolutionWidth] = std::to_string(video_resolution.first.first);
          codec_pair.second->FormatParams[kSdpVideoResolutionHeight] = std::to_string(video_resolution.first.second);
        }
        if (codec_pair.second->Name == kSdpCodecH264) {
          if (video_config_entries.find(kSdpSpropParameterSets) != video_config_entries.end()) {
            codec_pair.second->FormatParams[kSdpSpropParameterSets] = video_config_entries[kSdpSpropParameterSets];
          }
        } else if (codec_pair.second->Name == kSdpCodecH265) {
          auto h265_video_config_it = video_config_entries.find(kSdpSpropSps);
          if (h265_video_config_it != video_config_entries.end()) {
            codec_pair.second->FormatParams[kSdpSpropSps] = h265_video_config_it->second;
          }
          h265_video_config_it = video_config_entries.find(kSdpSpropPps);
          if (h265_video_config_it != video_config_entries.end()) {
            codec_pair.second->FormatParams[kSdpSpropPps] = h265_video_config_it->second;
          }
          h265_video_config_it = video_config_entries.find(kSdpSpropVps);
          if (h265_video_config_it != video_config_entries.end()) {
            codec_pair.second->FormatParams[kSdpSpropVps] = h265_video_config_it->second;
          }
        }
      }
    }
  }

  attr.stream_url = kMiniSdpUrlPrefix + stream_url;
  attr.origin_sdp = sdp_info->ToString();
  attr.seq = ntohs(mini_sdp_hdr->seq);
  attr.status_code = ntohs(mini_sdp_hdr->status_code);
  attr.is_imm_send = !mini_sdp_hdr->not_imm_send;
  attr.is_support_aac_fmtp = !mini_sdp_hdr->not_support_aac_fmtp;
  attr.svrsig = ip_addr + ":" + ice_ufrag + ":" + attr.svrsig;
  return offset;
}

MediaDescriptionPtr MiniSdpLoader::parseMedia(char* data, uint32_t& offset,
                                              MiniSdpHdr* mini_sdp_hdr, size_t len) {
  MiniMediaHdr *media_hdr = reinterpret_cast<MiniMediaHdr *>(data + offset);
  offset += sizeof(MiniMediaHdr);
  if (offset > len) return nullptr;
  MediaDescriptionPtr media_info = MakeMediaDescription();
  media_info->MediaType = SdpMediaType(media_hdr->media_type);
  std::string codec_name;
  media_info->AddrType = addr_type;
  media_info->TransType = SdpTransType(mini_sdp_trans_type_vec[mini_sdp_hdr->direction]);
  if (!ip_addr.empty() && ip_addr != "0.0.0.0" && mini_sdp_hdr->has_candidate) {
    CandidateInfo candidate;
    candidate.Ip = ip_addr;
    candidate.Port = ntohs(mini_sdp_hdr->candidate_port);
    candidate.TransportType = TransportLayerProtocolType::kOverUdp;
    media_info->Candidates.push_back(candidate);
  }

  for (int i = 0; i < media_hdr->codec_num; i++) {
    if (offset + kMiniSdpCodecDescLen > len) return nullptr;
    MiniCodecDesc* codec_desc = reinterpret_cast<MiniCodecDesc *>(data + offset);
    offset += sizeof(MiniCodecDesc);
    MiniAacConfig *aac_config = nullptr;
    if (!mini_sdp_hdr->not_support_aac_fmtp && (codec_desc->codec == 1 || codec_desc->codec == 2)) {
      // is LATM || ADTS
      aac_config = reinterpret_cast<MiniAacConfig*>(data + offset);
      if (offset + sizeof(MiniAacConfig) + aac_config->config_len > len) return nullptr;
      offset += sizeof(MiniAacConfig) + aac_config->config_len;
    }
    CodecDescriptionPtr code_info = MakeCodecDescription();
    if (codec_desc->codec >= mini_sdp_codec_name_vec.size()) {
      continue;
    }
    code_info->Name = mini_sdp_codec_name_vec[codec_desc->codec];
    codec_name = code_info->Name;
    code_info->Format = codec_desc->payload_type;
    code_info->Channels = codec_desc->channels;
    if (codec_desc->frequency >= mini_sdp_frequency_vec.size()) {
      continue;
    }
    code_info->SampleRate = mini_sdp_frequency_vec[codec_desc->frequency];
    if (codec_desc->nack) {
      code_info->Feedbacks.emplace(kSdpCodecNack);
    }
    // if (codec_desc->flex_fec) {
    //   code_info->Feedbacks.emplace(kSdpCodecFlexFec);
    // }
    if (codec_desc->transport_cc) {
      code_info->Feedbacks.emplace(kSdpCodecTransportCc);
    }
    if (codec_desc->goog_remb) {
      code_info->Feedbacks.emplace(kSdpCodecGoogleRemb);
    }
    if (codec_desc->rrtr) {
      code_info->Feedbacks.emplace(kSdpCodecRrtr);
    }
    if (codec_desc->bfame_enable) {
      code_info->FormatParams.emplace(kSdpCodecBFrameEnabled, "1");
    }
    if (media_info->MediaType == SdpMediaType::kVideo && code_info->Name.compare(kSdpCodecRtx) != 0) {
      code_info->FormatParams.emplace("level-asymmetry-allowed", "1");
      code_info->FormatParams.emplace("packetization-mode", "1");
      code_info->FormatParams.emplace("profile-level-id", "42e01f");
    }
    if (media_info->MediaType == SdpMediaType::kAudio && code_info->Name.compare(kSdpCodecRtx) != 0) {
      if (aac_config) {
        if (aac_config->object) {
          code_info->FormatParams.emplace("object", std::to_string(static_cast<int>(aac_config->object)));
        }
        code_info->FormatParams.emplace("PS-enabled", (aac_config->flag & kMiniAacFlagPs) ? "1" : "0");
        code_info->FormatParams.emplace("SBR-enabled", (aac_config->flag & kMiniAacFlagSbr) ? "1" : "0");
        code_info->FormatParams.emplace("stereo", (aac_config->flag & kMiniAacFlagStereo) ? "1" : "0");
        code_info->FormatParams.emplace("cpresent", (aac_config->flag & kMiniAacFlagCPresent) ? "1" : "0");
        if (aac_config->config_len > 0) {
          code_info->FormatParams.emplace("config", std::string(aac_config->config_data, aac_config->config_len));
        }
      } else if (!codec_desc->flex_fec) {
        code_info->FormatParams.emplace("stereo", "1");
      }
    }
    media_info->Codecs.emplace(code_info->Format, code_info);
  }

  uint8_t *ext_num = reinterpret_cast<uint8_t *>(data + offset);
  offset += sizeof(uint8_t);
  if (offset + *ext_num * sizeof(MiniExtDesc) > len) return nullptr;

  for (int i = 0; i < *ext_num; i++) {
    if (offset + sizeof(MiniExtDesc) > len) return nullptr;
    MiniExtDesc *ext_desc = reinterpret_cast<MiniExtDesc *>(data + offset);
    offset += sizeof(MiniExtDesc);
    uint8_t ext_id = ext_desc->id;
    if (ext_desc->uri >= mini_sdp_ext_vec.size()) {
      continue;
    }
    media_info->ExtMap.emplace(ext_id, mini_sdp_ext_vec[ext_desc->uri]);
  }
  // todo add stream_id to track
  uint8_t media_type = static_cast<uint8_t>(media_info->MediaType);
  if (media_type >= SdpMediaTypeString.size()) return nullptr;
  codec_name = SdpMediaTypeString[static_cast<uint8_t>(media_info->MediaType)];
  TrackDescriptionPtr track_info = MakeTrackDescription();
  track_info->Ssrc = ntohl(media_hdr->ssrc1);
  track_info->FecSsrc = ntohl(media_hdr->ssrc2);
  if (track_info->Ssrc) {
    // track_info->SetAttribute("msid", "- " + codec_name);
    // track_info->SetAttribute("mslabel", "-");
    track_info->SetAttribute("label", codec_name);
    media_info->Tracks.push_back(track_info);
  }
  return media_info;
}

int MiniSdpLoader::readStr16(std::string& dst, char* data, uint32_t& offset, size_t total_len) {
  constexpr size_t kU16SIZE = sizeof(uint16_t);
  if (offset + kU16SIZE > total_len) return kSdpRetWrongFormat;
  uint16_t *nlen = reinterpret_cast<uint16_t*>(data + offset);
  uint16_t len = ntohs(*nlen);
  offset += kU16SIZE;
  if (len + offset > total_len) return kSdpRetWrongFormat;
  dst.assign(data + offset, len);
  offset += len;
  return len;
}

int MiniSdpLoader::readStr32(std::string &dst, char *data, uint32_t &offset, size_t total_len) {
  constexpr size_t kU32SIZE = sizeof(uint32_t);
  if (offset + kU32SIZE > total_len) return kSdpRetWrongFormat;
  uint32_t *nlen = reinterpret_cast<uint32_t*>(data + offset);
  uint32_t len = ntohl(*nlen);
  offset += kU32SIZE;
  if (len + offset > total_len) return kSdpRetWrongFormat;
  dst.assign(data + offset, len);
  offset += len;
  return len;
}

bool MiniSdpLoader::parseRtxExt(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char *data, size_t lens) {
  constexpr size_t kMiniSdpRtxHdrLen = 6;
  constexpr size_t kMiniSdpRtxAptPairLen = 2;
  uint32_t offset = 0;
  auto &rtx_info_ref = mini_sdp_loader_ctx->rtx_info;
  while (offset < lens) {
    if (offset + kMiniSdpRtxHdrLen > lens) return false;
    uint8_t mid = *reinterpret_cast<uint8_t*>(data + offset);
    offset += 1;
    uint32_t ssrc = ntohl(*reinterpret_cast<uint32_t*>(data + offset));
    offset += 4;
    uint8_t cnt = *reinterpret_cast<uint8_t*>(data + offset);
    offset += 1;
    if (offset + cnt * kMiniSdpRtxAptPairLen > lens) return false;
    rtx_info_ref.emplace(mid, std::make_pair(ssrc, std::map<uint8_t, uint8_t>()));
    for (uint32_t i = 0; i < cnt; i++) {
      uint8_t rtx_pt = *reinterpret_cast<uint8_t*>(data + offset);
      offset++;
      uint8_t apt = *reinterpret_cast<uint8_t*>(data + offset);
      offset++;
      rtx_info_ref[mid].second.emplace(rtx_pt, apt);
    }
  }
  return true;
}

bool MiniSdpLoader::parseCandidateTransportType(MiniSdpLoaderContextPtr mini_sdp_loader_ctx,
                                                char* data, size_t lens,
                                                TransportLayerProtocolType transport_type) {
  constexpr size_t kMiniSdpIpv4Len = 4;
  constexpr size_t kMiniSdpIpv6Len = 16;
  constexpr size_t kMiniSdpPortLen = 2;
  uint8_t candidate_transport_type_cnt = *(reinterpret_cast<uint8_t*>(data));
  if (candidate_transport_type_cnt * kMiniSdpPortLen > lens) return false;
  uint32_t offset = 1;
  uint8_t ip_addr_type = *(reinterpret_cast<uint8_t*>(data + offset));
  offset++;
  struct TransportInfo transport_info;
  transport_info.transport_type = transport_type;

  std::string ip_addr;
  if (static_cast<SdpAddrType>(ip_addr_type) == SdpAddrType::kIPv4) {
    if (offset + kMiniSdpIpv4Len > lens) return false;
    uint32_t ip_addr_v4 = *reinterpret_cast<uint32_t*>(data + offset);
    std::string ip_addr_v4_str = ip2strv4(ip_addr_v4);
    ip_addr = ip_addr_v4_str;
    offset += 4;
  } else if (static_cast<SdpAddrType>(ip_addr_type) == SdpAddrType::kIPv6) {
    if (offset + kMiniSdpIpv6Len > lens) return false;
    uint32_t ip_addr_v6[4] = {0};
    for (uint32_t i = 0; i < 4; i++) {
      ip_addr_v6[i] = *reinterpret_cast<uint32_t*>(data + offset);
      offset += 4;
    }
    std::string ip_addr_v6_str = ip2strv6(reinterpret_cast<uint8_t*>(ip_addr_v6));
    ip_addr = ip_addr_v6_str;
  } else {
    return false;
  }
  transport_info.ip_addr = ip_addr;
  transport_info.prots.resize(candidate_transport_type_cnt);
  for (int i = 0; i < candidate_transport_type_cnt; i++) {
    transport_info.prots[i] = *(reinterpret_cast<uint16_t*>(data + offset));
    offset += 2;
  }
  mini_sdp_loader_ctx->mini_sdp_loader_ptr->transport_infos.emplace_back(std::move(transport_info));
  return true;
}

bool MiniSdpLoader::parseTcpTransportType(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char* data, size_t lens) {
  return parseCandidateTransportType(mini_sdp_loader_ctx, data, lens, TransportLayerProtocolType::kOverTcp);
}

bool MiniSdpLoader::parseUdpTransportType(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char* data, size_t lens) {
  return parseCandidateTransportType(mini_sdp_loader_ctx, data, lens, TransportLayerProtocolType::kOverUdp);
}

bool MiniSdpLoader::parseVideoConfig(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char* data, size_t lens) {
  // video_config_type | config_size | config | ···
  //      1Byte        |    1Byte    |
  // config list : sps, pps, vps
  uint32_t offset = 0;
  std::string video_config_sps, video_config_pps, video_config_vps;
  while (offset < lens) {
    uint8_t video_config_type = *reinterpret_cast<uint8_t*>(data + offset);
    if (video_config_type != kVideoConfigTypeH264 && video_config_type != kVideoConfigTypeH265) return true;
    offset++;
    uint8_t config_len = *reinterpret_cast<uint8_t*>(data + offset);
    offset++;
    if (config_len + offset > lens) return false;
    video_config_sps.assign(data + offset, config_len);
    offset += config_len;
    config_len = *reinterpret_cast<uint8_t*>(data + offset);
    offset++;
    if (config_len + offset > lens) return false;
    video_config_pps.assign(data + offset, config_len);
    offset += config_len;

    auto& video_config_entries = mini_sdp_loader_ctx->mini_sdp_loader_ptr->video_config_entries;
    if (video_config_type == VideoConfigType::kVideoConfigTypeH265) {
      config_len = *reinterpret_cast<uint8_t*>(data + offset);
      offset++;
      if (config_len + offset > lens) return false;
      video_config_vps.assign(data + offset, config_len);
      offset += config_len;
      video_config_entries[kSdpSpropSps] = video_config_sps;
      video_config_entries[kSdpSpropPps] = video_config_pps;
      video_config_entries[kSdpSpropVps] = video_config_vps;
    } else if (video_config_type == VideoConfigType::kVideoConfigTypeH264) {
      video_config_entries[kSdpSpropParameterSets] = video_config_sps + "," + video_config_pps;
    }
  }
  return true;
}

bool MiniSdpLoader::parseVideoResolution(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char* data, size_t lens) {
  // width | height
  // 4Bytes| 4Bytes
  constexpr size_t kMiniSdpVideoResolutionLen = 8;
  if (lens < kMiniSdpVideoResolutionLen) return false;
  uint32_t offset = 0;
  auto& video_resolution = mini_sdp_loader_ctx->mini_sdp_loader_ptr->video_resolution;
  video_resolution.first.first = ntohl(*reinterpret_cast<uint32_t*>(data + offset));
  offset += 4;
  video_resolution.first.second = ntohl(*reinterpret_cast<uint32_t*>(data + offset));
  video_resolution.second = true;
  return true;
}

bool MiniSdpLoader::parseClientInfo(MiniSdpLoaderContextPtr mini_sdp_loader_ctx, char* data, size_t lens) {
  // mini_sdp_loader_ctx->mini_sdp_loader_ptr->attr.c
  // client_type lens | client_type | client_version
  //     1Bytes       |   varBytes  |    4Bytes
  constexpr size_t kMaxClientTypeLen = 8;
  uint32_t offset = 0;
  uint8_t client_type_len = *reinterpret_cast<uint8_t*>(data + offset);
  offset++;
  if (offset + client_type_len > lens || client_type_len > kMaxClientTypeLen) return false;
  auto &attr = mini_sdp_loader_ctx->mini_sdp_loader_ptr->attr;
  attr.client_type.assign(data + offset, client_type_len);
  offset += client_type_len;
  attr.client_version = ntohl(*reinterpret_cast<uint32_t*>(data + offset));
  return true;
}

static int32_t EncodeStunAttribute(char* buff, uint32_t bufflen,
                                   uint16_t type, uint16_t len, const char* value) {
  uint32_t payload_len = len;
  uint32_t pad_len = 0;
  if (payload_len & 0x3) {
    payload_len = ((payload_len >> 2) + 1) << 2;
    pad_len = payload_len - len;
  }

  if (payload_len > bufflen) {
    return kSdpRetSizeExceeded;
  }

  StunAttribute* attr = reinterpret_cast<StunAttribute*>(buff);
  attr->type = htons(type);
  attr->length = htons(len);

  memcpy(buff + kStunAttributeHeaderLen, value, len);
  if (pad_len > 0) {
    memset(buff + kStunAttributeHeaderLen + payload_len - pad_len, 0, pad_len);
  }

  return payload_len + kStunAttributeHeaderLen;
}

static int32_t EncodeStunXorAttr(char* buff, uint32_t bufflen, uint16_t pay_attr_id, uint32_t xkey) {
  StunAttrDecodeKeyData data;
  data.target_attr_id = htons(pay_attr_id);
  data.decode_type = htons(kStunXorTypeKey);
  data.xkey = xkey;  // don't convert to big-end
  return EncodeStunAttribute(buff, bufflen,
                             kStunXorCodeAttributeType, sizeof(data), reinterpret_cast<char*>(&data));
}

static int32_t EncodeStunFingerprinterAttr(char* buff, uint32_t bufflen, const char* pay_data, uint32_t pay_len) {
  if (bufflen < 8) {
    return kSdpRetSizeExceeded;
  }

  uint32_t crc = 0x5354554e;
  uint8_t* pcrc = reinterpret_cast<uint8_t*>(&crc);
  const uint8_t* pv = reinterpret_cast<const uint8_t*>(pay_data);
  const uint8_t* pend = reinterpret_cast<const uint8_t*>(pay_data + pay_len);
  while ((pv + 4) < pend) {
    pcrc[0] ^= pv[0];
    pcrc[1] ^= pv[1];
    pcrc[2] ^= pv[2];
    pcrc[3] ^= pv[3];
    pv += 4;
  }

  // 0x8028 is Fingerprint
  return EncodeStunAttribute(buff, bufflen, 0x8028, sizeof(crc), reinterpret_cast<const char*>(&crc));
}

static void XorEncode(char* buff, uint16_t len, uint32_t xkey) {
  uint8_t* pxkey = reinterpret_cast<uint8_t*>(&xkey);
  uint8_t* psrc = reinterpret_cast<uint8_t*>(buff);
  for (uint16_t idx = 0; idx < len; idx++) {
    psrc[idx] ^= pxkey[idx % sizeof(uint32_t)];
  }
}

StunType CheckStunPrefix(const char* buff, size_t len, uint16_t* attr_type) {
  if (len < kStunPrefixLen) {
    return StunType::kStunNone;
  }

  const StunHeader* hdr = (const StunHeader*)buff;
  uint16_t stun_type = ntohs(hdr->type);
  if (stun_type != StunType::kStunPing && stun_type != StunType::kStunResp) {
    return StunType::kStunNone;
  }

  if (ntohl(hdr->cookie) != kStunMagicCookie) {
    return StunType::kStunNone;
  }

  const StunAttribute* attr = (const StunAttribute*)(buff + kStunHeaderLen);
  uint16_t rd_attr_type = ntohs(attr->type);

  switch (rd_attr_type) {
  case kMiniSdpOverStunAttributeType:
  case kStopStreamOverStunAttributeType:
  case kSigOverStunAttributeType:
    if (attr_type) {
      *attr_type = rd_attr_type;
    }
    break;
  default:
    return StunType::kStunNone;
  }

  return (StunType)stun_type;
}

ssize_t EncodeStunWithFixedPrefix(uint16_t stun_type, uint16_t attr_type, uint16_t attr_len, char* buff, size_t len) {
  // stun content must be 4-bytes aligned
  // the padding content would be ignored, but length must be computed

  // STUN header

  StunHeader* hdr = reinterpret_cast<StunHeader*>(buff);
  hdr->type = htons(stun_type);
  memcpy(hdr->transaction_id, kMiniSdpStunTransactionID, kStunTransactionIdLength);
  hdr->cookie = htonl(kStunMagicCookie);
  size_t offset = kStunHeaderLen;

  // MiniSDP Payload Attribute

  StunAttribute* attr = reinterpret_cast<StunAttribute*>(buff + offset);
  attr->type = htons(attr_type);
  attr->length = htons(attr_len);

  size_t payload_len = attr_len + kStunAttributeHeaderLen;
  size_t pad_len = 0;
  if (payload_len & 0x3) {
    payload_len = ((payload_len >> 2) + 1) << 2;
    pad_len = payload_len - attr_len - kStunAttributeHeaderLen;
  }
  offset += payload_len;

  if (offset > len) {
    return kSdpRetSizeExceeded;
  }

  if (pad_len > 0) {
    memset(buff + offset - pad_len, 0, pad_len);
  }

  // Fingerprint Attribute
  // append fingerprint, if left is enough
  // otherwise return

  if (offset + kStunAttrFingerPrintLen > len) {  // crc size is 4-bytes
    hdr->length = htons(payload_len);
    return offset;
  }

  int32_t r = EncodeStunFingerprinterAttr(buff + offset, len - offset, buff, offset);
  if (r < 0) {
    return r;
  }
  offset += r;

  hdr->length = htons(offset - kStunHeaderLen);

  return offset;
}

int32_t EncodeMsdpInStun(char* msdp_data, uint16_t msdp_len,
                         char* buff, uint32_t len,
                         const MsdpOverStunOption& option) {
  // stun content must be 4-bytes aligned
  // the padding content would be ignored, but length must be computed

  if (len < kStunHeaderLen) {
    return kSdpRetSizeExceeded;
  }

  // STUN header
  StunHeader* hdr = reinterpret_cast<StunHeader*>(buff);
  hdr->type = htons(option.stun_type);
  memcpy(hdr->transaction_id, kMiniSdpStunTransactionID, kStunTransactionIdLength);
  hdr->cookie = htonl(kStunMagicCookie);
  size_t offset = kStunHeaderLen;

  // MiniSDP Payload Attribute

  int32_t r = 0;

  uint32_t xkey = option.xor_code_key;
  if (xkey != XorCodeType::kXorCodeNone) {
    if (xkey == XorCodeType::kXorCodeRandom) {
      xkey = GenerateCrc(msdp_data, msdp_len);
    }
    XorEncode(hdr->transaction_id, kStunTransactionIdLength, xkey);
    XorEncode(msdp_data, msdp_len, xkey);
  }

  r = EncodeStunAttribute(buff + offset, len - offset, option.attr_type, msdp_len, msdp_data);
  if (r < 0) {
    return r;
  }
  offset += r;

  if (xkey != XorCodeType::kXorCodeNone) {
    r = EncodeStunXorAttr(buff + offset, len - offset, option.attr_type, xkey);
    if (r < 0) {
      return r;
    }
    offset += r;
  }

  // Fingerprint Attribute
  // append fingerprint, if left is enough
  // otherwise return

  if (offset + kStunAttrFingerPrintLen > len) {  // crc size is 4-bytes
    hdr->length = htons(offset - kStunHeaderLen);
    return offset;
  }

  r = EncodeStunFingerprinterAttr(buff + offset, len - offset, buff, offset);
  if (r < 0) {
    return r;
  }
  offset += r;

  hdr->length = htons(offset - kStunHeaderLen);

  return offset;
}

int32_t DecodeMsdpInStun(char* msdp_data, uint16_t msdp_len,
                         const char* buff, uint32_t len,
                         MsdpOverStunOption* option) {
  if (len < kStunPrefixLen || option == nullptr) {
    return kSdpRetWrongFormat;
  }

  const StunHeader* hdr = (const StunHeader*)buff;
  uint16_t stun_type = ntohs(hdr->type);
  if (stun_type != StunType::kStunPing && stun_type != StunType::kStunResp) {
    return kSdpRetWrongFormat;
  }

  if (ntohl(hdr->cookie) != kStunMagicCookie) {
    return kSdpRetWrongFormat;
  }

  uint16_t payload_attr_type = 0;
  const char* payload_data = nullptr;
  uint16_t payload_len = 0;

  uint16_t xor_target_attr_id = 0;
  uint32_t xkey = 0;

  uint32_t offset = kStunHeaderLen;
  while (offset < len) {
    const StunAttribute* attr = (const StunAttribute*)(buff + offset);
    uint16_t attr_len = ntohs(attr->length);
    uint16_t attr_type = ntohs(attr->type);
    if (offset + attr_len + kStunAttributeHeaderLen > len) {
      return kSdpRetWrongFormat;
    }
    offset += (attr_len + kStunAttributeHeaderLen);
    while ((offset & 0x3) && offset < len) {
      offset++;
    }

    switch (attr_type) {
    case kMiniSdpOverStunAttributeType:
    case kStopStreamOverStunAttributeType:
    case kSigOverStunAttributeType:
      payload_attr_type = attr_type;
      payload_len = attr_len;
      payload_data = attr->value;
      break;
    case kStunXorCodeAttributeType:
      if (attr_len == sizeof(StunAttrDecodeKeyData)) {
        const StunAttrDecodeKeyData* xkey_data = (const StunAttrDecodeKeyData*)attr->value;
        xor_target_attr_id = htons(xkey_data->target_attr_id);
        xkey = xkey_data->xkey;   // need not to convert
      }
      break;
    default:
      break;
    }
  }

  if (payload_attr_type == 0 || payload_data == nullptr || payload_len == 0) {
    return kSdpRetWrongFormat;
  }

  if (payload_len > msdp_len) {
    return kSdpRetSizeExceeded;
  }

  memcpy(msdp_data, payload_data, payload_len);

  if (xor_target_attr_id == payload_attr_type) {
    XorEncode(msdp_data, payload_len, xkey);
  }

  if (option != nullptr) {
    option->stun_type = stun_type;
    option->attr_type = payload_attr_type;
    option->xor_code_key = xkey;
  }

  return payload_len;
}

}  // namespace mini_sdp
