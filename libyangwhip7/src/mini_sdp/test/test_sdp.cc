/**
 * @file test/test_sdp.cc
 * @brief 
 * @version 0.1
 * @date 2021-01-14
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#include <iostream>
#include "mini_sdp/mini_sdp.h"

using namespace std;
using namespace mini_sdp;

constexpr size_t kUdpPacketMaxSize = 1400;

SdpType sdp_type = SdpType::kAnswer;
static char url[] = "webrtc://domain/live/stream-id";
static char origin_sdp[] =
  "v=0\r\n"
  "o=- 1 0 IN IP4 127.0.0.1\r\n"
  "s=webrtc_core\r\n"
  "t=0 0\r\n"
  "a=group:BUNDLE 0 1\r\n"
  "a=msid-semantic: WMS stream-id\r\n"
  "m=audio 1 UDP/TLS/RTP/SAVPF 111 112 124 125\r\n"
  "c=IN IP4 0.0.0.0\r\n"
  "a=rtcp:1 IN IP4 0.0.0.0\r\n"
  "a=candidate:foundation 1 udp 100 127.0.0.1 8000 typ srflx raddr 127.0.0.1 rport 8000 generation 0\r\n"
  "a=candidate:foundation 1 udp 100 127.0.0.1 443 typ srflx raddr 127.0.0.1 rport 443 generation 0\r\n"
  "a=ice-ufrag:de71a64097d807c3\r\n"
  "a=ice-pwd:be8577c0a03b0d3ffa4e5235\r\n"
  "a=fingerprint:sha-256 "
    "8A:BD:A6:61:75:AF:31:4C:02:81:2A:FA:12:92:4C:48:7B:9F:23:DD:BF:3D:51:30:E7:59:5C:9B:17:3D:92:34\r\n"
  "a=setup:passive\r\n"
  "a=sendrecv\r\n"
  "a=extmap:9 http://www.webrtc.org/experiments/rtp-hdrext/decoding-timestamp\r\n"
  "a=extmap:10 uri:webrtc:rtc:rtp-hdrext:video:CompositionTime\r\n"
  "a=extmap:21 http://www.webrtc.org/experiments/rtp-hdrext/meta-data-01\r\n"
  "a=extmap:22 http://www.webrtc.org/experiments/rtp-hdrext/meta-data-02\r\n"
  "a=extmap:23 http://www.webrtc.org/experiments/rtp-hdrext/meta-data-03\r\n"
  "a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
  "a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
  "a=mid:0\r\n"
  "a=rtcp-mux\r\n"
  "a=rtpmap:111 MP4A-ADTS/44100/2\r\n"
  "a=rtcp-fb:111 nack\r\n"
  "a=rtcp-fb:111 transport-cc\r\n"
  "a=fmtp:111 PS-enabled=0;SBR-enabled=1;config=40005724101fe0;cpresent=0;object=2;profile-level-id=1;stereo=1\r\n"
  "a=rtpmap:112 MP4A-ADTS/48000/2\r\n"
  "a=rtcp-fb:112 nack\r\n"
  "a=rtcp-fb:112 transport-cc\r\n"
  "a=fmtp:112 PS-enabled=0;SBR-enabled=1;config=40005724101fe0;cpresent=0;object=2;profile-level-id=1;stereo=1\r\n"
  "a=rtpmap:124 flexfec-03/48000/2\r\n"
  "a=rtpmap:125 flexfec-03/44100/2\r\n"
  "a=ssrc-group:FEC-FR 27172315 50331648\r\n"
  "a=ssrc:27172315 cname:webrtccore\r\n"
  "a=ssrc:27172315 msid:0_xxxx_d71956d9cc93e4a467b11e06fdaf039a opus\r\n"
  "a=ssrc:27172315 mslabel:0_xxxx_d71956d9cc93e4a467b11e06fdaf039a\r\n"
  "a=ssrc:27172315 label:opus\r\n"
  "a=ssrc:50331648 cname:webrtccore\r\n"
  "a=ssrc:50331648 msid:0_xxxx_d71956d9cc93e4a467b11e06fdaf039a opus\r\n"
  "a=ssrc:50331648 mslabel:0_xxxx_d71956d9cc93e4a467b11e06fdaf039a\r\n"
  "a=ssrc:50331648 label:opus\r\n"
  "m=video 1 UDP/TLS/RTP/SAVPF 102 108\r\n"
  "c=IN IP4 0.0.0.0\r\n"
  "a=rtcp:1 IN IP4 0.0.0.0\r\n"
  "a=candidate:foundation 1 udp 100 127.0.0.1 8000 typ srflx raddr 127.0.0.1 rport 8000 generation 0\r\n"
  "a=candidate:foundation 1 udp 100 127.0.0.1 443 typ srflx raddr 127.0.0.1 rport 443 generation 0\r\n"
  "a=ice-ufrag:de71a64097d807c3\r\n"
  "a=ice-pwd:be8577c0a03b0d3ffa4e5235\r\n"
  "a=extmap:9 http://www.webrtc.org/experiments/rtp-hdrext/decoding-timestamp\r\n"
  "a=extmap:10 http://www.webrtc.org/experiments/rtp-hdrext/video-composition-time\r\n"
  "a=extmap:21 http://www.webrtc.org/experiments/rtp-hdrext/meta-data-01\r\n"
  "a=extmap:22 http://www.webrtc.org/experiments/rtp-hdrext/meta-data-02\r\n"
  "a=extmap:23 http://www.webrtc.org/experiments/rtp-hdrext/meta-data-03\r\n"
  "a=extmap:14 urn:ietf:params:rtp-hdrext:toffset\r\n"
  "a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
  "a=extmap:13 urn:3gpp:video-orientation\r\n"
  "a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
  "a=extmap:12 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\r\n"
  "a=extmap:30 http://www.webrtc.org/experiments/rtp-hdrext/video-frame-type\r\n"
  "a=extmap:31 uri:webrtc:rtc:rtp-hdrext:video:frame-seq-range\r\n"
  "a=extmap:32 uri:webrtc:rtc:rtp-hdrext:video:frame-type\r\n"
  "a=fingerprint:sha-256 "
    "8A:BD:A6:61:75:AF:31:4C:02:81:2A:FA:12:92:4C:48:7B:9F:23:DD:BF:3D:51:30:E7:59:5C:9B:17:3D:92:34\r\n"
  "a=setup:passive\r\n"
  "a=sendrecv\r\n"
  "a=mid:1\r\n"
  "a=rtcp-mux\r\n"
  "a=rtcp-rsize\r\n"
  "a=rtpmap:102 H264/90000\r\n"
  "a=rtcp-fb:102 ccm fir\r\n"
  "a=rtcp-fb:102 goog-remb\r\n"
  "a=rtcp-fb:102 nack\r\n"
  "a=rtcp-fb:102 nack pli\r\n"
  "a=rtcp-fb:102 transport-cc\r\n"
  "a=rtcp-fb:102 rrtr\r\n"
  "a=fmtp:102 bframe-enabled=1;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f\r\n"
  "a=rtpmap:108 H264/90000\r\n"
  "a=rtcp-fb:108 ccm fir\r\n"
  "a=rtcp-fb:108 goog-remb\r\n"
  "a=rtcp-fb:108 nack\r\n"
  "a=rtcp-fb:108 nack pli\r\n"
  "a=rtcp-fb:108 transport-cc\r\n"
  "a=rtcp-fb:108 rrtr\r\n"
  "a=fmtp:108 BFrame-enabled=1;level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f\r\n"
  "a=ssrc:10395099 cname:webrtccore\r\n"
  "a=ssrc:10395099 msid:0_xxxx_d71956d9cc93e4a467b11e06fdaf039a h264\r\n"
  "a=ssrc:10395099 mslabel:0_xxxx_d71956d9cc93e4a467b11e06fdaf039a\r\n"
  "a=ssrc:10395099 label:h264\r\n"
  "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n"
  "c=IN IP4 0.0.0.0\r\n"
  "a=ice-ufrag:de71a64097d807c3\r\n"
  "a=ice-pwd:be8577c0a03b0d3ffa4e5235\r\n"
  "a=fingerprint:sha-256 98:77:E6:58:05:AF:BD:E5:CF:D4:0C:F1:51:0A:B5:33:D2:B9:A9:3D:93:A2:DC:FD:CA:95:D5:3F:A3:F3:A8:EA\r\n"
  "a=setup:actpass\r\n"
  "a=mid:2\r\n"
  "a=sctp-port:5000\r\n"
  "a=max-message-size:262144\r\n";

int main() {
  OriginSdpAttr attr;
  attr.origin_sdp = origin_sdp;
  attr.sdp_type = sdp_type;
  attr.stream_url = url;
  attr.seq = 0;
  attr.status_code = 101;
  attr.svrsig = "1h8s";
  attr.is_imm_send = true;
  attr.is_support_aac_fmtp = true;
  attr.is_push = kStreamPush;
  attr.is_crc = true;
  attr.netset = kNetWifi | kNetCellular;
  attr.stun_type = StunType::kStunResp;
  attr.xor_code_key = XorCodeType::kXorCodeRandom;

  char buffer[kUdpPacketMaxSize];
  int ret = ParseOriginSdpToMiniSdp(attr, buffer, kUdpPacketMaxSize);
  cout << "parse origin sdp to minisdp ret: " << ret << endl;

  OriginSdpAttr attr2;
  attr2.is_crc = true;
  ret = LoadMiniSdpToOriginSdp(buffer, static_cast<size_t>(ret), attr2);
  cout << "load minisdp to origin sdp ret: " << ret << endl;
  cout << "loaded tyep: " << (uint32_t)attr2.sdp_type << endl;
  cout << "loaded origin sdp: " << endl;
  cout << attr2.origin_sdp << endl;
  cout << "loaded url: " << attr2.stream_url << endl;
  cout << "loaded seq: " << attr2.seq << endl;
  cout << "loaded status code: " << attr2.status_code << endl;
  cout << "loaded svrsig: " << attr2.svrsig << endl;
  cout << "loaded is_push: " << attr2.is_push << endl;
  cout << "loaded net wifi: " << (attr2.netset & kNetWifi) << endl;
  cout << "loaded net cellular: " << (attr2.netset & kNetCellular) << endl;
  cout << "loaded agentid: " << attr2.agentid << endl;
  cout << "loaded xor_code_key: " << attr2.xor_code_key << endl;

  return 0;
}

