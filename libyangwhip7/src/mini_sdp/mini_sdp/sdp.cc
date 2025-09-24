/**
 * @file mini_sdp/sdp.cc
 * @brief 
 * @version 0.1
 * @date 2020-12-17
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#include "mini_sdp/sdp.h"
#include <sstream>
#include "mini_sdp/sdp_parser.h"

namespace mini_sdp {

/**
 * CodeDescription
 */

CodecDescription::CodecDescription(const std::string& name, uint32_t sample_rate, uint16_t channels)
: Name(name), Channels(channels), SampleRate(sample_rate) {
  // nothing
}

bool CodecDescription::IsSimpleEqual(const CodecDescription& rhs) const {
  return Channels == rhs.Channels &&
    SampleRate == rhs.SampleRate &&
    Name == rhs.Name;
}

bool CodecDescription::IsStrictEqual(const CodecDescription& rhs) const {
  return IsSimpleEqual(rhs) &&
    Feedbacks == rhs.Feedbacks &&
    FormatParams == rhs.FormatParams &&
    attributes_ == rhs.attributes_;
}

const std::string& CodecDescription::GetFormatParam(const std::string& key,
                                                    const std::string& def_val) const {
  auto it = FormatParams.find(key);
  return it != FormatParams.end() ? it->second : def_val;
}

bool CodecDescription::HasAttribute(const std::string& key) const {
  return attributes_.count(key);
}

const std::string& CodecDescription::GetAttribute(const std::string& key) const {
  static std::string g_empty_str;
  auto it = attributes_.find(key);
  return it != attributes_.end() ? it->second : g_empty_str;
}

void CodecDescription::SetAttribute(const std::string& key, const std::string& value) {
  attributes_.emplace(key, value);
}

std::string CodecDescription::ToString() const {
  std::ostringstream oss;
  if (Name == kSdpCodecRtx) return "";
  oss << "a=rtpmap:" << static_cast<int>(Format) << ' ' << Name << '/' << SampleRate;
  if (Channels > 0) oss << '/' << Channels;
  oss << kSdpEndOfLine;

  for (auto& fb : Feedbacks) {
    oss << "a=rtcp-fb:" << static_cast<int>(Format) << ' ' << fb << kSdpEndOfLine;
  }

  if (!FormatParams.empty()) {
    oss << "a=fmtp:" << static_cast<int>(Format) << ' ';
    auto it = FormatParams.begin();
    while (it != FormatParams.end()) {
      if (it != FormatParams.begin()) oss << ';';
      oss << it->first << '=' << it->second;
      it++;
    }
    oss << kSdpEndOfLine;
  }

  if (RtxPayloadType != 0) {
    oss << "a=rtpmap:" << static_cast<int>(RtxPayloadType) << ' ' << "rtx" << '/' << SampleRate;
    if (Channels > 0) oss << '/' << Channels;
    oss << kSdpEndOfLine;
    oss << "a=fmtp:" << static_cast<int>(RtxPayloadType) << ' ' << "apt=" << static_cast<int>(Format);
    oss << kSdpEndOfLine;
  }



  for (auto& attr : attributes_) {
    oss << "a=" << attr.first << ':' << static_cast<int>(Format) << ' ' << attr.second
      << kSdpEndOfLine;
  }

  return oss.str();
}

/**
 * TrackDescription
 */

TrackDescription::TrackDescription(uint32_t ssrc)
: Ssrc(ssrc) {
  // nothing
}

bool TrackDescription::HasAttribute(const std::string& key) const {
  return attributes_.count(key);
}

const std::string& TrackDescription::GetAttribute(const std::string& key) const {
  static std::string g_empty_str;
  auto it = attributes_.find(key);
  return it != attributes_.end() ? it->second : g_empty_str;
}

void TrackDescription::SetAttribute(const std::string& key, const std::string& value) {
  attributes_[key] = value;
}

std::string TrackDescription::ToString() const {
  std::ostringstream oss;

  if (FecSsrc) {
    oss << "a=ssrc-group:FEC-FR " << std::to_string(Ssrc) << " " << std::to_string(FecSsrc) << kSdpEndOfLine;
  }

  if (RtxSsrc) {
    oss << "a=ssrc-group:FID " << std::to_string(Ssrc) << " " << std::to_string(RtxSsrc) << kSdpEndOfLine;
  }

  for (auto& attr : attributes_) {
    oss << "a=ssrc:" << Ssrc << ' ' << attr.first << ':' << attr.second << kSdpEndOfLine;
  }

  if (FecSsrc) {
    for (auto& attr : attributes_) {
      oss << "a=ssrc:" << FecSsrc << ' ' << attr.first << ':' << attr.second << kSdpEndOfLine;
    }
  }

  if (RtxSsrc) {
    for (auto& attr : attributes_) {
      oss << "a=ssrc:" << RtxSsrc << ' ' << attr.first << ':' << attr.second << kSdpEndOfLine;
    }
  }

  return oss.str();
}

