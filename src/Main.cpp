//#include "MemWrapper.h"
#include "pe.h"

int sc_main(int argc, char *argv[]){

    sc_signal<BurstOp> burstReq;
    sc_signal<BurstOp> burstResp;
    
    double peClkCycle = 1000.0/200;
    double memClkCycle = 1000.0/1600;

    GL::cfgBfsParam("./config.txt");
    MemWrapper memWrapper("memWrapper", memClkCycle, peClkCycle, argc, argv);
    memWrapper.setNewStartVertex(GL::startingVertices[0]);
    std::cout << "The start vertex of the BFS is " << GL::startingVertices[0] << std::endl;
    memWrapper.burstReq(burstReq);
    memWrapper.burstResp(burstResp);

    pe peInst("peInst", 0, peClkCycle);
    peInst.mem = &memWrapper;
    peInst.burstReq(burstReq);
    peInst.burstResp(burstResp);

    sc_start();

    return 0;

}
