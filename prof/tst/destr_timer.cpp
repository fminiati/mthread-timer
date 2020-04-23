// Test destructor-call sequence. Specifically check that Timer<>'s destructor
// gets called last within scope, as you would expect (from stack unwindind).
//
#include <iostream>
#include <thread>
#include "Timer.h"

class Foo {
	protected:
	std::string _s;

	public:
	Foo(const std::string s)
		: _s(s) {}
	virtual ~Foo() {
		std::cout << "foo's calling "<<_s<<"'s destructor\n";
	}
};
class Bar : public Foo {
	public:
		Bar(const std::string s)
			: Foo::Foo(s) {}
		virtual ~Bar() {
		std::cout << "bar's calling "<<_s<<"'s destructor\n";
	}
};

int main(int argc, char* argv[]) {
    using namespace std::chrono_literals;
    using namespace fm_profile;

	Timer<> tmr("main");

	{  Timer<> t{"hello"};
	    std::cout << "Hello Timer!\n";
	    Foo foo("foo_hello");
	    Bar bar("bar_hello");
		std::this_thread::sleep_for(1ms);
        { Timer<> t{"indent"}; std::this_thread::sleep_for(0.5ms);
	    Bar bar("bar_indent");
	    Foo foo("foo_indent after bar_indent");
            { Timer<> t{"++dent"}; std::this_thread::sleep_for(1.5ms);
	    Bar bar("bar_++dent");
	    Foo foo("foo_++dent after bar_++dent");
            }
            { Timer<> t{"++bent"}; std::this_thread::sleep_for(0.3ms);
	    Foo foo("foo_++bent");
	    Bar bar("bar_++bent after foo_++bent");
            }
            { Timer<> t{"++bore"}; std::this_thread::sleep_for(0.2ms);
	    Foo foo("foo_++bore");
                { Timer<> t{"innermost"}; std::this_thread::sleep_for(0.1ms);
	    Bar bar("bar_innermostr after foo_++bore");
                }
            }
        }
        { Timer<> t{"postdent"}; std::this_thread::sleep_for(1.2ms);
	  Foo* foobar=new Bar("foobar");
          { Timer<> t{"inpost"}; std::this_thread::sleep_for(2ms);
          }
	  delete foobar;
        }
	}
    { Timer<> t{"posthello"}; //std::this_thread::sleep_for(1.1ms);
	    Foo foo("foo_posthello");
	    Bar bar("bar_ after foo_posthello");
      { Timer<> t{"phindent"}; std::this_thread::sleep_for(1ms);}
      { Timer<> t{"++phdent"}; std::this_thread::sleep_for(0.5ms);}
    }

	tmr.stop();
	//Timer<>::print_record();
}
