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
        sc_out<long> burstReq[PNUM];
        sc_in<long> burstResp[PNUM];
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
                int portIdx,
                long addr, 
                int length, 
                std::list<T> &buffer
                )
        {
            long burstIdx = GL::getBurstIdx();
            BurstOp* ptr = new BurstOp(type, portIdx, burstIdx, peIdx, addr, length);
            ptr->updateReqVec();
            ptr->updateAddrVec();
            GL::bursts.push_back(ptr);

            // It takes a number of cycles to transmit data over the bus to memory.
            // However, we assume it is only limited by the memory bandwidth and thus 
            // we rely the ramulator to contrain the memory write access.
            wait(peClkCycle * ptr->length / (int)sizeof(T), SC_NS);
            ptr->bufferToBurstReq<T>(buffer);
            burstReqQueue[portIdx].push_back(burstIdx);

            return burstIdx;
        }

        long createReadBurstReq(
                ramulator::Request::Type type, 
                int portIdx,
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

        std::vector<std::list<char>> depthBuffer0; //inspectDepthReadBuffer; 
        std::list<char> depthBuffer1;   //expandDepthWriteBuffer;
        std::list<char> depthBuffer2;   //expandDepthReadBuffer;

        std::list<int> frontierBuffer;  //inspectFrontierBuffer;
        std::list<int> rpaoBuffer;      //expandRpaoReadBuffer;
        std::list<int> ciaoBuffer;      //expandCiaoReadBuffer;
        std::list<int> rpaiBuffer;      //exapndRpaiReadBuffer; 
        std::list<int> ciaiBuffer;      //expandCiaiReadBuffer;  
        std::list<int> vidxBuffer;      //expandVidxForDepthWriteBuffer;


        std::vector<std::list<long>> burstReqQueue;
        std::vector<std::list<long>> burstRespQueue;

        std::map<long, bool> burstOpStatus;

        bool isBurstReqQueueEmpty();
        bool isBurstRespQueueEmpty();
        void init();
        bool isEndOfBfsIteration();
        //bool isAllReqProcessed();
        long getReadyOp();

        // processing thread
        void sendMemReq();
        void getMemResp();
        void bfsController();

        void issueInspectDepthReadReq0();
        void issueInspectDepthReadReq1();
        void issueInspectDepthReadReq2();
        void issueInspectDepthReadReq3();
        void processInspectDepthReadResp0();
        void processInspectDepthReadResp1();
        void processInspectDepthReadResp2();
        void processInspectDepthReadResp3();
        void inspectDepthAnalysis();
        void frontierAnalysis();
        void processExpandRpaoReadResp();
        void issueExpandCiaoReadReq();
        void processExpandCiaoReadResp();
        void issueExpandDepthReadReq();
        void processExpandDepthReadResp();
        void expandDepthAnalysis();
        void processExpandDepthWriteResp();

        void inspectDepthReqThread(
                long baseDepthMemAddr,
                int prIdx,
                int portIdx,
                int maxLen
                );

        void inspectDepthRespThread(
                int expectedLen,
                int portIdx,
                int prIdx
                );
};

#endif
