#include "pe.h"

void pe::init(){

    level = 0;
    dataWidth = 4; 
    
    for(int i = INSPECT_DEPTH_READ; i <= EXPAND_FRONTIER_READ; i++){
        std::list<BurstOp> opBuffer;
        burstReqQueue.push_back(opBuffer);
        burstRespQueue.push_back(opBuffer);
    }

}

// In order to process the memory request, 
// the address sent to the memory must be aligned to burstlen.
pe::pe(sc_module_name _name, 
        int _peIdx, 
        int _peClkCycle
        ) :sc_module(_name) 
{
    peIdx = _peIdx;
    peClkCycle = _peClkCycle;
    init();

    // Memory send and receive processes
    SC_THREAD(sendMemReq);
    SC_THREAD(getMemResp);

    // read depth for inspection
    SC_THREAD(issueInspectDepthReadReq); 
    SC_THREAD(processInspectDepthReadResp);
    SC_THREAD(inspectDepthRead);
    SC_THREAD(issueInspectFrontierWriteReq);
    SC_THREAD(processInspectFrontierWriteResp);
    SC_THREAD(bfsController);

    SC_THREAD(issueExapndFrontierReadReq);
    SC_THREAD(processExpandFrontierReadResp);
}

bool pe::isAllRespReceived(PortType ptype){
    for(auto it = burstOpPtype.begin(); it != burstOpPtype.end(); it++){
        if(it->second == ptype){
            if(burstOpStatus[it->first] == false){
                return false;
            }
        }
    }

    return true;
}

// When the flag is set, it will start traverse the depth array in the memory
void pe::issueInspectDepthReadReq(){

    // initial configuration 
    long depthMemAddr = GL::depthMemAddr;
    int currentLen = 0;
    ramulator::Request::Type type = ramulator::Request::Type::READ;
    int maxLen = GL::vertexNum * (int)(sizeof(unsigned char));

    while(true){
        // Reset these variables when the depth inspection is not going on
        if(inspectDepthReadReqFlag == false){
            depthMemAddr = GL::depthMemAddr;
            currentLen = 0;
            wait(peClkCycle, SC_NS);
            continue;
        }

        int actualLen;
        int burstIdx;
        while(currentLen < maxLen){
            if(currentLen + GL::baseLen > maxLen){
                actualLen = maxLen - currentLen;
            }
            else{
                actualLen = GL::baseLen;
            }

            burstIdx = createReadBurstReq(type, INSPECT_DEPTH_READ, depthMemAddr, actualLen);
            depthMemAddr += actualLen;
            burstOpStatus[burstIdx] = false;
            burstOpPtype[burstIdx] = INSPECT_DEPTH_READ;
            currentLen += actualLen;
        }

        // split the long requests into smaller yet standard burst read requests 
        inspectDepthReadReqFlag = false;
        std::cout << "Send all the depth read requests for inspection at ";
        std::cout << sc_time_stamp() << std::endl;
    }
}

void pe::processInspectDepthReadResp(){
    bool ov_flag1 = true; // once valid flag.
    bool ov_flag2 = true;
    while(true){
        if(!burstRespQueue[INSPECT_DEPTH_READ].empty() && 
                (inspectDepthReadBuffer.size() + GL::baseLen) < GL::depthBufferDepth){
            if(ov_flag1){
                std::cout << "Inspection: get into the depth response processing." << std::endl;
                ov_flag1 = false;
            }
            BurstOp op = burstRespQueue[INSPECT_DEPTH_READ].front();
            if(op.length < dataWidth){
                wait(peClkCycle, SC_NS);
            }
            else{
                wait((op.length/dataWidth) * peClkCycle, SC_NS);
            }

            burstOpStatus[op.burstIdx] = true;
            op.burstReqToBuffer<unsigned char>(inspectDepthReadBuffer);
            burstRespQueue[INSPECT_DEPTH_READ].pop_front();
        }
        else{
            wait(peClkCycle, SC_NS);
        }

        if((int)burstOpStatus.size() > 0 && isAllRespReceived(INSPECT_DEPTH_READ) && ov_flag2){
            std::cout << "All the depth resp obtained at " << sc_time_stamp() << std::endl;
            ov_flag2 = false;
        }

    }
}

