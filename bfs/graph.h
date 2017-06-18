#ifndef __GRAPH_H__
#define __GRAPH_H__

#include <vector>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdlib>
#include <sstream>
#include <cmath>
#include <map>
#include <algorithm>

#define HERE do {std::cout << "File: " << __FILE__ << " Line: " << __LINE__ << std::endl;} while(0)

class GL{
    public:
        static int burstlen;
};

class Vertex {
    public:
        int idx;
        int inDeg;
        int outDeg;

        std::vector<int> inVid;
        std::vector<int> outVid;

        explicit Vertex(int _idx) {
            idx = _idx;
        }

        ~Vertex(){
            // Nothing is done here.
        }

};

class Graph{
    public:
        int vertexNum;
        int edgeNum;
        std::vector<Vertex*> vertices; 

        Graph(const std::string &fname);
        ~Graph();
        void getRandomStartIndices(std::vector<int> &startIndices);
        void getStat();

    private:
        bool isUgraph;
        int getMaxIdx(const std::vector<std::vector<int>> &data);
        int getMinIdx(const std::vector<std::vector<int>> &data);
        void loadFile(
                const std::string& fname,
                std::vector<std::vector<int>> &data
                );

};

// Basically a different data structure that describes the graph
class CSR{
    public:
        std::vector<float> weight; // It is not actually used in bfs
        std::vector<int> rpao;    // row pointer array based on outgoing vertices
        std::vector<int> ciao;    // column index array based on outgoing vertices
        std::vector<int> rpai;    // row pointer array based on incoming vertices
        std::vector<int> ciai;    // column index array based on incoming vertices

        // The CSR is constructed based on the simple graph
        explicit CSR(const Graph &g);

        void setBfsParam(
                float _alpha, 
                float _beta, 
                int _hubVertexThreshold, 
                int _cacheThreshold, 
                int _bucketNum);

        // hybrid read based bfs with hub vertex cache
        bool cacheHybridBfs(const int &startIdx, std::ofstream &fhandle); 

        // hybrid read based bfs
        bool hybridBfs(const int &startIdx, std::ofstream &fhandle); 

        // frontier-based top-down bfs
        bool basicBfs(const int &startIdx, std::ofstream &fhandle); 

        // read based top-down bfs
        bool tdBfs(const int &startIdx, std::ofstream &fhandle); 

        // read based bottom-up bfs
        bool buBfs(const int &startIdx, std::ofstream &fhandle); 

        int getPotentialCacheSaving();
        void degreeAnalysis();
        ~CSR();

    private:
        const int vNum;
        const int eNum;
        float alpha;
        float beta;
        int hubVertexThreshold;
        int cacheThreshold;
        int bucketNum;

        bool isInBuffer(
                const std::vector<int> &buffer, 
                const int &idx);
        int getHubVertexNum();
        int getBurstNum(int num, int size);
};


#endif
