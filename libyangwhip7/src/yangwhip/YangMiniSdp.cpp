#include "YangMiniSdp.h"

#include <string>
#include <vector>
#include <cerrno>
#include <cstring>
#include <cctype>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <yangutil/sys/YangCUrl.h>
#include <yangutil/yangmemory.h>
#include <yangutil/sys/YangLog.h>

#include "mini_sdp.h"
#include "util.h"

// Helper: format webrtc URL for minisdp stream_url
static std::string format_webrtc_url(const YangUrlData& url){
    char buf[256];
    yang_memset(buf,0,sizeof(buf));
    yang_trace("format_webrtc_url: host=%s, server=%s, port=%d, app=%s, stream=%s", url.host, url.server, url.port, url.app, url.stream);

    yang_snprintf(buf,sizeof(buf)-1,"webrtc://%s:%d/%s",url.host,url.port,url.stream);
    
    return std::string(buf);
}

// Infer stream direction from offer SDP media direction attributes.
// If any audio/video m-line is sendonly => push; else if any is recvonly => pull; else default.
static mini_sdp::StreamDirection infer_stream_direction(const std::string& sdp_raw){
    // Make a lowercase copy and strip CR for easier searching
    std::string sdp; sdp.reserve(sdp_raw.size());
    for(char c : sdp_raw){ if(c!='\r') sdp.push_back((char)std::tolower((unsigned char)c)); }

    bool has_sendonly = false;
    bool has_recvonly = false;

    size_t pos = 0;
    while(true){
        size_t mpos = sdp.find("\nm=", pos);
        if(mpos == std::string::npos){
            if(pos==0 && sdp.rfind("m=", 0)==0) mpos = 0; else break;
        }
        // Section range: [mpos, next_mpos)
        size_t next = sdp.find("\nm=", mpos+1);
        std::string section = sdp.substr(mpos, (next==std::string::npos? sdp.size(): next) - mpos);

        // Only consider audio/video sections
        if(section.rfind("\nm=audio", 0)==0 || section.rfind("m=audio", 0)==0 ||
           section.rfind("\nm=video", 0)==0 || section.rfind("m=video", 0)==0){
            if(section.find("\na=sendonly") != std::string::npos) has_sendonly = true;
            if(section.find("\na=recvonly") != std::string::npos) has_recvonly = true;
        }

        if(next == std::string::npos) break;
        pos = next;
    }

    if(has_sendonly) return mini_sdp::kStreamPush;
    if(has_recvonly) return mini_sdp::kStreamPull;
    return mini_sdp::kStreamDefault;
}

// Defensive fix: if answer SDP has an audio m-line without payload types,
// append a default opus PT=111 and minimal rtpmap/fmtp so our SDP parser accepts it.
static void patch_audio_payload_if_missing(std::string& sdp){
    // Find m=audio line
    size_t pos = 0;
    while(true){
        size_t lineStart = sdp.find("\nm=audio ", pos);
        if(lineStart == std::string::npos){
            // also check if SDP starts with m=audio
            if(pos==0 && sdp.rfind("m=audio ", 0)==0) lineStart = 0; else break;
        }
        size_t mStart = (lineStart==0?0:lineStart+1); // skip leading \n
        size_t lineEnd = sdp.find('\n', mStart);
        if(lineEnd == std::string::npos) lineEnd = sdp.size();
        std::string mline = sdp.substr(mStart, lineEnd - mStart);

        // Tokenize by space to check fields
        int fields = 0; bool has_token=false;
        for(size_t i=0;i<mline.size();++i){ if(mline[i]==' ') { if(has_token){ fields++; has_token=false; } } else { has_token=true; } }
        if(has_token) fields++;

        // Expect at least 4 tokens: m=audio, port, proto, <fmt...>
        if(fields < 4){
            // Append default payload 111
            std::string patched = mline + " 111";
            sdp.replace(mStart, mline.size(), patched);

            // Insert rtpmap/fmtp right after this m-line
            std::string insertLines = "\na=rtpmap:111 opus/48000/2\n" "a=fmtp:111 minptime=10\n";
            sdp.insert(mStart + patched.size(), insertLines);
            yang_trace("[minisdp] patched audio m-line: added opus/111 payload and rtpmap/fmtp");
        } else {
            // If fields >=4 but audio has no explicit PT (rare), do nothing
        }

        // Move past this m-section to avoid infinite loop
        pos = lineEnd + 1;
    }
}

