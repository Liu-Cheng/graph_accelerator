#include "pe.h"

// Constructor
pe::pe(
        sc_module_name _name, 
        int _peIdx, 
        int _peClkCycle
        ) :sc_module(_name) 
{
    peIdx = _peIdx;
    peClkCycle = _peClkCycle;
    localCounter = 0;

    init();

    // Memory request/response process
    SC_THREAD(sendMemReq);
    SC_THREAD(getMemResp);

    // ================================================================
    // To do:
    // These memory read/write threads can be replaced with 
    // well-defined reusable DMA module.
    //
    // Here are the basic memory read/write processing rules:
    // 1) The read/write request length is limited to GL::baseLen which 
    // is larger than the basic memory operations. This rule helps to 
    // avoid long memory requests blocking shorter requests.
    //
    // 2) The read/write requests should aware the size of the local 
    // buffer such that they can ensure the responses can always be 
    // processed without blocking. Specifically, the read buffer should 
    // be able to accommodate the data fecthed from memory while the 
    // write buffer should have enough data for writing back to memory.
    //
    // 3) The responses and the requests from the same port must be 
    // processed in the same order. This can be a problem for random 
    // short memory request.
    // ================================================================

    // pipelined inspection threads
    SC_THREAD(issueInspectDepthReadReq0); 
    SC_THREAD(issueInspectDepthReadReq1); 
    SC_THREAD(issueInspectDepthReadReq2); 
    SC_THREAD(issueInspectDepthReadReq3); 
    SC_THREAD(processInspectDepthReadResp0);
    SC_THREAD(processInspectDepthReadResp1);
    SC_THREAD(processInspectDepthReadResp2);
    SC_THREAD(processInspectDepthReadResp3);
    SC_THREAD(inspectDepthAnalysis);

    // pipelined expansion threads
    SC_THREAD(frontierAnalysis);
    SC_THREAD(processExpandRpaoReadResp);
    SC_THREAD(issueExpandCiaoReadReq);
    SC_THREAD(processExpandCiaoReadResp);
    SC_THREAD(issueExpandDepthReadReq);
    SC_THREAD(processExpandDepthReadResp);
    SC_THREAD(expandDepthAnalysis);
    SC_THREAD(processExpandDepthWriteResp);

    // bfs controller
    SC_METHOD(bfsController);
    sensitive << peClk.pos();
}

void pe::sigInit(){
    for(int i = 0; i < PNUM; i++){
        burstReq[i].write(-1);
    }
}

void pe::init(){
    level = 0;
    topdown = true;
    bfsIterationStart = false;
    burstReqQueue.resize(PNUM);
    burstRespQueue.resize(PNUM);
    depthBuffer0.resize(PRNUM);
}

void pe::inspectDepthReqThread(
        long baseDepthMemAddr,
        int prIdx,
        int portIdx,
        int maxLen
        ){

    long depthMemAddr = 0;
    bool validFlag1 = false; 
    bool validFlag2 = false;
    int currentLen = maxLen;
    ramulator::Request::Type type = ramulator::Request::Type::READ;

    while(true){
        if(bfsIterationStart){
            depthMemAddr = baseDepthMemAddr;
            currentLen = 0;
            validFlag1 = true;
            validFlag2 = true;
        }

        while(currentLen < maxLen){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "issue inspect depth partition ";
                    std::cout << prIdx << " read request at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            int actualLen = GL::baseLen;
            if((currentLen + GL::baseLen) > maxLen){
                actualLen = maxLen - currentLen;
            }

            int bufferSize = (int)(depthBuffer0[prIdx].size());
            int toBeSentSize = actualLen/sizeof(char);
            if(bufferSize + toBeSentSize > GL::depthBufferDepth){
                wait(peClkCycle, SC_NS);
                continue;
            }

            long burstIdx = createReadBurstReq(
                    type, 
                    portIdx, 
                    depthMemAddr, 
                    actualLen);

            burstOpStatus[burstIdx] = false;
            depthMemAddr += actualLen;
            currentLen += actualLen;
        }

        if(validFlag2){
            if(GL::logon != 0){
                std::cout << "Level = " << (int)level << " ";
                std::cout << "all depth partition " << prIdx;
                std::cout << " read requests are sent at ";
                std::cout << sc_time_stamp() << std::endl;
            }
            validFlag2 = false;
        }

        wait(peClkCycle, SC_NS);
    }
}

