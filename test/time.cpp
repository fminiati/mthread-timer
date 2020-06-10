// Test your timer using default template parameters

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <thread>
#include <future>
#include <cassert>
#include "Timer.h"

using Clock=std::chrono::steady_clock;

int main(int argc, char* argv[]) {
    using namespace std::chrono_literals;
    using namespace fm::profiling;

    std::cout << "Hello Time Tests!\n";
    const std::string prog(argv[0]);

    int n_loops{0};
    for (auto i{0}; i<argc; ++i) {
        //if (strncmp(argv[i],"-f",2)==0) filename=argv[i+1];
        if (strncmp(argv[i],"-nl",3)==0) n_loops=std::stoi(argv[i+1]);
    }

    if (n_loops<=0) {
        std::cout<<"\n number of loops="<<n_loops<<".\n Run this prog with: "+prog+" -nl num_loops\n\n";
        return 0;
    }

    std::chrono::duration<double,std::micro> resolution;
    for (auto i{0}; i<n_loops; ) {
        const auto t_i{Clock::now()};
        const auto t_e{Clock::now()};
        if (t_e>t_i) {
            ++i;
            resolution+=t_e-t_i;
        }
    }

    std::chrono::duration<double,std::micro> latency;
    {
        const auto t_i{Clock::now()};
        for (auto i{0}; i<n_loops; ++i) {
            Clock::now();
        }
        const auto t_e{Clock::now()};
        latency=t_e-t_i;
    }

    std::chrono::duration<double,std::micro> timer_overhead;
    {
        for (auto i{0}; i<n_loops; ++i) {
            const auto t_i{Clock::now()};
	        { Timer_t<> tmr("main"); }
            const auto t_e{Clock::now()};
            timer_overhead+=t_e-t_i;
        }
    }

    std::cout<< "\n Measurements for " << n_loops << " loops \n\n";
    std::cout<< " Resolution     : " << resolution.count() << " us\n";
    std::cout<< " Latency        : " << latency.count() << " us\n";
    std::cout<< " Timer Overhead : " << timer_overhead.count() << " us\n\n";
    std::cout<< " Measurements per loop\n\n";
    std::cout<< " Resolution     : " << resolution.count() / n_loops << " us\n";
    std::cout<< " Latency        : " << latency.count() / n_loops << " us\n";
    std::cout<< " Timer Overhead : " << timer_overhead.count() / n_loops << " us\n\n";
}
