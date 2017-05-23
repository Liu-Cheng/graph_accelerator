#ifndef __MEM_IF_H__
#define __MEM_IF_H__

template<typename T>
class memIF : public sc_module{

    SC_HAS_PROCESS(memIF);

    public:
        // io for accelerator
        sc_in<bool> clk;
        sc_in<bool> ena;
        sc_in<bool> we;
        sc_in<long> addr;
        sc_in<int> length;

        sc_in<T> din;
        sc_in<bool> invalid;

        sc_out<T> dout;
        sc_out<bool> outvalid;
        sc_out<bool> complete;

        // io for memory model
        sc_out<BurstOp> burstReq;
        sc_in<BurstOp> burstResp;

        readIF(double _clkCycle);

    private:
        double clkCycle;
        std::list<BurstOp> reqQueue;
        std::list<BurstOp> respQueue;

        void issueReadReq();
        void processReadResp();
        void issueWriteReq();
        void processWriteResp();
};

#endif
