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

class Vertex {
    public:
        long idx;
        int in_deg;
        int out_deg;

        std::vector<long> in_vids;
        std::vector<long> out_vids;

        explicit Vertex(int _idx) {
            idx = _idx;
        }

        ~Vertex(){
            // Nothing is done here.
        }

};

class Graph{
    public:
        long vertex_num;
        long edge_num;
        std::vector<Vertex*> vertices; 

        Graph(const std::string &fname);
        ~Graph();

    private:
        long getMaxIdx(const std::vector<std::vector<long>> &data);
        long getMinIdx(const std::vector<std::vector<long>> &data);
        bool isValidData(const std::vector<std::vector<long>> &data);
        void loadFile(
                const std::string& fname,
                std::vector<std::vector<long>> &data
                );

};

// Basically a different data structure that describes the graph
class CSR{
    public:
        std::vector<float> weight; // It is not actually used in bfs
        std::vector<long> rpao;    // row pointer array based on outgoing vertices
        std::vector<long> ciao;    // column index array based on outgoing vertices
        std::vector<long> rpai;    // row pointer array based on incoming vertices
        std::vector<long> ciai;    // column index array based on incoming vertices

        // The CSR is constructed based on the simple graph
        explicit CSR(const Graph &g);
        bool bfs(const long &start_idx, std::ofstream &fhandle);
        bool basic_bfs(const long &start_idx, std::ofstream &fhandle);
        ~CSR();

    private:
        const long v_num;
        const long e_num;

        bool isInBuffer(
                const std::vector<long> &buffer, 
                const long &idx);
        int getHubVertexNum(const int &threshold);
};


#endif
