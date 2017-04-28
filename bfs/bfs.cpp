#include <ctime>
#include "graph.h"

int main(int argc, char** argv){

    double alpha = 0.85;
    int repeat_num = 10;
    std::vector<long> start_indices{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    Graph* gptr = new Graph("./data/rmat2.txt");
    //Graph* gptr = new Graph("./data/mydata.txt");
    CSR* csr_ptr = new CSR(*gptr);

    // result will be dumped to the files
    std::string fname = argv[1];
    std::ofstream fhandle(fname.c_str());
    if(!fhandle.is_open()){
        HERE;
        std::cout << "Failed to open " << fname << std::endl;
        exit(EXIT_FAILURE);
    }

    double total_time = 0;
    for(auto idx : start_indices){
        std::clock_t begin = clock();
        //csr_ptr->basic_bfs(idx, fhandle);
        csr_ptr->bfs(idx, fhandle);
        std::clock_t end = clock();
        double elapsed_sec = double(end - begin)/CLOCKS_PER_SEC;
        total_time += elapsed_sec;
    }
    fhandle.close();

    double avg_time = total_time / repeat_num;
    std::cout << "BFS rum time: " << avg_time << " seconds." << std::endl;

    return 0;

}
