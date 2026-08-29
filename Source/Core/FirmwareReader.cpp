#include "FirmwareReader.h"
#include <eigenapi.h>
#include <cstring>
#include <stdexcept>

namespace ecm {

bool FirmwareReader::open(const std::string filename, int oFlags, void** fd) {
    if (filename == PICO_FIRMWARE) {
        bytesRead[0] = 0;
        *fd = (void*)0;
    } else if (filename == BASESTATION_FIRMWARE) {
        bytesRead[1] = 0;
        *fd = (void*)1;
    } else if (filename == PSU_FIRMWARE) {
        bytesRead[2] = 0;
        *fd = (void*)2;
    } else {
        return false;
    }
    
    return true;
}

ssize_t FirmwareReader::read(void* fd, void* data, size_t byteCount) {
    std::intptr_t fileIndex = reinterpret_cast<std::intptr_t>(fd);
    if (fileIndex < 0 || fileIndex >= IHX_FILE_COUNT || bytesRead[fileIndex] < 0) {
        throw std::runtime_error("Invalid fd or read() without first calling open()");
    }
    
    if (bytesRead[fileIndex] >= sizes[fileIndex]) {
        std::memset(data, 0, byteCount);
        return 0;
    }

    const char* curPos = bytes[fileIndex] + bytesRead[fileIndex];
    size_t actualBytes = std::min(byteCount, static_cast<size_t>(sizes[fileIndex] - bytesRead[fileIndex]));
    
    std::memcpy(data, curPos, actualBytes);
    bytesRead[fileIndex] += static_cast<int>(actualBytes);
    
    // If requested more than available, zero out the rest
    if (actualBytes < byteCount) {
        std::memset(static_cast<char*>(data) + actualBytes, 0, byteCount - actualBytes);
    }
    
    return static_cast<ssize_t>(actualBytes);
}

void FirmwareReader::close(void* fd) {
    std::intptr_t fileIndex = reinterpret_cast<std::intptr_t>(fd);
    if (fileIndex < 0 || fileIndex >= IHX_FILE_COUNT || bytesRead[fileIndex] < 0) {
        throw std::runtime_error("fd is not open or is invalid");
    }
    bytesRead[fileIndex] = -1;
}

void FirmwareReader::setPath(const std::string path) {
    // Path not used in this in-memory implementation
    juce::ignoreUnused(path);
}

bool FirmwareReader::confirmResources() {
    return bytes[0] != nullptr && sizes[0] > 0 && *bytes[0] == ':' &&
           bytes[1] != nullptr && sizes[1] > 0 && *bytes[1] == ':' &&
           bytes[2] != nullptr && sizes[2] > 0 && *bytes[2] == ':';
}

} // namespace ecm
