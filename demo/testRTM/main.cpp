#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <sstream>
#include <yangrtc/YangWhip.h>
#include <yangrtc/YangPeerConnection.h>
#include <yangrtc/YangPeerInfo.h>
#include <yangutil/sys/YangLog.h>
#include <yangutil/sys/YangTime.h>
#include <yangutil/yangavinfo.h>
#include <yangutil/yangavinfotype.h>
#include "RtpFlvWriter.h"
#include <signal.h>
#include <sys/stat.h>
#include <vector>

static FILE* g_videoAnnexB=NULL;
static FILE* g_audioRaw=NULL;
static RtpFlvWriter g_flv;
static int g_writeFlv=1;
static volatile sig_atomic_t g_stop = 0;

static void on_sigint(int){ g_stop = 1; }

static void receiveAudio(void* user,YangFrame* frame){
    (void)user; if(!frame||!frame->payload||frame->nb<=0) return; if(g_audioRaw){ uint16_t sz=frame->nb; fwrite(&sz,1,2,g_audioRaw); fwrite(frame->payload,1,frame->nb,g_audioRaw); }
}
static void receiveVideo(void* user,YangFrame* frame){
    (void)user; if(!frame||!frame->payload||frame->nb<=0) return; if(g_videoAnnexB){ fwrite(frame->payload,1,frame->nb,g_videoAnnexB); } if(g_writeFlv){ g_flv.writeH264AvcTag(frame->payload,frame->nb,(uint32_t)(frame->pts/90)); }
}
static void receiveMsg(void* user,YangFrame* f){ (void)user;(void)f; }
static int sendRtcMessage(void* user,int puid,YangRtcMessageType t){ (void)user;(void)puid;(void)t; return Yang_Ok; }

