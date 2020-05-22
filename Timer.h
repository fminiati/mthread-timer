//
// Copyright (C) 2020 Francesco Miniati <francesco.miniati@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#ifndef TIMER_H
#define TIMER_H

#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <utility>
#include <functional>
#include <atomic>
#include <cassert>
#include <cmath>

namespace fm::profiling {

#ifdef USE_TIMER
    constexpr unsigned TimerGranularityLim{1+USE_TIMER};
    #ifdef TIMER_OVERHEAD
        constexpr bool TimerOverHead{true};
    #else
        constexpr bool TimerOverHead{false};
    #endif
    #ifdef TIMER_STATS
        constexpr bool TimerStats{true};
    #else
        constexpr bool TimerStats{false};
    #endif
#else
    constexpr unsigned TimerGranularityLim{0};
    constexpr bool     TimerOverHead{false};
    constexpr bool     TimerStats{false};
#endif


    // time record
    template <typename I=size_t, typename R=double> struct TimeRecord {
        I _count;                           // number of calls
        std::chrono::duration<R> _duration; // calls duration
#ifdef TIMER_OVERHEAD
        std::chrono::duration<R> _overhead; // measurements overhad
#endif
#ifdef TIMER_STATS
        struct { R _rms, _max; } _stats{};
#endif
    };

    // type traits for record
    template <typename T> struct time_record_type_traits {};
    template <typename I, typename R> struct time_record_type_traits<TimeRecord<I,R>> {
        using cnt_t =I;
        using rep_t =R;
    };
    template <typename T> using record_cnt_t = typename time_record_type_traits<T>::cnt_t;
    template <typename T> using record_rep_t = typename time_record_type_traits<T>::rep_t;


    // register for time records: map measurements to identifiers
    template <typename Record=TimeRecord<>, typename Label=std::string, template <typename,typename> typename Map=std::unordered_map>
    using TimeRegister = Map<Label,Record>;

    // type traits for register
    template <typename T> struct time_register_type_traits {};
    template <typename R, typename L> struct time_register_type_traits<TimeRegister<R,L>> {
        using record_t=R;
        using label_t =L;
    };
    template <typename T> using register_label_t = typename time_register_type_traits<T>::label_t;
    template <typename T> using register_record_t= typename time_register_type_traits<T>::record_t;


#ifdef MULTI_THREAD
    // In multithread case, different threads write conncurrently to a pool of Registers,
    // arranged in a static array. Threads' access to Registers is controlled by atomic_gates
    // which enforce atomicity of RMW operations. Upon request the AtomicGates searches for a
    // free gate (a hash function converts thread_id to an index, which is then incremented till
    // it reaches a free gate) and returns the array index of the corrensponding Register.
    // Based on simple tests, performance remains negligibly affected by collisions up to a
    // load factors < 70%. Hence, we use about 40% more Registers/Gates than threads.

    // wait-free utility to manage atomic access to Registers by threads in multi-thread case
    template <typename Key=std::thread::id, typename Hash=std::hash<Key>> struct AtomicGates {

        // find a free gate, lock it and return its index
        auto lock_gate(Key&& a_key) {
            assert(_gates.size()>0 && _free_gates.fetch_sub(1,std::memory_order_acq_rel)>0);

            const auto gate_count{_gates.size()};
            auto gid = Hash{}(std::move(a_key)) % gate_count;
            while (_gates[gid].test_and_set(std::memory_order_acquire))
                ++gid %= gate_count;

            return gid;
        }

        // free locked gate
        void free_gate(const auto a_gid) {
            assert(a_gid<_gates.size() && _free_gates.fetch_add(1,std::memory_order_acq_rel)>=0);

            _gates[a_gid].clear(std::memory_order_release);
        }

        // freeze all gates to safely use registers
        static void lock_all_gates() {
            for (auto& g : _gates) { // let them finish first
                while (g.test_and_set(std::memory_order_acquire))
                    std::this_thread::yield();
            }
        }

        // open all gates
        static void free_all_gates() {
            for (auto& g : _gates) g.clear(std::memory_order_release);
        }

        // use thread count to set up atomic gates, return gate count
        static auto setup_gates(const auto a_thread_count) {
            assert(a_thread_count>0 && _gates.size()==0);

            // set gate count large enough for efficiet allocation of atomic access
            // to registers (see comment above)
            const auto gate_count=1+static_cast<unsigned>(std::floor(a_thread_count/0.7));

            // ugly but necessary...
            std::vector<atomic_gate> v(gate_count); _gates.swap(v);
    #ifndef NDEBUG
            // initialise number of free gates
            _free_gates.store(gate_count,std::memory_order_release);
    #endif
            return gate_count;
        }

        // std::atomic_flag initialised (waiting for c++20)
        struct atomic_gate : public std::atomic_flag {
            atomic_gate() : std::atomic_flag{ATOMIC_FLAG_INIT} {}
        };

        private:
        static std::vector<atomic_gate> _gates;
    #ifndef NDEBUG
        static std::atomic<int> _free_gates;
    #endif
    };
#else
    template <typename V=void> using AtomicGates=void;
#endif


