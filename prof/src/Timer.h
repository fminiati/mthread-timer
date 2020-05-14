// F. Miniati
// Timer.h: a simple templated instrument to time code execution.
// Timer measures the number of funciton calls and their total duration using templated
// parameter type I and R defaulted to size_t and std::chrono::duration<double>.
// Time intervals are measured with a clock of template type Clock, which defaults
// to std::chrono::high_resolution_clock type.
// A measurement starts at Timer's construction and stops and is recorded as Timer
// goes out of scope, unless the stop() function has already been called.
// A granularity template parameter allows to switch on and off Timers for
// progressively lighter blockss of code.
// All measurements are recorded on the same object, a static map.
// Timers' name tags are appended to a static string so that embedded functions calls 
// with the desired level of granularity can be traced to measure their footprint on the
// calling function performance.
//
// to use compile with: -DUSE_TIMER[=TIMER_GRANULARITY] -DTHREAD_COUNT[=NUM_THREADS] \
//                      -DTIMER_OVERHEAD -DTIMER_STATS
//
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
#include <cstring>
#include <cassert>
#include <cmath>

namespace fm_profile {

    #ifdef USE_TIMER
    constexpr unsigned TimerGranularityLim{1+USE_TIMER};
    #ifdef TIMER_OVERHEAD
        constexpr bool MeasureTimerOverHead{true};
    #endif
    #ifdef TIMER_STATS
        constexpr bool ComputeStats{true};
    #else
        constexpr bool ComputeStats{false};
    #endif
    #else
    constexpr unsigned TimerGranularityLim{0};
    #endif


    #ifdef THREAD_COUNT
    constexpr unsigned ThreadCount{THREAD_COUNT};
    #endif

    // time record
    template <typename I=size_t, typename R=double>
    struct TimeRecord {
        I _count; // number of calls
        std::chrono::duration<R> _duration; // calls duratio
        std::chrono::duration<R> _overhead; // measurements overhad
#ifdef TIMER_STATS
        struct { R _rms, _max; } _stats{};
#endif
    };


    // register for time records: map measurements to identifiers
    template <typename Record=TimeRecord<>, typename Label=std::string, template <typename,typename> typename Map=std::unordered_map>
    using TimeRegister = Map<Label,Record>;

    template <typename T> struct time_register_type_traits {};
    template <typename Record, typename Label>
    struct time_register_type_traits<TimeRegister<Record,Label>> {
        using label_t =Label;
        using record_t=Record;
    };
    template <typename T> using register_label_t = typename time_register_type_traits<T>::label_t;
    template <typename T> using register_record_t= typename time_register_type_traits<T>::record_t;


#ifdef THREAD_COUNT
    // In multithread case, different threads write conncurrently to a pool of Registers,
    // arranged in a static array. Threads' access to Registers is controlled by atomic_gates
    // which enforce atomicity of RMW operations. Upon request the GateKeeper searches for a
    // free gate (a hash function converts thread_id to an index, which is then incremented till
    // it reaches a free gate) and returns the array index of the corrensponding Register.
    // Based on simple tests, performance remains negligibly affected by collisions up to a
    // load factors < 70%. Hence, we use about 40% more Registers/Gates than threads.
    constexpr unsigned RegisterCount{1+static_cast<unsigned>(std::floor(ThreadCount/0.7))};

    // wait-free utility to manage atomic access to Registers by threads in multi-thread case
    template <typename Key, unsigned GateCount, typename Hash=std::hash<Key>>
    struct AtomicGatesKeeper {

        // find a free gate, lock it and return its index
        auto lock_gate(Key&& a_key) {
            assert(_free_gates.fetch_sub(1,std::memory_order_acq_rel)>0);

            auto gid = Hash{}(std::move(a_key)) % GateCount;
            while (_gates[gid].test_and_set(std::memory_order_acquire))
                ++gid %= GateCount;

            return gid;
        }

        // free locked gate
        void free_gate(const auto a_gid) {
            assert(_free_gates.fetch_add(1,std::memory_order_acq_rel)>=0);

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

        // std::atomic_flag initialised
        struct atomic_gate : public std::atomic_flag {
            atomic_gate() : std::atomic_flag{ATOMIC_FLAG_INIT} {}
        };

        private:
        static std::array<atomic_gate,GateCount> _gates;
        static std::atomic<int> _free_gates;
    };
#endif


