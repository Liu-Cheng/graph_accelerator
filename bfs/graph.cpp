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
    size_t num = start_indices.size();
    start_indices.clear();
    size_t n = 0;
    while(n < num){
        int max_idx = vertex_num - 1;
        int idx = rand()%max_idx;
        if(vertices[idx]->out_vids.empty() || std::find(start_indices.begin(), start_indices.end(), idx) != start_indices.end()){
            continue;
        }
        start_indices.push_back(idx);
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

void CSR::setBfsParam(float _alpha, float _beta, int _hub_vertex_threshold, int _cache_threshold, int _bucket_num){
    alpha = _alpha;
    beta = _beta;
    hub_vertex_threshold = _hub_vertex_threshold;
    cache_threshold = _cache_threshold;
    bucket_num = _bucket_num;
}

CSR::CSR(const Graph &g) : v_num(g.vertex_num), e_num(g.edge_num){

    // Default parameter
    alpha = 0.2;
    beta = 0.1;
    hub_vertex_threshold = 1024;
    cache_threshold = 256;
    bucket_num = 1024;

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

// This is the basic bfs algorithm. It will not have any duplicated 
// vertices in the frontier due to the fully sequential inspection. 
// (Any depth update will be visible by the following inspection in 
// the same iteration.) This approach is difficult to parallelize.
bool CSR::basicBfs(const int &start_idx, std::ofstream &fhandle){
    // Statistic information
    int read_bytes = 0;
    int write_bytes = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(v_num);
    std::fill(depth.begin(), depth.end(), -1);

    depth[start_idx] = 0;
    std::vector<int> frontier;
    frontier.push_back(start_idx);
    std::vector<size_t> frontier_distribution;

    while(!frontier.empty()){
        std::vector<int> next_frontier;

        // Traverse the frontier
        for(auto vidx : frontier){
            read_bytes += 4; // read vidx

            read_bytes += 4; // read rpao[vidx+1]
            for(int cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){

                read_bytes += 4; // read ciao[cidx]
                int out_ngb = ciao[cidx];

                read_bytes += 1; // read depth[out_ngb]
                if(depth[out_ngb] == -1){

                    write_bytes += 1; // write depth[out_ngb]
                    depth[out_ngb] = level + 1;

                    // write out_ngb to next_frontier
                    write_bytes += 4;
                    next_frontier.push_back(out_ngb);
                }
            }
        }

        // Update frontier
        frontier_distribution.push_back(frontier.size());
        frontier = next_frontier;
        level++;
    }

    for(auto d : depth){
        fhandle << d << " ";
    }
    fhandle << std::endl;

    std::cout << "read/write bytes " << read_bytes << " " << write_bytes << std::endl;
    
    std::cout << "frontier distribution: " << std::endl;
    for(auto num : frontier_distribution){
        std::cout << num << " ";
    }
    std::cout << std::endl;

    return true;
}

// read based top-down bfs
bool CSR::tdBfs(const int &start_idx, std::ofstream &fhandle){

    int read_bytes = 0;
    int write_bytes = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(v_num);
    std::fill(depth.begin(), depth.end(), -1);
    depth[start_idx] = 0;

    std::vector<int> frontier;
    std::vector<size_t> frontier_distribution;
    bool end_of_bfs;

    do{
        frontier.clear();

        // get frontier 
        for(int idx = 0; idx < v_num; idx++){
            read_bytes += 1; // read depth[idx]
            if(depth[idx] == level){ 
                write_bytes += 4;
                frontier.push_back(idx);
            }
        }

        // Traverse the frontier
        // As we may repeate depth written back to memory, it will be efficient to add a buffer to do batch write-back
        auto traverse = [&depth, level, &read_bytes, &write_bytes, this](const std::vector<int> &frontier){
            for(auto vidx : frontier){
                read_bytes += 4; // read vidx from frontier
                read_bytes += 4; // read rpao[vidx]
                for(int cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){

                    read_bytes += 4; // read ciao[cidx]
                    int out_ngb = ciao[cidx];

                    read_bytes += 1; // read depth[out_ngb]
                    if(depth[out_ngb] == -1){

                        write_bytes += 1; // write depth[out_ngb]
                        depth[out_ngb] = level + 1; 
                    }
                }
            }
        };
        traverse(frontier);
        frontier_distribution.push_back(frontier.size());

        // update depth
        level++;
        end_of_bfs = frontier.empty();

    } while(!end_of_bfs); 


    for(auto d : depth){
        fhandle << d << " ";
    }
    fhandle << std::endl;

    std::cout << "read/write bytes: " << read_bytes << " " << write_bytes << std::endl;
    std::cout << "frontier distribution: " << std::endl;
    for(auto num : frontier_distribution){
        std::cout << num << " ";
    }
    std::cout << std::endl;


    return true;
}

// Read based bottom-up bfs
bool CSR::buBfs(const int &start_idx, std::ofstream &fhandle){

    int read_bytes = 0;
    int write_bytes = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(v_num);
    std::fill(depth.begin(), depth.end(), -1);
    depth[start_idx] = 0;

    std::vector<int> frontier;
    std::vector<size_t> frontier_distribution;

    int visited_ngb_num;
    bool end_of_bfs;

    do{
        for(int idx = 0; idx < v_num; idx++){
            read_bytes += 1; // read depth[idx]
            if(depth[idx] == -1){
                write_bytes += 4; // write idx to mid_frontier
                frontier.push_back(idx);
            }
        }

        // Traverse the frontier
        // When none of the frontier has a visited incoming neighboring, 
        // it also indicates the end of the BFS. 
        visited_ngb_num = 0;
        auto traverse = [&depth, &visited_ngb_num, &read_bytes, &write_bytes, level, this](const std::vector<int> &frontier){
            for(auto vidx : frontier){
                read_bytes += 4; // read vidx from frontier
                read_bytes += 4; // read rpai[vidx+1]
                for(int cidx = rpai[vidx]; cidx < rpai[vidx+1]; cidx++){

                    read_bytes += 4; // read ciai[cidx]
                    int in_ngb = ciai[cidx];

                    read_bytes += 1; // read depth[in_ngb]
                    if(depth[in_ngb] == level){

                        write_bytes += 1; // write depth[vidx]
                        depth[vidx] = level + 1;
                        visited_ngb_num++;
                        break;
                    }
                }
            }
        };
        traverse(frontier);
        frontier_distribution.push_back(frontier.size());

        // update depth
        level++;
        end_of_bfs = frontier.empty() || (visited_ngb_num == 0);
        frontier.clear();

    } while(!end_of_bfs); 

    for(auto d : depth){
        fhandle << d << " ";
    }
    fhandle << std::endl;

    std::cout << "read/write bytes: " << read_bytes << " " << write_bytes << std::endl;
    std::cout << "frontier distribution: " << std::endl;
    for(auto num : frontier_distribution){
        std::cout << num << " ";
    }
    std::cout << std::endl;


    return true;

}

/*
// Read based bottom-up bfs
bool CSR::buBfsModified(const int &start_idx, std::ofstream &fhandle){

    int read_bytes = 0;
    int write_bytes = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(v_num);
    std::fill(depth.begin(), depth.end(), -1);
    depth[start_idx] = 0;

    std::vector<int> ping_frontier;
    std::vector<int> pong_frontier;
    std::vector<size_t> frontier_distribution;

    int visited_ngb_num;
    bool end_of_bfs;

    do{
        // Get the frontier: This can be further optimized, as exploring the frontier may not be 
        // as efficient as exploring the depth directly when the frontier is still large.
        // We don't have to explore all the depth information of the graph. Instead we can 
        // simply inspect the last frontier. Similar idea also applies for the top-down bfs.
        // The only concern is how much does the frontier generation takes the overall bfs time.
        if(level > 0){
            for(auto vidx : pong_frontier){
                read_bytes += 4; // read vidx from pong_frontier
                read_bytes += 1; // read depth[vidx];
                if(depth[vidx] == -1){
                    write_bytes += 4; // write ping_frontier
                    ping_frontier.push_back(vidx);
                }
            }
        }
        else{
            for(int idx = 0; idx < v_num; idx++){
                read_bytes += 1; // read depth[idx]
                if(depth[idx] == -1){
                    write_bytes += 4; // write idx to mid_frontier
                    ping_frontier.push_back(idx);
                }
            }
        }

        // Traverse the frontier
        // When none of the frontier has a visited incoming neighboring, 
        // it also indicates the end of the BFS. 
        visited_ngb_num = 0;
        auto traverse = [&depth, &visited_ngb_num, &read_bytes, &write_bytes, level, this](const std::vector<int> &frontier){
            for(auto vidx : frontier){
                read_bytes += 4; // read vidx from frontier
                read_bytes += 4; // read rpai[vidx+1]
                for(int cidx = rpai[vidx]; cidx < rpai[vidx+1]; cidx++){

                    read_bytes += 4; // read ciai[cidx]
                    int in_ngb = ciai[cidx];

                    read_bytes += 1; // read depth[in_ngb]
                    if(depth[in_ngb] == level){

                        write_bytes += 1; // write depth[vidx]
                        depth[vidx] = level + 1;
                        visited_ngb_num++;
                        break;
                    }
                }
            }
        };
        traverse(ping_frontier);
        frontier_distribution.push_back(ping_frontier.size());

        // update depth
        level++;
        end_of_bfs = ping_frontier.empty() || (visited_ngb_num == 0);
        pong_frontier = ping_frontier;
        ping_frontier.clear();

    } while(!end_of_bfs); 

    for(auto d : depth){
        fhandle << d << " ";
    }
    fhandle << std::endl;

    std::cout << "read/write bytes: " << read_bytes << " " << write_bytes << std::endl;
    std::cout << "frontier distribution: " << std::endl;
    for(auto num : frontier_distribution){
        std::cout << num << " ";
    }
    std::cout << std::endl;


    return true;

}
*/

// This function focuses only on the criteria of switching between top-down and bottom-up bfs
bool CSR::hybridBfs(const int &start_idx, std::ofstream &fhandle){
    int read_bytes = 0;
    int write_bytes = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(v_num);
    std::fill(depth.begin(), depth.end(), -1);
    depth[start_idx] = 0;

    std::vector<int> frontier;
    std::vector<size_t> frontier_distribution;
    std::vector<int> frontier_hub_vertex;
    std::vector<bool> bfs_type;
    int total_hub_vertex_num = getHubVertexNum();
    int visited_ngb_num = 0;
    bool top_down = true;
    bool end_of_bfs;

    do{
        // Clean the frontier container
        frontier.clear();
        bfs_type.push_back(top_down);

        // get frontier and classfiy the frontier
        int frontier_hub_vertex_num = 0;
        if(top_down){
            for(int idx = 0; idx < v_num; idx++){
                read_bytes += 1; // read depth[idx]
                if(depth[idx] == level){ 
                    write_bytes += 4;
                    frontier.push_back(idx);

                }
            }
        }
        else{
            for(int idx = 0; idx < v_num; idx++){
                read_bytes += 1; // read depth[idx]
                if(depth[idx] == -1){
                    write_bytes += 4; // write idx to frontier
                    frontier.push_back(idx);
                }
            }
        }

        // Traverse the frontier
        if(top_down){
            // top-down traverse
            auto traverse = [&frontier_hub_vertex_num, &depth, level, &read_bytes, &write_bytes, this](const std::vector<int> &frontier){
                for(auto vidx : frontier){
                    read_bytes += 4; // read vidx from frontier

                    read_bytes += 4; // read rpao[vidx]
                    int degree = rpao[vidx+1] - rpao[vidx];

                    if(degree >= hub_vertex_threshold){
                        frontier_hub_vertex_num++;
                    }

                    for(int cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){

                        read_bytes += 4; // read ciao[cidx]
                        int out_ngb = ciao[cidx];

                        read_bytes += 1; // read depth[out_ngb]
                        if(depth[out_ngb] == -1){

                            write_bytes += 1; // write depth[out_ngb]
                            depth[out_ngb] = level + 1; 
                        }
                    }
                }
            };
            traverse(frontier);
        }
        else{
            // bottom-up traverse
            // When none of the frontier has a visited incoming neighboring, 
            // it also indicates the end of the BFS. 
            visited_ngb_num = 0;
            auto traverse = [&frontier_hub_vertex_num, &depth, &visited_ngb_num, &read_bytes, &write_bytes, level, this](const std::vector<int> &frontier){
                for(auto vidx : frontier){
                    read_bytes += 4; // read vidx from frontier
                    read_bytes += 4; // read rpai[vidx+1] 
                    int degree = rpai[vidx+1] - rpai[vidx];

                    for(int cidx = rpai[vidx]; cidx < rpai[vidx+1]; cidx++){
                        read_bytes += 4; // read ciai[cidx]
                        int in_ngb = ciai[cidx];

                        read_bytes += 1; // read depth[in_ngb]
                        if(depth[in_ngb] == level){

                            if(degree >= hub_vertex_threshold){
                                frontier_hub_vertex_num++;
                            }

                            write_bytes += 1; // write depth[vidx]
                            depth[vidx] = level + 1;
                            visited_ngb_num++;
                            break;
                        }
                    }
                }
            };

            traverse(frontier);

        }

        frontier_distribution.push_back(frontier.size());
        frontier_hub_vertex.push_back(frontier_hub_vertex_num);

        // update depth
        level++;
        end_of_bfs = frontier.empty() || (top_down == false && visited_ngb_num == 0);
        float hub_percentage = frontier_hub_vertex_num * 1.0 / total_hub_vertex_num; 
        if(top_down && hub_percentage >= alpha){
            top_down = false;
        }
        else if(top_down == false && visited_ngb_num <= beta){
            top_down = true;
        }

    } while(!end_of_bfs); 


    for(auto d : depth){
        fhandle << d << " ";
    }
    fhandle << std::endl;

    std::cout << "read/write bytes: " << read_bytes << " " << write_bytes << std::endl;

    std::cout << "bfs type: " << std::endl;
    for(auto s : bfs_type){
        std::cout << s << " ";
    }
    std::cout << std::endl;

    std::cout << "frontier distribution: " << std::endl;
    for(auto num : frontier_distribution){
        std::cout << num << " ";
    }
    std::cout << std::endl;

    std::cout << "total hub vertex amount: " << total_hub_vertex_num << std::endl;
    std::cout << "hub vertex amount: " << std::endl;
    for(auto num : frontier_hub_vertex){
        std::cout << num << " ";
    }
    std::cout << std::endl;


    return true;
}


// This function further takes hub vertex cache into consideration.
bool CSR::cacheHybridBfs(const int &start_idx, std::ofstream &fhandle){
    int read_bytes = 0;
    int write_bytes = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(v_num);
    std::fill(depth.begin(), depth.end(), -1);
    depth[start_idx] = 0;

    std::vector<int> frontier;
    std::vector<size_t> frontier_distribution;
    int total_hub_vertex_num = getHubVertexNum();
    int visited_ngb_num = 0;
    bool top_down = true;
    bool end_of_bfs;

    std::vector<int> ping_buffer;
    std::vector<int> pong_buffer;

    do{
        // Clean the frontier container
        frontier.clear();

        // get frontier and classfiy the frontier
        int frontier_hub_vertex_num = 0;
        if(top_down){
            for(int idx = 0; idx < v_num; idx++){
                read_bytes += 1; // read depth[idx]
                if(depth[idx] == level){ 
                    write_bytes += 4; // write frontier
                    frontier.push_back(idx);
                }
            }
        }
        else{
            for(int idx = 0; idx < v_num; idx++){
                read_bytes += 1; // read depth[idx]
                if(depth[idx] == -1){
                    write_bytes += 4; // write idx to frontier
                    frontier.push_back(idx);
                }
            }
        }

        // Traverse the frontier
        if(top_down){
            // top-down traverse
            auto traverse = [&ping_buffer, &frontier_hub_vertex_num, &depth, level, &read_bytes, &write_bytes, this](const std::vector<int> &frontier){
                for(auto vidx : frontier){
                    read_bytes += 4; // read vidx from frontier
                    read_bytes += 4; // read rpao[vidx]
                    int degree = rpao[vidx+1] - rpao[vidx];

                    if(degree >= hub_vertex_threshold){
                        frontier_hub_vertex_num++;
                    }
                    if(degree >= cache_threshold){
                        ping_buffer.push_back(vidx);
                    }

                    for(int cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){

                        read_bytes += 4; // read ciao[cidx]
                        int out_ngb = ciao[cidx];

                        if(isInBuffer(ping_buffer, out_ngb)){
                            continue;
                        }
                        else if(depth[out_ngb] == -1){
                            read_bytes += 1; // read depth[out_ngb]
                            write_bytes += 1; // write depth[out_ngb]
                            depth[out_ngb] = level + 1; 
                        }
                        else{
                            read_bytes += 1;
                        }
                    }
                }
            };
            traverse(frontier);
        }
        else{
            // bottom-up traverse
            // When none of the frontier has a visited incoming neighboring, 
            // it also indicates the end of the BFS. 
            visited_ngb_num = 0;
            auto traverse = [&ping_buffer, &pong_buffer, &frontier_hub_vertex_num, &depth, &visited_ngb_num, &read_bytes, &write_bytes, level, this](const std::vector<int> &frontier){
                for(auto vidx : frontier){
                    read_bytes += 4; // read vidx from frontier
                    read_bytes += 4; // read rpai[vidx+1]
                    int degree = rpai[vidx+1] - rpai[vidx];

                    for(int cidx = rpai[vidx]; cidx < rpai[vidx+1]; cidx++){
                        read_bytes += 4; // read ciai[cidx]
                        int in_ngb = ciai[cidx];

                        if(isInBuffer(ping_buffer, in_ngb)){
                            write_bytes += 1; // write depth
                            depth[vidx] = level + 1;
                            if(degree >= hub_vertex_threshold){
                                frontier_hub_vertex_num++;
                            }
                            if(degree >= cache_threshold){
                                pong_buffer.push_back(vidx);
                            }
                            visited_ngb_num++;
                            break;
                        }
                        else if(depth[in_ngb] == level){
                            read_bytes += 1; // read depth[in_ngb]
                            write_bytes += 1; // write depth[vidx]
                            depth[vidx] = level + 1;
                            if(degree >= hub_vertex_threshold){
                                frontier_hub_vertex_num++;
                            }
                            if(degree >= cache_threshold){
                                pong_buffer.push_back(vidx);
                            }
                            visited_ngb_num++;
                            break;
                        }
                        else{
                            read_bytes += 1;
                        }
                    }
                }
            };

            traverse(frontier);

        }

        frontier_distribution.push_back(frontier.size());

        // update depth
        level++;
        end_of_bfs = frontier.empty() || (top_down == false && visited_ngb_num == 0);
        float hub_percentage = frontier_hub_vertex_num * 1.0 / total_hub_vertex_num; 
        if(top_down && hub_percentage >= alpha){
            top_down = false;
        }
        else if(top_down == false && visited_ngb_num <= beta){
            top_down = true;
            ping_buffer = pong_buffer;
            pong_buffer.clear();
        }
        else if(top_down == false){
            ping_buffer = pong_buffer;
            pong_buffer.clear();
        }


    } while(!end_of_bfs); 


    for(auto d : depth){
        fhandle << d << " ";
    }
    fhandle << std::endl;

    std::cout << "read/write bytes: " << read_bytes << " " << write_bytes << std::endl;
    std::cout << "frontier distribution: " << std::endl;
    for(auto num : frontier_distribution){
        std::cout << num << " ";
    }
    std::cout << std::endl;

}

// Basically, we want to know the vertex degree distribution 
void CSR::degreeAnalysis(){

    std::map<int, int> in_degree_distribution;
    std::map<int, int> out_degree_distribution;
    for(int idx = 0; idx < v_num; idx++){
        int out_degree = rpao[idx+1] - rpao[idx];
        int in_degree = rpai[idx+1] - rpai[idx];

        if(in_degree_distribution.find(in_degree) == in_degree_distribution.end()){
            in_degree_distribution[in_degree] = 1;
        }
        else{
            in_degree_distribution[in_degree]++;
        }

        if(out_degree_distribution.find(out_degree) == out_degree_distribution.end()){
            out_degree_distribution[out_degree] = 1;
        }
        else{
            out_degree_distribution[out_degree]++;
        }
    }

    std::cout << "in degree distribution: " << std::endl;
    for(auto it = in_degree_distribution.begin(); it != in_degree_distribution.end(); it++){
        std::cout << it->first << " " << it->second << std::endl;
    }

    std::cout << "out degree distribution: " << std::endl;
    for(auto it = out_degree_distribution.begin(); it != out_degree_distribution.end(); it++){
        std::cout << it->first << " " << it->second << std::endl;
    }

}

int CSR::getHubVertexNum(){
    int max_degree = 0;
    int hub_vertex_num = 0;
    for(int idx = 0; idx < v_num; idx++){
        int degree = rpao[idx+1] - rpao[idx];
        if(degree >= hub_vertex_threshold){
            hub_vertex_num++;
        }
        if(degree > max_degree) max_degree = degree;
    }

    // Fix the situation when there is no hub verties
    if(hub_vertex_num == 0)
        hub_vertex_num = 1;

    return hub_vertex_num;
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