    // use granulrity param to define when timer is onduty 
    constexpr bool OnDuty(const unsigned g) {return g<TimerGranularityLim;}

    // use alias template to set Timer on/off duty based on input granularity
    template <bool B, typename R, typename C, typename A> class Timer;

    template <unsigned Granularity=1,
              typename Register=TimeRegister<>,
              typename Clock=std::chrono::high_resolution_clock,
              typename AtomicMapper=AtomicGates<>>
    using Timer_t = Timer<OnDuty(Granularity),Register,Clock,AtomicMapper>;


    // default timer does nothing because it is off duty
    template <bool B, typename R, typename C, typename A> class Timer {
        public:
        Timer(const std::string a_name) {}
	    void stop() {}
        static void print_record() {}
        ~Timer() {}
    };


    // timer measures when on duty
    constexpr bool onduty{true};

    template <typename Register, typename Clock, typename AtomicMapper>
    class Timer<onduty,Register,Clock,AtomicMapper> {
#ifdef MULTI_THREAD
        // measurementes are stored in stratic registers;
        static std::vector<Register> _registers;

        // manages atomic access, hence gates, to array of register
        AtomicMapper _gates_keeper{};

        // thread local static variables
        thread_local static unsigned _register_gate;
        thread_local static unsigned _thread_timer_cnt;
#else
        static Register _register;
#endif
        // Label tracking call sequence
        thread_local static register_label_t<Register> _call_sequence;

        // member data
        std::string _name;
        std::chrono::time_point<Clock> _t_up;
        bool _stop{true};

        // print out measurements
        static void print_record(const register_label_t<Register> a_record_label,
                                 const register_record_t<Register>& a_record,
                                 const Register& a_register,
                                 const unsigned  a_level,
                                 std::ostream&   a_ostream);

        public:
        // constructor
        Timer(const std::string a_name) {
            // measure timer overhead
            if constexpr (TimerOverHead) _t_up=Clock::now();

#ifdef MULTI_THREAD
            // first thread timer opens gate to a register
            if (_thread_timer_cnt==0)
                _register_gate=_gates_keeper.lock_gate(std::this_thread::get_id());

            // count timers in this thread
            ++_thread_timer_cnt;

            // reference relevant register
            Register& _register=_registers[_register_gate];
#endif
            // record (thread's) timer's name
            _name=(_call_sequence.size()>0?"::":"")+a_name;

            // update (thread's) function calls tree
            _call_sequence+=_name;

            // measure timer overhead
            if constexpr (TimerOverHead)
               _register[_call_sequence]._overhead += Clock::now()-_t_up;

            // timer starts
            _t_up=Clock::now();
        }

        // stop and record measurement at destruction unless !_stop
        ~Timer() {
            if (_stop) stop();
        }

        // stop measurement before going out of scope
        void stop() {
            // stop time
            const auto t_dw=Clock::now();
            const std::chrono::duration<record_rep_t<register_record_t<Register>>> duration=t_dw-_t_up;

            // invalidate destructor call to this function
            _stop=false;

            // get the record...
#ifdef MULTI_THREAD
            auto& record=_registers[_register_gate][_call_sequence];
#else
            auto& record=_register[_call_sequence];
#endif
            // ... update it
            ++record._count;
            record._duration += duration;

            if constexpr (TimerStats) {
                auto& [t_rms,t_max]=record._stats;
                const auto dt=duration.count();
                t_rms += dt*dt;
                t_max = std::max(t_max,dt);
            }

            _call_sequence.resize(_call_sequence.size()-_name.size());

#ifdef MULTI_THREAD
            // final book-keeping: free gate if this thread no more uses it
            if (--_thread_timer_cnt==0)
                _gates_keeper.free_gate(_register_gate);
#endif
            // measure timer overhead
            if constexpr (TimerOverHead) record._overhead += Clock::now()-t_dw;
        }

#ifdef MULTI_THREAD
        // thread count is not used except for setting the register count
        static void set_thread_count(const auto a_thread_count) {
            assert(a_thread_count>0 && _registers.size()==0);

            // initialize keeper and get register count = gate count
            const auto register_count = AtomicMapper::setup_gates(a_thread_count);

            // resize register
            _registers.resize(register_count);
        };

