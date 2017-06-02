#include <ctime>
#include "graph.h"

int main(int argc, char** argv){

    int repeat_num = 1;
    std::vector<int> start_indices{478};
    std::string fname = argv[1];
    std::string bfs_type = argv[2];
    std::string graph_type = argv[3];

    Graph* gptr;
    if(graph_type == "dblp"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/dblp.ungraph.txt");
    }
    else if(graph_type == "youtube"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/youtube.ungraph.txt");
    }
    else if(graph_type == "lj"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/lj.ungraph.txt");
    }
    else if(graph_type == "pokec"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/pokec-relationships.txt");
    }
    else if(graph_type == "wiki-talk"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/wiki-Talk.txt");
    }
    else if(graph_type == "lj1"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/LiveJournal1.txt");
    }
    else if(graph_type == "orkut"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/orkut.ungraph.txt");
    }
    else if(graph_type == "rmat"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-2m-256m.txt");
    }
    else if(graph_type == "twitter"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/twitter_rv.txt");
    }
    else if(graph_type == "friendster"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/friendster.ungraph.txt");
    }
    else if(graph_type == "example"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-1k-10k.txt");
    }
    else{
        gptr = new Graph("./data/mydata.txt");
    }

    gptr->getRandomStartIndices(start_indices);
    gptr->getStat();
    CSR* csr_ptr = new CSR(*gptr);
    //csr_ptr->degreeAnalysis();
    csr_ptr->setBfsParam(0.2, 5000, 1024, 256, 1024);

    // result will be dumped to the files
    std::ofstream fhandle(fname.c_str());
    if(!fhandle.is_open()){
        HERE;
        std::cout << "Failed to open " << fname << std::endl;
        exit(EXIT_FAILURE);
    }

    double total_time = 0;
    for(auto idx : start_indices){
        std::clock_t begin = clock();
        if(bfs_type == "basic"){
            csr_ptr->basicBfs(idx, fhandle);
        }
        else if(bfs_type == "td"){
            csr_ptr->tdBfs(idx, fhandle);
        }
        else if(bfs_type == "bu"){
            csr_ptr->buBfs(idx, fhandle);
        }
        else if(bfs_type == "hybrid"){
            csr_ptr->hybridBfs(idx, fhandle);
        }
        else if(bfs_type == "cache"){
            csr_ptr->cacheHybridBfs(idx, fhandle);
        }
        else{
            HERE;
            std::cout << "Unknown bfs type." << std::endl;
            exit(EXIT_FAILURE);
        }

        std::clock_t end = clock();
        double elapsed_sec = double(end - begin)/CLOCKS_PER_SEC;
        total_time += elapsed_sec;
    }
    fhandle.close();

    double avg_time = total_time / repeat_num;
    std::cout << "BFS rum time: " << avg_time << " seconds." << std::endl;

    return 0;

}