void pe::issueInspectDepthReadReq0(){
    int portIdx = 0; // portIdx = 0;
    int maxLen = (GL::vertexNum/PRNUM) * (int)(sizeof(char));
    long baseDepthMemAddr = GL::depthMemAddr; 
    int prIdx = 0;
    inspectDepthReqThread(
            baseDepthMemAddr,
            prIdx,
            portIdx,
            maxLen
            );
}

void pe::issueInspectDepthReadReq1(){
    int portIdx = 1;
    int maxLen = GL::vertexNum/PRNUM * (int)(sizeof(char));
    long baseDepthMemAddr = portIdx * (GL::vertexNum/PRNUM) * (int)(sizeof(char)); 
    int prIdx = 1;
    inspectDepthReqThread(
            baseDepthMemAddr,
            prIdx,
            portIdx,
            maxLen
            );
}

void pe::issueInspectDepthReadReq2(){
    int portIdx = 2;
    int maxLen = GL::vertexNum/PRNUM * (int)(sizeof(char));
    long baseDepthMemAddr = portIdx * (GL::vertexNum/PRNUM) * (int)(sizeof(char)); 
    int prIdx = 2;
    inspectDepthReqThread(
            baseDepthMemAddr,
            prIdx,
            portIdx,
            maxLen
            );
}

void pe::issueInspectDepthReadReq3(){
    int portIdx = 3;
    int maxLen = (GL::vertexNum - portIdx * (GL::vertexNum/PRNUM)) * (int)(sizeof(char));
    long baseDepthMemAddr = portIdx * (GL::vertexNum/PRNUM) * (int)(sizeof(char)); 
    int prIdx = 3;
    inspectDepthReqThread(
            baseDepthMemAddr,
            prIdx,
            portIdx,
            maxLen
            );
}

void pe::inspectDepthRespThread(
        int expectedLen,
        int portIdx,
        int prIdx
        ){
    bool validFlag1 = false; 
    bool validFlag2 = false;
    int totalLen = 0;
    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
            validFlag2 = true;
            totalLen = 0;
        }

        if(burstRespQueue[portIdx].empty() == false){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process inspect depth " << prIdx;
                    std::cout << " read response at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            long burstIdx = burstRespQueue[portIdx].front();
            BurstOp* ptr = GL::bursts[burstIdx];
            int num = ptr->length/((int)sizeof(char));
            wait(num * peClkCycle, SC_NS);
            burstOpStatus.erase(burstIdx);
            ptr->burstReqToBuffer<char>(depthBuffer0[prIdx]);
            burstRespQueue[portIdx].pop_front();
            totalLen += ptr->length;
        }
        else{
            wait(peClkCycle, SC_NS);
        }

        if(validFlag2 && totalLen == expectedLen){
            if(GL::logon != 0){
                std::cout << "Level = " << (int)level << " ";
                std::cout << "all depth of part " << prIdx;
                std::cout << " read responses are processed at ";
                std::cout << sc_time_stamp() << std::endl;
            }
            validFlag2 = false;
        }
    }
}

void pe::processInspectDepthReadResp0(){
    int portIdx = 0;
    int expectedLen = (GL::vertexNum/PRNUM) * (int)(sizeof(char));
    int prIdx = 0;

    inspectDepthRespThread(
            expectedLen,
            portIdx,
            prIdx
            );
}

void pe::processInspectDepthReadResp1(){
    int portIdx = 1;
    int expectedLen = (GL::vertexNum/PRNUM) * (int)(sizeof(char));
    int prIdx = 1;

    inspectDepthRespThread(
            expectedLen,
            portIdx,
            prIdx
            );
}

