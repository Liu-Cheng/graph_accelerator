# Standalone cycle-accurate BFS accelerator
In order to explore various optimization techniques on the BFS accelerator, 
we developed a cycle-accurate model using SystemC. As the memory access is 
critical to the accelerator's performance, we use Ramulator \[1\] as the memory model.

[\[1\] Kim et al. *Ramulator: A Fast and Extensible DRAM Simulator.* IEEE CAL
2015.](https://users.ece.cmu.edu/~omutlu/pub/ramulator_dram_simulator-ieee-cal15.pdf)  

## Getting Started
[SystemC-2.3.1](http://accellera.org/downloads/standards/systemc) 
is used for the accelerator design. You need to download and compile it first. Then 
you may change the SystemC library path accordingly in the Makefile. 
In order to compile Ramulator, a C++11 compiler (e.g., `clang++` or `g++-5`) is required. 
You may refer to [Ramulator git repo](https://github.com/CMU-SAFARI/ramulator) 
for more details. When the compilation environment is ready, you can compile 
and run the example using the following commands.   

$ cd graph_accelerator  
$ make   
$ make exe 
       
### Contributors
- Cheng Liu (National University of Singapore) 