        // consolidate threads records into single printable record:
        // this function may need differerntiate depending on application, so there will be
        // a default version and the possibility for the user to override it.
        static struct {
            void operator() (auto& a_register, auto a_all_registers) {
               const auto first{std::begin(a_all_registers)};
               const auto last{std::end(a_all_registers)};

               // sort in descending order
               std::sort(first,last,[](const auto& a,const auto& b){return a.size()>b.size();});

               // for each register...
               for (auto r_it{first}; r_it!=last; ++r_it) {
                   // for each record...
                   for (auto [label,record] : *r_it) {
                       // search for same record-labels in successive records
                       for (auto th_rit{r_it+1}; th_rit!=last; ++th_rit) {
                           // if you find consolidate into current record
                           if (const auto& th_node=th_rit->extract(label); !th_node.empty()) {
                               const auto& th_record=th_node.mapped();
                               record._count   +=th_record._count;
                               record._duration+=th_record._duration;
                               if constexpr (TimerOverHead)
                                   record._overhead+=th_record._overhead;
                               if constexpr (TimerStats) {
                                   record._stats._rms+=th_record._stats._rms;
                                   record._stats._max+=(record._stats._max,th_record._stats._max);
                               }
                           }
                       }
                   }
                   // these records are now unique and can be merged into single register
                   a_register.merge(*r_it);
               }
            }
        } _consolidate;
        using f_consolidate_t=std::function<void(Register&,std::vector<Register>)>;
#else
        static struct { void operator()() {} } _consolidate;
        using f_consolidate_t=std::function<void()>;
#endif

        // print out measurements
        static void print_record(std::ostream& a_ostream=std::cout, f_consolidate_t a_consolidate_records=_consolidate) {
#ifndef MULTI_THREAD
            // now print out records
            print_record("",{},_register,0,a_ostream);
#else
            // freeze access to registers during print out
            // this implies waiting till all Timers are done recording
            AtomicMapper::lock_all_gates();

            // use a_consolidate input function to consolidate thread's records
            Register full_record{};
            a_consolidate_records(full_record,_registers);

            // now print out records
            print_record("",{},full_record,0,a_ostream);

            // free access to all registers
            AtomicMapper::free_all_gates();
#endif
        }
    };

