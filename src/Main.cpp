#include "MemWrapper.h"
#include "pe.h"

int sc_main(int argc, char *argv[]){

    sc_set_time_resolution(1, SC_NS);

    // Only burstIdx is transferred.
    sc_signal<long> burstReq;
    sc_signal<long> burstResp;
    sc_signal<bool> bfsDone;

    double peClkCycle = 5000;
    double memClkCycle = 625;
    sc_clock peClk("peClk", peClkCycle, SC_NS, 0.5);

    GL::cfgBfsParam("./config.txt");
    MemWrapper memWrapper("memWrapper", memClkCycle, peClkCycle, argc, argv);
    memWrapper.setNewStartVertex(GL::startingVertices[0]);
    memWrapper.burstReq(burstReq);
    memWrapper.burstResp(burstResp);
    memWrapper.bfsDone(bfsDone);
    memWrapper.sigInit();

    pe peInst("peInst", 0, peClkCycle);
    peInst.burstReq(burstReq);
    peInst.burstResp(burstResp);
    peInst.bfsDone(bfsDone);
    peInst.peClk(peClk);
    peInst.sigInit();

    sc_start();

    return 0;

}
