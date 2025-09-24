#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string>

class RtpFlvWriter {
public:
    RtpFlvWriter();
    ~RtpFlvWriter();
    bool open(const std::string& path, bool writeVideo, bool writeAudio);
    void close();
    void writeH264AvcTag(const uint8_t* data,int len,uint32_t tsMs);
private:
    FILE* m_fp;
    bool m_hasVideo;
    bool m_headerWritten;
    void writeFlvHeader();
    void writeTag(uint8_t tagType,const uint8_t* payload,int size,uint32_t ts);
};
