#include "MemWrapper.h"
#include "pe.h"

int sc_main(int argc, char *argv[]){

    sc_set_time_resolution(1, SC_NS);
    sc_signal<BurstOp> burstReq;
    sc_signal<BurstOp> burstResp;
    sc_signal<bool> bfsDone;

    double peClkCycle = 10;
    double memClkCycle = 2;
    sc_clock peClk("peClk", peClkCycle, SC_NS, 0.5);


    GL::cfgBfsParam("./config.txt");
    MemWrapper memWrapper("memWrapper", memClkCycle, peClkCycle, argc, argv);
    memWrapper.setNewStartVertex(GL::startingVertices[0]);
    memWrapper.burstReq(burstReq);
    memWrapper.burstResp(burstResp);
    memWrapper.bfsDone(bfsDone);

    pe peInst("peInst", 0, peClkCycle);
    peInst.burstReq(burstReq);
    peInst.burstResp(burstResp);
    peInst.bfsDone(bfsDone);
    peInst.peClk(peClk);

    sc_start();

    return 0;

}
