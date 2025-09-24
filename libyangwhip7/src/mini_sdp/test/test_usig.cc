/**
 * @file test/test_usig.cc
 * @brief 
 * @version 0.1
 * @date 2021-05-25
 * 
 * @copyright Copyright (c) 2021 Tencent. All rights reserved.
 * 
 */
#include "mini_sdp/mini_sdp.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "mini_sdp/util.h"

using namespace std;
using namespace mini_sdp;

constexpr char kServerIp[] = "127.0.0.1";
constexpr uint16_t kServerPort = 8000;
constexpr size_t kUdpPacketMaxSize = 1400;

static uint64_t GetNowMs() {
  timeval tm;
  gettimeofday(&tm, NULL);
  uint64_t now_ms = tm.tv_sec * 1000 + tm.tv_usec / 1000;
  return now_ms;
}

int main(int argc, char** argv) {
  UdpSigAttrDetect attr;
  attr.token = "domain";
  attr.stime = (uint32_t)GetNowMs();
  attr.stun_type = StunType::kStunPing;
  cout << "attr.stime = " << attr.stime << endl;
  cout << "attr.token = " << attr.token << endl;

  char req_buff[kUdpPacketMaxSize];
  ssize_t ret = UdpSigEncode(req_buff, kUdpPacketMaxSize, attr);
  if (ret > 0) {
    cout << "encode success " << ret << endl;
  } else {
    cout << "encode failed " << ret << endl;
    return 0;
  }

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

  UdpSigAttrDetect rattr;
  ret = UdpSigDecode(rsp_buff, n, rattr);
  if (ret > 0) {
    cout << "decode success " << ret << endl;
  } else {
    cout << "decode failed " << ret << endl;
    return 0;
  }

  cout << "rattr.stime = " << rattr.stime << endl;
  cout << "rattr.token = " << rattr.token << endl;

  if (attr.stime == rattr.stime && attr.token == rattr.token) {
    cout << "verify success, time used: " << (uint32_t)GetNowMs() - rattr.stime << endl;
  } else {
    cout << "verify failed" << endl;
  }
}