void pe::inspectDepthRead(){
    bool ov_flag1 = true;
    bool ov_flag2 = true;
    frontierSize = 0;
    int idx;
    while(true){
        if(inspectFlag == false){
            idx = 0;
            wait(peClkCycle, SC_NS);
            continue;
        }

        if(inspectDepthReadBuffer.empty() == false 
                && (int)(inspectFrontierWriteBuffer.size()) < GL::frontierBufferDepth)
        {
            if(ov_flag1){
                std::cout << "Inspection: get into the depth inspection process." << std::endl;
                ov_flag1 = false;
            }

            unsigned char d = inspectDepthReadBuffer.front();
            inspectDepthReadBuffer.pop_front();
            if(d == level){
                inspectFrontierWriteBuffer.push_back(idx);
                frontierSize++;
                std::cout << "Frontier: " << idx << " " << std::endl;
            }
            idx++;
        }

        if(idx == GL::vertexNum && ov_flag2){
            std::cout << "All the depth information has been inspected at " << sc_time_stamp() << std::endl;
            inspectFlag = false;
            ov_flag2 = false;
        }

        wait(peClkCycle, SC_NS);
    }
}


void pe::issueInspectFrontierWriteReq(){
    bool ov_flag1 = true;
    bool ov_flag2 = true;

    int sizeThreshold = GL::baseLen/((int)sizeof(int));
    ramulator::Request::Type type = ramulator::Request::Type::WRITE;
    long frontierMemAddr;
    long burstIdx;
    while(true){
        if(inspectFrontierWriteReqFlag == false){
            frontierMemAddr = GL::frontierMemAddr;
            wait(peClkCycle, SC_NS);
            continue;
        }

        int len;
        if((int)(inspectFrontierWriteBuffer.size()) > sizeThreshold){
            if(ov_flag1){
                std::cout << "Inspection: get into frontier writing process." << std::endl;
                ov_flag1 = false;
            }

            len = GL::baseLen;
            burstIdx = createWriteBurstReq<int>(
                    type, 
                    INSPECT_FRONTIER_WRITE, 
                    frontierMemAddr, 
                    len, 
                    inspectFrontierWriteBuffer);

            frontierMemAddr += len;
            burstOpStatus[burstIdx] = false;
            burstOpPtype[burstIdx] = INSPECT_FRONTIER_WRITE;
            wait(peClkCycle, SC_NS);
        }
        else if(inspectFlag == false 
                && (int)(inspectFrontierWriteBuffer.size()) > 0 
                && (int)(inspectFrontierWriteBuffer.size()) <= sizeThreshold)
        {
            if(ov_flag1){
                std::cout << "Inspection: get into frontier writing process." << std::endl;
                ov_flag1 = false;
            }

            len = (int)(inspectFrontierWriteBuffer.size() * sizeof(int)); 
            burstIdx = createWriteBurstReq<int>(
                    type, 
                    INSPECT_FRONTIER_WRITE, 
                    frontierMemAddr, 
                    len, 
                    inspectFrontierWriteBuffer);

            frontierMemAddr += len;
            burstOpStatus[burstIdx] = false;
            burstOpPtype[burstIdx] = INSPECT_FRONTIER_WRITE;
            inspectFrontierWriteReqFlag = false;
            wait(peClkCycle, SC_NS);
        }
        else{
            wait(peClkCycle, SC_NS);
        }

        if(
                inspectFlag == false 
                && ov_flag1 == false
                && inspectFrontierWriteBuffer.empty() 
                && ov_flag2)
        {
            std::cout << "All the inspected frontier have been sent out to memory at ";
            std::cout << sc_time_stamp() << std::endl;
            ov_flag2 = false;
        }
    }
}

// Write response
void pe::processInspectFrontierWriteResp(){
    bool ov_flag1 = true;
    bool ov_flag2 = true;

    while(true){
        if(burstRespQueue[INSPECT_FRONTIER_WRITE].empty() == false){
            if(ov_flag1){
                std::cout << "Inspection: get frontier write response." << std::endl;
                ov_flag1 = false;
            }
            BurstOp op = burstRespQueue[INSPECT_FRONTIER_WRITE].front();
            burstOpStatus[op.burstIdx] = true;
            burstRespQueue[INSPECT_FRONTIER_WRITE].pop_front();
        }

        if(
                ov_flag2 
                && ov_flag1 == false 
                && inspectFrontierWriteReqFlag == false 
                && burstRespQueue[INSPECT_FRONTIER_WRITE].empty())
        {
            inspectDone = true;
            std::cout << "Inspection is done at " << sc_time_stamp() << std::endl;
            ov_flag2 = false;
        }

        wait(peClkCycle, SC_NS);
    }
}

