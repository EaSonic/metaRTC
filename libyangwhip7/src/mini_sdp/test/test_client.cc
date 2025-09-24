/**
 * @file test/test_client.cc
 * @brief Client side implementation of UDP client-server model 
 * @version 0.1
 * @date 2021-01-14
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "mini_sdp/mini_sdp.h"
#include "mini_sdp/util.h"

using namespace std;
using namespace mini_sdp;

constexpr char kServerIp[] = "172.24.12.72";
constexpr uint16_t kServerPort = 8000;
constexpr size_t kUdpPacketMaxSize = 1400;

SdpType sdp_type = SdpType::kOffer;
static char url[] = "webrtc://test.ctcdn.cn/path/streamid";
static char origin_sdp[] =
  "v=0\r\n"
  "o=- 0 0 IN IP4 127.0.0.1\r\n"
  "s=-\r\n"
  "t=0 0\r\n"
  "a=group:BUNDLE audio video\r\n"
  "a=msid-semantic: WMS \r\n"
  "m=audio 9 RTP/AVPF 116 117 105 118 108 119 120 121 122 123 124 125 126 106 127 107\r\n"
  "c=IN IP4 0.0.0.0\r\n"
  "a=rtcp:9 IN IP4 0.0.0.0\r\n"
  "a=ice-ufrag:v1BXqBjLmiygMO9n\r\n"
  "a=ice-pwd:rp3RNABeh1bGfpfHAjOqX7NB\r\n"
  "a=setup:actpass\r\n"
  "a=mid:audio\r\n"
  "a=recvonly\r\n"
  "a=rtcp-mux\r\n"
  "a=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
  "a=extmap:18 http://www.webrtc.org/experiments/rtp-hdrext/decoding-timestamp\r\n"
  "a=extmap:27 uri:webrtc:rtc:rtp-hdrext:audio:aac-config\r\n"
  "a=rtpmap:116 MP4A-LATM/48000\r\n"
  "a=rtcp-fb:116 nack\r\n"
  "a=rtcp-fb:116 rrtr\r\n"
  "a=rtcp-fb:116 transport-cc\r\n"
  "a=fmtp:116 PS-enabled=0;SBR-enabled=0;config=4001d6131056e29d4880;cpresent=0;object=29;stereo=0\r\n"
  "a=rtpmap:117 MP4A-LATM/44100/2\r\n"
  "a=rtcp-fb:117 nack\r\n"
  "a=rtcp-fb:117 rrtr\r\n"
  "a=rtcp-fb:117 transport-cc\r\n"
  "a=fmtp:117 PS-enabled=0;SBR-enabled=0;config=4000242000;cpresent=0;object=2;stereo=0\r\n"
  "a=rtpmap:105 rtx/44100/2\r\n"
  "a=fmtp:105 apt=117\r\n"
  "a=rtpmap:118 MP4A-LATM/48000/2\r\n"
  "a=rtcp-fb:118 nack\r\n"
  "a=rtcp-fb:118 rrtr\r\n"
  "a=rtcp-fb:118 transport-cc\r\n"
  "a=fmtp:118 PS-enabled=0;SBR-enabled=0;config=4000232000;cpresent=0;object=2;stereo=0\r\n"
  "a=rtpmap:108 rtx/48000/2\r\n"
  "a=fmtp:108 apt=118\r\n"
  "a=rtpmap:119 MP4A-LATM/44100\r\n"
  "a=rtcp-fb:119 nack\r\n"
  "a=rtcp-fb:119 rrtr\r\n"
  "a=rtcp-fb:119 transport-cc\r\n"
  "a=fmtp:119 PS-enabled=0;SBR-enabled=0;config=4000241000;cpresent=0;object=2;stereo=0\r\n"
  "a=rtpmap:120 MP4A-LATM/48000\r\n"
  "a=rtcp-fb:120 nack\r\n"
  "a=rtcp-fb:120 rrtr\r\n"
  "a=rtcp-fb:120 transport-cc\r\n"
  "a=fmtp:120 PS-enabled=0;SBR-enabled=0;config=4000231000;cpresent=0;object=2;stereo=0\r\n"
  "a=rtpmap:121 MP4A-LATM/44100\r\n"
  "a=rtcp-fb:121 nack\r\n"
  "a=rtcp-fb:121 rrtr\r\n"
  "a=rtcp-fb:121 transport-cc\r\n"
  "a=fmtp:121 PS-enabled=0;SBR-enabled=0;config=4001d7141056e2a54880;cpresent=0;object=29;stereo=0\r\n"
  "a=rtpmap:122 MP4A-LATM/48000\r\n"
  "a=rtcp-fb:122 nack\r\n"
  "a=rtcp-fb:122 rrtr\r\n"
  "a=rtcp-fb:122 transport-cc\r\n"
  "a=fmtp:122 PS-enabled=0;SBR-enabled=0;config=400056131056e29800;cpresent=0;object=5;stereo=0\r\n"
  "a=rtpmap:123 MP4A-LATM/44100\r\n"
  "a=rtcp-fb:123 nack\r\n"
  "a=rtcp-fb:123 rrtr\r\n"
  "a=rtcp-fb:123 transport-cc\r\n"
  "a=fmtp:123 PS-enabled=0;SBR-enabled=0;config=400057141056e2a000;cpresent=0;object=5;stereo=0\r\n"
  "a=rtpmap:124 MP4A-LATM/48000/2\r\n"
  "a=rtcp-fb:124 nack\r\n"
  "a=rtcp-fb:124 rrtr\r\n"
  "a=rtcp-fb:124 transport-cc\r\n"
  "a=fmtp:124 PS-enabled=0;SBR-enabled=0;config=4001d6131056e29d4880;cpresent=0;object=29;stereo=0\r\n"
  "a=rtpmap:125 MP4A-LATM/44100/2\r\n"
  "a=rtcp-fb:125 nack\r\n"
  "a=rtcp-fb:125 rrtr\r\n"
  "a=rtcp-fb:125 transport-cc\r\n"
  "a=fmtp:125 PS-enabled=0;SBR-enabled=0;config=4001d7141056e2a54880;cpresent=0;object=29;stereo=0\r\n"
  "a=rtpmap:126 MP4A-LATM/48000/2\r\n"
  "a=rtcp-fb:126 nack\r\n"
  "a=rtcp-fb:126 rrtr\r\n"
  "a=rtcp-fb:126 transport-cc\r\n"
  "a=fmtp:126 PS-enabled=0;SBR-enabled=0;config=400056231056e29800;cpresent=0;object=5;stereo=0\r\n"
  "a=rtpmap:106 rtx/48000/2\r\n"
  "a=fmtp:106 apt=126\r\n"
  "a=rtpmap:127 MP4A-LATM/44100/2\r\n"
  "a=rtcp-fb:127 nack\r\n"
  "a=rtcp-fb:127 rrtr\r\n"
  "a=rtcp-fb:127 transport-cc\r\n"
  "a=fmtp:127 PS-enabled=0;SBR-enabled=0;config=400057241056e2a000;cpresent=0;object=5;stereo=0\r\n"
  "a=rtpmap:107 rtx/44100/2\r\n"
  "a=fmtp:107 apt=127\r\n"
  "m=video 9 RTP/AVPF 96 97 98 99\r\n"
  "c=IN IP4 0.0.0.0\r\n"
  "a=rtcp:9 IN IP4 0.0.0.0\r\n"
  "a=ice-ufrag:v1BXqBjLmiygMO9n\r\n"
  "a=ice-pwd:rp3RNABeh1bGfpfHAjOqX7NB\r\n"
  "a=setup:actpass\r\n"
  "a=mid:video\r\n"
  "a=recvonly\r\n"
  "a=rtcp-mux\r\n"
  "a=rtcp-rsize\r\n"
  "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
  "a=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
  "a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\r\n"
  "a=extmap:18 http://www.webrtc.org/experiments/rtp-hdrext/decoding-timestamp\r\n"
  "a=extmap:19 uri:webrtc:rtc:rtp-hdrext:video:CompositionTime\r\n"
  "a=extmap:21 uri:webrtc:rtc:rtp-hdrext:video:frame-seq-range\r\n"
  "a=extmap:22 uri:webrtc:rtc:rtp-hdrext:video:frame-type\r\n"
  "a=extmap:23 uri:webrtc:rtc:rtp-hdrext:video:reference-frame-timestamp\r\n"
  "a=rtpmap:96 H264/90000\r\n"
  "a=rtcp-fb:96 goog-remb\r\n"
  "a=rtcp-fb:96 nack\r\n"
  "a=rtcp-fb:96 rrtr\r\n"
  "a=rtcp-fb:96 transport-cc\r\n"
  "a=fmtp:96 bframe-enabled=1;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\n"
  "a=rtpmap:97 rtx/90000\r\n"
  "a=fmtp:97 apt=96\r\n"
  "a=rtpmap:98 H265/90000\r\n"
  "a=rtcp-fb:98 goog-remb\r\n"
  "a=rtcp-fb:98 nack\r\n"
  "a=rtcp-fb:98 rrtr\r\n"
  "a=rtcp-fb:98 transport-cc\r\n"
  "a=fmtp:98 bframe-enabled=1;level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\n"
  "a=rtpmap:99 rtx/90000\r\n"
  "a=fmtp:99 apt=98\r\n";

static uint64_t GetNowMs() {
  timeval tm;
  gettimeofday(&tm, NULL);
  uint64_t now_ms = tm.tv_sec * 1000 + tm.tv_usec / 1000;
  return now_ms;
}

int main() {
  uint64_t start_time = GetNowMs();

  int sockfd;
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  uint32_t ipui;
  str2ipv4(kServerIp, &ipui);

  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(kServerPort);
  servaddr.sin_addr.s_addr = ipui;

  OriginSdpAttr attr;
  attr.origin_sdp = origin_sdp;
  attr.sdp_type = sdp_type;
  attr.stream_url = url;
  attr.seq = 0;
  attr.status_code = 0;
  attr.svrsig = "1h8s";

  char req_buff[kUdpPacketMaxSize];
  ssize_t ret = ParseOriginSdpToMiniSdp(attr, req_buff, kUdpPacketMaxSize);
  cout << "parse origin sdp to minisdp ret: " << ret << endl;

  int flags = 0;
#ifdef __linux__
  flags = MSG_CONFIRM;
#endif
  sendto(sockfd, req_buff, static_cast<size_t>(ret), flags,
    (const struct sockaddr *)&servaddr, sizeof(servaddr));

  char rsp_buff[kUdpPacketMaxSize];
  socklen_t len = sizeof(struct sockaddr_in);
  int n = recvfrom(sockfd, reinterpret_cast<char*>(rsp_buff), kUdpPacketMaxSize, 0,
                   (struct sockaddr *)&servaddr, &len);

  OriginSdpAttr rattr;
  ret = LoadMiniSdpToOriginSdp(rsp_buff, static_cast<size_t>(n), rattr);

  cout << "load minisdp to origin sdp ret: " << ret << endl;
  cout << "rsp url: " << rattr.stream_url << endl;
  cout << "rsp seq: " << rattr.seq << endl;
  cout << "rsp status code: " << rattr.status_code << endl;
  cout << "rsp svrsig: " << rattr.svrsig << endl;
  cout << "rsp origin sdp:" << endl;
  cout << "--- SDP Start ---" << endl;
  cout << rattr.origin_sdp << endl;
  cout << "--- SDP End --- " << endl;

  cout << endl;

  uint64_t end_time = GetNowMs();
  cout << "time used: " << end_time - start_time << " ms" << endl;

  close(sockfd);
  return 0;
}