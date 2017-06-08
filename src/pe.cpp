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
    SC_THREAD(issueInspectDepthReadReq); 
    SC_THREAD(processInspectDepthReadResp);
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
    burstReq.write(-1);
}

void pe::init(){

    level = 0;
    bfsIterationStart = false;
    for(int i = INSPECT_DEPTH_READ; i <= EXPAND_DEPTH_WRITE; i++){
        std::list<long> buffer;
        burstReqQueue.push_back(buffer);
        burstRespQueue.push_back(buffer);
    }

}

void pe::issueInspectDepthReadReq(){
    // constant configurations
    ramulator::Request::Type type = ramulator::Request::Type::READ;
    int maxLen = GL::vertexNum * (int)(sizeof(char));

    // configurations that must be reset before bfs starts. 
    long depthMemAddr = 0; 
    int currentLen = maxLen; 
    bool validFlag1 = false; 
    bool validFlag2 = false;

    while(true){
        // Reset the configurations when the bfs iteration starts.
        // Note that it lasts for only a single cycle.
        if(bfsIterationStart){
            depthMemAddr = GL::depthMemAddr;
            currentLen = 0;
            validFlag1 = true;
            validFlag2 = true;
        }

        while(currentLen < maxLen){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "issue inspect depth read request at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            int actualLen = GL::baseLen;
            if((currentLen + GL::baseLen) > maxLen){
                actualLen = maxLen - currentLen;
            }

            // check if read buffer size available
            int bufferSize = (int)(inspectDepthReadBuffer.size());
            int toBeSentSize = actualLen/sizeof(char);
            if(bufferSize + toBeSentSize > GL::depthBufferDepth){
                wait(peClkCycle, SC_NS);
                continue;
            }

            long burstIdx = createReadBurstReq(
                    type, 
                    INSPECT_DEPTH_READ, 
                    depthMemAddr, 
                    actualLen);

            burstOpStatus[burstIdx] = false;
            depthMemAddr += actualLen;
            currentLen += actualLen;
        }

        if(validFlag2){
            if(GL::logon != 0){
                std::cout << "Level = " << (int)level << " ";
                std::cout << "all depth read requests are sent at ";
                std::cout << sc_time_stamp() << std::endl;
            }
            validFlag2 = false;
        }

        // Keep the loop running 
        wait(peClkCycle, SC_NS);
    }
}

void pe::processInspectDepthReadResp(){
    bool validFlag1 = false; 
    bool validFlag2 = false;

    int totalLen = 0;
    while(true){
        // reset configurations when new bfs iterations start
        if(bfsIterationStart){
            validFlag1 = true;
            validFlag2 = true;
            totalLen = 0;
        }

        if(burstRespQueue[INSPECT_DEPTH_READ].empty() == false){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process inspect depth read response at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            // latency caused by copying burst response to buffer
            long burstIdx = burstRespQueue[INSPECT_DEPTH_READ].front();
            BurstOp* ptr = GL::bursts[burstIdx];
            int num = ptr->length/((int)sizeof(char));
            wait(num * peClkCycle, SC_NS);
            //burstOpStatus[burstIdx] = true;
            burstOpStatus.erase(burstIdx);
            ptr->burstReqToBuffer<char>(inspectDepthReadBuffer);
            burstRespQueue[INSPECT_DEPTH_READ].pop_front();
            totalLen += ptr->length;
        }
        else{
            wait(peClkCycle, SC_NS);
        }

        if(validFlag2 && totalLen == GL::vertexNum * (int)sizeof(char)){
            if(GL::logon != 0){
                std::cout << "Level = " << (int)level << " ";
                std::cout << "all depth read responses are processed at ";
                std::cout << sc_time_stamp() << std::endl;
            }
            validFlag2 = false;
        }
    }
}