// The expansion must wait until the inspection is done. Thus before the expansion starts, 
// all the memory request must have been processed.
bool pe::isInspectDone(){
    for(auto it = burstOpStatus.begin(); it != burstOpStatus.end(); it++){
        if(it->second == false){
            return false;
        }
    }
    return true;
}


// When the flag is set, it will start traverse the frontier array in the memory
void pe::issueExpandFrontierReadReq(){

    // initial configuration 
    long frontierMemAddr = GL::frontierMemAddr;
    int currentLen = 0;
    ramulator::Request::Type type = ramulator::Request::Type::READ;
    int maxLen = frontierSize * (int)(sizeof(int));

    expandFrontierReadReqFlag == false;
    while(true){
        // Reset these variables when the depth inspection is not going on
        if(expandFrontierReadReqFlag == false){
            frontierMemAddr = GL::frontierMemAddr;
            currentLen = 0;
            wait(peClkCycle, SC_NS);
            continue;
        }

        int actualLen;
        int burstIdx;
        while(currentLen < maxLen){
            if(currentLen + GL::baseLen > maxLen){
                actualLen = maxLen - currentLen;
            }
            else{
                actualLen = GL::baseLen;
            }

            burstIdx = createReadBurstReq(type, EXPAND_FRONTIER_READ, frontierMemAddr, actualLen);
            frontierMemAddr += actualLen;
            burstOpStatus[burstIdx] = false;
            burstOpPtype[burstIdx] = EXPAND_FRONTIER_READ;
            currentLen += actualLen;
        }

        // split the long requests into smaller yet standard burst read requests 
        expandFrontierReadReqFlag = false;
        std::cout << "Send all the frontier read requests for expansion at ";
        std::cout << sc_time_stamp() << std::endl;
    }
}

void pe::processExpandFrontierReadResp(){
    bool ov_flag1 = true; // once valid flag.
    bool ov_flag2 = true;
    int totalLen = 0;
    while(true){
        if(!burstRespQueue[EXPAND_FRONTIER_READ].empty() && 
                (expandFrontierReadBuffer.size() + GL::baseLen) < GL::frontierBufferDepth){
            if(ov_flag1){
                std::cout << "Expansion: get into the frontier read resp processing." << std::endl;
                ov_flag1 = false;
            }
            BurstOp op = burstRespQueue[EXPAND_FRONTIER_READ].front();
            if(op.length < dataWidth){
                wait(peClkCycle, SC_NS);
            }
            else{
                wait((op.length/dataWidth) * peClkCycle, SC_NS);
            }

            totalLen += op.len;
            burstOpStatus[op.burstIdx] = true;
            op.burstReqToBuffer<int>(expandFrontierReadBuffer);
            burstRespQueue[EXPAND_FRONTIER_READ].pop_front();
        }
        else{
            wait(peClkCycle, SC_NS);
        }

        if(totalLen == (frontierSize * (int)sizeof(int)) && totalLen > 0){
            std::cout << "All the frontier read resp obtained at " << sc_time_stamp() << std::endl;
            ov_flag2 = false;
        }

    }
}

// Analyze the frontier

