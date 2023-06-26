#pragma once
#include <JuceHeader.h>
#include "eigenapi_ec.h"
#include <sys/fcntl.h>
#include <unistd.h>

#define IHX_FILE_COUNT 3

class FWR_InMem : public EigenApi::IFW_Reader {
        
public:
    bool open(const std::string filename, int oFlags, void* *fd) override;
    ssize_t read(void* fd, void *data, size_t byteCount) override;
    void close(void* fd) override;
    void setPath(const std::string path);
    bool confirmResources() override;

private:
    int bytesRead[IHX_FILE_COUNT] = { -1, -1, -1 };
    const char* bytes[IHX_FILE_COUNT] = { BinaryData::pico_ihx,     BinaryData::bs_mm_fw_0103_ihx,     BinaryData::psu_mm_fw_0102_ihx };
    int sizes[IHX_FILE_COUNT] = {         BinaryData::pico_ihxSize, BinaryData::bs_mm_fw_0103_ihxSize, BinaryData::psu_mm_fw_0102_ihxSize };
};
