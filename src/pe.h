#ifndef __PE_H__
#define __PE_H__

#include <list> 
#include <iostream>
#include <iterator>
#include <map>
#include "Request.h"
#include "common.h"
#include "systemc.h"

class pe : public sc_module{

    SC_HAS_PROCESS(pe);

    public:
        // Even though there may be multiple requests generated at the same time,
        // only one of the request goes to the ramulator eventually in each cycle.
        sc_out<BurstOp> burstReq;
        sc_in<BurstOp> burstResp;

        int peIdx;
        pe(sc_module_name _name, int _peIdx, int _peClkCycle);
        ~pe(){};


        void setPeClkCycle(int _peClkCycle);

        // As it takes time to write data to memory, even though the write requests 
        // can always be accommodated. Thus we add additional delay here to simulate 
        // the transmission cost here.
        template<typename T>
        long createWriteBurstReq(
                ramulator::Request::Type type, 
                PortType ptype,
                long addr, 
                int length, 
                std::list<T> &buffer
                )
        {
            long burstIdx = GL::getBurstIdx();
            BurstOp op(type, ptype, burstIdx, peIdx, addr, length);
            op.updateReqVec();
            op.updateAddrVec();

            // It takes a number of cycles to transmit data over the bus to memory.
            // However, we assume it is only limited by the memory bandwidth and thus 
            // we rely the ramulator to contrain the memory write access.
            wait(peClkCycle * op.length / (int)sizeof(T), SC_NS);
            op.bufferToBurstReq<T>(buffer);
            burstReqQueue[ptype].push_back(op);

            return burstIdx;
        }

        long createReadBurstReq(
                ramulator::Request::Type type, 
                PortType ptype,
                long addr, 
                int length 
                );

    private:
        unsigned char level;
        int peClkCycle;
        int frontierSize;
        bool bfsIterationStart;

        std::list<unsigned char> inspectDepthReadBuffer; 
        std::list<int> inspectFrontierBuffer;
        std::list<int> expandRpaoReadBuffer;
        std::list<int> expandCiaoReadBuffer;
        std::list<int> exapndRpaiReadBuffer; 
        std::list<int> expandCiaiReadBuffer;  
        std::list<int> expandVidxForDepthWriteBuffer;
        std::list<unsigned char> expandDepthWriteBuffer;
        std::lIst<unsigned char> expandDepthReadBuffer;
        std::vector<std::list<BurstOp>> burstReqQueue;
        std::vector<std::list<BurstOp>> burstRespQueue;

        // It keeps the status of the burst requests. If a burst with burstIdx is not found 
        // in the mapper, it doesn't exist. If it is found to be false, the request is generated 
        // but is not responsed yet. If it is set true, the request is responsed.
        // These data structure will remain valid until the end of the object life.
        std::map<long, bool> burstOpStatus;
        std::map<long, PortType> burstOpPtype;

        bool isBurstReqQueueEmpty();
        bool isMemReqQueueEmpty();
        void init();
        PortType burstReqArbiter(PortType winner);
        bool isEndOfBfsIteration();

        // processing thread
        void sendMemReq();
        void getMemResp();
        void bfsController();

        void issueInspectDepthReadReq();
        void processInspectDepthReadResp();
        void inspectDepthAnalysis();
        void frontierAnalysis();
        void processExpandRpaoReadResp();
        void issueExpandCiaoReadReq();
        void processExpandCiaoReadResp();
        void issueExpandDepthReadReq();
        void processExpandDepthReadResp();
        void expandDepthAnalysis();
        void processExpandDepthWriteResp();
};

#endif