void pe::issueExpandRpaiReadReq(){
    bool ov_flag1 = true;
    ramulator::Request::Type type = ramulator::Request::Type::WRITE;
    int burstIdx;
    while(true){
        if(
                expandFrontierReadBuffer.empty() == false 
                && ((int)expandRpaoReadBuffer.size() + 2 < GL::rpaoBufferDepth)
          ){
            if(ov_flag1){
                std::cout << "Expansion: read rapo from memory." << std::endl;
                ov_flag1 = false;
            }

            int vidx = expandFrontierReadBuffer.front();
            long rpaoMemAddr = GL::rpaoMemAddr + vidx * sizeof(int);
            int len = sizeof(int) * 2;
            burstIdx = createReadBurstReq(type, EXPAND_RPAO_READ, rpaoMemAddr, len);
            burstOpStatus[burstIdx] = false;
            burstOpPtype[burstIdx] = EXPAND_RPAO_READ;
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

// Read rpao
void pe::processExpandRpaoReadResp(){
    bool ov_flag1 = true;

    while(true){
        if(burstRespQueue[EXPAND_RPAO_READ].empty()  == false){
            if(ov_flag1){
                std::cout << "Expand: process rpao read resp." << std::endl;
                ov_flag1 = false;
            }

            BurstOp op = burstRespQueue[EXPAND_RPAO_READ].front();
            if(op.length < dataWidth){
                wait(peClkCycle, SC_NS);
            }
            else{
                wait((op.length/dataWidth) * peClkCycle, SC_NS);
            }

            burstOpStatus[op.burstIdx] = true;
            op.burstReqToBuffer<int>(expandRpaoReadBuffer);
            burstRespQueue[EXPAND_RPAO_READ].pop_front();

        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

// Read ciao
void pe::issueExpandCiaoReadReq(){
    bool ov_flag1 = true;
    ramulator::Request::Type type = ramulator::Request::Type::WRITE;
    int burstIdx;
    while(true){
        if(
                (int)expandRpaoReadBuffer.size() >= 2 
                && ((int)expandCiaoReadBuffer.size() < GL::ciaoBufferDepth)
          ){
            if(ov_flag1){
                std::cout << "Expansion: read rapo from memory." << std::endl;
                ov_flag1 = false;
            }

            int srcIdx = expandRpaoReadBuffer.front();
            expandRpaoReadBuffer.pop_front();
            int dstIdx = expandRpaoReadBuffer.front();
            expandRpaoReadBuffer.pop_front();
            long ciaoMemAddr = GL::ciaoMemAddr + srcIdx * sizeof(int);
            int maxLen = sizeof(int) * (dstIdx - srcIdx);

            int actualLen = 0;
            int len = 0;
            // Fetch ciao from memory to buffer.
            while(len < maxLen){
                if(len + GL:baseLen <= maxLen){
                    actualLen = GL::baseLen;
                }
                else{
                    actualLen = maxLen - len;
                }

                burstIdx = createReadBurstReq(type, EXPAND_CIAO_READ, ciaoMemAddr, actualLen);
                burstOpStatus[burstIdx] = false;
                burstOpPtype[burstIdx] = EXPAND_CIAO_READ;
                ciaoMemAddr += actualLen;
                len += actualLen;
            }
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

void pe::processExpandCiaoReadResp(){
    bool ov_flag1 = true;

    while(true){
        if(burstRespQueue[EXPAND_CIAO_READ].empty() == false){
        }
        else{
            wait(peClkCycle, SC_NS);
        }
    }
}

// Read depth
void pe::issueExpandDepthReadReq(){
}

void pe::processExpandDepthReadResp(){
}
// Analyze depth
void pe::expandDepthAnalysis(){
}
// write depth
void pe::expandDepthWriteReq(){
}

void pe::expandDepthWriteResp(){
}

void pe::bfsController(){
    inspectDone = false;
    wait(10 * peClkCycle, SC_NS);
    inspectDepthReadReqFlag = true;
    inspectFlag = true;
    inspectFrontierWriteReqFlag = true;
    wait(10 * peClkCycle, SC_NS);

    while(true){
        if(isInspectDone() == true && inspectDone){
            std::cout << "End of the BFS inspection." << std::endl;
            
            // Print the first four data in Ram
            std::cout << "The first 4 frontier data stored in Ram: ";
            for(int i = 0; i < 4; i++){
                long addr = i * 4 + GL::frontierMemAddr;
                std::cout << mem->getSingleDataFromRam<int>(addr) << " ";
            }
            std::cout << std::endl;

            sc_stop();
        }
        wait(peClkCycle, SC_NS);
    }
}

long pe::createReadBurstReq(
        ramulator::Request::Type type, 
        PortType ptype,
        long addr, 
        int length)
{
    long burstIdx = GL::getBurstIdx();
    BurstOp op(type, ptype, burstIdx, peIdx, addr, length);
    op.updateReqVec();
    op.updateAddrVec();

    wait(peClkCycle, SC_NS);
    burstReqQueue[ptype].push_back(op);
    
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
    while(true){
        if(isBurstReqQueueEmpty() == false){
            PortType ptype = burstReqArbiter(winner);
            winner = ptype;
            BurstOp op = burstReqQueue[ptype].front(); 
            long departTime = (long)(sc_time_stamp()/sc_time(1, SC_NS));
            op.setDepartPeTime(departTime);
            burstReq.write(op);
            burstReqQueue[ptype].pop_front();
        }
        else{
            BurstOp op(false);
            burstReq.write(op);
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

void pe::getMemResp(){
    while(true){
        BurstOp op = burstResp.read();
        if(op.valid){
            long arriveTime = (long)(sc_time_stamp()/sc_time(1, SC_NS));
            op.setArrivePeTime(arriveTime);
            burstRespQueue[op.ptype].push_back(op);
        } 

        wait(peClkCycle, SC_NS);
    }
}

void pe::setPeClkCycle(int _peClkCycle){
    peClkCycle = _peClkCycle;
}

void pe::setDataWidth(int width){
    dataWidth = width; 
}
