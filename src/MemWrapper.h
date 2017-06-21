#ifndef __MEM_WRAPPER_H__
#define __MEM_WRAPPER_H__

#include "Processor.h"
#include "Config.h"
#include "Controller.h"
#include "SpeedyController.h"
#include "Memory.h"
#include "DRAM.h"
#include "Statistics.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>
#include <functional>
#include <map>
#include <iterator>
#include <list>
#include <vector>
#include <algorithm>
#include "common.h"
#include "graph.h"
#include <ctime>

/* Standards */
#include "Gem5Wrapper.h"
#include "DDR3.h"
#include "DDR4.h"
#include "DSARP.h"
#include "GDDR5.h"
#include "LPDDR3.h"
#include "LPDDR4.h"
#include "WideIO.h"
#include "WideIO2.h"
#include "HBM.h"
#include "SALP.h"
#include "ALDRAM.h"
#include "TLDRAM.h"

class MemWrapper : public sc_module{

    SC_HAS_PROCESS(MemWrapper);

    public:
        // Memory related configurations
        std::string name;
        std::string standard;
        Config configs;
        string stats_out;
        std::vector<const char*> files;

        // Signals from/to pes. They will be processed following the peClk.
        sc_in <long> burstReq[PNUM];
        sc_out <long> burstResp[PNUM];
        sc_in <bool> bfsDone;

        // In addition, as the requests are stored in order, 
        // it is also the basis of the data memory content management.  
        std::vector<std::list<long>> burstReqQueue;

        // It stores the response obtained from the ramulator and it will 
        // be gradually removed when the response is sent out.
        std::vector<std::list<long>> burstRespQueue;

        // It stores all the requests to be sent to the ramulator
        // It will gradually be removed when it is processed.
        // It also helps with the synchronization between 
        // the mem clock domain and the pe clock domain.
        std::list<Request> reqQueue; 

        double findTime;
        double removeTime;
        double memProcessTime;
        
        MemWrapper(sc_module_name _name, 
                double _memClkCycle, 
                double _peClkCycle,
                int argc, 
                char* argv[]);

        template<typename T>
        void run_acc(const Config& configs, Memory<T, Controller>& memory);

        template<typename T>
        void start_run(const Config& configs, T* spec, const vector<const char*>& files);
        void getBurstReq();
        void runMemSim();
        bool getMemReq(Request &req);
        void sendBurstResp();
        void memReqMonitor();
        void respMonitor();
        void cleanRam(); // clean the ram content for new bfs traverse
        void setNewStartVertex(int idx); // set ram for a different start vertices of bfs.
        void statusMonitor();
        void sigInit();
        ~MemWrapper(){};

        // Get data from ram.
        template<typename T>
        T getSingleDataFromRam(long addr){
            T t;
            T* p = &t;
            for(int i = 0; i < (int)sizeof(T); i++){
                *((char*)p + i) = ramData[addr + i];
            }
            return t;
        }


    private:
        int memSize;               // # of bytes
        std::vector<char> ramData; // byte level memory data management.

        // Keep two map containers that help to keep a record of the 
        // number of basic requests processed of a burst
        std::map<long, int> totalReqNum;
        std::map<long, int> processedReqNum;

        double memClkCycle;
        double peClkCycle;

        void loadConfig(int argc, char* argv[]);
        Graph* loadGraph(const std::string &cfgFileName);
        int calBurstLen();
        long getMaxDepartTime(const std::vector<long> &reqVec);
        long getMinArriveTime(const std::vector<long> &reqVec);
        void cleanProcessedRequests(long idx);
        void shallowReqCopy(const Request &simpleReq, Request &req);
        void ramInit(const std::string &cfgFileName);
        void dumpDepth(const std::string &fname);
        bool updateWriteResp();

        // Update ram on a specified addr with specified data type.
        void cleanRespQueue(const std::vector<long> &reqVec);

        // Update ram on a specified addr with specified data type.
        template <typename T>
        void updateSingleDataToRam(long addr, T t){
            T* p = &t;
            for(int i = 0; i < (int)sizeof(T); i++){
                ramData[addr+i] = *((char*)p + i);
            }
        }

};

#endif 
