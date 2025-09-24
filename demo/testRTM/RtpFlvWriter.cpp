#include "RtpFlvWriter.h"
#include <string.h>

RtpFlvWriter::RtpFlvWriter():m_fp(NULL),m_hasVideo(false),m_headerWritten(false){}
RtpFlvWriter::~RtpFlvWriter(){ close(); }

bool RtpFlvWriter::open(const std::string& path,bool writeVideo,bool writeAudio){
    (void)writeAudio; // Opus not packed into FLV here
    m_fp=fopen(path.c_str(),"wb");
    if(!m_fp) return false;
    m_hasVideo=writeVideo;
    writeFlvHeader();
    return true;
}
void RtpFlvWriter::close(){ if(m_fp){ fclose(m_fp); m_fp=NULL; } }

void RtpFlvWriter::writeFlvHeader(){
    if(!m_fp||m_headerWritten) return; m_headerWritten=true;
    uint8_t hdr[9]; memcpy(hdr,"FLV",3); hdr[3]=0x01; hdr[4]=(m_hasVideo?0x01:0x00);
    hdr[5]=0;hdr[6]=0;hdr[7]=0;hdr[8]=9; fwrite(hdr,1,9,m_fp);
    uint32_t zero=0; fwrite(&zero,1,4,m_fp);
}

void RtpFlvWriter::writeTag(uint8_t tagType,const uint8_t* payload,int size,uint32_t ts){
    if(!m_fp) return; uint8_t hdr[11]; hdr[0]=tagType; hdr[1]=(size>>16)&0xFF; hdr[2]=(size>>8)&0xFF; hdr[3]=size&0xFF; hdr[4]=(ts>>16)&0xFF; hdr[5]=(ts>>8)&0xFF; hdr[6]=ts&0xFF; hdr[7]=(ts>>24)&0xFF; hdr[8]=0; hdr[9]=0; hdr[10]=0; fwrite(hdr,1,11,m_fp); fwrite(payload,1,size,m_fp); uint32_t prev=11+size; uint8_t ps[4]={ (uint8_t)(prev>>24),(uint8_t)(prev>>16),(uint8_t)(prev>>8),(uint8_t)prev}; fwrite(ps,1,4,m_fp);
}

void RtpFlvWriter::writeH264AvcTag(const uint8_t* data,int len,uint32_t tsMs){
    if(!m_fp||len<=0) return; writeTag(0x09,data,len,tsMs);
}
