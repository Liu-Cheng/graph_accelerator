#include "graph.h"

int GL::burstlen = 64;

void Graph::loadFile(
        const std::string& fname, 
        std::vector<std::vector<int>> &data
        )
{
    std::ifstream fhandle(fname.c_str());
    if(!fhandle.is_open()){
        HERE;
        std::cout << "Failed to open " << fname << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string line;
    while(std::getline(fhandle, line)){
        std::istringstream iss(line);
        data.push_back(
            std::vector<int>(std::istream_iterator<int>(iss),
            std::istream_iterator<int>())
        );
    }
    fhandle.close();
}

void Graph::getStat(){
    int zeroOutVertexNum = 0;
    for(auto it = vertices.begin(); it != vertices.end(); it++){
        if((*it)->outVid.empty()){
            zeroOutVertexNum++;
        }
    }
    std::cout << "Zero outgoing vertex percentage is ";
    std::cout << zeroOutVertexNum * 1.0 / vertexNum << std::endl;
}

int Graph::getMaxIdx(const std::vector<std::vector<int>> &data){
    int maxIdx = data[0][0]; 
    for(auto it1 = data.begin(); it1 != data.end(); it1++){
        for(auto it2 = it1->begin(); it2 != it1->end(); it2++){            
            if(maxIdx <= (*it2)){
                maxIdx = *it2;
            }
        }
    }
    return maxIdx;
}

int Graph::getMinIdx(const std::vector<std::vector<int>> &data){
    int minIdx = data[0][0]; 
    for(auto it1 = data.begin(); it1 != data.end(); it1++){
        for(auto it2 = it1->begin(); it2 != it1->end(); it2++){            
            if(minIdx >= (*it2)){
                minIdx = *it2;
            }
        }
    }
    return minIdx;
}

void Graph::getRandomStartIndices(std::vector<int> &startIndices){
    size_t num = startIndices.size();
    startIndices.clear();
    size_t n = 0;
    while(n < num){
        int maxIdx = vertexNum - 1;
        int idx = rand()%maxIdx;
        if(vertices[idx]->outVid.empty() || 
           std::find(startIndices.begin(), 
           startIndices.end(), idx) != startIndices.end()
        ){
            continue;
        }
        startIndices.push_back(idx);
        n++;
    }
}

Graph::Graph(const std::string& fname){

    // Check if it is undirectional graph
    auto found = fname.find("ungraph", 0);
    if(found != std::string::npos)
        isUgraph = true;
    else
        isUgraph = false;

    std::vector<std::vector<int>> data;
    loadFile(fname, data);
    vertexNum = getMaxIdx(data) + 1;
    edgeNum = (int)data.size();
    std::cout << "vertex num: " << vertexNum << std::endl;
    std::cout << "edge num: " << edgeNum << std::endl;

    for(int i = 0; i < vertexNum; i++){
        Vertex* v = new Vertex(i);
        vertices.push_back(v);
    }

    for(auto it = data.begin(); it != data.end(); it++){
        int srcIdx = (*it)[0];
        int dstIdx = (*it)[1];
        vertices[srcIdx]->outVid.push_back(dstIdx);
        vertices[dstIdx]->inVid.push_back(srcIdx);
        if(isUgraph && srcIdx != dstIdx){
            vertices[dstIdx]->outVid.push_back(srcIdx);
            vertices[srcIdx]->inVid.push_back(dstIdx);
        }
    }

    for(auto it = vertices.begin(); it != vertices.end(); it++){
        (*it)->inDeg = (int)(*it)->inVid.size();
        (*it)->outDeg = (int)(*it)->outVid.size();
    }
}

void CSR::setBfsParam(
        float _alpha, 
        float _beta, 
        int _hubVertexThreshold, 
        int _cacheThreshold, 
        int _bucketNum)
{
    alpha = _alpha;
    beta = _beta;
    hubVertexThreshold = _hubVertexThreshold;
    cacheThreshold = _cacheThreshold;
    bucketNum = _bucketNum;
}

CSR::CSR(const Graph &g) : vNum(g.vertexNum), eNum(g.edgeNum){

    // Default parameter
    alpha = 0.2;
    beta = 0.1;
    hubVertexThreshold = 1024;
    cacheThreshold = 256;
    bucketNum = 1024;

    // Assign random data to weight though it is not used in bfs.
    weight.resize(eNum);
    for(auto &w : weight){
        w = (rand()%100)/10.0;
    }

    rpao.resize(vNum+1);
    rpai.resize(vNum+1);
    rpao[0] = 0;
    rpai[0] = 0;
    for(int i = 0; i < vNum; i++){
        rpao[i+1] = rpao[i] + g.vertices[i]->outDeg;
        rpai[i+1] = rpai[i] + g.vertices[i]->inDeg;
    }

    for(int i = 0; i < vNum; i++){
        for(auto id : g.vertices[i]->outVid){
            ciao.push_back(id);
        }
        for(auto id : g.vertices[i]->inVid){
            ciai.push_back(id);
        }
    }

}

bool CSR::basicBfs(const int &startIdx, std::ofstream &fhandle){
    // Statistic information
    int readBytes = 0;
    int writeBytes = 0;
    int readBursts = 0;
    int writeBursts = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(vNum);
    std::fill(depth.begin(), depth.end(), -1);

    depth[startIdx] = 0;
    std::vector<int> frontier;
    frontier.push_back(startIdx);
    std::vector<size_t> frontierDist;

    while(!frontier.empty()){
        std::vector<int> nextFrontier;

        // burst read for consecutive frontier
        readBursts += getBurstNum((int)frontier.size(), 4);

        // Traverse the frontier
        for(auto vidx : frontier){
            readBytes += 4; // read vidx
            readBytes += 4 * 2; // read rpao[vidx], rpao[vidx+1]
            readBursts++;
            readBursts += getBurstNum(rpao[vidx+1] - rpao[vidx], 4);
            for(int cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){
                readBytes += 4; // read ciao[cidx]
                int outNgb = ciao[cidx]; // consecutive read

                readBytes += 1; // read depth[outNgb]
                readBursts += 1;
                if(depth[outNgb] == -1){

                    writeBytes += 1; // write depth[outNgb]
                    writeBursts += 1;
                    depth[outNgb] = level + 1;

                    // write outNgb to nextFrontier
                    writeBytes += 4; // consecutive write
                    nextFrontier.push_back(outNgb);
                }
            }
        }
        writeBursts += getBurstNum((int)nextFrontier.size(), 4);

        // Update frontier
        frontierDist.push_back(frontier.size());
        frontier = nextFrontier;
        level++;
    }

    for(auto d : depth){
        fhandle << d << std::endl;
    }

    std::cout << "read/write bytes " << readBytes << " " << writeBytes << std::endl;
    std::cout << "read/write bursts " << readBursts << " " << writeBursts << std::endl;

    //fhandle << "frontier distribution: " << std::endl;
    //for(auto num : frontierDist){
    //    fhandle << num << " ";
    //}
    //fhandle << std::endl;
    fhandle.close();

    return true;
}

// read based top-down bfs
bool CSR::tdBfs(const int &startIdx, std::ofstream &fhandle){

    int readBytes = 0;
    int writeBytes = 0;
    int readBursts = 0;
    int writeBursts = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(vNum);
    std::fill(depth.begin(), depth.end(), -1);
    depth[startIdx] = 0;

    std::vector<int> frontier;
    std::vector<size_t> frontierDist;
    bool eofBfs;

    do{
        fhandle << "Level = " << level << std::endl;
        frontier.clear();
        readBursts += getBurstNum(vNum, 1);
        for(int idx = 0; idx < vNum; idx++){
            readBytes += 1; // read depth[idx]
            if(depth[idx] == level){ 
                frontier.push_back(idx);
                fhandle << idx << " ";
            }
        }
        fhandle << std::endl << std::endl;
        for(auto vidx : frontier){
            readBytes += 4 * 2; // read rpao[vidx], rpao[vidx+1]
            readBursts += 1;
            readBursts += getBurstNum(rpao[vidx+1] - rpao[vidx], 4);
            for(int cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){

                readBytes += 4; // read ciao[cidx]
                int outNgb = ciao[cidx];

                readBytes += 1; // read depth[outNgb]
                readBursts += 1;
                if(depth[outNgb] == -1){

                    writeBytes += 1; // write depth[outNgb]
                    writeBursts += 1;
                    depth[outNgb] = level + 1; 
                }
            }
        }
        frontierDist.push_back(frontier.size());

        // update depth
        level++;
        eofBfs = frontier.empty();

    } while(!eofBfs); 


    for(auto d : depth){
        fhandle << d << std::endl;
    }

    std::cout << "read/write bytes: " << readBytes << " " << writeBytes << std::endl;
    std::cout << "read/write bursts: " << readBursts << " " << writeBursts << std::endl;

    //fhandle << "frontier distribution: " << std::endl;
    //for(auto num : frontierDist){
    //    fhandle << num << " ";
    //}
    //fhandle << std::endl;
    fhandle.close();

    return true;
}

// Read based bottom-up bfs
bool CSR::buBfs(const int &startIdx, std::ofstream &fhandle){

    int readBytes = 0;
    int writeBytes = 0;
    int readBursts = 0;
    int writeBursts = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(vNum);
    std::fill(depth.begin(), depth.end(), -1);
    depth[startIdx] = 0;

    std::vector<int> frontier;
    std::vector<size_t> frontierDist;

    int visitedNgbNum;
    bool eofBfs;

    do{
        readBursts += getBurstNum(vNum, 1);
        for(int idx = 0; idx < vNum; idx++){
            readBytes += 1; // read depth[idx]
            if(depth[idx] == -1){
                frontier.push_back(idx);
            }
        }

        // Traverse the frontier
        // When none of the frontier has a visited incoming neighboring, 
        // it also indicates the end of the BFS. 
        visitedNgbNum = 0;
        for(auto vidx : frontier){
            readBytes += 4 * 2; // read rpai[vidx+1], rpai[vidx]
            readBursts += 1;

            readBursts += getBurstNum(rpai[vidx+1] - rpai[vidx], 4);
            for(int cidx = rpai[vidx]; cidx < rpai[vidx+1]; cidx++){

                readBytes += 4; // read ciai[cidx]
                int inNgb = ciai[cidx];

                readBytes += 1; // read depth[inNgb]
                readBursts += 1;
                if(depth[inNgb] == level){

                    writeBytes += 1; // write depth[vidx]
                    depth[vidx] = level + 1;
                    writeBursts += 1;
                    visitedNgbNum++;
                    break;
                }
            }
        }
        frontierDist.push_back(frontier.size());

        // update depth
        level++;
        eofBfs = frontier.empty() || (visitedNgbNum == 0);
        frontier.clear();

    } while(!eofBfs); 

    for(auto d : depth){
        fhandle << d << std::endl;
    }

    std::cout << "read/write bytes: " << readBytes << " " << writeBytes << std::endl;
    std::cout << "read/write bursts: " << readBursts << " " << writeBursts << std::endl;

    //fhandle << "frontier distribution: " << std::endl;
    //for(auto num : frontierDist){
    //    fhandle << num << " ";
    //}
    //fhandle << std::endl;
    fhandle.close();

    return true;

}

bool CSR::hybridBfs(const int &startIdx, std::ofstream &fhandle){
    int readBytes = 0;
    int writeBytes = 0;
    int readBursts = 0;
    int writeBursts = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(vNum);
    std::fill(depth.begin(), depth.end(), -1);
    depth[startIdx] = 0;

    std::vector<int> frontier;
    std::vector<size_t> frontierDist;
    std::vector<int> frontierHubVertex;
    std::vector<bool> bfsType;
    int totalHubVertexNum = getHubVertexNum();
    int visitedNgbNum = 0;
    bool topDown = true;
    bool eofBfs;

    do{
        // Clean the frontier container
        frontier.clear();
        bfsType.push_back(topDown);

        // get frontier and classfiy the frontier
        int frontierHubVertexNum = 0;
        if(topDown){
            readBursts += getBurstNum(vNum, 1);
            for(int idx = 0; idx < vNum; idx++){
                readBytes += 1; // read depth[idx]
                if(depth[idx] == level){ 
                    frontier.push_back(idx);
                    if(rpao[idx+1] - rpao[idx] >= hubVertexThreshold){
                        frontierHubVertexNum++;
                    }
                }
            }
        }
        else{
            readBursts += getBurstNum(vNum, 1);
            for(int idx = 0; idx < vNum; idx++){
                readBytes += 1; // read depth[idx]
                if(depth[idx] == -1){
                    frontier.push_back(idx);
                }
                else if(depth[idx] == level){
                    if(rpao[idx+1] - rpao[idx] >= hubVertexThreshold){
                        frontierHubVertexNum++;
                    }
                }
            }
        }

        if(topDown){
            for(auto vidx : frontier){
                readBytes += 4 * 2; // read rpao[vidx+1] rpao[vidx]
                readBursts += 1;
                readBursts += getBurstNum(rpao[vidx+1] - rpao[vidx], 4);
                for(int cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){
                    readBytes += 4; // read ciao[cidx]
                    int outNgb = ciao[cidx];

                    readBytes += 1; // read depth[outNgb]
                    readBursts += 1;
                    if(depth[outNgb] == -1){
                        writeBytes += 1; // write depth[outNgb]
                        writeBursts += 1;
                        depth[outNgb] = level + 1; 
                    }
                }
            }
        }
        else{
            // bottom-up traverse
            // When none of the frontier has a visited incoming neighboring, 
            // it also indicates the end of the BFS. 
            visitedNgbNum = 0;
            for(auto vidx : frontier){
                readBytes += 4 * 2; // read rpai[vidx+1] rpai[vidx] 
                readBursts += 1;
                int degree = rpai[vidx+1] - rpai[vidx];

                readBursts += getBurstNum(rpai[vidx+1] - rpai[vidx], 4);
                for(int cidx = rpai[vidx]; cidx < rpai[vidx+1]; cidx++){
                    readBytes += 4; // read ciai[cidx]
                    int inNgb = ciai[cidx];

                    readBytes += 1; // read depth[inNgb]
                    readBursts += 1;
                    if(depth[inNgb] == level){
                        writeBytes += 1; // write depth[vidx]
                        writeBursts += 1;
                        depth[vidx] = level + 1;
                        visitedNgbNum++;
                        break;
                    }
                }
            }


        }

        frontierDist.push_back(frontier.size());
        frontierHubVertex.push_back(frontierHubVertexNum);

        // update depth
        level++;
        eofBfs = frontier.empty() || (topDown == false && visitedNgbNum == 0);
        float hubPercentage = frontierHubVertexNum * 1.0 / totalHubVertexNum; 
        if(topDown && hubPercentage >= alpha){
            topDown = false;
        }
        else if(topDown == false && visitedNgbNum <= beta){
            topDown = true;
        }

    } while(!eofBfs); 


    for(auto d : depth){
        fhandle << d << std::endl;
    }

    std::cout << "read/write bytes: " << readBytes << " " << writeBytes << std::endl;
    std::cout << "read/write bursts: " << readBursts << " " << writeBursts << std::endl;

    //fhandle << "bfs type: " << std::endl;
    //for(auto s : bfsType){
    //    fhandle << s << " ";
    //}
    //fhandle << std::endl;

    //fhandle << "frontier distribution: " << std::endl;
    //for(auto num : frontierDist){
    //    fhandle << num << " ";
    //}
    //fhandle << std::endl;
    fhandle.close();

    std::cout << "total hub vertex amount: " << totalHubVertexNum << std::endl;
    std::cout << "hub vertex amount: " << std::endl;
    for(auto num : frontierHubVertex){
        std::cout << num << " ";
    }
    std::cout << std::endl;

    return true;
}


// This function further takes hub vertex cache into consideration.
bool CSR::cacheHybridBfs(const int &startIdx, std::ofstream &fhandle){
    int readBytes = 0;
    int writeBytes = 0;
    int readBursts = 0;
    int writeBursts = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(vNum);
    std::fill(depth.begin(), depth.end(), -1);
    depth[startIdx] = 0;

    std::vector<int> frontier;
    std::vector<size_t> frontierDist;
    int totalHubVertexNum = getHubVertexNum();
    int visitedNgbNum = 0;
    bool topDown = true;
    bool eofBfs;

    std::vector<int> pingBuffer;
    std::vector<int> pongBuffer;

    do{
        // Clean the frontier container
        frontier.clear();

        // get frontier and classfiy the frontier
        int frontierHubVertexNum = 0;
        if(topDown){
            readBursts += getBurstNum(vNum, 1);
            for(int idx = 0; idx < vNum; idx++){
                readBytes += 1; // read depth[idx]
                if(depth[idx] == level){ 
                    frontier.push_back(idx);
                    int degree = rpao[idx+1] - rpao[idx];
                    if(degree >= cacheThreshold){
                        pingBuffer.push_back(idx);
                    }
                }
            }
        }
        else{
            readBursts += getBurstNum(vNum, 1);
            for(int idx = 0; idx < vNum; idx++){
                readBytes += 1; // read depth[idx]
                if(depth[idx] == -1){
                    frontier.push_back(idx);
                }
                else if(depth[idx] == level){
                    int degree = rpao[idx+1] - rpao[idx];
                    if(degree >= cacheThreshold){
                        pingBuffer.push_back(idx);
                    }
                }
            }
        }

        // Traverse the frontier
        if(topDown){
            for(auto vidx : frontier){
                readBytes += 4 * 2; // read rpao[vidx+1] rpao[vidx]
                readBursts += 1;
                int degree = rpao[vidx+1] - rpao[vidx];
                if(degree >= hubVertexThreshold){
                    frontierHubVertexNum++;
                }
                //if(degree >= cacheThreshold){
                //    pingBuffer.push_back(vidx);
                //}

                readBursts += getBurstNum(rpao[vidx+1] - rpao[vidx], 4);
                for(int cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){

                    readBytes += 4; // read ciao[cidx]
                    int outNgb = ciao[cidx];

                    if(isInBuffer(pingBuffer, outNgb)){
                        continue;
                    }
                    else if(depth[outNgb] == -1){
                        readBytes += 1; // read depth[outNgb]
                        writeBytes += 1; // write depth[outNgb]
                        readBursts += 1;
                        writeBursts += 1;
                        depth[outNgb] = level + 1; 
                    }
                    else{
                        readBytes += 1;
                        readBursts += 1;
                    }
                }
            }
        }
        else{
            // bottom-up traverse
            visitedNgbNum = 0;
            for(auto vidx : frontier){
                readBytes += 4 * 2; // read rpai[vidx+1]
                readBursts += 1;
                int degree = rpai[vidx+1] - rpai[vidx];

                readBursts += getBurstNum(rpai[vidx+1] - rpai[vidx], 4);
                for(int cidx = rpai[vidx]; cidx < rpai[vidx+1]; cidx++){
                    readBytes += 4; // read ciai[cidx]
                    int inNgb = ciai[cidx];

                    if(isInBuffer(pingBuffer, inNgb)){
                        writeBytes += 1; // write depth
                        writeBursts += 1;
                        depth[vidx] = level + 1;
                        if(degree >= hubVertexThreshold){
                            frontierHubVertexNum++;
                        }
                        //if(degree >= cacheThreshold){
                        //    pongBuffer.push_back(vidx);
                        //}
                        visitedNgbNum++;
                        break;
                    }
                    else if(depth[inNgb] == level){
                        readBytes += 1; // read depth[inNgb]
                        writeBytes += 1; // write depth[vidx]
                        writeBursts += 1;
                        readBursts += 1;
                        depth[vidx] = level + 1;
                        if(degree >= hubVertexThreshold){
                            frontierHubVertexNum++;
                        }
                        //if(degree >= cacheThreshold){
                        //    pongBuffer.push_back(vidx);
                        //}
                        visitedNgbNum++;
                        break;
                    }
                    else{
                        readBytes += 1;
                        readBursts += 1;
                    }
                }
            }
        }

        frontierDist.push_back(frontier.size());

        // update depth
        level++;
        eofBfs = frontier.empty() || (topDown == false && visitedNgbNum == 0);
        float hubPercentage = frontierHubVertexNum * 1.0 / totalHubVertexNum; 
        if(topDown && hubPercentage >= alpha){
            topDown = false;
        }
        else if(topDown == false && visitedNgbNum <= beta){
            topDown = true;
            //pingBuffer = pongBuffer;
            //pongBuffer.clear();
        }
        else if(topDown == false){
            //pingBuffer = pongBuffer;
            //pongBuffer.clear();
        }
        pingBuffer.clear();


    } while(!eofBfs); 


    for(auto d : depth){
        fhandle << d << std::endl;
    }

    std::cout << "read/write bytes: " << readBytes << " " << writeBytes << std::endl;
    std::cout << "read/write bursts: " << readBursts << " " << writeBursts << std::endl;

    //fhandle << "frontier distribution: " << std::endl;
    //for(auto num : frontierDist){
    //    fhandle << num << " ";
    //}
    //fhandle << std::endl;
    fhandle.close();

}

// Basically, we want to know the vertex degree distribution 
void CSR::degreeAnalysis(){

    std::map<int, int> inDegDist;
    std::map<int, int> outDegDist;
    for(int idx = 0; idx < vNum; idx++){
        int outDegree = rpao[idx+1] - rpao[idx];
        int inDegree = rpai[idx+1] - rpai[idx];

        if(inDegDist.find(inDegree) == inDegDist.end()){
            inDegDist[inDegree] = 1;
        }
        else{
            inDegDist[inDegree]++;
        }

        if(outDegDist.find(outDegree) == outDegDist.end()){
            outDegDist[outDegree] = 1;
        }
        else{
            outDegDist[outDegree]++;
        }
    }

    std::cout << "in degree distribution: " << std::endl;
    for(auto it = inDegDist.begin(); it != inDegDist.end(); it++){
        std::cout << it->first << " " << it->second << std::endl;
    }

    std::cout << "out degree distribution: " << std::endl;
    for(auto it = outDegDist.begin(); it != outDegDist.end(); it++){
        std::cout << it->first << " " << it->second << std::endl;
    }

}

int CSR::getPotentialCacheSaving(){
    int totalSaving = 0;
    for(int idx = 0; idx < vNum; idx++){
        int degree = rpai[idx+1] - rpai[idx];
        if(degree >= cacheThreshold){
            totalSaving += degree;
        }
    }

    return totalSaving;
}

int CSR::getHubVertexNum(){
    int maxDeg = 0;
    int hubVertexNum = 0;
    for(int idx = 0; idx < vNum; idx++){
        int degree = rpao[idx+1] - rpao[idx];
        if(degree >= hubVertexThreshold){
            hubVertexNum++;
        }
        if(degree > maxDeg) maxDeg = degree;
    }

    // Fix the situation when there is no hub verties
    if(hubVertexNum == 0)
        hubVertexNum = 1;

    return hubVertexNum;
}

bool CSR::isInBuffer(
        const std::vector<int> &buffer, 
        const int &idx)
{
    if(std::find(buffer.begin(), buffer.end(), idx) != buffer.end()){
        return true;
    }
    else{
        return false;
    }
}

int CSR::getBurstNum(int num, int size){
    int length = num * size;
    if(length % GL::burstlen == 0){
        return length/GL::burstlen;
    }
    else{
        return (length/GL::burstlen + 1);
    }
}