void pe::processInspectDepthReadResp2(){
    int portIdx = 2;
    int expectedLen = (GL::vertexNum/PRNUM) * (int)(sizeof(char));
    int prIdx = 2;

    inspectDepthRespThread(
            expectedLen,
            portIdx,
            prIdx
            );
}

void pe::processInspectDepthReadResp3(){
    int portIdx = 3;
    int expectedLen = (GL::vertexNum - portIdx * (GL::vertexNum/PRNUM)) * (int)(sizeof(char));
    int prIdx = 3;

    inspectDepthRespThread(
            expectedLen,
            portIdx,
            prIdx
            );
}

// Analyze depth for frontier and send frontier read requests.
void pe::inspectDepthAnalysis(){
    bool validFlag1 = false;
    bool validFlag2 = false;
    bfsDone.write(false);

    int idx[PRNUM];
    auto resetIdx = [](int idx[PRNUM]){
        for(int i = 0; i < PRNUM; i++){
            idx[i] = i * (GL::vertexNum/PRNUM);
        }
    };
    resetIdx(idx);

    while(true){
        if(bfsIterationStart){
            resetIdx(idx);
            validFlag1 = true;
            validFlag2 = true;
            frontierSize = 0;
        }

        bool notEmpty = false;
        for(int i = 0; i < PRNUM; i++){
            if(depthBuffer0[i].empty() == false){
                notEmpty = true;
                break;
            }
        }

        if(notEmpty && (int)(frontierBuffer.size()) < GL::frontierBufferDepth){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "analyze inspect depth at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            for(int i = 0; i < PRNUM; i++){
                if(depthBuffer0[i].empty() == false){
                    char d = depthBuffer0[i].front();
                    depthBuffer0[i].pop_front();
                    if(d == level){
                        frontierBuffer.push_back(idx[i]);
                        frontierSize++;
                    }
                    idx[i]++;
                }
            }
        }

        bool allAnalyzed = true;
        for(int i = 0; i < PRNUM; i++){
            if(i < PRNUM - 1){
                allAnalyzed &= (idx[i] == (i+1) * (GL::vertexNum/PRNUM));
            }
            else{
                allAnalyzed &= (idx[i] == GL::vertexNum);
            }
        }

        if(allAnalyzed && validFlag2){
            if(GL::logon != 0){
                std::cout << "Level = " << (int)level << " ";
                std::cout << "all the inspect depth are analyzed at ";
                std::cout << sc_time_stamp() << std::endl;
            }
            validFlag2 = false;

            // bfs complete
            if(frontierSize == 0){
                std::cout << "Empty frontier is detected." << std::endl;
                double runtime = (long)(sc_time_stamp()/sc_time(1, SC_NS))/1000;
                std::cout << "BFS performance is " << GL::edgeNum/runtime;
                std::cout << " billion traverse per second." << std::endl;
                std::cout << "This is the end of the BFS traverse." << std::endl;
                bfsDone.write(true);
                //sc_stop();
            }
        }

        wait(peClkCycle, SC_NS);
    }
}

