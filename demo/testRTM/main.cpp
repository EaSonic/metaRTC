#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <yangrtc/YangWhip.h>
#include <yangrtc/YangPeerConnection.h>
#include <yangutil/sys/YangLog.h>
#include <yangutil/sys/YangTime.h>
#include <yangutil/yangavinfo.h>
#include <yangutil/yangavinfotype.h>
#include "RtpFlvWriter.h"

static FILE* g_videoAnnexB=NULL;
static FILE* g_audioRaw=NULL;
static RtpFlvWriter g_flv;
static int g_writeFlv=0;

static void receiveAudio(void* user,YangFrame* frame){
    (void)user; if(!frame||!frame->payload||frame->nb<=0) return; if(g_audioRaw){ uint16_t sz=frame->nb; fwrite(&sz,1,2,g_audioRaw); fwrite(frame->payload,1,frame->nb,g_audioRaw); }
}
static void receiveVideo(void* user,YangFrame* frame){
    (void)user; if(!frame||!frame->payload||frame->nb<=0) return; if(g_videoAnnexB){ fwrite(frame->payload,1,frame->nb,g_videoAnnexB); } if(g_writeFlv){ g_flv.writeH264AvcTag(frame->payload,frame->nb,(uint32_t)(frame->pts/90)); }
}
static void receiveMsg(void* user,YangFrame* f){ (void)user;(void)f; }
static int sendRtcMessage(void* user,int puid,YangRtcMessageType t){ (void)user;(void)puid;(void)t; return Yang_Ok; }

int main(int argc,char* argv[]){
    if(argc<3){ printf("Usage: %s <webrtc_url> <out_dir> [--flv]\n",argv[0]); return 0; }
    std::string url=argv[1]; std::string outdir=argv[2]; if(argc>3 && strcmp(argv[3],"--flv")==0) g_writeFlv=1;
    std::string vfile=outdir+"/video.h264"; std::string afile=outdir+"/audio.opus"; std::string flvfile=outdir+"/out.flv";
    g_videoAnnexB=fopen(vfile.c_str(),"wb"); g_audioRaw=fopen(afile.c_str(),"wb"); if(g_writeFlv){ g_flv.open(flvfile, true, false);} 
    YangPeerConnection pc; memset(&pc,0,sizeof(pc));
    YangContext yctx; yctx.init(); // initializes avinfo etc.
    yctx.avinfo.sys.mediaServer=Yang_Server_Whip_Whep;
    pc.peer.peerInfo.rtc.rtcLocalPort=yctx.avinfo.rtc.rtcLocalPort++;
    pc.peer.peerInfo.direction=YangRecvonly; pc.peer.peerInfo.uid=1; pc.peer.peerInfo.familyType=Yang_IpFamilyType_IPV4;
    pc.peer.peerCallback.recvCallback.context=NULL; pc.peer.peerCallback.recvCallback.receiveAudio=receiveAudio; pc.peer.peerCallback.recvCallback.receiveVideo=receiveVideo; pc.peer.peerCallback.recvCallback.receiveMsg=receiveMsg;
    yctx.sendRtcMessage.context=NULL; yctx.sendRtcMessage.sendRtcMessage=sendRtcMessage;
    memcpy(&pc.peer.peerCallback.rtcCallback,&yctx.rtcCallback,sizeof(YangRtcCallback));
    yang_create_peerConnection(&pc);
    pc.addAudioTrack(&pc.peer,(YangAudioCodec)yctx.avinfo.audio.audioDecoderType);
    pc.addVideoTrack(&pc.peer,Yang_VED_H264);
    pc.addTransceiver(&pc.peer,YangMediaAudio,YangRecvonly);
    pc.addTransceiver(&pc.peer,YangMediaVideo,YangRecvonly);
    // enable RTP dump via internal rtc context (first video track)
    if(pc.peer.peerCallback.rtcCallback.sendRequest){ /* placeholder to avoid unused warning */ }
    // direct enable: locate underlying rtc context if exposed (not available here) so rely on global flag already added elsewhere if any.
    if(yang_whip_connectWhipWhepServer(&pc.peer,(char*)url.c_str(),1)!=Yang_Ok){ fprintf(stderr,"Connect failed\n"); }
    printf("Receiving... press Ctrl+C to stop. Files in %s\n", outdir.c_str());
    while(1){ yang_usleep(1000*100); }
    return 0;
}
