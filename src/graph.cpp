#include "graph.h"

void Graph::loadFile(
        const std::string& fname, 
        std::vector<std::vector<int>> &data
        ){

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

// Check the number of vertices without out going neighbors,
// as it affects the BFS results.
void Graph::getStat(){
    int zero_outgoing_vertex_num = 0;
    for(auto it = vertices.begin(); it != vertices.end(); it++){
        if((*it)->out_vids.empty()){
            zero_outgoing_vertex_num++;
        }
    }
    std::cout << "Zero outgoing vertex percentage is " << zero_outgoing_vertex_num * 1.0 / vertex_num << std::endl;
}

int Graph::getMaxIdx(const std::vector<std::vector<int>> &data){
    int max_idx = data[0][0]; 
    for(auto it1 = data.begin(); it1 != data.end(); it1++){
        for(auto it2 = it1->begin(); it2 != it1->end(); it2++){            
            if(max_idx <= (*it2)){
                max_idx = *it2;
            }
        }
    }
    return max_idx;
}

int Graph::getMinIdx(const std::vector<std::vector<int>> &data){
    int min_idx = data[0][0]; 
    for(auto it1 = data.begin(); it1 != data.end(); it1++){
        for(auto it2 = it1->begin(); it2 != it1->end(); it2++){            
            if(min_idx >= (*it2)){
                min_idx = *it2;
            }
        }
    }
    return min_idx;
}

void Graph::getRandomStartIndices(std::vector<int> &start_indices){
    start_indices.clear();
    int n = 0;
    while(n < GL::startNum){
        int max_idx = vertex_num - 1;
        int idx = rand()%max_idx;
        if(vertices[idx]->out_vids.empty() || std::find(start_indices.begin(), start_indices.end(), idx) != start_indices.end()){
            continue;
        }
        start_indices.push_back(idx);
        n++;
    }
}

void Graph::printOngb(int vidx){
    for(auto x : vertices[vidx]->out_vids){
        std::cout << x << " ";
    }
    std::cout << std::endl;
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
    vertex_num = getMaxIdx(data) + 1;
    edge_num = (int)data.size();
    std::cout << "vertex num: " << vertex_num << std::endl;
    std::cout << "edge num: " << edge_num << std::endl;

    for(int i = 0; i < vertex_num; i++){
        Vertex* v = new Vertex(i);
        vertices.push_back(v);
    }

    for(auto it = data.begin(); it != data.end(); it++){
        int src_idx = (*it)[0];
        int dst_idx = (*it)[1];
        vertices[src_idx]->out_vids.push_back(dst_idx);
        vertices[dst_idx]->in_vids.push_back(src_idx);
        if(isUgraph && src_idx != dst_idx){
            vertices[dst_idx]->out_vids.push_back(src_idx);
            vertices[src_idx]->in_vids.push_back(dst_idx);
        }
    }

    for(auto it = vertices.begin(); it != vertices.end(); it++){
        (*it)->in_deg = (int)(*it)->in_vids.size();
        (*it)->out_deg = (int)(*it)->out_vids.size();
    }
}


