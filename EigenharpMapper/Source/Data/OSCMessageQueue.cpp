#include "OSCMessageQueue.h"

using namespace OSC;

void OSCMessageFifo::add(const Message *message) {
    if (getMessageCount() >= queueSize-1) {
        std::cout << "OSCMessage buffer overflow!" << std::endl;
        return;
    }
        
    int start1, size1, start2, size2;
    fifo.prepareToWrite(MessageSize, start1, size1, start2, size2);
    if (size1 > 0) {
        memcpy(&buffer[start1], message, size1*sizeof(int));
    }
    if (size2 > 0) {
        memcpy(&buffer[start2], message + size1, size2*sizeof(int));
    }
    fifo.finishedWrite(size1 + size2);
}

void OSCMessageFifo::read(Message *message) {
    int start1, size1, start2, size2;
    fifo.prepareToRead(MessageSize, start1, size1, start2, size2);
    if (size1 > 0) {
        memcpy(message, &buffer[start1], size1*sizeof(int));
    }
    if (size2 > 0) {
        memcpy(message + size1, &buffer[start2], size2*sizeof(int));
    }
    fifo.finishedRead(size1 + size2);
}

int OSCMessageFifo::getMessageCount() const {
    return fifo.getNumReady()/MessageSize;
}
