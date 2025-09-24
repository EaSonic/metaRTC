/**
 * @file mini_sdp/sdp_parser.cc
 * @brief 
 * @version 0.1
 * @date 2021-01-07
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#include "mini_sdp/sdp_parser.h"
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>
#include "mini_sdp/util.h"
#include "mini_sdp/compat.h"

namespace mini_sdp {

constexpr size_t kSdpLineTypeSize = 2;

static const std::unordered_map<std::string, SessionAttrParseHandle>& GetSessionAttrParseHandleMap() {
  static NoDestructor<std::unordered_map<std::string, SessionAttrParseHandle>> instance({
    {"group", SessionAttrParseGroup}
  });
  return *instance;
}

static const std::unordered_map<std::string, MediaAttrParseHandle>& GetMediaAttrParseHandleMap() {
  static NoDestructor<std::unordered_map<std::string, MediaAttrParseHandle>> instance({
    {"ice-ufrag",   MediaAttrParseIceUfrag},
    {"ice-pwd",     MediaAttrParseIcePwd},
    {"ice-options", MediaAttrParseIceOptions},
    {"fingerprint", MediaAttrParseFingerprint},
    {"setup",       MediaAttrParseSetup},
    {"mid",         MediaAttrParseMid},
    {"extmap",      MediaAttrParseExtmap},
    {"sendrecv",    MediaAttrParseTransType},
    {"sendonly",    MediaAttrParseTransType},
    {"recvonly",    MediaAttrParseTransType},
    {"inactive",    MediaAttrParseTransType},
    {"rtpmap",      MediaAttrParseRtpmap},
    {"rtcp-fb",     MediaAttrParseRtcpFb},
    {"fmtp",        MediaAttrParseFmtp},
    {"ssrc",        MediaAttrParseSsrc},
    {"candidate",   MediaAttrParseCandidate},
    {"msid",        MediaAttrParseMsid},
    {"ssrc-group",  MediaAttrParseSsrcGroup}
  });
  return *instance;
}

std::pair<SdpAddrType, bool> ParseSdpAddrType(const char* word, size_t len) {
  std::pair<SdpAddrType, bool> rpair;
  rpair.second = true;
  if (IsStrEqual(word, len, kSdpAddrIP4, sizeof(kSdpAddrIP4) - 1)) {
    rpair.first = SdpAddrType::kIPv4;
  } else if (IsStrEqual(word, len, kSdpAddrIP6, sizeof(kSdpAddrIP6) - 1)) {
    rpair.first = SdpAddrType::kIPv6;
  } else {
    rpair.second = false;
  }
  return rpair;
}

std::pair<SdpTransType, bool> ParseSdpTransType(const char* word, size_t len) {
  std::pair<SdpTransType, bool> rpair;
  rpair.second = true;
  if (IsStrEqual(word, len, kSdpTransSendRecv, sizeof(kSdpTransSendRecv) - 1)) {
    rpair.first = SdpTransType::kSendRecv;
  } else if (IsStrEqual(word, len, kSdpTransSendOnly, sizeof(kSdpTransSendOnly) - 1)) {
    rpair.first = SdpTransType::kSendOnly;
  } else if (IsStrEqual(word, len, kSdpTransRecvOnly, sizeof(kSdpTransRecvOnly) - 1)) {
    rpair.first = SdpTransType::kRecvOnly;
  } else if (IsStrEqual(word, len, kSdpTransInactive, sizeof(kSdpTransInactive) - 1)) {
    rpair.first = SdpTransType::kInactive;
  } else {
    rpair.second = false;
  }
  return rpair;
}

std::pair<SdpMediaType, bool> ParseSdpMediaType(const char* word, size_t len) {
  std::pair<SdpMediaType, bool> rpair;
  rpair.second = true;
  if (IsStrEqual(word, len, kSdpMediaAudio, sizeof(kSdpMediaAudio) - 1)) {
    rpair.first = SdpMediaType::kAudio;
  } else if (IsStrEqual(word, len, kSdpMediaVideo, sizeof(kSdpMediaVideo) - 1)) {
    rpair.first = SdpMediaType::kVideo;
  } else if (IsStrEqual(word, len, kSdpMediaData, sizeof(kSdpMediaData) - 1)) {
    rpair.first = SdpMediaType::kData;
  } else {
    rpair.second = false;
  }
  return rpair;
}

std::pair<SdpRoleType, bool> ParseSdpRoleType(const char* word, size_t len) {
  std::pair<SdpRoleType, bool> rpair;
  rpair.second = true;
  if (IsStrEqual(word, len, kSdpRoleActive, sizeof(kSdpRoleActive) - 1)) {
    rpair.first = SdpRoleType::kActive;
  } else if (IsStrEqual(word, len, kSdpRoleActpass, sizeof(kSdpRoleActpass) - 1)) {
    rpair.first = SdpRoleType::kActpass;
  } else if (IsStrEqual(word, len, kSdpRolePassive, sizeof(kSdpRolePassive) - 1)) {
    rpair.first = SdpRoleType::kPassive;
  } else {
    rpair.second = false;
  }
  return rpair;
}

SdpParser::SdpParser(const char* data, size_t length)
: data_(data), length_(length), stat_info_(StatCode::kNotParsed, "") {
  // nothing
}

SdpParser::~SdpParser() {
  // nothing
}

bool SdpParser::Parse() {
  sd_ptr_ = MakeSessionDescription();
  while (loadNextLine()) {
    if (!parseLine()) return false;
  }
  // append the last media
  if (isInMediaLevel()) appendMedia();
  return true;
}

void SdpParser::setStatInfo(StatCode code, const std::string& msg, size_t line) {
  if (line != 0) {
    stat_info_ = std::make_pair(code, msg + ", line " + std::to_string(line));
  } else {
    stat_info_ = std::make_pair(code, msg);
  }
}

bool SdpParser::loadNextLine() {
  if (length_ == 0) return false;
  line_data_ = data_;
  line_length_ = 0;
  while (line_length_ < length_ && *data_ != '\r' && *data_ != '\n') {
    line_length_++;
    data_++;
  }
  length_ -= line_length_;

  while (length_ > 0 && (*data_ == '\r' || *data_ == '\n')) {
    data_++;
    length_--;
  }

  line_idx_++;

  return true;
}

bool SdpParser::parseLine() {
  // parse line
  switch (*line_data_) {
  case 'a': return parseLineAttribute();  // Attribute
  case 'b': return true;  // Bandwidth
  case 'c': return parseLineConnection();  // Connection
  case 'e': return true;  // Email
  case 'i': return parseLineSessionInfo();  // Session Information
  case 'k': return true;  // Encrypt
  case 'm': return parseLineMedia();
  case 'o': return parseLineOrigin();   // Origin
  case 'p': return true;  // Phone
  case 'r': return true;  // Repeat
  case 's': return parseLineSessionName();  // Session Name
  case 't': return true;  // Timing
  case 'v': return true;  // Version
  case 'u': return true;  // Uri
  case 'z': return true;  // Time Zone
  default:
    setStatInfo(StatCode::kUnknownLine, "unknown line", line_idx_);
    return false;
  }
}

bool SdpParser::parseLineOrigin() {
  // o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
  std::vector<StrSlice> slices = StrSplit(line_data_ + kSdpLineTypeSize,
                                          line_length_ - kSdpLineTypeSize,
                                          ' ');
  if (slices.size() != 6) {
    setStatInfo(StatCode::kFormatError, "format error", line_idx_);
    return false;
  }

  sd_ptr_->UserName       = slices[0].ToString();
  sd_ptr_->SessionId      = slices[1].ToString();
  sd_ptr_->SessionVersion = slices[2].ToString();

  auto raddr  = ParseSdpAddrType(slices[4].ptr, slices[4].len);
  if (raddr.second) {
    sd_ptr_->AddrType = raddr.first;
  }

  if (!raddr.second) {
    setStatInfo(StatCode::kParamError, "param error", line_idx_);
    return false;
  }

  return true;
}

bool SdpParser::parseLineSessionName() {
  sd_ptr_->UserName = std::string(line_data_ + kSdpLineTypeSize, line_length_ - kSdpLineTypeSize);
  return true;
}

bool SdpParser::parseLineSessionInfo() {
  sd_ptr_->SessionInfo = std::string(line_data_ + kSdpLineTypeSize, line_length_ - kSdpLineTypeSize);
  return true;
}

bool SdpParser::parseLineConnection() {
  // c=<nettype> <addrtype> <connection-address>
  std::vector<StrSlice> slices = StrSplit(line_data_ + kSdpLineTypeSize,
                                          line_length_ - kSdpLineTypeSize,
                                          ' ');
  auto raddr = ParseSdpAddrType(slices[1].ptr, slices[1].len);
  if (!raddr.second) {
    setStatInfo(StatCode::kParamError, "param error", line_idx_);
    return false;
  }

  if (isInMediaLevel()) {
    cur_media_ptr_->AddrType = raddr.first;
  } else if (sd_ptr_->AddrType != raddr.first) {
    setStatInfo(StatCode::kParamError, "addr type conflict", line_idx_);
    return false;
  }

  return true;
}

bool SdpParser::parseLineMedia() {
  // m=<media> <port> <proto> <fmt>
  std::vector<StrSlice> slices = StrSplit(line_data_ + kSdpLineTypeSize,
                                          line_length_ - kSdpLineTypeSize,
                                          ' ');
  if (slices.size() < 4) {
    setStatInfo(StatCode::kFormatError, "format error", line_idx_);
    return false;
  }

  auto rmedia = ParseSdpMediaType(slices[0].ptr, slices[0].len);
  if (!rmedia.second) {
    setStatInfo(StatCode::kParamError, "media type not supported", line_idx_);
    return false;
  }

  // append pre media
  if (isInMediaLevel()) appendMedia();

  cur_media_ptr_ = MakeMediaDescription();
  cur_media_ptr_->MediaType = rmedia.first;
  cur_media_ptr_->Port      = atoi(slices[1].ptr);
  cur_media_ptr_->Protos    = slices[2].ToString();

  if (rmedia.first == SdpMediaType::kData) {
    cur_media_ptr_->MediaName = slices[3].ToString();
  }

  return true;
}

bool SdpParser::parseLineAttribute() {
  // a=<key>:[<fmt> ]<value>
  auto rpair = StrGetFirstSplit(line_data_ + kSdpLineTypeSize, line_length_ - kSdpLineTypeSize, ':');
  auto& key = rpair.first;

  const char* data = rpair.second;
  size_t len = data != nullptr ? line_data_ + line_length_ - data : 0;
  if (isInSessionLevel()) {
    auto it = GetSessionAttrParseHandleMap().find(key);
    if (it != GetSessionAttrParseHandleMap().end()) {
      if (!(it->second)(sd_ptr_, std::move(key), data, len)) {
        std::string errmsg = std::string(line_data_, line_length_);
        errmsg += ": param error";
        setStatInfo(StatCode::kParamError, errmsg, line_idx_);
        return false;
      }
    } else if (data != nullptr) {
      sd_ptr_->SetAttribute(key, std::string(data, len));
    } else {
      sd_ptr_->SetAttribute(key, "");
    }
  } else {
    auto it = GetMediaAttrParseHandleMap().find(key);
    if (it != GetMediaAttrParseHandleMap().end()) {
      if (!(it->second)(cur_media_ptr_, std::move(key), data, len)) {
        std::string errmsg = std::string(line_data_, line_length_);
        errmsg += ": param error";
        setStatInfo(StatCode::kParamError, errmsg, line_idx_);
        return false;
      }
    } else if (data != nullptr) {
      cur_media_ptr_->SetAttribute(key, std::string(data, len));
    } else {
      cur_media_ptr_->SetAttribute(key, "");
    }
  }

  return true;
}

void SdpParser::appendMedia() {
  std::string mid = cur_media_ptr_->MediaId;
  if (mid.empty()) mid = std::to_string(cur_media_id_++);
  while (sd_ptr_->Medias.count(mid) > 0) {
    mid = std::to_string(cur_media_id_++);
  }
  sd_ptr_->Medias.emplace(mid, cur_media_ptr_);
}

bool SessionAttrParseGroup(SessionDescriptionPtr sess, std::string&& key, const char* data, size_t len) {
  // a=group:BUNDLE <mid> <mid>
  std::vector<StrSlice> slices = StrSplit(data, len, ' ');
  if (slices.size() <= 1) return true;

  for (size_t idx = 1; idx < slices.size(); idx++) {
    sess->GroupBundle.emplace_back(slices[idx].ptr, slices[idx].len);
  }
  return true;
}

bool MediaAttrParseIceUfrag(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=ice-ufrag
  media->IceUfrag.assign(data, len);
  return true;
}

bool MediaAttrParseIcePwd(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=ice-pwd
  media->IcePwd.assign(data, len);
  return true;
}

bool MediaAttrParseIceOptions(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=ice-options
  media->IceOptions.assign(data, len);
  return true;
}

bool MediaAttrParseFingerprint(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=fingerprint
  std::vector<StrSlice> slices = StrSplit(data, len, ' ');

  if (slices.size() > 0) {
    media->Fingerprint.first.assign(slices[0].ptr, slices[0].len);
  }
  if (slices.size() > 1) {
    media->Fingerprint.second.assign(slices[1].ptr, slices[1].len);
  }
  return true;
}

bool MediaAttrParseSetup(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=setup
  auto rpair = ParseSdpRoleType(data, len);
  if (!rpair.second) return false;
  media->RoleType = rpair.first;
  return true;
}

bool MediaAttrParseMid(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=mid
  media->MediaId.assign(data, len);
  return true;
}

bool MediaAttrParseExtmap(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=extmap:<id> <uri>
  std::vector<StrSlice> slices = StrSplit(data, len, ' ');
  if (slices.size() != 2) return false;
  int64_t id = atoi(slices[0].ptr);
  if (id < 0 || id > 255) return false;
  media->ExtMap.emplace((uint8_t)id, slices[1].ToString());
  return true;
}

bool MediaAttrParseTransType(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=sendrecv / a=sendonly / a=recvonly / a=inactive
  auto rpair = ParseSdpTransType(key.c_str(), key.size());
  if (!rpair.second) return false;
  media->TransType = rpair.first;
  return true;
}

bool MediaAttrParseRtpmap(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=rtpmap:<fmt> <name>/<sample_rate>[/<channels>]
  std::vector<StrSlice> slices = StrSplit(data, len, ' ');
  if (slices.size() != 2) return false;

  int64_t fmt = atoi(slices[0].ptr);
  if (fmt < 0 || fmt > 255) return false;

  std::vector<StrSlice> codec_slices = StrSplit(slices[1].ptr, slices[1].len, '/');
  if (codec_slices.size() < 2) return false;

  auto codec = MakeCodecDescription();
  codec->Format = fmt;
  codec->Name = codec_slices[0].ToString();
  codec->SampleRate = atol(codec_slices[1].ptr);

  if (codec_slices.size() > 2) {
    codec->Channels = atol(codec_slices[2].ptr);
  }

  media->Codecs.emplace(fmt, codec);

  return true;
}

bool MediaAttrParseRtcpFb(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=rtcp-fb:<fmt> <value>
  auto rpair = StrGetFirstSplit(data, len, ' ');
  if (rpair.second == nullptr) return false;

  if (rpair.first.empty() || !std::isdigit(rpair.first[0])) {
    return true;
  }

  int64_t fmt = stol(rpair.first);
  if (fmt < 0 || fmt > 255) return false;

  auto it = media->Codecs.find(fmt);
  if (it == media->Codecs.end()) return false;

  it->second->Feedbacks.emplace(rpair.second, data + len - rpair.second);

  return true;
}

bool MediaAttrParseFmtp(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=fmtp:<fmt> <key>=<value>[;<key>=<value>]
  std::vector<StrSlice> slices = StrSplit(data, len, ' ');
  if (slices.size() != 2) return false;

  int64_t fmt = atoi(slices[0].ptr);
  if (fmt < 0 || fmt > 255) return false;

  auto it = media->Codecs.find(fmt);
  if (it == media->Codecs.end()) return false;
  auto& codec = it->second;

  std::vector<StrSlice> kvs = StrSplit(slices[1].ptr, slices[1].len, ';');
  for (auto& kv : kvs) {
    const char* pos = (const char*)memchr(kv.ptr, '=', kv.len);
    if (pos == nullptr) {
      codec->FormatParams.emplace(kv.ToString(), "");
    } else {
      codec->FormatParams.emplace(std::string(kv.ptr, pos - kv.ptr), std::string(pos + 1, kv.ptr + kv.len - pos - 1));
    }
  }

  return true;
}

bool MediaAttrParseSsrc(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=ssrc:<ssrc> <key>:<value>
  std::vector<StrSlice> slices = StrSplit(data, len, ' ');
  if (slices.size() < 2) return false;

  int64_t ssrc = atoll(slices[0].ptr);
  if (ssrc < 0 || ssrc > std::numeric_limits<uint32_t>::max()) return false;

  TrackDescriptionPtr track;
  for (auto &track_iter_ptr : media->Tracks) {
    if (track_iter_ptr->Ssrc == ssrc) {
      track = track_iter_ptr;
      break;
    } else if (track_iter_ptr->RtxSsrc == ssrc || track_iter_ptr->FecSsrc == ssrc) {
      return true;
    }
  }
  if (!track) {
    track = MakeTrackDescription();
    track->Ssrc = ssrc;
    media->Tracks.push_back(track);
  }

  auto rpair = StrGetFirstSplit(slices[1].ptr, slices[1].len, ':');
  auto& attr_key = rpair.first;
  if (rpair.second != nullptr) {
    track->SetAttribute(attr_key, std::string(rpair.second, slices[1].ptr + slices[1].len - rpair.second));
  } else {
    track->SetAttribute(attr_key, "");
  }

  return true;
}

bool MediaAttrParseCandidate(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=candidate:foundation 1 udp 100 <ip> <port> ...
  std::vector<StrSlice> slices = StrSplit(data, len, ' ');
  if (slices.size() < 6) return false;

  int64_t port = atoll(slices[5].ptr);
  if (port < 0 || port > std::numeric_limits<uint16_t>::max()) return false;

  CandidateInfo info;
  info.Ip.assign(slices[4].ptr, slices[4].len);
  info.Port = port;
  if (slices[2].IsEqual("udp")) {
    info.TransportType = TransportLayerProtocolType::kOverUdp;
  } else if (slices[2].IsEqual("tcp")) {
    info.TransportType = TransportLayerProtocolType::kOverTcp;
  }
  media->Candidates.push_back(info);

  return true;
}

bool MediaAttrParseMsid(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  std::vector<StrSlice> slices = StrSplit(data, len, ' ');
  if (slices.size() < 2) return false;

  media->StreamId.assign(slices[0].ptr, slices[0].len);
  media->TrackId.assign(slices[1].ptr, slices[1].len);
  return true;
}

bool MediaAttrParseSsrcGroup(MediaDescriptionPtr media, std::string&& key, const char* data, size_t len) {
  // a=ssrc-group:<type> ssrc1 ssrc2....
  std::vector<StrSlice> slices = StrSplit(data, len, ' ');
  if (slices.size() < 3) return false;

  std::string ssrc_group_key = std::string(slices[0].ptr, slices[0].len);
  uint32_t ssrc_media = static_cast<uint32_t>(atoll(std::string(slices[1].ptr, slices[1].len).c_str()));
  uint32_t ssrc_association = static_cast<uint32_t>(atoll(std::string(slices[2].ptr, slices[2].len).c_str()));
  TrackDescriptionPtr track;
  for (auto &track_iter_ptr : media->Tracks) {
    if (track_iter_ptr->Ssrc == ssrc_media) {
      track = track_iter_ptr;
      break;
    }
  }
  if (!track) {
    track = MakeTrackDescription();
    track->Ssrc = ssrc_media;
    media->Tracks.push_back(track);
  }
  if (ssrc_group_key == kMiniSdpSsrcGroupRtx) {
    track->RtxSsrc = ssrc_association;
  } else if (ssrc_group_key == kMiniSdpSsrcGroupFec) {
    track->FecSsrc = ssrc_association;
  }
  return true;
}

}  // namespace mini_sdp