extern "C" int32_t yang_minisdp_getSignal(const char* offerSdp,const char* url,char** premoteSdp){
    if(!offerSdp || !url || !premoteSdp) return -1;

    YangUrlData urlData; yang_memset(&urlData,0,sizeof(urlData));
    if(yang_http_url_parse(Yang_IpFamilyType_IPV4,(char*)url,&urlData)!=0){
        // try generic webrtc url parser
        yang_memset(&urlData,0,sizeof(urlData));
        if(yang_url_parse(Yang_IpFamilyType_IPV4,(char*)url,&urlData)!=0){
            yang_error("minisdp: url parse failed: %s", url);
            return -2;
        }
    }

    mini_sdp::OriginSdpAttr attr;
    attr.sdp_type = mini_sdp::SdpType::kOffer;
    // ensure CRLF line ends per minisdp examples; if offerSdp already has \r\n, keep as-is
    std::string sdp_str(offerSdp);
    if(sdp_str.find("\r\n")==std::string::npos){
        // replace \n with \r\n
        std::string out; out.reserve(sdp_str.size()*2);
        for(size_t i=0;i<sdp_str.size();++i){
            if(sdp_str[i]=='\n') out += "\r\n"; else out += sdp_str[i];
        }
        attr.origin_sdp.swap(out);
    }else{
        attr.origin_sdp.swap(sdp_str);
    }
    attr.stream_url = format_webrtc_url(urlData);
    static uint16_t g_seq = 0; attr.seq = g_seq++;

    // Set is_push based on offer SDP direction
    attr.is_push = infer_stream_direction(attr.origin_sdp);
    yang_trace("[minisdp] inferred is_push=%s from SDP direction", 
               (attr.is_push==mini_sdp::kStreamPush?"push":(attr.is_push==mini_sdp::kStreamPull?"pull":"default")));

    char sendBuf[1400];
    ssize_t slen = mini_sdp::ParseOriginSdpToMiniSdp(attr, sendBuf, sizeof(sendBuf));
    if(slen <= 0){
        yang_error("minisdp: build request failed: %ld", (long)slen);
        return -3;
    }

    // Debug: print full offer SDP and constructed mini_sdp size
    yang_trace("\n[minisdp] offer SDP (len=%d):\n%s", (int)attr.origin_sdp.size(), attr.origin_sdp.c_str());
    yang_trace("[minisdp] stream_url=%s, seq=%u, mini_sdp bytes=%ld", attr.stream_url.c_str(), (unsigned)attr.seq, (long)slen);

    // UDP socket send
    int sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0){
        yang_error("minisdp: socket error: %d", errno);
        return -4;
    }

    // timeout 1500ms
    struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 500*1000;
    ::setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in servaddr; ::memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(urlData.port>0?urlData.port:8000);

    uint32_t ipui; if(mini_sdp::str2ipv4(urlData.server, &ipui)!=0){
        // fallback to inet_addr
        servaddr.sin_addr.s_addr = ::inet_addr(urlData.server);
    }else{
        servaddr.sin_addr.s_addr = ipui;
    }

    ssize_t sent = ::sendto(sockfd, sendBuf, (size_t)slen, 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if(sent != slen){
        yang_error("minisdp: sendto failed: %d", errno);
        ::close(sockfd);
        return -5;
    }

    char rspBuf[1400];
    socklen_t len = sizeof(servaddr);
    int n = ::recvfrom(sockfd, rspBuf, sizeof(rspBuf), 0, (struct sockaddr*)&servaddr, &len);
    if(n <= 0){
        yang_error("minisdp: recvfrom timeout or error: %d", errno);
        ::close(sockfd);
        return -6;
    }
    ::close(sockfd);

    // Debug: print raw UDP response bytes (first 256 bytes as hex)
    {
        static const char* hex = "0123456789ABCDEF";
        char hexbuf[256*3+1]; int k=0; int max=(n>256?256:n);
        for(int i=0;i<max;i++){
            unsigned char b = (unsigned char)rspBuf[i];
            hexbuf[k++] = hex[(b>>4)&0xF];
            hexbuf[k++] = hex[b&0xF];
            hexbuf[k++] = ' ';
        }
        hexbuf[k]=0;
        yang_trace("[minisdp] UDP response %d bytes (first %d as hex):\n%s", n, (n>256?256:n), hexbuf);
    }

    mini_sdp::OriginSdpAttr rattr;
    ssize_t r = mini_sdp::LoadMiniSdpToOriginSdp(rspBuf, (size_t)n, rattr);
    if(r <= 0){
        yang_error("minisdp: parse response failed: %ld", (long)r);
        return -7;
    }
    // convert CRLF to LF as yang stack prefers '\n'
    std::string answer = rattr.origin_sdp;
    std::string lf; lf.reserve(answer.size());
    for(size_t i=0;i<answer.size();++i){
        if(answer[i]=='\r') continue; // drop CR
        lf.push_back(answer[i]);
    }

    // Patch if audio m-line lacks payload types
    patch_audio_payload_if_missing(lf);

    size_t outLen = lf.size();
    char* out = (char*)yang_calloc(outLen+1,1);
    yang_memcpy(out, lf.c_str(), outLen);
    *premoteSdp = out;

    // Debug: print full parsed answer SDP
    yang_trace("\n[minisdp] answer SDP (len=%d):\n%s", (int)lf.size(), lf.c_str());
    return 0;
}
