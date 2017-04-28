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

bool Graph::isValidData(const std::vector<std::vector<long>> &data){
    
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

    return true;
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
bool CSR::basic_bfs(const long &start_idx, std::ofstream &fhandle){
    // Statistic information
    int read_bytes = 0;
    int write_bytes = 0;

    int level = 0;
    std::vector<int> depth;
    depth.resize(v_num);
    std::fill(depth.begin(), depth.end(), -1);

    depth[start_idx] = 0;
    std::vector<long> frontier;
    frontier.push_back(start_idx);

    while(!frontier.empty()){
        std::vector<long> next_frontier;

        // Traverse the frontier
        for(auto vidx : frontier){
            read_bytes += 4; // read vidx

            read_bytes += 4; // read rpao[vidx+1]
            for(int cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){

                read_bytes += 4; // read ciao[cidx]
                long out_ngb = ciao[cidx];

                read_bytes += 4; // read depth[out_ngb]
                if(depth[out_ngb] == -1){

                    write_bytes += 4; // write depth[out_ngb]
                    depth[out_ngb] = level + 1;

                    // write out_ngb to next_frontier
                    write_bytes += 4;
                    next_frontier.push_back(out_ngb);
                }
            }
        }

        // Update frontier
        // suppose we can switch frontier and next_frontier without additional memory read/write. 
        // read_bytes += (int)next_frontier.size() * 4;
        // write_bytes += (int)next_frontier.size() * 4;
        frontier = next_frontier;
        level++;
    }

    for(auto d : depth){
        fhandle << d << " ";
    }
    fhandle << std::endl;

    std::cout << "read " << read_bytes << " bytes from memory." << std::endl;
    std::cout << "write " << write_bytes << " bytes to memory." << std::endl;

    return true;
}

bool CSR::isInBuffer(
        const std::vector<long> &buffer, 
        const long &idx){
    if(std::find(buffer.begin(), buffer.end(), idx) == buffer.end())
        return false;
    else
        return true;
};

bool CSR::bfs(const long &start_idx, std::ofstream &fhandle){
    // Statistic information
    int read_bytes = 0;
    int write_bytes = 0;

    int level = 0;
    std::vector<long> depth;
    depth.resize(v_num);
    std::fill(depth.begin(), depth.end(), -1);
    depth[start_idx] = 0;

    //std::vector<long, int> hub_vertex_depth;
    std::vector<long> lrg_frontier;
    std::vector<long> mid_frontier;
    std::vector<long> sml_frontier;
    std::vector<long> hub_vertex_ping_buffer;
    std::vector<long> hub_vertex_pong_buffer;

    int hub_vertex_threshold = 256;
    int lrg_threshold = 1024;
    int sml_threshold = 16;
    int total_hub_vertex_num = getHubVertexNum(hub_vertex_threshold);
    bool visited_ngb_num = 0;
    bool top_down = true;
    bool end_of_bfs;

    std::cout << "bfs starts: " << std::endl;
    do{
        // Clean the frontier container
        lrg_frontier.clear();
        mid_frontier.clear();
        sml_frontier.clear();

        // get frontier and classfiy the frontier
        int frontier_hub_vertex_num = 0;
        if(top_down) std::cout << "top down bfs." << std::endl;
        else std::cout << "bottom up bfs." << std::endl;
        if(top_down){
            for(long idx = 0; idx < v_num; idx++){

                read_bytes += 4; // read depth[idx]
                if(depth[idx] == level){ 

                    read_bytes += 4; // read rpao[idx+1]
                    int degree = rpao[idx+1] - rpao[idx];
                    if(degree <= sml_threshold){

                        // write idx to sml_frontier
                        write_bytes += 4;
                        sml_frontier.push_back(idx);
                    }
                    else if(degree >= lrg_threshold){

                        // write idx to lrg_frontier
                        write_bytes += 4;
                        lrg_frontier.push_back(idx);
                    }
                    else{

                        // write idx to mid_frontier
                        write_bytes += 4;
                        mid_frontier.push_back(idx);
                    }

                    // hub vertex buffer may be updated here.
                    if(degree >= hub_vertex_threshold){
                        frontier_hub_vertex_num++;
                        hub_vertex_ping_buffer.push_back(idx);
                    }
                }
            }
        }
        else{
            for(long idx = 0; idx < v_num; idx++){
                
                read_bytes += 4; // read depth[idx]
                if(depth[idx] == -1){

                    read_bytes += 4; // read rpai[idx+1]
                    int degree = rpai[idx+1] - rpai[idx];

                    if(degree <= sml_threshold){
                        write_bytes += 4; // write idx to sml_frontier
                        sml_frontier.push_back(idx);
                    }
                    else if(degree >= lrg_threshold){
                        write_bytes += 4; // write idx to lrg_frontier.
                        lrg_frontier.push_back(idx);
                    }
                    else{
                        write_bytes += 4; // write idx to mid_frontier
                        mid_frontier.push_back(idx);
                    }

                    if(degree >= hub_vertex_threshold){
                        frontier_hub_vertex_num++;
                    }
                }
            }
        }

        // Traverse the frontier
        if(top_down){
            // top-down traverse
            auto traverse = [&hub_vertex_ping_buffer, &depth, level, &read_bytes, &write_bytes, this](const std::vector<long> &frontier){
                for(auto vidx : frontier){
                    read_bytes += 4; // read vidx from frontier

                    read_bytes += 4; // read rpao[vidx]
                    int degree = rpao[vidx+1] - rpao[vidx];
                    for(long cidx = rpao[vidx]; cidx < rpao[vidx+1]; cidx++){

                        read_bytes += 4; // read ciao[cidx]
                        long out_ngb = ciao[cidx];

                        // We need to analyze if it is highly possible for 
                        // neighbor vertices pointing to the same level+1 vertex.
                        if(isInBuffer(hub_vertex_ping_buffer, out_ngb)){
                            continue;
                        }
                        else if(depth[out_ngb] == -1){

                            read_bytes += 4; // read depth[out_ngb]
                            write_bytes += 4; // write depth[out_ngb]
                            depth[out_ngb] = level + 1; 
                        }
                        else{
                            read_bytes += 4; // read depth[out_ngb]
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
            auto traverse = [hub_vertex_threshold, &hub_vertex_ping_buffer, &hub_vertex_pong_buffer, &depth, &visited_ngb_num, &read_bytes, &write_bytes, level, this](const std::vector<long> &frontier){
                for(auto vidx : frontier){
                    read_bytes += 4; // read vidx from frontier
                    read_bytes += 4; // read rpai[vidx+1]
                    int degree = rpai[vidx+1] - rpai[vidx];
                    for(long cidx = rpai[vidx]; cidx < rpai[vidx+1]; cidx++){

                        read_bytes += 4; // read ciai[cidx]
                        long in_ngb = ciai[cidx];

                        if(isInBuffer(hub_vertex_pong_buffer, in_ngb)){
                            write_bytes += 4; // write depth[vidx]
                            depth[vidx] = level + 1;
                            visited_ngb_num++;
                            break;
                        }
                        else if(depth[in_ngb] == level){

                            read_bytes += 4; // read depth[in_ngb]
                            // we need to further investigate if the vertices with large in-degree
                            // can help reduce the memory access. If a vertex has a large in degree but small 
                            // out degree, it will not be useful...
                            if(degree > hub_vertex_threshold){
                                hub_vertex_ping_buffer.push_back(vidx);
                            }

                            write_bytes += 4; // write depth[vidx]
                            depth[vidx] = level + 1;
                            visited_ngb_num++;
                            break;
                        }
                        else{
                            read_bytes += 4; // read depth[in_ngb]
                        }
                    }
                }
            };

            traverse(lrg_frontier);
            traverse(mid_frontier);
            traverse(sml_frontier);

        }
        // print buffer size
        std::cout << "ping buffer: " << hub_vertex_ping_buffer.size() << std::endl;
        std::cout << "pong buffer: " << hub_vertex_pong_buffer.size() << std::endl;

        // update depth
        level++;
        end_of_bfs = lrg_frontier.empty() && mid_frontier.empty() && sml_frontier.empty();
        end_of_bfs = end_of_bfs || (top_down == false && visited_ngb_num == 0);

        float hub_percentage = frontier_hub_vertex_num * 1.0 / total_hub_vertex_num; 
        if(hub_percentage >= 0.08){
            top_down = false;
        }

        if(top_down == false){
            hub_vertex_pong_buffer = hub_vertex_ping_buffer;
            hub_vertex_ping_buffer.clear();
        }

    } while(!end_of_bfs); 


    for(auto d : depth){
        fhandle << d << " ";
    }
    fhandle << std::endl;

    std::cout << "read " << read_bytes << " bytes from memory." << std::endl;
    std::cout << "write " << write_bytes << " bytes to memory." << std::endl;

}

int CSR::getHubVertexNum(const int &threshold){
    int max_degree = 0;
    int hub_vertex_num = 0;
    for(long idx = 0; idx < v_num; idx++){
        int degree = rpao[idx+1] - rpao[idx];
        if(degree >= threshold){
            hub_vertex_num++;
        }
        if(degree > max_degree) max_degree = degree;
    }

    // Fix the situation when there is no hub verties
    if(hub_vertex_num == 0)
        hub_vertex_num = 1;

    std::cout << "max degree is " << max_degree << std::endl;

    return hub_vertex_num;
}