    template <typename Register, typename C, typename A>
	void Timer<onduty,Register,C,A>::print_record(const register_label_t<Register> a_record_label,
                                                  const register_record_t<Register>& a_record,
                                                  const Register& a_register,
                                                  const unsigned  a_level,
                                                  std::ostream&   a_ostream) {
        // declare static root Timer set to zero-level Timer's
        static std::pair<register_label_t<Register>,register_record_t<Register>> root;

        // fat lambda that helps printing individual measurements
        auto prnt_rec=[&a_ostream,a_level](const auto name, const auto rec, const auto es_count) {
            // useful scope and constants
            using namespace std::string_literals;
            constexpr auto tabsize{3};
            const auto indent=a_level*tabsize;

            // formatting width sizes
            constexpr int NFW{14}, DFW{10}, PFW{10}, CW{80}, TW{2};
            const int RFW{std::max(PFW,4+(int)root.first.size())};

            // formatting string output
            auto _cnt_string=[](const auto w, const auto s) {
                const auto s2=(w-std::size(s))/2;
                return std::string(s2,' ')+s+std::string(s2,' ');
            };

            // here print out a single nested timer measurement
            const std::string tab(TW,' ');
            if (es_count>0) {
                a_ostream << std::string(indent,' ') << std::left << std::setfill('.')
                << std::setw(NFW-1) << name << ":" << tab
                << std::setw(PFW) << std::setfill(' ') << _cnt_string(PFW,std::to_string(rec._count)) << tab
                << std::setw(DFW) << std::scientific << std::setprecision(3) << rec._duration.count() << tab
                << std::setw(PFW) << std::scientific << std::setprecision(2) << rec._duration.count()/es_count << tab
                << std::setw(RFW) << rec._duration.count()/root.second._duration.count();
                if constexpr (TimerOverHead) {
                    a_ostream << tab << std::setw(PFW) << rec._overhead.count()
                              << tab << std::setw(PFW) << rec._overhead.count()/rec._duration.count();
                }
                if constexpr (TimerStats) {
                    if (name!="total") {
                        const auto t_ave = rec._duration.count()/rec._count;
                        const auto t_rms = std::sqrt(rec._stats._rms/rec._count-t_ave*t_ave);
                        a_ostream << tab << std::setw(PFW) << t_ave
                                  << tab << std::setw(PFW) << t_rms << tab << std::setw(PFW) << rec._stats._max;
                    }
                }
                a_ostream <<"\n";
            }
            // here print out a_label'ed measurement and prepare header for nested measurements
            else {
                a_ostream << std::string(CW,'=') << "\n"
                << name << ": call-cnt: " << rec._count
                << ", time: "<< std::scientific << rec._duration.count() << " s";
                if constexpr (TimerOverHead)
                    a_ostream << ", overi-head: " << std::setw(PFW) << rec._overhead.count() << " s";
                a_ostream << '\n' << std::string(CW,'-') << "\n";

                // this avoids printing out headers for one entry case
                if (es_count==0) {
                    a_ostream << std::setw(indent) << std::setfill(' ') << std::left << "L-"+std::to_string(indent/tabsize)
                    << _cnt_string(NFW,"name"s) << tab << _cnt_string(PFW,"call-cnt"s) << tab << _cnt_string(DFW,"t[s]"s) << tab
                    << _cnt_string(PFW,"t/t_en-scp"s) << tab << _cnt_string(RFW,"t/t_"+root.first);

                    if constexpr (TimerOverHead)
                        a_ostream << tab << _cnt_string(PFW,"tmr_oh[s]"s) << tab << _cnt_string(PFW,"tmr_oh/t"s);

                    if constexpr (TimerStats) {
                        a_ostream << tab << _cnt_string(PFW,"t[s]/cnt"s) << tab << _cnt_string(PFW,"t_rms[s]"s)
                                << tab << _cnt_string(PFW,"t_max[s]"s);
                    }
                    a_ostream <<"\n";
                }
            }
        };

        // special case of only one entry
        if (a_register.size()==1) {
            const auto& [name,record] = *a_register.cbegin();
            prnt_rec(name,record,-1);
        }
        // time-record of labeled scope
        else {
            // find records of nested scopes
            std::vector<std::pair<register_label_t<Register>,register_record_t<Register>>> nested_records;

            for (const auto& [name,rec] : a_register) {
                if (a_record_label==name.substr(0,a_record_label.size()) &&
                    name.find("::",a_record_label.size())==std::string::npos) {
                    nested_records.emplace_back(std::make_pair(name.substr(a_record_label.size()),rec) );
                }
            }

            // sort records by duration
            std::sort(nested_records.begin(), nested_records.end(),
                      [](const auto& a, const auto& b){ return a.second._duration>b.second._duration; });

            if (a_record._count>0 && nested_records.size()>0) {
                // print only if record exists and contains other timers
                prnt_rec(a_record_label.substr(0,a_record_label.size()-2),a_record,0);

                // print finer timer-mesurementes and total
                register_record_t<Register> total{};
                for (const auto& [name,subrec] : nested_records) {
                    prnt_rec (name,subrec,a_record._duration.count());
                    total._count+=subrec._count;
                    total._duration+=subrec._duration;
                    if constexpr (TimerOverHead)
                        total._overhead+=subrec._overhead;
                }
                prnt_rec("total",total,a_record._duration.count());
            }

            // analyse nested-timers
            for (const auto& [name,subrec] : nested_records) {
                if (a_level==0) root={name,subrec};
                print_record(a_record_label+name+"::",subrec,a_register,a_level+1,a_ostream);
            }
        }
        if (a_level==0) a_ostream <<std::string(80,'-')<<"\n\n\n";
    }

    // define static variables
    template <typename T, typename C, typename A>
    thread_local register_label_t<T> Timer<onduty,T,C,A>::_call_sequence{};
#ifndef MULTI_THREAD
    template <typename T, typename C, typename A>
    T Timer<onduty,T,C,A>::_register{};
#else
    template <typename T, typename C, typename A>
    std::vector<T> Timer<onduty,T,C,A>::_registers{};

    template <typename T, typename C, typename A>
    thread_local unsigned Timer<onduty,T,C,A>::_register_gate{};

    template <typename T, typename C, typename A>
    thread_local unsigned Timer<onduty,T,C,A>::_thread_timer_cnt{0};

    template <typename K, typename H>
    std::vector<typename AtomicGates<K,H>::atomic_gate> AtomicGates<K,H>::_gates{};

    #ifndef NDEBUG
    template <typename K, typename H>
    std::atomic<int> AtomicGates<K,H>::_free_gates{};
    #endif
#endif
};

#endif // TIMER_H
//
// Copyright (C) 2020 Francesco Miniati <francesco.miniati@gmail.com>