/**
 * MediaDescription
 */

MediaDescription::MediaDescription(SdpMediaType type, bool is_encrypt)
: MediaType(type),
  Protos(is_encrypt ? kSdpMediaProtoEncryptDefault : kSdpMediaProtoNotEncryptDefault) {
  // nothing
}

bool MediaDescription::HasAttribute(const std::string& key) const {
  return attributes_.count(key);
}

const std::string& MediaDescription::GetAttribute(const std::string& key) const {
  static std::string g_empty_str;
  auto it = attributes_.find(key);
  return it != attributes_.end() ? it->second : g_empty_str;
}

void MediaDescription::SetAttribute(const std::string& key, const std::string& value) {
  attributes_[key] = value;
}

void MediaDescription::SetRtxInfo(const std::map<uint8_t, uint8_t>& rtx_payload_info) {
  if (rtx_payload_info.empty()) return;
  for (const auto& payload_pair : rtx_payload_info) {
    uint8_t rtx_pt = payload_pair.first;
    uint8_t apt = payload_pair.second;
    if (Codecs.count(apt) != 0) {
      Codecs[rtx_pt]->FormatParams.emplace("apt", std::to_string(apt));
      Codecs[rtx_pt]->AssociatePayloadType = apt;
      Codecs[apt]->RtxPayloadType = rtx_pt;
    }
  }
}

std::string MediaDescription::ToString() const {
  std::ostringstream oss;

  // main line
  oss << "m=";
  switch (MediaType) {
  case SdpMediaType::kAudio:
    oss << kSdpMediaAudio;
    break;
  case SdpMediaType::kVideo:
    oss << kSdpMediaVideo;
    break;
  case SdpMediaType::kData:
    oss << kSdpMediaData;
    break;
  default:
    oss << "unknown";
    break;
  }
  oss << ' ' << Port << ' ' << Protos;
  if (MediaType == SdpMediaType::kAudio || MediaType == SdpMediaType::kVideo) {
    for (auto& codec : Codecs) {
      if (codec.second->Name.compare(kSdpCodecRtx) == 0) {
        continue;
      }
      oss << ' ' << int(codec.first);
      if (codec.second->RtxPayloadType != 0) {
        oss << ' ' << int(codec.second->RtxPayloadType);
      }
    }
  } else if (MediaType == SdpMediaType::kData) {
    oss << ' ' << MediaName;
  }
  oss << kSdpEndOfLine;

  // connection line
  if (AddrType == SdpAddrType::kIPv4) {
    oss << "c=IN IP4 0.0.0.0" << kSdpEndOfLine;
    if (MediaType != SdpMediaType::kData) {
      oss << "a=rtcp:" << Port << " IN IP4 0.0.0.0" << kSdpEndOfLine;
    }
  } else if (AddrType == SdpAddrType::kIPv6) {
    oss << "c=IN IP6 ::" << kSdpEndOfLine;
    if (MediaType != SdpMediaType::kData) {
      oss << "a=rtcp:" << Port << " IN IP6 ::" << kSdpEndOfLine;
    }
  }

  // candidates
  if (MediaType != SdpMediaType::kData) {
    for (auto& candidate : Candidates) {
      oss << "a=candidate:foundation 1 ";
      switch (candidate.TransportType) {
      case TransportLayerProtocolType::kOverUdp:
        oss << "udp ";
        break;
      case TransportLayerProtocolType::kOverTcp:
        oss << "tcp ";
        break;
      default:
        break;
      }
      oss << "100 " << candidate.Ip << ' ' << candidate.Port
          << ' ' << "typ srflx raddr " << candidate.Ip << " rport " << candidate.Port
          << " generation 0" << kSdpEndOfLine;
    }
  }

  // ice
  if (!IceUfrag.empty()) {
    oss << "a=ice-ufrag:" << IceUfrag << kSdpEndOfLine;
  }
  if (!IcePwd.empty()) {
    oss << "a=ice-pwd:" << IcePwd << kSdpEndOfLine;
  }
  if (!IceOptions.empty()) {
    oss << "a=ice-options:" << IceOptions << kSdpEndOfLine;
  }

  // fingerprint
  if (!Fingerprint.first.empty()) {
    oss << "a=fingerprint:" << Fingerprint.first << ' ' << Fingerprint.second << kSdpEndOfLine;
  }

  // role type
  switch (RoleType) {
  case SdpRoleType::kActpass:
    oss << "a=setup:actpass" << kSdpEndOfLine;
    break;
  case SdpRoleType::kActive:
    oss << "a=setup:active" << kSdpEndOfLine;
    break;
  case SdpRoleType::kPassive:
    oss << "a=setup:passive" << kSdpEndOfLine;
    break;
  case SdpRoleType::kRoleNone:
  default:
    break;
  }

  // mid
  if (!MediaId.empty()) oss << "a=mid:" << MediaId << kSdpEndOfLine;

  // transport type
  switch (TransType) {
  case SdpTransType::kSendRecv:
    oss << "a=sendrecv" << kSdpEndOfLine;
    break;
  case SdpTransType::kRecvOnly:
    oss << "a=recvonly" << kSdpEndOfLine;
    break;
  case SdpTransType::kSendOnly:
    oss << "a=sendonly" << kSdpEndOfLine;
    break;
  case SdpTransType::kInactive:
    oss << "a=inactive" << kSdpEndOfLine;
    break;
  case SdpTransType::kTransNone:
  default:
    break;
  }

  if (MediaType != SdpMediaType::kData) {
    oss << "a=rtcp-mux" << kSdpEndOfLine;
  }
  if (MediaType == SdpMediaType::kVideo) {
    oss << "a=rtcp-rsize" << kSdpEndOfLine;
  }

  if (MediaType == SdpMediaType::kData) {
    oss << "a=sctp-port:" << kSdpSctpPortDefault << kSdpEndOfLine;
    oss << "a=max-message-size:" << kSdpSctpMaxMessageSizeDefault << kSdpEndOfLine;
  }

  // extmap lines
  for (auto& ext : ExtMap) {
    oss << "a=extmap:" << static_cast<int>(ext.first) << ' ' << ext.second << kSdpEndOfLine;
  }

  for (auto& attr : attributes_) {
    oss << "a=" << attr.first << ':' << attr.second << kSdpEndOfLine;
  }

  // codecs
  for (auto& codec : Codecs) {
    oss << codec.second->ToString();
  }

  // tracks
  for (auto& track_iter_ptr : Tracks) {
    oss << track_iter_ptr->ToString();
  }

  return oss.str();
}

