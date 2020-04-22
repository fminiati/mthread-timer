# Test your timer using default template parameters
#

#include <iostream>
#include <thread>
#include "Timer.h"

int main(int argc, char* argv[]) {
    using namespace std::chrono_literals;
    using namespace fm_profile;

	Timer<> tmr("main");

	{  Timer<> t{"hello"};
	   { Timer<> t("cout"); std::cout << "Hello Timer!\n";}
           { Timer<> t{"indent"}; //std::this_thread::sleep_for(0.5ms);
             { Timer<> t{"++dent"}; std::this_thread::sleep_for(1.5ms);
             }
             { Timer<> t{"++bent"}; std::this_thread::sleep_for(0.3ms);
             }
             { Timer<> t{"++bore"}; //std::this_thread::sleep_for(0.2ms);
                { Timer<> t{"innermost"}; std::this_thread::sleep_for(0.1ms);
                }
             }
           }
	   { Timer<> t{"postdent"}; //std::this_thread::sleep_for(1.2ms);
             { Timer<> t{"inpost"}; std::this_thread::sleep_for(2ms);
             }
           }
	}
        { Timer<> t{"posthello"}; //std::this_thread::sleep_for(1.1ms);
          { Timer<> t{"phindent"}; std::this_thread::sleep_for(1ms);}
          { Timer<> t{"++phdent"}; std::this_thread::sleep_for(0.5ms);}
        }

	tmr.stop();
	Timer<>::print_record();
}