int main(int argc,char* argv[]){
    // Defaults per requirement
    std::string url = "webrtc://test.pull:8000/live/test_stream.sdp?arch_hrchy=h1";
    std::string outdir = ".";
    std::string logFile = "./testRTM.log";
    int logLevel = 5; // default 5

    bool urlExplicit=false, logExplicit=false, lvlExplicit=false;
    std::vector<std::string> nonFlags;

    for(int i=1;i<argc;){
        const char* a = argv[i];
        if(strcmp(a,"--flv")==0){ g_writeFlv=1; ++i; continue; }
        if(strcmp(a,"--url")==0 && i+1<argc){ url = argv[i+1]; urlExplicit=true; i+=2; continue; }
        if(strcmp(a,"--log")==0 && i+1<argc){ logFile = argv[i+1]; logExplicit=true; i+=2; continue; }
        if(strcmp(a,"--log-level")==0 && i+1<argc){ logLevel = atoi(argv[i+1]); lvlExplicit=true; i+=2; continue; }
        if(a[0] != '-'){
            nonFlags.emplace_back(a);
        }
        ++i;
    }
    // Back-compat positional: first non-flag as url (if --url not used), second as outdir
    if(!urlExplicit && !nonFlags.empty()) url = nonFlags[0];
    if(nonFlags.size()>=2) outdir = nonFlags[1];

    // If FLV is requested, also enable EASON_DUMP_VIDEO for Annex B dump
    if(g_writeFlv){
        setenv("EASON_DUMP_VIDEO", "1", 1);
        const char* ev = getenv("EASON_DUMP_VIDEO");
        printf("EASON_DUMP_VIDEO=%s\n", ev ? ev : "");
    }

    // Setup logging (always enable file logging with chosen path/level)
    yang_setLogLevel(logLevel);
    yang_setLogFile2(1, (char*)logFile.c_str());
    std::string vfile=outdir+"/video.h264"; std::string afile=outdir+"/audio.opus"; std::string flvfile=outdir+"/out.flv";
    // Convert url if using webrtc:// to HTTP WHIP so minisdp and HTTP fallback both parse
    std::string httpUrl = url; // to_http_whip_if_webrtc(url); If conversion needed.
    // stderr summary of arguments
    fprintf(stderr,
            "[testRTM] args:\n  url=%s\n  out_dir=%s\n  --flv=%s\n  --log=%s\n  --log-level=%s\n",
            url.c_str(), outdir.c_str(), (g_writeFlv?"on":"off"),
            (logFile.empty()?"<none>":logFile.c_str()),
            (logLevel<0?"<default>":std::to_string(logLevel).c_str()));
    if(httpUrl != url){
        fprintf(stderr, "[testRTM] converted to WHIP url=%s\n", httpUrl.c_str());
        yang_trace((char*)"testRTM: converted url to %s", (char*)httpUrl.c_str());
    }
    // fprintf(stderr, "[testRTM] opening output files...\n");
    // g_videoAnnexB=fopen(vfile.c_str(),"wb");
    // g_audioRaw=fopen(afile.c_str(),"wb");
    // if(g_writeFlv){ g_flv.open(flvfile, true, false);} 
    // fprintf(stderr, "[testRTM] video=%s (%s), audio=%s (%s), flv=%s (%s)\n",
    //         vfile.c_str(), (g_videoAnnexB?"ok":"fail"),
    //         afile.c_str(), (g_audioRaw?"ok":"fail"),
    //         flvfile.c_str(), (g_writeFlv?"enabled":"disabled"));
    YangPeerConnection pc; memset(&pc,0,sizeof(pc));
    fprintf(stderr, "[testRTM] init av/peer...\n");
    // Minimal C-style initialization
        YangAVInfo avinfo; memset(&avinfo,0,sizeof(avinfo)); yang_init_avinfo(&avinfo);
        avinfo.sys.mediaServer=Yang_Server_Whip_Whep;
        // init peer info from avinfo
        yang_init_peerInfo(&pc.peer.peerInfo);
        pc.peer.peerInfo.direction=YangRecvonly;
        pc.peer.peerInfo.uid=1;
        pc.peer.peerInfo.familyType=Yang_IpFamilyType_IPV4;
        pc.peer.peerInfo.rtc.rtcLocalPort=avinfo.rtc.rtcLocalPort++;
        // callbacks
        pc.peer.peerCallback.recvCallback.context=NULL;
        pc.peer.peerCallback.recvCallback.receiveAudio=receiveAudio;
        pc.peer.peerCallback.recvCallback.receiveVideo=receiveVideo;
        pc.peer.peerCallback.recvCallback.receiveMsg=receiveMsg;
        pc.peer.peerCallback.rtcCallback.context=NULL;
        pc.peer.peerCallback.rtcCallback.setMediaConfig=NULL;
        pc.peer.peerCallback.rtcCallback.sendRequest=NULL;
        // sendRtcMessage not used
    fprintf(stderr, "[testRTM] create peer connection...\n");
    yang_create_peerConnection(&pc);
    fprintf(stderr, "[testRTM] add tracks/transceivers...\n");
    pc.addAudioTrack(&pc.peer,(YangAudioCodec)avinfo.audio.audioDecoderType);
    pc.addVideoTrack(&pc.peer,Yang_VED_H264);
    pc.addTransceiver(&pc.peer,YangMediaAudio,YangRecvonly);
    pc.addTransceiver(&pc.peer,YangMediaVideo,YangRecvonly);
        // enable internal rtp dump flag if symbol available via peer.conn->context
        if(pc.peer.conn){
            // best-effort: struct layout may expose context pointer
            // (left empty intentionally if not accessible)
        }
        fprintf(stderr, "[testRTM] connect (minisdp->http fallback) url=%s ...\n", httpUrl.c_str());
        if(yang_whip_connectWhipWhepServer(&pc.peer,(char*)httpUrl.c_str(),1)!=Yang_Ok){
            fprintf(stderr,"[testRTM] Connect failed.\n");
        }else{
            fprintf(stderr,"[testRTM] Connect ok. Receiving...\n");
        }
    // Install Ctrl+C handler
    struct sigaction sa; memset(&sa,0,sizeof(sa)); sa.sa_handler=on_sigint; sigemptyset(&sa.sa_mask); sa.sa_flags=0; sigaction(SIGINT,&sa,nullptr);

    printf("Receiving... press Ctrl+C to stop. Files in %s\n", outdir.c_str());
    while(!g_stop){ yang_usleep(1000*100); }

    fprintf(stderr, "[testRTM] stopping... destroy peer and close I/O...\n");
    // Stop peer (tracks destroyed inside)
    if (pc.close) pc.close(&pc.peer);
    yang_destroy_peerConnection(&pc);

    if (g_audioRaw) { fclose(g_audioRaw); g_audioRaw=NULL; }
    if (g_videoAnnexB) { fclose(g_videoAnnexB); g_videoAnnexB=NULL; }
    if (g_writeFlv) { g_flv.close(); }

    // Post-pack: if there is h264_dump_1.h264 or h265_dump_1.h265, convert to FLV for convenience
    auto file_exists = [](const std::string& p){ struct stat st{}; return ::stat(p.c_str(), &st)==0 && st.st_size>0; };
    std::string dump264 = "h264_dump_1.h264"; // uid=1 as we set above
    std::string dump265 = "h265_dump_1.h265";
    std::string postFlv = outdir+"/dump_post.flv";
    if (file_exists(dump264) || file_exists(dump265)) {
        fprintf(stderr, "[testRTM] post-pack dump to %s ...\n", postFlv.c_str());
        RtpFlvWriter post;
        if (post.open(postFlv, /*video*/true, /*audio*/false)) {
            // Only h264 supported here. If h265 dump exists, you can convert externally or extend writer similarly.
            bool ok=false;
            if (file_exists(dump264)) ok = post.writeH264AnnexBFileToFlv(dump264);
            // TODO: add writeH265AnnexBFileToFlv when needed
            if(!ok) fprintf(stderr, "[testRTM] post-pack failed to mux H264 dump into FLV\n");
            post.close();
        } else {
            fprintf(stderr, "[testRTM] post-pack open %s failed\n", postFlv.c_str());
        }
    }

    if(!logFile.empty()) yang_closeLogFile();
    return 0;
}
