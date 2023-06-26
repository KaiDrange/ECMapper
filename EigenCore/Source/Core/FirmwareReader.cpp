#include "FirmwareReader.h"

bool FWR_InMem::open(const std::string filename, int oFlags, void* *fd)
{
    std::cout << "Attempting to open " << filename << std::endl;
    if (filename.compare(PICO_FIRMWARE) == 0)
    {
        bytesRead[0] = 0;
        *fd = (void*)0;
    }
    else if (filename.compare(BASESTATION_FIRMWARE) == 0)
    {
        bytesRead[1] = 0;
        *fd = (void*)1;
    }
    else if (filename.compare(PSU_FIRMWARE) == 0)
    {
        bytesRead[2] = 0;
        *fd = (void*)2;
    }
    else return false;
    
    return true;
}

ssize_t FWR_InMem::read(void* fd, void *data, size_t byteCount)
{
    std::cout << "Reading from InMem" << std::endl;
    std::intptr_t fileIndex = (std::intptr_t)fd;
    if (fileIndex < 0 || fileIndex > IHX_FILE_COUNT - 1 || bytesRead[fileIndex] < 0)
        throw std::runtime_error("Invalid fd or Read() without first calling Open()");
    
    if (bytesRead[fileIndex] >= sizes[fileIndex])
    {
        std::cout << "EOF!" << std::endl;
        for (int i = 0; i < byteCount; i++)
            ((char*)data)[i] = 0;
        return 0;
    }

    const char* curPos = bytes[fileIndex] + bytesRead[fileIndex];
    memcpy(data, curPos, byteCount);
    bytesRead[fileIndex] += byteCount;
    return byteCount;
}

void FWR_InMem::close(void* fd)
{
    std::intptr_t fileIndex = (std::intptr_t)fd;
    if (fileIndex < 0 || fileIndex > IHX_FILE_COUNT - 1 || bytesRead[fileIndex] < 0)
        throw std::runtime_error("fd is not open or is invalid");
    bytesRead[fileIndex] = -1;
    fd = nullptr;
}

bool FWR_InMem::confirmResources()
{
    return
        bytes[0] != nullptr && sizes[0] > 0 && *bytes[0] == ':' &&
        bytes[1] != nullptr && sizes[1] > 0 && *bytes[1] == ':' &&
        bytes[2] != nullptr && sizes[2] > 0 && *bytes[2] == ':';
}