/**
 * SessionDescription
 */

bool SessionDescription::HasAttribute(const std::string& key) {
  return attributes_.count(key);
}

const std::string& SessionDescription::GetAttribute(const std::string& key) {
  static std::string g_empty_str;
  auto it = attributes_.find(key);
  return it != attributes_.end() ? it->second : g_empty_str;
}

void SessionDescription::SetAttribute(const std::string& key, const std::string& value) {
  attributes_[key] = value;
}

std::string SessionDescription::ToString() const {
  std::ostringstream oss;

  // version
  oss << "v=" << Version << kSdpEndOfLine;

  // origin
  oss << "o=" << (UserName.empty() ? kSdpPlaceholder : UserName)
    << ' ' << (SessionId.empty() ? "0" : SessionId)
    << ' ' << (SessionVersion.empty() ? "0" : SessionVersion)
    << (AddrType == SdpAddrType::kIPv4 ? " IN IP4 127.0.0.1" : " IN IP6 ::1")
    << kSdpEndOfLine;

  // session name
  oss << "s=" << (SessionName.empty() ? kSdpPlaceholder : SessionName) << kSdpEndOfLine;

  // time
  oss << "t=0 0" << kSdpEndOfLine;

  // session info
  if (!SessionInfo.empty()) oss << "i=" << SessionInfo << kSdpEndOfLine;

  // group:BUNDLE
  oss << "a=group:BUNDLE";
  for (auto& mid : GroupBundle) {
    oss << ' ' << mid;
  }
  oss << kSdpEndOfLine;

  // media stream id
  oss << "a=msid-semantic: WMS " << MediaStreamId << kSdpEndOfLine;

  // attributes
  for (auto& attr : attributes_) {
    oss << "a=" << attr.first;
    if (!attr.second.empty()) oss << ':' << attr.second;
    oss << kSdpEndOfLine;
  }

  // media
  if (GroupBundle.empty()) {
    for (auto& media : Medias) {
      oss << media.second->ToString();
    }
  } else {
    for (auto& mid : GroupBundle) {
      auto it = Medias.find(mid);
      if (it != Medias.end()) {
        oss << it->second->ToString();
      }
    }

    std::set<std::string> used_mid(GroupBundle.begin(), GroupBundle.end());
    for (auto& media : Medias) {
      if (used_mid.count(media.first) == 0) {
        oss << media.second->ToString();
      }
    }
  }

  return oss.str();
}

}  // namespace mini_sdp
