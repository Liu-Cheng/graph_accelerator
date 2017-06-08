#include "MemWrapper.h"

// The memory configuration is initially ported from ramulator.
// which are mostly parsed from input argument. We don't want to change it for now.
MemWrapper::MemWrapper(sc_module_name _name,
        double _memClkCycle,
        double _peClkCycle,
        int argc, 
        char* argv[]) 
    : sc_module(_name), configs(argv[1]){

    loadConfig(argc, argv);
    memClkCycle = _memClkCycle; 
    peClkCycle = _peClkCycle; 
    GL::burstLen = calBurstLen();
    ramInit("./config.txt");

    SC_THREAD(runMemSim);
    SC_THREAD(getBurstReq);
    SC_THREAD(sendBurstResp);
    SC_THREAD(statusMonitor);

}

Graph* MemWrapper::loadGraph(const std::string &cfgFileName){
    std::ifstream fhandle(cfgFileName.c_str());
    if(!fhandle.is_open()){
        HERE;
        std::cout << "Failed to open " << cfgFileName << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string graphName;
    std::string graphDir;
    while(!fhandle.eof()){
        std::string cfgKey;
        fhandle >> cfgKey;
        if(cfgKey == "graphName"){
            fhandle >> graphName;
        }
        else if(cfgKey == "graphDir"){
            fhandle >> graphDir;
        }
    }
    fhandle.close();

    Graph* gptr;
    std::string fname;
    if(graphName == "dblp"){
        fname = graphDir + "dblp.ungraph.txt";
    }
    else if(graphName == "youtube"){
        fname = graphDir + "youtube.ungraph.txt";
    }
    else if(graphName == "lj"){
        fname = graphDir + "lj.ungraph.txt";
    }
    else if(graphName == "pokec"){
        fname = graphDir + "pokec-relationships.txt";
    }
    else if(graphName == "wiki-talk"){
        fname = graphDir + "wiki-Talk.txt";
    }
    else if(graphName == "lj1"){
        fname = graphDir + "LiveJournal1.txt";
    }
    else if(graphName == "orkut"){
        fname = graphDir + "orkut.ungraph.txt";
    }
    else if(graphName == "rmat"){
        fname = graphDir + "rmat-2m-256m.txt";
    }
    else if(graphName == "rmat1k10k"){
        fname = graphDir + "rmat-1k-10k.txt";
    }
    else if(graphName == "rmat10k100k"){
        fname = graphDir + "rmat-10k-100k.txt";
    }
    else if(graphName == "ramt100k1m"){
        fname = graphDir + "rmat-100k-1m.txt";
    }
    else if(graphName == "twitter"){
        fname = graphDir + "twitter_rv.txt";
    }
    else if(graphName == "friendster"){
       fname = graphDir + "friendster.ungraph.txt";
    }
    else{
        HERE;
        std::cout << "Unrecognized graph " << graphName << std::endl;
        exit(EXIT_FAILURE);
    }
    gptr = new Graph(fname.c_str());

    // Update global graph information
    GL::vertexNum = gptr->vertex_num;
    GL::edgeNum = gptr->edge_num;
    gptr->getRandomStartIndices(GL::startingVertices);
    //gptr->printOngb(GL::startingVertices[0]);

    return gptr;

}

void MemWrapper::sigInit(){
    burstResp.write(-1);
}

// To prepare for new bfs traverse, we need to clean 
// both the initial depth and frontier data.
void MemWrapper::cleanRam(){
    long depthAddr = GL::depthMemAddr; 
    for(int i = 0; i < GL::vertexNum; i++){
        updateSingleDataToRam<signed char>(depthAddr, -1);
        depthAddr += (long)sizeof(signed char);
    }

    long frontierAddr = GL::frontierMemAddr;
    for(int i = 0; i < GL::vertexNum; i++){
        updateSingleDataToRam<int>(frontierAddr, -1);
        frontierAddr += (long)sizeof(int);
    }
}

void MemWrapper::setNewStartVertex(int idx){
    long addr = GL::depthMemAddr + idx * sizeof(signed char);
    updateSingleDataToRam<signed char>(addr, 0);
}

// It reads request from pe and thus is synchronized to the pe's clock
void MemWrapper::getBurstReq(){
    while(true){
        long burstIdx = burstReq.read();
        if(burstIdx != -1){
            BurstOp* ptr = GL::bursts[burstIdx];
            burstReqQueue.push_back(burstIdx);
            totalReqNum[burstIdx] = ptr->getReqNum(); 
            processedReqNum[burstIdx] = 0;
            ptr->convertToReq(reqQueue);
        }
        wait(peClkCycle, SC_NS);
    }
}

// The memory response will be sent in the same order with 
// its incoming order. Meanwhile, the response can only be 
// sent at the right timestamp.
void MemWrapper::sendBurstResp(){
    while(true){
        if(burstReqQueue.empty()){
            //std::cout << "burstReqQueue is empty." << std::endl;
            burstResp.write(-1);
            wait(peClkCycle, SC_NS);
            continue;
        }

        int idx = burstReqQueue.front();
        if(std::find(burstRespQueue.begin(), burstRespQueue.end(), idx) == burstRespQueue.end()){
            burstResp.write(-1);
        }
        else{
            BurstOp* ptr = GL::bursts[idx];
            long respReadyTime = ptr->departMemTime; 
            long currentTimeStamp = (long)(sc_time_stamp()/sc_time(1, SC_NS));
            if(respReadyTime <= currentTimeStamp){
                burstResp.write(idx);
                if(ptr->type == ramulator::Request::Type::WRITE){
                    ptr->reqToRam(ramData);
                }
                else{
                    ptr->ramToReq(ramData);
                }
                burstReqQueue.remove(idx);
                burstRespQueue.remove(idx);
            }
        }

        wait(peClkCycle, SC_NS);
    }
}

int MemWrapper::calBurstLen(){

    int burstlen;
    if (standard == "DDR3") {
        DDR3* ddr3 = new DDR3(configs["org"], configs["speed"]);
        burstlen = ddr3->prefetch_size * ddr3->channel_width / 8;
    } else if (standard == "DDR4") {
        DDR4* ddr4 = new DDR4(configs["org"], configs["speed"]);
        burstlen = ddr4->prefetch_size * ddr4->channel_width / 8;
    } else if (standard == "SALP-MASA") {
        SALP* salp8 = new SALP(configs["org"], configs["speed"], "SALP-MASA", configs.get_subarrays());
        burstlen = salp8->prefetch_size * salp8->channel_width / 8;
    } else if (standard == "LPDDR3") {
        LPDDR3* lpddr3 = new LPDDR3(configs["org"], configs["speed"]);
        burstlen = lpddr3->prefetch_size * lpddr3->channel_width / 8;
    } else if (standard == "LPDDR4") {
        // total cap: 2GB, 1/2 of others
        LPDDR4* lpddr4 = new LPDDR4(configs["org"], configs["speed"]);
        burstlen = lpddr4->prefetch_size * lpddr4->channel_width / 8;
    } else if (standard == "GDDR5") {
        GDDR5* gddr5 = new GDDR5(configs["org"], configs["speed"]);
        burstlen = gddr5->prefetch_size * gddr5->channel_width / 8;
    } else if (standard == "HBM") {
        HBM* hbm = new HBM(configs["org"], configs["speed"]);
        burstlen = hbm->prefetch_size * hbm->channel_width / 8;
    } else if (standard == "WideIO") {
        // total cap: 1GB, 1/4 of others
        WideIO* wio = new WideIO(configs["org"], configs["speed"]);
        burstlen = wio->prefetch_size * wio->channel_width / 8;
    } else if (standard == "WideIO2") {
        // total cap: 2GB, 1/2 of others
        WideIO2* wio2 = new WideIO2(configs["org"], configs["speed"], configs.get_channels());
        wio2->channel_width *= 2;
        burstlen = wio2->prefetch_size * wio2->channel_width / 8;
    }
    // Various refresh mechanisms
    else if (standard == "DSARP") {
        DSARP* dsddr3_dsarp = new DSARP(configs["org"], configs["speed"], DSARP::Type::DSARP, configs.get_subarrays());
        burstlen = dsddr3_dsarp->prefetch_size * dsddr3_dsarp->channel_width / 8;
    } else if (standard == "ALDRAM") {
        ALDRAM* aldram = new ALDRAM(configs["org"], configs["speed"]);
        burstlen = aldram->prefetch_size * aldram->channel_width / 8;
    } else if (standard == "TLDRAM") {
        TLDRAM* tldram = new TLDRAM(configs["org"], configs["speed"], configs.get_subarrays());
        burstlen = tldram->prefetch_size * tldram->channel_width / 8;
    }
    else{
        HERE;
        std::cout << "Unknown memory standard." << std::endl;
        exit(EXIT_FAILURE);
    }

    return burstlen;
}

void MemWrapper::runMemSim(){

    if (standard == "DDR3") {
        DDR3* ddr3 = new DDR3(configs["org"], configs["speed"]);
        start_run(configs, ddr3, files);
    } else if (standard == "DDR4") {
        DDR4* ddr4 = new DDR4(configs["org"], configs["speed"]);
        start_run(configs, ddr4, files);
    } else if (standard == "SALP-MASA") {
        SALP* salp8 = new SALP(configs["org"], configs["speed"], "SALP-MASA", configs.get_subarrays());
        start_run(configs, salp8, files);
    } else if (standard == "LPDDR3") {
        LPDDR3* lpddr3 = new LPDDR3(configs["org"], configs["speed"]);
        start_run(configs, lpddr3, files);
    } else if (standard == "LPDDR4") {
        // total cap: 2GB, 1/2 of others
        LPDDR4* lpddr4 = new LPDDR4(configs["org"], configs["speed"]);
        start_run(configs, lpddr4, files);
    } else if (standard == "GDDR5") {
        GDDR5* gddr5 = new GDDR5(configs["org"], configs["speed"]);
        start_run(configs, gddr5, files);
    } else if (standard == "HBM") {
        HBM* hbm = new HBM(configs["org"], configs["speed"]);
        start_run(configs, hbm, files);
    } else if (standard == "WideIO") {
        // total cap: 1GB, 1/4 of others
        WideIO* wio = new WideIO(configs["org"], configs["speed"]);
        start_run(configs, wio, files);
    } else if (standard == "WideIO2") {
        // total cap: 2GB, 1/2 of others
        WideIO2* wio2 = new WideIO2(configs["org"], configs["speed"], configs.get_channels());
        wio2->channel_width *= 2;
        start_run(configs, wio2, files);
    }
    // Various refresh mechanisms
    else if (standard == "DSARP") {
        DSARP* dsddr3_dsarp = new DSARP(configs["org"], configs["speed"], DSARP::Type::DSARP, configs.get_subarrays());
        start_run(configs, dsddr3_dsarp, files);
    } else if (standard == "ALDRAM") {
        ALDRAM* aldram = new ALDRAM(configs["org"], configs["speed"]);
        start_run(configs, aldram, files);
    } else if (standard == "TLDRAM") {
        TLDRAM* tldram = new TLDRAM(configs["org"], configs["speed"], configs.get_subarrays());
        start_run(configs, tldram, files);
    }
    printf("Simulation done. Statistics written to %s\n", stats_out.c_str());

}

void MemWrapper::loadConfig(int argc, char* argv[]){
    if (argc < 2) {
        printf("Usage: %s <configs-file> --mode=cpu,dram,acc [--stats <filename>] <trace-filename1> <trace-filename2> Example: %s ramulator-configs.cfg --mode=cpu cpu.trace cpu.trace\n", argv[0], argv[0]);
    }

    standard = configs["standard"];
    assert(standard != "" || "DRAM standard should be specified.");

    const char *trace_type = strstr(argv[2], "=");
    trace_type++;
    if (strcmp(trace_type, "acc") == 0){
        configs.add("trace_type", "acc");
    } else {
        printf("invalid trace type: %s\n", trace_type);
        assert(false);
    }

    int trace_start = 3;
    Stats::statlist.output(standard+".stats");
    stats_out = standard + string(".stats");

    // When the accelerator is used, there is no need for trace files.
    if(argc >=3){
        for(int i = trace_start; i < argc; i++){
            files.push_back(argv[i]);
        }
    }

    configs.set_core_num(argc - trace_start);
}

template<typename T>
void MemWrapper::start_run(const Config& configs, T* spec, const vector<const char*>& files) {
    // initiate controller and memory
    int C = configs.get_channels(), R = configs.get_ranks();
    // Check and Set channel, rank number
    spec->set_channel_number(C);
    spec->set_rank_number(R);
    std::vector<Controller<T>*> ctrls;
    for (int c = 0 ; c < C ; c++) {
        DRAM<T>* channel = new DRAM<T>(spec, T::Level::Channel);
        channel->id = c;
        channel->regStats("");
        Controller<T>* ctrl = new Controller<T>(configs, channel);
        ctrls.push_back(ctrl);
    }
    Memory<T, Controller> memory(configs, ctrls);

    if (configs["trace_type"] == "acc") {
        run_acc(configs, memory);
    }
    else {
        std::cout << "Error: unexpected trace type." << std::endl;
        exit(EXIT_FAILURE);
    }
}

// The reqQueue can always accept requests from pe, 
// but the memory requests may not be processed by the 
// ramulator due to the internal queue limitation. This 
// infinite queue here is used to simplify the synchronization 
// between pe and ramulator. For example, they don't have to 
// constantly check the internal queue if it is ready to accept 
// new requests. At the same time, it keeps the original parallel 
// memory processing limitation of the DRAM model.
bool MemWrapper::getMemReq(Request &req){
    std::list<Request>::iterator it;
    if(reqQueue.empty() == false){
        Request tmp = reqQueue.front();
        shallowReqCopy(tmp, req);
        req.udf.arriveMemTime = (long)(sc_time_stamp()/sc_time(1, SC_NS));
        reqQueue.pop_front();
        return true;
    }
    else{
        return false;
    }
}

template<typename T>
void MemWrapper::run_acc(const Config& configs, Memory<T, Controller>& memory) {
    /* run simulation */
    bool stall = false;
    bool success = false;
    int reads = 0;
    int writes = 0;
    int clks = 0;
    Request::Type type = Request::Type::READ;
    map<int, int> latencies;

    // Callback function
    auto read_complete = [this, &latencies](Request& r){
        long latency = r.depart - r.arrive;
        latencies[latency]++;

        //update departMemTime
        r.udf.departMemTime = r.udf.arriveMemTime + memClkCycle * latency;
        int burstIdx = r.udf.burstIdx;

        processedReqNum[burstIdx]++;
        if(processedReqNum[burstIdx] == 1){
            GL::bursts[burstIdx]->arriveMemTime = r.udf.arriveMemTime;
        }
        if(processedReqNum[burstIdx] == totalReqNum[burstIdx]){
            GL::bursts[burstIdx]->departMemTime = r.udf.departMemTime;
            burstRespQueue.push_back(burstIdx);
        }
    };

    std::vector<int> addr_vec;
    Request req(addr_vec, type, read_complete);

    // Keep waiting for the memory request processing
    while (true){
        if (!stall){
            success = getMemReq(req);  
        }

        if (success){
            stall = !memory.send(req); 
            if (!stall){
                if (req.type == Request::Type::READ){ 
                    reads++;
                }
                // At this time, we can already assume that the write operation is done.
                else if (req.type == Request::Type::WRITE){ 
                    writes++;
                    long currentTimeStamp = (long)(sc_time_stamp()/sc_time(1, SC_NS));
                    req.udf.departMemTime = currentTimeStamp;
                    int burstIdx = req.udf.burstIdx;
                    processedReqNum[burstIdx]++;
                    if(processedReqNum[burstIdx] ==1){
                        GL::bursts[burstIdx]->arriveMemTime = req.udf.arriveMemTime;
                    }
                    if(processedReqNum[burstIdx] == totalReqNum[burstIdx]){
                        GL::bursts[burstIdx]->departMemTime = req.udf.departMemTime;
                        burstRespQueue.push_back(burstIdx);
                    }
                }
            }
        }

        wait(memClkCycle, SC_NS);
        memory.tick();
        clks ++;
        Stats::curTick++; // memory clock, global, for Statistics
    }
    // This a workaround for statistics set only initially lost in the end
    // memory.finish();
    // Stats::statlist.printall();
}


// We don't want to mess up the callback function while copying
void MemWrapper::shallowReqCopy(const Request &simpleReq, Request &req){
    req.type = simpleReq.type;
    req.coreid = simpleReq.coreid;
    req.addr = simpleReq.addr;
    req.udf.burstIdx = simpleReq.udf.burstIdx;
    req.udf.reqIdx = simpleReq.udf.reqIdx;
    req.udf.peIdx = simpleReq.udf.peIdx;
    req.udf.arriveMemTime = simpleReq.udf.arriveMemTime;
    req.udf.departMemTime = simpleReq.udf.departMemTime;
}

// Global variables are reset here while this part should be 
// moved to somewhere that is easy to be noticed.
void MemWrapper::ramInit(const std::string &cfgFileName){
    Graph* gptr = loadGraph(cfgFileName);
    ramData.resize(GL::edgeNum * 4 * 4);

    std::vector<signed char> depth;
    //std::vector<float> weight;
    std::vector<int> rpao;
    std::vector<int> ciao;
    std::vector<int> rpai;
    std::vector<int> ciai;
    std::vector<int> frontier;

    rpao.resize(gptr->vertex_num + 1);
    rpai.resize(gptr->vertex_num + 1);
    rpao[0] = 0;
    rpai[0] = 0;
    for(int i = 0; i < gptr->vertex_num; i++){
        rpao[i+1] = rpao[i] + gptr->vertices[i]->out_deg;
        rpai[i+1] = rpai[i] + gptr->vertices[i]->in_deg;
    }

    for(int i = 0; i < gptr->vertex_num; i++){
        for(auto id : gptr->vertices[i]->out_vids){
            ciao.push_back(id);
        }
        for(auto id : gptr->vertices[i]->in_vids){
            ciai.push_back(id);
        }
    }

    depth.resize(gptr->vertex_num);
    frontier.resize(gptr->vertex_num);
    for(int i = 0; i < gptr->vertex_num; i++){
        depth[i] = -1;
        frontier[i] = -1;
    }

    auto alignMyself = [](long addr)->long{
        int bw = 8;
        long mask = 0xFF;
        long result = addr;
        if((addr & mask) != 0){
            result = ((addr >> bw) + 1) << bw; 
        }

        return result;
    };

    // Init memory
    long depthAddr = GL::depthMemAddr = 0;

    long rpaoAddr = depthAddr + (long)sizeof(signed char) * GL::vertexNum;
    GL::rpaoMemAddr = rpaoAddr = alignMyself(rpaoAddr);

    long ciaoAddr = rpaoAddr + (long)sizeof(int) * (GL::vertexNum + 1);
    GL::ciaoMemAddr = ciaoAddr = alignMyself(ciaoAddr);

    long rpaiAddr = ciaoAddr + (long)sizeof(int) * GL::edgeNum;
    GL::rpaiMemAddr = rpaiAddr = alignMyself(rpaiAddr);

    long ciaiAddr = rpaiAddr + (long)sizeof(int) * (GL::vertexNum + 1);
    GL::ciaiMemAddr = ciaiAddr = alignMyself(ciaiAddr); 

    long frontierAddr = ciaiAddr + (long)sizeof(int) * GL::edgeNum;
    GL::frontierMemAddr = frontierAddr = alignMyself(frontierAddr);

    for(auto d : depth){
        updateSingleDataToRam<signed char>(depthAddr, d);
        depthAddr += (long)sizeof(signed char);
    }

    // fill memory with the data
    auto fillMem = [this](const std::vector<int> &vec, long &addr){
        for(auto val : vec){
            updateSingleDataToRam<int>(addr, val);
            addr += (long)sizeof(int);
        }
    };

    fillMem(rpao, rpaoAddr);
    fillMem(ciao, ciaoAddr);
    fillMem(rpai, rpaiAddr);
    fillMem(ciai, ciaiAddr);
    fillMem(frontier, frontierAddr);
}


void MemWrapper::dumpDepth(const std::string &fname){
    std::ofstream fhandle(fname.c_str());
    if(!fhandle.is_open()){
        HERE;
        std::cout << "Failed to open " << fname << std::endl;
        exit(EXIT_FAILURE);
    }

    long addr = GL::depthMemAddr;
    for(int i = 0; i < GL::vertexNum; i++){
        fhandle << (int)(getSingleDataFromRam<char>(addr)) << std::endl;
        addr += sizeof(char);
    }
}

void MemWrapper::statusMonitor(){
    while(true){
        if(bfsDone.read()){
            dumpDepth("./depth.txt");
            std::cout << "Simulation completes." << std::endl;
            sc_stop();
        }
        wait(peClkCycle, SC_NS);
    }
}