    // use granulrity param to define when timer is onduty 
    constexpr bool OnDuty(const unsigned g) {return g<TimerGranularityLim;}

    // use alias template to set Timer on/off duty based on input granularity
    template <bool B, typename R, typename C> class Timer;

    template <unsigned Granularity=1,
              typename Register=TimeRegister<>,
              typename Clock=std::chrono::high_resolution_clock>
    using Timer_t = Timer<OnDuty(Granularity),Register,Clock>;


    // default timer does nothing because it is off duty
    template <bool B, typename R, typename C> class Timer {
        public:
        Timer(const std::string a_name) {}
	    void stop() {}
        static void print_record() {}
        ~Timer() {}
    };

    // timer measures when on duty
    constexpr bool onduty{true};
    template <typename Register, typename Clock> class Timer<onduty,Register,Clock> {

        // print out measurements
	    void static print_record(const register_label_t<Register> a_record_label,
                                 const register_record_t<Register>& a_record,
                                 const Register& a_register,
                                 const unsigned a_level);

#ifdef THREAD_COUNT
        // measurementes are stored in stratic registers;
        static std::array<Register,RegisterCount> _registers;

        // manages atomic access to array of register
        AtomicGatesKeeper<std::thread::id,RegisterCount> _gates_keeper{};

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

        public:
        // constructor
        Timer(const std::string a_name) {

            // measure timer overhead
            if constexpr (MeasureTimerOverHead) _t_up=Clock::now();

#ifdef THREAD_COUNT
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
            if constexpr (MeasureTimerOverHead)
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

            // stop timer
            const auto t_dw=Clock::now();

            // invalidate destructor call to this function
            _stop=false;

            // get records...
#ifdef THREAD_COUNT
            auto& [count,duration,overhead]=_registers[_register_gate][_call_sequence];
#else
            auto& [count,duration,overhead]=_register[_call_sequence];
#endif
            // ... update them
            ++count;
            duration += t_dw-_t_up;
            _call_sequence.resize(_call_sequence.size()-_name.size());

            if constexpr (ComputeStats) {
                const auto dt=(t_dw-_t_up).count();
                auto& [t_rms,t_max]=_records[_seq]._stats;
                t_rms += dt*dt;
                t_max = std::max(t_max,dt);
            }
#ifdef THREAD_COUNT
            // final book-keeping: free gate if this thread no more uses it
            if (--_thread_timer_cnt==0)
                _gates_keeper.free_gate(_register_gate);
#endif
            // measure timer overhead
            if constexpr (MeasureTimerOverHead) overhead += Clock::now()-t_dw;
        }

#ifdef THREAD_COUNT
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
                           if (const auto& th_record=th_rit->extract(label); !th_record.empty()) {
                               const auto& [th_cnt,th_dur,th_ohd]=th_record.mapped();
                               record._count   +=th_cnt;
                               record._duration+=th_dur;
                               record._overhead+=th_ohd;
                           }
                       }
                   }
                   // these records are now unique and can be merged into single register
                   a_register.merge(*r_it);
               }
            }
        } _consolidate;
        using f_consolidate_t=std::function<void(Register&,std::array<Register,RegisterCount>)>;
#else
        static struct { void operator()() {} } _consolidate;
        using f_consolidate_t=std::function<void()>;
