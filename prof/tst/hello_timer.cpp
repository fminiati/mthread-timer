// Test your timer using default template parameters
//
#include <iostream>
#include <thread>
#include "Timer.h"

int main(int argc, char* argv[]) {
    using namespace std::chrono_literals;
    using namespace fm_profile;

	Timer_t<> tmr("main");

	{  Timer_t<2> t{"hello"};
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
	}
        { Timer_t<2> t{"posthello"}; //std::this_thread::sleep_for(1.1ms);
          { Timer_t<3> t{"phindent"}; std::this_thread::sleep_for(1ms);}
          { Timer_t<3> t{"++phdent"}; std::this_thread::sleep_for(0.5ms);}
        }

	tmr.stop();
	Timer_t<>::print_record();
}
