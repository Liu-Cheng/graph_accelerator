#include "memIF.h"

memIF::memIF(double _clkCycle){
    clkCycle = _clkCycle;

    SC_THREAD(issueReadReq);
    sensitive << clk.pos();

    SC_THREAD(processReadResp);
    sensitive << clk.pos();
}

void memIF::issueReadReq(){
    if(ena.read() && we.read() == false){
        // create read request
        // memaddr: addr.read()
        // length: length.read()
        BurstOp op = createBurstReadReq();
        burstReq.push_back(op);
    }
}

void memIF::processReadResp(){
    while(readRespQueue.empty() == false){
        BurstOp op = readRespQueue.front();
        readRespQueue.pop_front();
        std::list<T> buffer;
        copyReadReqToBuffer(readBuffer);
        while(buffer.empty() == false){
            dout.write(buffer.front());
            buffer.pop_front();
            outvalid = true;
            complete.write(buffer.empty());
            wait(clkCycle, SC_NS);
        }
        wait(clkCycle, SC_NS);
    }
}


void memIF::issueWriteReq(){
    if(ena.read() && we.read()){
        BurstOp op = createBurstWriteReq();
        copyBufferToReq();
        reqQueue.push_back(op);
    }
}


void memIF::readData(){
}

void memIF::sendData(){
}