// Analyze frontier and load corresponding rpao from memory.
void pe::frontierAnalysis(){
    ramulator::Request::Type type = ramulator::Request::Type::READ;
    bool validFlag1 = false;
    int portIdx = 4;

    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(frontierBuffer.empty() == false && 
           ((int)rpaoBuffer.size() + 2 < GL::rpaoBufferDepth))
        {
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "frontier analysis starts at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            int vidx = frontierBuffer.front();
            frontierBuffer.pop_front();
            long rpaoMemAddr = GL::rpaoMemAddr + vidx * sizeof(int);
            int len = sizeof(int) * 2;
            long burstIdx = createReadBurstReq(type, portIdx, rpaoMemAddr, len);
            burstOpStatus[burstIdx] = false;
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

void pe::processExpandRpaoReadResp(){
    bool validFlag1 = false;
    int portIdx = 4;

    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(burstRespQueue[portIdx].empty()  == false){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process rpao read response at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            long burstIdx = burstRespQueue[portIdx].front();
            BurstOp* ptr = GL::bursts[burstIdx];
            int num = ptr->length/sizeof(int);
            wait(num * peClkCycle, SC_NS);

            //burstOpStatus[burstIdx] = true;
            burstOpStatus.erase(burstIdx);
            ptr->burstReqToBuffer<int>(rpaoBuffer);
            burstRespQueue[portIdx].pop_front();
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

// Read ciao
void pe::issueExpandCiaoReadReq(){
    ramulator::Request::Type type = ramulator::Request::Type::READ;
    int portIdx = 5;

    bool validFlag1 = false;
    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if((int)rpaoBuffer.size() >= 2){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process ciao read request at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            int srcIdx = rpaoBuffer.front();
            rpaoBuffer.pop_front();
            int dstIdx = rpaoBuffer.front();
            rpaoBuffer.pop_front();
            long ciaoMemAddr = GL::ciaoMemAddr + srcIdx * sizeof(int);
            int maxLen = sizeof(int) * (dstIdx - srcIdx);

            int len = 0;
            while(len < maxLen){
                int actualLen = GL::baseLen;
                if(len + GL::baseLen > maxLen){
                    actualLen = maxLen - len;
                }

                int bufferSize = (int)ciaoBuffer.size();
                int toBeSentSize = actualLen / (int)sizeof(int);
                if(bufferSize + toBeSentSize < GL::ciaoBufferDepth){
                    long burstIdx = createReadBurstReq(type, portIdx, ciaoMemAddr, actualLen);
                    burstOpStatus[burstIdx] = false;
                    ciaoMemAddr += actualLen;
                    len += actualLen;
                }
                else{
                    wait(peClkCycle, SC_NS);
                }
            }
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

void pe::processExpandCiaoReadResp(){
    bool validFlag1 = false;
    int portIdx = 5;
    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(burstRespQueue[portIdx].empty() == false){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process ciao read resp at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            long burstIdx = burstRespQueue[portIdx].front();
            BurstOp* ptr = GL::bursts[burstIdx];
            int num = ptr->length/(int)sizeof(int);
            wait(num * peClkCycle, SC_NS);
            //burstOpStatus[burstIdx] = true;
            burstOpStatus.erase(burstIdx);
            ptr->burstReqToBuffer<int>(ciaoBuffer);
            burstRespQueue[portIdx].pop_front();
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

// Read depth
void pe::issueExpandDepthReadReq(){
    ramulator::Request::Type type = ramulator::Request::Type::READ;
    int portIdx = 6;

    bool validFlag1 = false;
    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(ciaoBuffer.empty() == false && 
           ((int)(ciaoBuffer.size())) < GL::depthBufferDepth)
        {
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "issue expand depth read at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            int vidx = ciaoBuffer.front();
            vidxBuffer.push_back(vidx);
            ciaoBuffer.pop_front();
            long depthMemAddr = GL::depthMemAddr + vidx * sizeof(char);
            long burstIdx = createReadBurstReq(type, portIdx, depthMemAddr, 1);
            burstOpStatus[burstIdx] = false;
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

void pe::processExpandDepthReadResp(){
    bool validFlag1 = false;
    int portIdx = 6;

    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(burstRespQueue[portIdx].empty() == false){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process depth read resp at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            long burstIdx = burstRespQueue[portIdx].front();
            BurstOp* ptr = GL::bursts[burstIdx];
            int num = ptr->length/(int)sizeof(char);
            wait(num * peClkCycle, SC_NS);
            //burstOpStatus[ptr->burstIdx] = true;
            burstOpStatus.erase(ptr->burstIdx);
            ptr->burstReqToBuffer<char>(depthBuffer1);
            burstRespQueue[portIdx].pop_front();
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

void pe::expandDepthAnalysis(){
    ramulator::Request::Type type = ramulator::Request::Type::WRITE;
    bool validFlag1 = false;
    int portIdx = 7;

    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(depthBuffer1.empty() == false &&
           (int)depthBuffer2.size() <= GL::depthBufferDepth)
        {
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "expand depth analysis starts at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            int d = depthBuffer1.front();
            int vidx = vidxBuffer.front();
            depthBuffer1.pop_front();
            vidxBuffer.pop_front();
            if(d == -1){
                d = level + 1;
                depthBuffer2.push_back(d);
                long depthMemAddr = GL::depthMemAddr + vidx * sizeof(char);
                int burstIdx = createWriteBurstReq<char>(
                    type, 
                    portIdx, 
                    depthMemAddr, 
                    1, 
                    depthBuffer2);

                burstOpStatus[burstIdx] = false;
            }
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

void pe::processExpandDepthWriteResp(){
    bool validFlag1 = false;
    int portIdx = 7;
    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(burstRespQueue[portIdx].empty() == false){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process depth write response at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            long burstIdx = burstRespQueue[portIdx].front();
            BurstOp* ptr = GL::bursts[burstIdx];
            //burstOpStatus[ptr->burstIdx] = true;
            burstOpStatus.erase(ptr->burstIdx);
            burstRespQueue[portIdx].pop_front();
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

void pe::bfsController(){
    if(localCounter < 20){
        localCounter++;
    }

    if(localCounter == 10){
        bfsIterationStart = true;
    }
    else{
        bfsIterationStart = false;
    }

    if(isEndOfBfsIteration() && localCounter == 20){
        burstOpStatus.clear();
        level++;
        if(GL::logon != 0){
            std::cout << "level changes at " << sc_time_stamp() << std::endl;
        }
        localCounter = 0;
    }

}

bool pe::isEndOfBfsIteration(){
    bool isIterationEnd = true;
    isIterationEnd &= isBurstReqQueueEmpty();
    isIterationEnd &= isBurstRespQueueEmpty();
    isIterationEnd &= burstOpStatus.empty();
    for(int i = 0; i < PRNUM; i++){
        isIterationEnd &= depthBuffer0[i].empty();
    }
    isIterationEnd &= frontierBuffer.empty();
    isIterationEnd &= rpaoBuffer.empty();
    isIterationEnd &= ciaoBuffer.empty();
    isIterationEnd &= depthBuffer1.empty();
    isIterationEnd &= depthBuffer2.empty();

    return isIterationEnd;
}

long pe::createReadBurstReq(
        ramulator::Request::Type type, 
        int portIdx,
        long addr, 
        int length)
{
    long burstIdx = GL::getBurstIdx();
    BurstOp* ptr = new BurstOp(type, portIdx, burstIdx, peIdx, addr, length);
    ptr->updateReqVec();
    ptr->updateAddrVec();
    GL::bursts.push_back(ptr);

    wait(peClkCycle, SC_NS);
    burstReqQueue[portIdx].push_back(burstIdx);
    
    return burstIdx;
}

void pe::sendMemReq(){ 
    while(true){
        for(int i = 0; i < PNUM; i++){
            if(burstReqQueue[i].empty() == false){
                long burstIdx = burstReqQueue[i].front(); 
                BurstOp* ptr = GL::bursts[burstIdx];
                long departTime = (long)(sc_time_stamp()/sc_time(1, SC_NS));
                ptr->departPeTime = departTime;
                burstReq[i].write(burstIdx);
                burstReqQueue[i].pop_front();
            }
            else{
                burstReq[i].write(-1);
            }
        }

        wait(peClkCycle, SC_NS);
    }
}

bool pe::isBurstReqQueueEmpty(){
    bool empty = true;
    for(const auto &v : burstReqQueue){
        empty = empty && v.empty();
    }
    return empty;
}

bool pe::isBurstRespQueueEmpty(){
    bool empty = true;
    for(const auto &v : burstRespQueue){
        empty = empty && v.empty();
    }
    return empty;
}

void pe::getMemResp(){
    while(true){
        for(int i = 0; i < PNUM; i++){
            long burstIdx = burstResp[i].read();
            if(burstIdx != -1){
                long arriveTime = (long)(sc_time_stamp()/sc_time(1, SC_NS));
                GL::bursts[burstIdx]->arrivePeTime = arriveTime;
                burstRespQueue[i].push_back(burstIdx);
            } 
        }
        wait(peClkCycle, SC_NS);
    }
}

void pe::setPeClkCycle(int _peClkCycle){
    peClkCycle = _peClkCycle;
}

