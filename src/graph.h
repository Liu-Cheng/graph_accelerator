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
#include "common.h"

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
        void printOngb(int vidx);

    private:
        bool isUgraph;
        int getMaxIdx(const std::vector<std::vector<int>> &data);
        int getMinIdx(const std::vector<std::vector<int>> &data);
        void loadFile(
                const std::string& fname,
                std::vector<std::vector<int>> &data
                );

};

#endif
