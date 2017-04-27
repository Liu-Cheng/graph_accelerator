#include "graph.h"

void Graph::loadFile(
        const std::string& fname, 
        std::vector<std::vector<long>> &data
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
                std::vector<long>(std::istream_iterator<long>(iss),
                    std::istream_iterator<long>())
                );
    }
    fhandle.close();
}

bool Graph::isValidData(std::vector<std::vector<long>> &data){
    
    // Check if the vertex index starts from 0
    long max_idx = getMaxIdx(data); 
    long min_idx = getMinIdx(data);
    for(auto it = data.begin(); it != data.end(); it++){
        if(it->size() != 2){
            HERE;
            std::cout << "Unexpected vector size. " << std::endl;
            return false;
        }

        // Check if there is self loop
        if((*it)[0] == (*it)[1]){
            HERE;
            std::cout << "Self loop is detected. " << std::endl;
            return false;
        }
    }

    if(min_idx != 0){
        HERE;
        std::cout << " The vertex indices don't start from 0." << std::endl;
        return false;
    }

    // Check if the vertex indices are continuous
    std::vector<long> missing_vids;
    long num = max_idx - min_idx + 1; 
    if(isContinuous(data, missing_vids, num) == false){
        reshapeData(data, missing_vids);

        /*
        missing_vids.clear();
        max_idx = getMaxIdx(data); 
        min_idx = getMinIdx(data);
        num = max_idx - min_idx + 1;
        if(isContinuous(data, missing_vids, num) == false){
            HERE;
            std::cout << "Failed to make the vertex indices continuous." << std::endl;
            exit(EXIT_FAILURE);
        }
        else{
            HERE;
            std::cout << "Graph data has been reshaped successfully." << std::endl;
        }
        */
        std::cout << missing_vids.size() << " missing indices are found." << std::endl;
    }

    return true;
}

bool Graph::isContinuous(
        const std::vector<std::vector<long>> &data,
        std::vector<long> &missing_vids,
        const long &num
        ){

    std::vector<bool> id_exist;
    id_exist.resize(num);
    std::fill(id_exist.begin(), id_exist.end(), false);
    for(auto it1 = data.begin(); it1 != data.end(); it1++){
        for(auto it2 = it1->begin(); it2 != it1->end(); it2++){
            id_exist[*it2] = true;
        }
    }

    long missing_idx = 0;
    for(auto it = id_exist.begin(); it != id_exist.end(); it++){
        if((*it) == false){
            missing_vids.push_back(missing_idx);
        }

        missing_idx++;
    }

    if(missing_vids.empty()) return true;
    else return false;
}

// Make sure the vertex indices are continuous
void Graph::reshapeData(
        std::vector<std::vector<long>> &data,
        std::vector<long> &missing_vids
        )
{
    std::reverse(missing_vids.begin(), missing_vids.end());
    for(auto idx : missing_vids){
        for(auto vit = data.begin(); vit != data.end(); vit++){
            for(auto it = vit->begin(); it != vit->end(); it++){
                if((*it) > idx){
                    (*it)--;
                }
            }
        }
    }
}

long Graph::getMaxIdx(const std::vector<std::vector<long>> &data){
    long max_idx = data[0][0]; 
    for(auto it1 = data.begin(); it1 != data.end(); it1++){
        for(auto it2 = it1->begin(); it2 != it1->end(); it2++){            
            if(max_idx <= (*it2)){
                max_idx = *it2;
            }
        }
    }
    return max_idx;
}

long Graph::getMinIdx(const std::vector<std::vector<long>> &data){
    long min_idx = data[0][0]; 
    for(auto it1 = data.begin(); it1 != data.end(); it1++){
        for(auto it2 = it1->begin(); it2 != it1->end(); it2++){            
            if(min_idx >= (*it2)){
                min_idx = *it2;
            }
        }
    }
    return min_idx;
}

Graph::Graph(const std::string& fname){

    std::vector<std::vector<long>> data;
    loadFile(fname, data);

    if(isValidData(data) == false){
        HERE;
        std::cout << "The data is invalid. " << std::endl;
        exit(EXIT_FAILURE);
    }

    vertex_num = getMaxIdx(data) - getMinIdx(data) + 1;
    edge_num = (long)data.size();
    std::cout << "vertex num: " << vertex_num << std::endl;
    std::cout << "edge num: " << edge_num << std::endl;

    for(int i = 0; i < vertex_num; i++){
        Vertex* v = new Vertex(i);
        vertices.push_back(v);
    }

    for(auto it = data.begin(); it != data.end(); it++){
        long src_idx = (*it)[0];
        long dst_idx = (*it)[1];
        vertices[src_idx]->out_vids.push_back(dst_idx);
        vertices[dst_idx]->in_vids.push_back(src_idx);
    }

    for(auto it = vertices.begin(); it != vertices.end(); it++){
        (*it)->in_deg = (int)(*it)->in_vids.size();
        (*it)->out_deg = (int)(*it)->out_vids.size();
    }
}

CSR::CSR(const Graph &g) : v_num(g.vertex_num), e_num(g.edge_num){
    // Assign random data to weight though it is not used in bfs.
    weight.resize(e_num);
    for(auto &w : weight){
        w = (rand()%100)/10.0;
    }

    rpao.resize(v_num+1);
    rpai.resize(v_num+1);
    rpao[0] = 0;
    rpai[0] = 0;
    for(int i = 0; i < v_num; i++){
        rpao[i+1] = rpao[i] + g.vertices[i]->out_deg;
        rpai[i+1] = rpai[i] + g.vertices[i]->in_deg;
    }

    for(int i = 0; i < v_num; i++){
        for(auto id : g.vertices[i]->out_vids){
            ciao.push_back(id);
        }
        for(auto id : g.vertices[i]->in_vids){
            ciai.push_back(id);
        }
    }
}

