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
        sc_out<long> burstReq;
        sc_in<long> burstResp;
        sc_in<bool> peClk;
        sc_out<bool> bfsDone;

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
            BurstOp* ptr = new BurstOp(type, ptype, burstIdx, peIdx, addr, length);
            ptr->updateReqVec();
            ptr->updateAddrVec();
            GL::bursts.push_back(ptr);

            // It takes a number of cycles to transmit data over the bus to memory.
            // However, we assume it is only limited by the memory bandwidth and thus 
            // we rely the ramulator to contrain the memory write access.
            wait(peClkCycle * ptr->length / (int)sizeof(T), SC_NS);
            ptr->bufferToBurstReq<T>(buffer);
            burstReqQueue[ptype].push_back(burstIdx);


            return burstIdx;
        }

        long createReadBurstReq(
                ramulator::Request::Type type, 
                PortType ptype,
                long addr, 
                int length 
                );

        void sigInit();

    private:
        char level;
        int peClkCycle;
        int frontierSize;
        bool bfsIterationStart;
        int localCounter;

        std::list<char> inspectDepthReadBuffer; 
        std::list<int> inspectFrontierBuffer;
        std::list<int> expandRpaoReadBuffer;
        std::list<int> expandCiaoReadBuffer;
        std::list<int> exapndRpaiReadBuffer; 
        std::list<int> expandCiaiReadBuffer;  
        std::list<int> expandVidxForDepthWriteBuffer;
        std::list<char> expandDepthWriteBuffer;
        std::list<char> expandDepthReadBuffer;
        std::vector<std::list<long>> burstReqQueue;
        std::vector<std::list<long>> burstRespQueue;

        // It keeps the status of the burst requests. If a burst with burstIdx is not found 
        // in the mapper, it doesn't exist. If it is found to be false, the request is generated 
        // but is not responsed yet. If it is set true, the request is responsed.
        // These data structure will remain valid until the end of the object life.
        std::map<long, bool> burstOpStatus;

        bool isBurstReqQueueEmpty();
        bool isBurstRespQueueEmpty();
        void init();
        PortType burstReqArbiter(PortType winner);
        bool isEndOfBfsIteration();
        //bool isAllReqProcessed();
        long getReadyOp();

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
