// Test your timer using default template parameters

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <future>
#include <cassert>
#include "Timer.h"

int main(int argc, char* argv[]) {
    using namespace std::chrono_literals;
    using namespace fm::profiling;

    const std::string prog(argv[0]);

#ifdef MULTI_THREAD
    int n_threads{0};
    for (auto i{0}; i<argc; ++i)
        if (strncmp(argv[i],"-n",3)==0) n_threads=std::stoi(argv[i+1]);

    if (n_threads<=0) {
        std::cout<<"\nthread count="<<n_threads<<". Run this prog with: "+prog+" -n num_threds\n\n";
        return 0;
    }

    Timer_t<>::set_thread_count(n_threads);
#endif

	Timer_t<> tmr("main");
    std::string timer_prefix{};

    auto timering=[s=timer_prefix]() {
	   Timer_t<2> t{s+"hello"};
	   { Timer_t<3> t("cout"); std::cout << "Hello Timer_t!\n";}
           { Timer_t<3> t{"indent"}; //std::this_thread::sleep_for(0.5ms);
             { Timer_t<4> t{"++dent"}; std::this_thread::sleep_for(1.5ms);
             }
             { Timer_t<4> t{"++bent"}; std::this_thread::sleep_for(0.3ms);
             }
             { Timer_t<4> t{"++bore"}; //std::this_thread::sleep_for(0.2ms);
                { Timer_t<5> t{"innermost"}; std::this_thread::sleep_for(0.1ms);
                }
             }
           }
	   { Timer_t<3> t{"postdent"}; //std::this_thread::sleep_for(1.2ms);
             { Timer_t<2> t{"inpost"}; std::this_thread::sleep_for(2ms);
             }
           }
	};
    auto timering_more=[s=timer_prefix]() {
        { Timer_t<2> t{s+"posthello"}; //std::this_thread::sleep_for(1.1ms);
          { Timer_t<3> t{"phindent"}; std::this_thread::sleep_for(1ms);}
          { Timer_t<3> t{"++phdent"}; std::this_thread::sleep_for(0.5ms);}
        }
    };

    timering();

#ifdef MULTI_THREAD
    timer_prefix="main::";
    std::vector<std::future<void>> fs;
    for (auto i{1}; i<n_threads; ++i) fs.emplace_back(std::async(timering));
    for (auto& f : fs) f.wait();
    timer_prefix={};
#endif

    timering_more();

#ifdef MULTI_THREAD
    timer_prefix="main::";
    for (auto& f : fs) f=std::move(std::async(std::launch::async,timering_more));
    for (auto& f : fs) f.wait();
#endif

	tmr.stop();
	Timer_t<>::print_record();
}
