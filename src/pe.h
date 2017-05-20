#ifndef __PE_H__
#define __PE_H__

#include <list> 
#include <iostream>
#include <iterator>
#include <map>
#include "Request.h"
#include "common.h"
#include "systemc.h"
#include "MemWrapper.h" // for debugging purpose

class pe : public sc_module{

    SC_HAS_PROCESS(pe);

    public:
        // Even though there may be multiple requests generated at the same time,
        // only one of the request goes to the ramulator eventually in each cycle.
        sc_out<BurstOp> burstReq;
        sc_in<BurstOp> burstResp;

        int peIdx;
        MemWrapper *mem; 

        pe(sc_module_name _name, int _peIdx, int _peClkCycle);
        ~pe(){};


        void runtimeMonitor();
        void dumpResp();
        void setPeClkCycle(int _peClkCycle);
        void bfsController();

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

            // It takes a few cycles to read data from buffer
            wait(peClkCycle * op.length / dataWidth, SC_NS);

            // It takes a number of cycles to transmit data over the bus to memory.
            // However, we assume it is only limited by the memory bandwidth and thus 
            // we rely the ramulator to contrain the memory write access.
            // wait(peClkCycle * op.getReqNum() * GL::burstLen/dataWidth, SC_NS);

            std::cout << "Write request is created: " << std::endl;
            std::cout << "data: ";
            for(auto x : buffer){
                std::cout << x << " ";
            }
            std::cout << std::endl;

            op.bufferToBurstReq<T>(buffer);
            burstReqQueue[ptype].push_back(op);
            std::cout << op << std::endl;

            return burstIdx;
        }

        long createReadBurstReq(
                ramulator::Request::Type type, 
                PortType ptype,
                long addr, 
                int length 
                );

        void setDataWidth(int width);

    private:
        unsigned char level;
        int peClkCycle;
        int dataWidth; 

        // inspection process related flag
        bool inspectDepthReadReqFlag;
        bool inspectFrontierWriteReqFlag;
        bool inspectFlag;
        bool inspectDone;
        int frontierSize;

        // expansion process
        bool expandFrontierReadReqFlag;
        //bool expandReadDepthFlag;
        //bool expandWriteDepthFlag;
        //bool expandReadRpaoFlag;
        //bool expandReadRpaiFlag;
        //bool expandReadCiaoFlag;
        //bool expandReadCiaiFlag;

        std::list<unsigned char> inspectDepthReadBuffer; 
        std::list<int> inspectFrontierWriteBuffer;
        std::list<int> expandRpaoReadBuffer;
        std::list<int> expandCiaoReadBuffer;
        std::list<int> exapndRpaiReadBuffer; 
        std::list<int> expandCiaiReadBuffer;  
        std::list<unsigned char> expandDepthWriteBuffer;
        std::lIst<unsigned char> expandDepthReadBuffer;
        std::list<int> expandFrontierReadBuffer; 
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
        bool isInspectDone();
        PortType burstReqArbiter(PortType winner);
        bool isAllRespReceived(PortType ptype);
        // processing thread
        void sendMemReq();
        void getMemResp();

        void issueInspectDepthReadReq();
        void processInspectDepthReadResp();
        void inspectDepthRead();
        void issueInspectFrontierWriteReq();
        void processInspectFrontierWriteResp();

        void issueExpandFrontierReadReq();
        void processExpandFrontierReadResp();
        void issueExpandRpaoReadReq();
        void processExpandRpaoReadResp();
        void issueExpandCiaoReadReq();
        void processExpandCiaoReadResp();
        void issueExpandDepthReadReq();
        void processExpandDepthReadResp();
        void issueExpandDepthWriteReq();
        void processExpandDepthWriteResp();

};

#endif
