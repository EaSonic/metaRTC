#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

class RtpFlvWriter {
public:
    RtpFlvWriter();
    ~RtpFlvWriter();
    bool open(const std::string& path, bool writeVideo, bool writeAudio);
    void close();
    void writeH264AvcTag(const uint8_t* data,int len,uint32_t tsMs);
    // Parse an Annex B H.264 elementary stream file and write a valid FLV:
    // emits AVC sequence header (avcC) then a NALU tag with 4-byte length-prefixed NALs.
    // Returns true on success.
    bool writeH264AnnexBFileToFlv(const std::string& annexbPath);
private:
    FILE* m_fp;
    bool m_hasVideo;
    bool m_headerWritten;
    void writeFlvHeader();
    void writeTag(uint8_t tagType,const uint8_t* payload,int size,uint32_t ts);
    // Helpers for proper AVC-in-FLV
    void writeH264SequenceHeader(const std::vector<uint8_t>& avcc, uint32_t tsMs);
    void writeH264NalusLenPref(const std::vector<uint8_t>& nalsLenPref, bool key, uint32_t dtsMs, int32_t ctsMs);
};