// This function will not change the data in CSR
bool CSR::basic_bfs(const long &start_idx){
    int level = 0;
    std::vector<int> depth;
    depth.resize(v_num);
    std::fill(depth.begin(), depth.end(), -1);

    depth[start_idx] = true;
    std::vector<long> frontier;
    frontier.push_back(start_idx);

    while(!frontier.empty()){
        std::vector<long> next_frontier;

        // Traverse the frontier
        for(auto vidx : frontier){
            for(int cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){
                long out_ngb = ciao[cidx];
                if(depth[out_ngb] == -1){
                    depth[out_ngb] = level + 1;
                    next_frontier.push_back(out_ngb);
                }
            }
        }

        // Update frontier
        frontier = next_frontier;
        level++;
    }

    std::string fname = "gold.txt";
    std::ofstream fhandle(fname.c_str());
    if(!fhandle.is_open()){
        HERE;
        std::cout << "Failed to open " << fname << std::endl;
        exit(EXIT_FAILURE);
    }
    for(auto d : depth){
        std::cout << d << " ";
    }
    std::cout << std::endl;

    return true;
}

bool CSR::bfs(const long &start_idx){
    int level = 0;
    std::vector<long> depth;
    depth.resize(v_num);
    std::fill(depth.begin(), depth.end(), -1);
    depth[start_idx] = 0;

    //std::vector<long, int> hub_vertex_depth;
    std::vector<long> lrg_frontier;
    std::vector<long> mid_frontier;
    std::vector<long> sml_frontier;

    int hub_vertex_threshold = 4096;
    int lrg_threshold = 1024;
    int sml_threshold = 16;
    int total_hub_vertex_num = getHubVertexNum(hub_vertex_threshold);
    bool visited_ngb_num = 0;
    bool top_down = true;
    bool end_of_bfs;
    do{
        // Clean the frontier container
        lrg_frontier.clear();
        mid_frontier.clear();
        sml_frontier.clear();

        // get frontier and classfiy the frontier
        int frontier_hub_vertex_num = 0;
        if(top_down){
            for(long idx = 0; idx < v_num; idx++){
                if(depth[idx] == level){ 
                    int degree = rpao[idx+1] - rpao[idx];
                    if(degree <= sml_threshold){
                        sml_frontier.push_back(idx);
                    }
                    else if(degree >= lrg_threshold){
                        lrg_frontier.push_back(idx);
                    }
                    else{
                        mid_frontier.push_back(idx);
                    }

                    // hub vertex buffer may be updated here.
                    if(degree >= hub_vertex_threshold){
                        hub_vertex_threshold++;
                    }
                }
            }
        }
        else{
            for(long idx = 0; idx < v_num; idx++){
                if(depth[idx] == -1){
                    int degree = rpai[idx+1] - rpai[idx];
                    if(degree <= sml_threshold){
                        sml_frontier.push_back(idx);
                    }
                    else if(degree >= lrg_threshold){
                        lrg_frontier.push_back(idx);
                    }
                    else{
                        mid_frontier.push_back(idx);
                    }

                    if(degree >= hub_vertex_threshold){
                        hub_vertex_threshold++;
                    }
                }
            }
        }

        // Traverse the frontier
        if(top_down){
            // top-down traverse
            auto traverse = [&depth, level, this](const std::vector<long> &frontier){
                for(auto vidx : frontier){
                    for(long cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){
                        long out_ngb = ciao[cidx];
                        if(depth[out_ngb] == -1){
                            depth[out_ngb] = level + 1; 
                        }
                    }
                }
            };

            traverse(lrg_frontier);
            traverse(mid_frontier);
            traverse(sml_frontier);
        }
        else{
            // bottom-up traverse
            // When none of the frontier has a visited incoming neighboring, 
            // it also indicates the end of the BFS. 
            visited_ngb_num = 0;
            auto traverse = [&depth, &visited_ngb_num, level, this](const std::vector<long> &frontier){
                for(auto vidx : frontier){
                    for(long cidx = rpai[vidx]; cidx < rpai[vidx+1]; cidx++){
                        long in_ngb = ciai[cidx];
                        if(depth[in_ngb] == level){
                            depth[vidx] = level + 1;
                            visited_ngb_num++;
                            break;
                        }
                    }
                }
            };

            traverse(lrg_frontier);
            traverse(mid_frontier);
            traverse(sml_frontier);

        }

        float hub_percentage = frontier_hub_vertex_num * 1.0 / total_hub_vertex_num; 
        if(hub_percentage >= 0.3){
            top_down = false;
        }

        // update depth
        level++;
        end_of_bfs = lrg_frontier.empty() && mid_frontier.empty() && sml_frontier.empty();
        end_of_bfs = end_of_bfs || (top_down == false && visited_ngb_num == 0);

    } while(!end_of_bfs); 

    /*
    for(auto d : depth){
        std::cout << d << " ";
    }
    std::cout << std::endl;
    */

}

int CSR::getHubVertexNum(const int &threshold){
    int hub_vertex_num = 0;
    for(long idx = 0; idx < v_num; idx++){
        int degree = rpao[idx+1] - rpao[idx];
        if(degree >= threshold){
            hub_vertex_num++;
        }
    }

    // Fix the situation when there is no hub verties
    if(hub_vertex_num == 0)
        hub_vertex_num = 1;

    return hub_vertex_num;
}

