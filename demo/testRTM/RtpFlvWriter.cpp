#include "RtpFlvWriter.h"
#include <string.h>
#include <algorithm>

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

// Build AVCDecoderConfigurationRecord from SPS/PPS
static std::vector<uint8_t> build_avcc(const std::vector<uint8_t>& sps, const std::vector<uint8_t>& pps){
    std::vector<uint8_t> b;
    if(sps.size()<4) return b;
    // SPS bytes: profile_idc, constraint flags, level_idc at offsets 1..3 skipping NAL header
    uint8_t profile = sps.size()>1? sps[1]:0;
    uint8_t compat  = sps.size()>2? sps[2]:0;
    uint8_t level   = sps.size()>3? sps[3]:0;
    b.reserve(7 + 2 + (2+sps.size()) + 1 + (2+pps.size()));
    b.push_back(0x01);              // configurationVersion
    b.push_back(profile);           // AVCProfileIndication
    b.push_back(compat);            // profile_compatibility
    b.push_back(level);             // AVCLevelIndication
    b.push_back(0xFF);              // lengthSizeMinusOne: 3 => 4 bytes
    b.push_back(0xE1);              // numOfSequenceParameterSets: 1
    b.push_back((uint8_t)((sps.size()>>8)&0xFF)); b.push_back((uint8_t)(sps.size()&0xFF));
    b.insert(b.end(), sps.begin(), sps.end());
    b.push_back(0x01);              // numOfPictureParameterSets
    b.push_back((uint8_t)((pps.size()>>8)&0xFF)); b.push_back((uint8_t)(pps.size()&0xFF));
    b.insert(b.end(), pps.begin(), pps.end());
    return b;
}

static bool is_startcode3(const uint8_t* p){ return p[0]==0 && p[1]==0 && p[2]==1; }
static bool is_startcode4(const uint8_t* p){ return p[0]==0 && p[1]==0 && p[2]==0 && p[3]==1; }

