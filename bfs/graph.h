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
        int idx;
        int in_deg;
        int out_deg;

        std::vector<int> in_vids;
        std::vector<int> out_vids;

        explicit Vertex(int _idx) {
            idx = _idx;
        }

        ~Vertex(){
            // Nothing is done here.
        }

};

class Graph{
    public:
        int vertex_num;
        int edge_num;
        std::vector<Vertex*> vertices; 

        Graph(const std::string &fname);
        ~Graph();
        void getRandomStartIndices(std::vector<int> &start_indices);
        void getStat();

    private:
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
        bool bfs(const int &start_idx, std::ofstream &fhandle);
        bool basic_bfs(const int &start_idx, std::ofstream &fhandle);
        ~CSR();

    private:
        const int v_num;
        const int e_num;

        bool isInBuffer(
                const std::vector<int> &buffer, 
                const int &idx);
        int getHubVertexNum(const int &threshold);
        void hubVertexAnalysis(const int &threshold);
};


#endif