#endif

        // print out measurements
        static void print_record(f_consolidate_t a_consolidate_records=_consolidate) {

#ifndef THREAD_COUNT
            // now print out records
            print_record("",{},_register,0);
#else
            // freeze access to registers during print out
            // this implies waiting till all Timers are done recording
            AtomicGatesKeeper<std::thread::id,RegisterCount>::lock_all_gates();

            // use a_consolidate input function to consolidate thread's records
            Register full_record{};
            a_consolidate_records(full_record,_registers);

            // now print out records
            print_record("",{},full_record,0);

            // free access to all registers
            AtomicGatesKeeper<std::thread::id,RegisterCount>::free_all_gates();
#endif
        }
    };

    template <typename Register, typename C>
	void Timer<onduty,Register,C>::print_record(const register_label_t<Register> a_record_label,
                                                const register_record_t<Register>& a_record,
                                                const Register& a_register,
                                                const unsigned a_level) {
        constexpr auto tabsize{3};

        // declare static root Timer set to zero-level Timer's
        static std::pair<register_label_t<Register>,register_record_t<Register>> root;

        // fat lambda that helps printing individual measurements
        auto prnt_rec=[indent=a_level*tabsize](const auto name, const auto rec, const auto es_cnt) {

            using namespace std::string_literals;

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
            if (es_cnt>0) {
                std::cout << std::string(indent,' ') << std::left << std::setfill('.')
                << std::setw(NFW-1) << name << ":" << tab
                << std::setw(PFW) << std::setfill(' ') << _cnt_string(PFW,std::to_string(rec._count)) << tab
                << std::setw(DFW) << std::scientific << std::setprecision(3) << rec._duration.count() << tab
                << std::setw(PFW) << std::scientific << std::setprecision(2) << rec._duration.count()/es_cnt << tab
                << std::setw(RFW) << rec._duration.count()/root.second._duration.count();
                if constexpr (MeasureTimerOverHead) {
                    std::cout << tab << std::setw(PFW) << std::scientific <<std::setprecision(2) << rec._overhead.count() << tab
                    << std::setw(PFW) << std::scientific << std::setprecision(2) << rec._overhead.count()/rec._duration.count();
                }
                if constexpr (ComputeStats) {
                    if (name!="total") {
                        const auto t_ave = rec._dur.count()/rec._cnt;
                        const auto t_rms = std::sqrt(rec._stats._rms/rec._cnt-t_ave*t_ave);
                        std::cout << std::scientific << std::setprecision(3)
                                << std::setw(PFW) << t_ave << tab << std::setw(PFW) << t_rms << tab
                                << std::setw(PFW) << rec._stats._max;
                    }
                }
                std::cout<<"\n";
            }
            // here print out a_label'ed measurement and prepare header for nested measurements
            else {
                std::cout << std::string(CW,'=') << "\n"
                << name << ": call-cnt: " << rec._count
                << ", time: "<< std::scientific << rec._duration.count() << " s\n"
                << std::string(CW,'-') << "\n";
                std::cout << std::setw(indent) << std::setfill(' ') << std::left << "L-"+std::to_string(indent/tabsize)
                << _cnt_string(NFW,"name"s) << tab << _cnt_string(PFW,"call-cnt"s) << tab << _cnt_string(DFW,"time[s]"s) << tab
                << _cnt_string(PFW,"t/t_caller"s) << tab << _cnt_string(RFW,"t/t_"+root.first);
                if constexpr (MeasureTimerOverHead)
                    std::cout << tab << _cnt_string(PFW,"tmr-ohd[s]"s) << tab << _cnt_string(PFW,"t/t_callee"s);
                std::cout<<"\n";
                 if constexpr (ComputeStats) {
                     std::cout << tab << _cnt_string(PFW,"t[s]/cnt") << tab << _cnt_string(PFW,"t_rms[s]")
                               << tab << _cnt_string(PFW,"t_max[s]");
                 }
            }
        };

        if (a_register.size()==1) { // special case of only one entry

            const auto& [name,record] = *a_register.cbegin();
            prnt_rec(name,record,0);
        }
        else { // time-record of labeled scope

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
                    total._overhead+=subrec._overhead;
                }
                prnt_rec("total",total,a_record._duration.count());
            }

            // analyse nested-timers
            for (const auto& [name,subrec] : nested_records) {
                if (a_level==0) root={name,subrec};
                print_record(a_record_label+name+"::",subrec,a_register,a_level+1);
            }
        }
        if (a_level==0) std::cout<<std::string(80,'-')<<"\n";
    }


    template <typename T, typename C>
    thread_local register_label_t<T> Timer<onduty,T,C>::_call_sequence;
#ifdef THREAD_COUNT
    template <typename K, unsigned N, typename H>
    std::array<typename AtomicGatesKeeper<K,N,H>::atomic_gate,N>
    AtomicGatesKeeper<K,N,H>::_gates{};

    template <typename T, typename C>
    thread_local unsigned Timer<onduty,T,C>::_register_gate{};

    template <typename K, unsigned N, typename H>
    std::atomic<int> AtomicGatesKeeper<K,N,H>::_free_gates{N};

    template <typename T, typename C>
    thread_local unsigned Timer<onduty,T,C>::_thread_timer_cnt{0};

    template <typename T, typename C>
    std::array<T,RegisterCount> Timer<onduty,T,C>::_registers{};
#else
    template <typename T, typename C> T Timer<onduty,T,C>::_register{};
#endif
};
