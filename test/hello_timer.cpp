// Test your timer using default template parameters

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <thread>
#include <future>
#include <cassert>
#include "Timer.h"

int main(int argc, char *argv[])
{
    using namespace std::chrono_literals;
    using namespace fm::profiling;

    std::cout << "Hello Timer_t!\n";
    const std::string prog(argv[0]);

    std::string filename{};
    int n_loops{0};
    for (auto i{0}; i < argc; ++i)
    {
        if (strncmp(argv[i], "-f", 2) == 0)
            filename = argv[i + 1];
        if (strncmp(argv[i], "-nl", 3) == 0)
            n_loops = std::stoi(argv[i + 1]);
    }

#ifndef MULTI_THREAD
    if (n_loops <= 0)
    {
        std::cout << "\n number of loops=" << n_loops
                  << ".\n Run this prog with: " + prog + " -nl num_loops [-f output_filename]\n\n";
        return 0;
    }
#else
    int n_threads{0};
    for (auto i{0}; i < argc; ++i)
        if (strncmp(argv[i], "-nt", 3) == 0)
            n_threads = std::stoi(argv[i + 1]);

    if (n_threads <= 0 || n_loops <= 0)
    {
        std::cout << "\n thread count=" << n_threads << " and number of loops=" << n_loops << ".\n"
                  << " Run this prog with: " + prog + " -nt num_threads -nl num_loops [-f output_filename]\n\n";
        return 0;
    }

    Timer_t<>::set_thread_count(n_threads);
#endif

    Timer_t<> tmr("main");
    std::string timer_prefix{};

    auto timering = [&s = timer_prefix]() {
        Timer_t<2> t{s + "hello"};
        {
            Timer_t<3> t("cout");
        }
        {
            Timer_t<3> t{"indent"}; //std::this_thread::sleep_for(0.5ms);
            {
                Timer_t<4> t{"++dent"};
                std::this_thread::sleep_for(1.5ms);
            }
            {
                Timer_t<4> t{"++bent"};
                std::this_thread::sleep_for(0.3ms);
            }
            {
                Timer_t<4> t{"++bore"}; //std::this_thread::sleep_for(0.2ms);
                {
                    Timer_t<5> t{"innermost"};
                    std::this_thread::sleep_for(0.1ms);
                }
            }
        }
        {
            Timer_t<3> t{"postdent"}; //std::this_thread::sleep_for(1.2ms);
            {
                Timer_t<2> t{"inpost"};
                std::this_thread::sleep_for(2ms);
            }
        }
    };
    auto timering_more = [&s = timer_prefix]() {
        {
            Timer_t<2> t{s + "posthello"}; //std::this_thread::sleep_for(1.1ms);
            {
                Timer_t<3> t{"phindent"};
                std::this_thread::sleep_for(1ms);
            }
            {
                Timer_t<3> t{"++phdent"};
                std::this_thread::sleep_for(0.5ms);
            }
        }
    };

    for (auto i{0}; i < n_loops; ++i)
    {

        timering();

#ifdef MULTI_THREAD
        timer_prefix = "main::";
        std::vector<std::future<void>> fs;
        for (auto i{1}; i < n_threads; ++i)
            fs.emplace_back(std::async(timering));
        for (auto &f : fs)
            f.wait();
        timer_prefix = {};
#endif

        timering_more();

#ifdef MULTI_THREAD
        timer_prefix = "main::";
        for (auto &f : fs)
            f = std::move(std::async(std::launch::async, timering_more));
        for (auto &f : fs)
            f.wait();
        timer_prefix = {};
#endif
    }

    tmr.stop();
    if (filename.size())
    {
        std::fstream file("timer.txt", std::ios_base::out | std::ios_base::trunc);
        Timer_t<>::print_record(file);
        file.close();
    }
    else
    {
        Timer_t<>::print_record();
    }
}
