#include <ctime>
#include "graph.h"

int main(int argc, char** argv){

    int repeatNum = 1;
    std::vector<int> startIndices{1429};
    std::string fname = argv[1];
    std::string bfsType = argv[2];
    std::string graphType = argv[3];

    Graph* gptr;
    if(graphType == "dblp"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/dblp.ungraph.txt");
    }
    else if(graphType == "youtube"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/youtube.ungraph.txt");
    }
    else if(graphType == "lj"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/lj.ungraph.txt");
    }
    else if(graphType == "pokec"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/pokec-relationships.txt");
    }
    else if(graphType == "wiki-talk"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/wiki-Talk.txt");
    }
    else if(graphType == "lj1"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/LiveJournal1.txt");
    }
    else if(graphType == "orkut"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/orkut.ungraph.txt");
    }
    else if(graphType == "rmat"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-2m-256m.txt");
    }
    else if(graphType == "rmat1k10k"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat1k10k.txt");
    }
    else if(graphType == "rmat10k100k"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat10k100k.txt");
    }
    else if(graphType == "twitter"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/twitter_rv.txt");
    }
    else if(graphType == "friendster"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/friendster.ungraph.txt");
    }
    else if(graphType == "example"){
        gptr = new Graph("/home/liucheng/gitrepo/graph-data/rmat-1k-10k.txt");
    }
    else{
        gptr = new Graph("./data/mydata.txt");
    }

    gptr->getRandomStartIndices(startIndices);
    //gptr->getStat();
    CSR* csrPtr = new CSR(*gptr);
    //csrPtr->degreeAnalysis();
    if(graphType == "dblp"){
        csrPtr->setBfsParam(0.2, 5000, 128, 64, 1024);
    }
    else if(graphType == "youtube"){
        csrPtr->setBfsParam(0.2, 5000, 1024, 256, 1024);
    }
    else if(graphType == "pokec"){
        csrPtr->setBfsParam(0.2, 5000, 4096, 128, 1024);
    }
    else if(graphType == "lj"){
        csrPtr->setBfsParam(0.2, 5000, 1024, 256, 1024);
    }
    else if(graphType == "wiki-talk"){
        csrPtr->setBfsParam(0.2, 5000, 1024, 256, 1024);
    }

    // result will be dumped to the files
    std::ofstream fhandle(fname.c_str());
    if(!fhandle.is_open()){
        HERE;
        std::cout << "Failed to open " << fname << std::endl;
        exit(EXIT_FAILURE);
    }

    double totalTime = 0;
    for(auto idx : startIndices){
        std::cout << "startIdx = " << idx << std::endl;
        std::clock_t begin = clock();
        if(bfsType == "basic"){
            csrPtr->basicBfs(idx, fhandle);
        }
        else if(bfsType == "td"){
            csrPtr->tdBfs(idx, fhandle);
        }
        else if(bfsType == "bu"){
            csrPtr->buBfs(idx, fhandle);
        }
        else if(bfsType == "hybrid"){
            csrPtr->hybridBfs(idx, fhandle);
        }
        else if(bfsType == "cache"){
            csrPtr->cacheHybridBfs(idx, fhandle);
            std::cout << "potential cache saving: ";
            std::cout << csrPtr->getPotentialCacheSaving() << std::endl;
        }
        else{
            HERE;
            std::cout << "Unknown bfs type." << std::endl;
            exit(EXIT_FAILURE);
        }

        std::clock_t end = clock();
        double elapsedTime = double(end - begin)/CLOCKS_PER_SEC;
        totalTime += elapsedTime;
    }
    fhandle.close();

    double avgTime = totalTime / repeatNum;
    std::cout << "BFS rum time: " << avgTime << " seconds." << std::endl;

    return 0;

}
