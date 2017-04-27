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

        Graph(const std::string &file_name);
        ~Graph();

    private:
        bool isContinuous(
                const std::vector<std::vector<long>> &data,
                std::vector<long> &missing_vids,
                const long &num
                );
        void reshapeData(
                std::vector<std::vector<long>> &data,
                std::vector<long> &missing_vids
                );
        long getMaxIdx(const std::vector<std::vector<long>> &data);
        long getMinIdx(const std::vector<std::vector<long>> &data);
        bool isValidData(std::vector<std::vector<long>> &data);
        void loadFile(
                const std::string& file_name,
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
        bool bfs(const long &start_idx);
        bool basic_bfs(const long &start_idx);
        ~CSR();

    private:
        const long v_num;
        const long e_num;

        int getHubVertexNum(const int &threshold);
};


#endif
