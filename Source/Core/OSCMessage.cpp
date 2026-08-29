#include "OSCMessage.h"

namespace ecm::osc {

void MessageFifo::add(const Message& message) {
    int start1, size1, start2, size2;
    fifo.prepareToWrite(1, start1, size1, start2, size2);
    
    if (size1 > 0) {
        buffer[start1] = message;
        fifo.finishedWrite(1);
    } else if (size2 > 0) {
        buffer[start2] = message;
        fifo.finishedWrite(1);
    }
}

bool MessageFifo::read(Message& message) {
    int start1, size1, start2, size2;
    fifo.prepareToRead(1, start1, size1, start2, size2);
    
    if (size1 > 0) {
        message = buffer[start1];
        fifo.finishedRead(1);
        return true;
    } else if (size2 > 0) {
        message = buffer[start2];
        fifo.finishedRead(1);
        return true;
    }
    
    return false;
}

int MessageFifo::getMessageCount() const {
    return fifo.getNumReady();
}

} // namespace ecm::osc