// Analyze depth for frontier and send frontier read requests.
void pe::inspectDepthAnalysis(){
    bool validFlag1 = false;
    bool validFlag2 = false;
    bfsDone.write(false);

    int idx = 0;
    while(true){
        if(bfsIterationStart){
            idx = 0;
            validFlag1 = true;
            validFlag2 = true;
            frontierSize = 0;
        }

        if(inspectDepthReadBuffer.empty() == false && 
           (int)(inspectFrontierBuffer.size()) < GL::frontierBufferDepth)
        {
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "analyze inspect depth at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            char d = inspectDepthReadBuffer.front();
            inspectDepthReadBuffer.pop_front();
            if(d == level){
                inspectFrontierBuffer.push_back(idx);
                frontierSize++;
            }
            idx++;
        }

        if(idx == GL::vertexNum && validFlag2){
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

    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(inspectFrontierBuffer.empty() == false && 
           ((int)expandRpaoReadBuffer.size() + 2 < GL::rpaoBufferDepth))
        {
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "frontier analysis starts at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            int vidx = inspectFrontierBuffer.front();
            inspectFrontierBuffer.pop_front();
            long rpaoMemAddr = GL::rpaoMemAddr + vidx * sizeof(int);
            int len = sizeof(int) * 2;
            long burstIdx = createReadBurstReq(type, EXPAND_RPAO_READ, rpaoMemAddr, len);
            burstOpStatus[burstIdx] = false;
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

void pe::processExpandRpaoReadResp(){
    bool validFlag1 = false;

    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(burstRespQueue[EXPAND_RPAO_READ].empty()  == false){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process rpao read response at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            long burstIdx = burstRespQueue[EXPAND_RPAO_READ].front();
            BurstOp* ptr = GL::bursts[burstIdx];
            int num = ptr->length/sizeof(int);
            wait(num * peClkCycle, SC_NS);

            //burstOpStatus[burstIdx] = true;
            burstOpStatus.erase(burstIdx);
            ptr->burstReqToBuffer<int>(expandRpaoReadBuffer);
            burstRespQueue[EXPAND_RPAO_READ].pop_front();
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

// Read ciao
void pe::issueExpandCiaoReadReq(){
    ramulator::Request::Type type = ramulator::Request::Type::READ;

    bool validFlag1 = false;
    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if((int)expandRpaoReadBuffer.size() >= 2){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process ciao read request at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            int srcIdx = expandRpaoReadBuffer.front();
            expandRpaoReadBuffer.pop_front();
            int dstIdx = expandRpaoReadBuffer.front();
            expandRpaoReadBuffer.pop_front();
            long ciaoMemAddr = GL::ciaoMemAddr + srcIdx * sizeof(int);
            int maxLen = sizeof(int) * (dstIdx - srcIdx);

            int len = 0;
            while(len < maxLen){
                int actualLen = GL::baseLen;
                if(len + GL::baseLen > maxLen){
                    actualLen = maxLen - len;
                }

                int bufferSize = (int)expandCiaoReadBuffer.size();
                int toBeSentSize = actualLen / (int)sizeof(int);
                if(bufferSize + toBeSentSize < GL::ciaoBufferDepth){
                    long burstIdx = createReadBurstReq(type, EXPAND_CIAO_READ, ciaoMemAddr, actualLen);
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
    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(burstRespQueue[EXPAND_CIAO_READ].empty() == false){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process ciao read resp at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            long burstIdx = burstRespQueue[EXPAND_CIAO_READ].front();
            BurstOp* ptr = GL::bursts[burstIdx];
            int num = ptr->length/(int)sizeof(int);
            wait(num * peClkCycle, SC_NS);
            //burstOpStatus[burstIdx] = true;
            burstOpStatus.erase(burstIdx);
            ptr->burstReqToBuffer<int>(expandCiaoReadBuffer);
            burstRespQueue[EXPAND_CIAO_READ].pop_front();
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

// Read depth
void pe::issueExpandDepthReadReq(){
    ramulator::Request::Type type = ramulator::Request::Type::READ;

    bool validFlag1 = false;
    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(expandCiaoReadBuffer.empty() == false && 
           ((int)(expandDepthReadBuffer.size())) < GL::depthBufferDepth)
        {
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "issue expand depth read at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            int vidx = expandCiaoReadBuffer.front();
            expandVidxForDepthWriteBuffer.push_back(vidx);
            expandCiaoReadBuffer.pop_front();
            long depthMemAddr = GL::depthMemAddr + vidx * sizeof(char);
            long burstIdx = createReadBurstReq(type, EXPAND_DEPTH_READ, depthMemAddr, 1);
            burstOpStatus[burstIdx] = false;
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

void pe::processExpandDepthReadResp(){
    bool validFlag1 = false;

    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(burstRespQueue[EXPAND_DEPTH_READ].empty() == false){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process depth read resp at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            long burstIdx = burstRespQueue[EXPAND_DEPTH_READ].front();
            BurstOp* ptr = GL::bursts[burstIdx];
            int num = ptr->length/(int)sizeof(char);
            wait(num * peClkCycle, SC_NS);
            //burstOpStatus[ptr->burstIdx] = true;
            burstOpStatus.erase(ptr->burstIdx);
            ptr->burstReqToBuffer<char>(expandDepthReadBuffer);
            burstRespQueue[EXPAND_DEPTH_READ].pop_front();
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

void pe::expandDepthAnalysis(){
    ramulator::Request::Type type = ramulator::Request::Type::WRITE;
    bool validFlag1 = false;

    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(expandDepthReadBuffer.empty() == false &&
           (int)expandDepthWriteBuffer.size() <= GL::depthBufferDepth)
        {
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "expand depth analysis starts at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            int d = expandDepthReadBuffer.front();
            int vidx = expandVidxForDepthWriteBuffer.front();
            expandDepthReadBuffer.pop_front();
            expandVidxForDepthWriteBuffer.pop_front();
            if(d == -1){
                d = level + 1;
                expandDepthWriteBuffer.push_back(d);
                long depthMemAddr = GL::depthMemAddr + vidx * sizeof(char);
                int burstIdx = createWriteBurstReq<char>(
                    type, 
                    EXPAND_DEPTH_WRITE, 
                    depthMemAddr, 
                    1, 
                    expandDepthWriteBuffer);

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
    while(true){
        if(bfsIterationStart){
            validFlag1 = true;
        }

        if(burstRespQueue[EXPAND_DEPTH_WRITE].empty() == false){
            if(validFlag1){
                if(GL::logon != 0){
                    std::cout << "Level = " << (int)level << " ";
                    std::cout << "process depth write response at ";
                    std::cout << sc_time_stamp() << std::endl;
                }
                validFlag1 = false;
            }

            long burstIdx = burstRespQueue[EXPAND_DEPTH_WRITE].front();
            BurstOp* ptr = GL::bursts[burstIdx];
            //burstOpStatus[ptr->burstIdx] = true;
            burstOpStatus.erase(ptr->burstIdx);
            burstRespQueue[EXPAND_DEPTH_WRITE].pop_front();
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
    isIterationEnd &= inspectDepthReadBuffer.empty();
    isIterationEnd &= inspectFrontierBuffer.empty();
    isIterationEnd &= expandRpaoReadBuffer.empty();
    isIterationEnd &= expandCiaoReadBuffer.empty();
    isIterationEnd &= expandDepthReadBuffer.empty();
    isIterationEnd &= expandDepthWriteBuffer.empty();

    return isIterationEnd;
}

long pe::createReadBurstReq(
        ramulator::Request::Type type, 
        PortType ptype,
        long addr, 
        int length)
{
    long burstIdx = GL::getBurstIdx();
    BurstOp* ptr = new BurstOp(type, ptype, burstIdx, peIdx, addr, length);
    ptr->updateReqVec();
    ptr->updateAddrVec();
    GL::bursts.push_back(ptr);

    wait(peClkCycle, SC_NS);
    burstReqQueue[ptype].push_back(burstIdx);
    
    return burstIdx;
}


// It decides which of the requests will win for sending the request to memory.
// Round robin strategy is used here. Note that the arbiter will only be invoked 
// when there is unempty queue available.
PortType pe::burstReqArbiter(PortType winner){

    int num = (int)burstReqQueue.size();
    for(int i = 0; i < num; i++){
        int nextIdx = ((int)winner + 1 + i)%num;
        if(burstReqQueue[nextIdx].empty() == false){
            return (PortType)nextIdx; 
        }
    }

    return winner;
}

/*=-----------------------------------------------------------
 * The MemWrapper will have an infinite request queue 
 * that can always accomodate all the requests from 
 * PEs. Then the queue will gradually send the requests to the
 * ramulator which has the admission control.
 *=---------------------------------------------------------*/
void pe::sendMemReq(){ 
    PortType winner = INSPECT_DEPTH_READ;
    burstReq.write(-1); //default signal value
    while(true){
        if(isBurstReqQueueEmpty() == false){
            PortType ptype = burstReqArbiter(winner);
            winner = ptype;
            long burstIdx = burstReqQueue[ptype].front(); 
            BurstOp* ptr = GL::bursts[burstIdx];
            long departTime = (long)(sc_time_stamp()/sc_time(1, SC_NS));
            ptr->departPeTime = departTime;
            burstReq.write(burstIdx);
            burstReqQueue[ptype].pop_front();
        }
        else{
            burstReq.write(-1);
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

/*
bool pe::isAllReqProcessed(){
    for(auto it = burstOpStatus.begin(); it != burstOpStatus.end(); it++){
        if(it->second == false){
            return false;
        }
    }
    return true;
}
*/

bool pe::isBurstRespQueueEmpty(){
    bool empty = true;
    for(const auto &v : burstRespQueue){
        empty = empty && v.empty();
    }
    return empty;
}

void pe::getMemResp(){
    while(true){
        long burstIdx = burstResp.read();
        if(burstIdx != -1){
            long arriveTime = (long)(sc_time_stamp()/sc_time(1, SC_NS));
            GL::bursts[burstIdx]->arrivePeTime = arriveTime;
            BurstOp* ptr = GL::bursts[burstIdx];
            burstRespQueue[ptr->ptype].push_back(burstIdx);
        } 
        wait(peClkCycle, SC_NS);
    }
}

void pe::setPeClkCycle(int _peClkCycle){
    peClkCycle = _peClkCycle;
}