bool RtpFlvWriter::writeH264AnnexBFileToFlv(const std::string& annexbPath){
    if(!m_fp) return false;
    // Read whole file
    FILE* fp = fopen(annexbPath.c_str(), "rb"); if(!fp) return false;
    fseek(fp,0,SEEK_END); long sz=ftell(fp); fseek(fp,0,SEEK_SET);
    std::vector<uint8_t> data; data.resize((size_t)sz);
    if(sz>0) fread(data.data(),1,(size_t)sz,fp); fclose(fp);
    if(data.empty()) return false;

    // Split NALs
    std::vector<std::vector<uint8_t>> nals;
    size_t i=0, n=data.size();
    auto find_sc = [&](size_t k)->size_t{
        while(k+3<n){ if(is_startcode3(&data[k]) || (k+4<n && is_startcode4(&data[k]))) return k; ++k; }
        return n;
    };
    while(true){
        size_t sc = find_sc(i); if(sc>=n) break;
        size_t sc_len = is_startcode3(&data[sc])?3:4;
        size_t nal_start = sc + sc_len;
        size_t next = find_sc(nal_start);
        size_t nal_end = (next<n)? next : n;
        if(nal_end>nal_start) nals.emplace_back(data.begin()+nal_start, data.begin()+nal_end);
        i = nal_end;
    }
    if(nals.empty()) return false;

    // Extract SPS/PPS (types 7 and 8)
    std::vector<uint8_t> sps, pps;
    for(const auto& nal : nals){ if(nal.empty()) continue; uint8_t t = nal[0] & 0x1F; if(t==7) sps=nal; else if(t==8) pps=nal; }
    if(sps.empty()||pps.empty()) {
        // try to proceed, but many players will fail without avcC
    }
    std::vector<uint8_t> avcc = build_avcc(sps,pps);
    if(!avcc.empty()) writeH264SequenceHeader(avcc, 0);

    // Group into frames: prefer AUD boundaries (NAL type 9). Skip AUD NALs in payload.
    struct Frame { std::vector<std::vector<uint8_t>> nals; bool key=false; };
    std::vector<Frame> frames;
    frames.reserve(256);
    auto flush_frame = [&](Frame& f){ if(!f.nals.empty()) { frames.push_back(std::move(f)); f = Frame(); } };
    bool has_aud=false; Frame cur;
    for(const auto& nal : nals){ if(nal.empty()) continue; uint8_t t = nal[0] & 0x1F; if(t==9){ has_aud=true; flush_frame(cur); continue; } if(t==7||t==8) continue; // SPS/PPS already in avcC
        if(t==5) cur.key = true; // IDR
        // keep SEI and slices
        cur.nals.push_back(nal);
    }
    flush_frame(cur);

    if(!has_aud){
        // Fallback: accumulate SEI (6) and emit with the next slice (1/5) as one frame.
        frames.clear(); Frame f;
        for(const auto& nal : nals){
            if(nal.empty()) continue; uint8_t t = nal[0] & 0x1F;
            if(t==7||t==8||t==9) continue; // skip SPS/PPS/AUD
            if(t==6){ f.nals.push_back(nal); continue; } // collect SEI
            if(t==1 || t==5){
                if(t==5) f.key = true;
                f.nals.push_back(nal);
                // finalize this frame (SEI + one slice)
                frames.push_back(std::move(f)); f = Frame();
                continue;
            }
            // other NAL types (e.g., filler, etc.) attach if we already have pending data
            if(!f.nals.empty()) f.nals.push_back(nal);
        }
        if(!f.nals.empty()) frames.push_back(std::move(f));
    }

    // Emit frames with synthetic timestamps.
    const uint32_t frameDurMs = 33; // ~30fps default
    uint32_t dts = 0;
    for(const auto& f : frames){
        // Build length-prefixed payload of this frame’s NALs
        std::vector<uint8_t> lp; lp.reserve(f.nals.size()*6);
        for(const auto& nal : f.nals){ uint32_t len=(uint32_t)nal.size(); lp.push_back((uint8_t)((len>>24)&0xFF)); lp.push_back((uint8_t)((len>>16)&0xFF)); lp.push_back((uint8_t)((len>>8)&0xFF)); lp.push_back((uint8_t)(len&0xFF)); lp.insert(lp.end(), nal.begin(), nal.end()); }
        writeH264NalusLenPref(lp, f.key, dts, 0);
        dts += frameDurMs;
    }
    return !frames.empty();
}

void RtpFlvWriter::writeH264SequenceHeader(const std::vector<uint8_t>& avcc, uint32_t tsMs){
    std::vector<uint8_t> payload;
    // FrameType=1 (key), CodecID=7 (AVC)
    payload.push_back((1<<4) | 7);
    payload.push_back(0x00); // AVCPacketType=0 (sequence header)
    payload.push_back(0); payload.push_back(0); payload.push_back(0); // CTS=0
    payload.insert(payload.end(), avcc.begin(), avcc.end());
    writeTag(0x09, payload.data(), (int)payload.size(), tsMs);
}

void RtpFlvWriter::writeH264NalusLenPref(const std::vector<uint8_t>& nalsLenPref, bool key, uint32_t dtsMs, int32_t ctsMs){
    std::vector<uint8_t> payload;
    uint8_t frameType = key? 1 : 2;
    payload.push_back((frameType<<4) | 7);
    payload.push_back(0x01); // NALU
    payload.push_back((uint8_t)((ctsMs>>16)&0xFF));
    payload.push_back((uint8_t)((ctsMs>>8)&0xFF));
    payload.push_back((uint8_t)(ctsMs&0xFF));
    payload.insert(payload.end(), nalsLenPref.begin(), nalsLenPref.end());
    writeTag(0x09, payload.data(), (int)payload.size(), dtsMs);
}
