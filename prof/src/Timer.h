// F. Miniati
// Timer.h: a simple templated instrument to time code execution.
// Timer measure number of funciton calls and their total duration using templated
// parameter type I and R defaulted to size_t and std::chrono::duration<double>.
// Time intervals are measured with a clock of template type Clock, which defaults
// to std::chrono::high_resolution_clock type.
// A measurement starts at Timer's construction and stops+ is recorded as Timer
// goes out of scope, unless the stop() function has already been called.
// A granularity template parameter allows to switch on and off Timers for
// progressively lighter scopes of code.
// Measurements are recorded on a static unique_map, so all Timers record in the same object.
// Timers' name tags are appended to a static string so that embedded functions calls 
// with the deisred granularity level can be traced to measure their footprint on the 
// calling function performance.
//
// to use compile with: -DUSETIME[=TIMER_GRANULARITY] -DTIMER_STATS
//
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <utility>
#include <cstring>
#include <cmath>

namespace fm_profile {

#ifdef USE_TIMER
constexpr unsigned TimerGranularityLim{1+USE_TIMER};
#ifdef TIMER_STATS
    constexpr bool EnableStats{true};
#else
    constexpr bool EnableStats{false};
#endif
#else
constexpr unsigned TimerGranularityLim{0};
#endif

    // time record
    template <typename I=size_t, typename R=double>
    struct TimeRecord {
        I _cnt;
        std::chrono::duration<R> _dur;
#ifdef TIMER_STATS
        struct { R _rms{}, _max{}, _min{std::numeric_limits<R>::max()}; } _stats;
#endif
    };

    // use granulrity param to define when timer is onduty 
    constexpr bool onduty{true};
    constexpr bool OnDuty(const unsigned g) {return g<TimerGranularityLim;}

    // use alias template to select on/off duty based on input granularity
    template <bool B, typename T, typename Clock> class Timer;
    template <unsigned Granularity=1,
	     typename T=TimeRecord<>, 
	     typename Clock=std::chrono::high_resolution_clock>
    using Timer_t = Timer<OnDuty(Granularity),T,Clock>;

    // default timer does nothing because it is off duty
    template <bool B, typename T, typename Clock> class Timer {
    public:
	    Timer(const std::string a_name) {}
	    void stop() {}
            static void print_record() {}
	    ~Timer() {}
    };

    // timer measures when on duty
    template <typename T, typename Clock> class Timer<onduty,T,Clock> {
        // print out measurements
        void static print_record(const std::string s, const T r, const auto i);
        // use static member data to record measurements
        static std::unordered_map<std::string,T> _records;
        static std::string _seq;
        // member data
        std::string _name;
        std::chrono::time_point<Clock> _t_up;
        bool _stop{true};

        public:
	// start measurement at construction
        Timer(const std::string a_name)
        : _name((_seq.size()>0?"::":"")+a_name), _t_up(Clock::now()) {
            _seq=_seq+_name;
        }

	// stop and record measurement at destruction unless !_stop 
        ~Timer() {
            if (_stop) stop();
        }

	// stop measurement before going out of scope
        void stop() {
            const auto t_dw= Clock::now();
            const std::chrono::duration<double> dur=t_dw-_t_up;
            ++_records[_seq]._cnt;
            _records[_seq]._dur += dur;

            if constexpr (EnableStats) {
                const auto dt=dur.count();
                std::cout << " dt="<<dt<<'\n';
                auto& [t_rms,t_max,t_min]=_records[_seq]._stats;
                t_rms += dt*dt;
                t_max = std::max(t_max,dt);
                t_min = std::min(t_min,dt);
            }

            _seq.resize(_seq.size()-_name.size());
            _stop=false;
        }

	// print out measurements 
        static void print_record() {
            print_record("",T{},0);
        }
    };

    template <typename T, typename C>
	void Timer<onduty,T,C>::print_record(const std::string a_seq, const T a_tr, const auto ts) {

        constexpr auto tabsize=3;
        const auto& _records=Timer<onduty,T,C>::_records;
        static std::pair<std::string,T> root;

        std::vector<std::pair<std::string,T>> recs;
        for (const auto& [name,rec] : _records) {
            if (a_seq==name.substr(0,a_seq.size()) && name.find("::",a_seq.size())==std::string::npos) {
                recs.emplace_back(std::make_pair(name.substr(a_seq.size()),rec) );
            }
        }
        std::sort(recs.begin(),recs.end(),[](const auto& a, const auto& b){return a.second._dur>b.second._dur;});

        auto prnt_rec=[ts](const auto name, const auto rec, const auto tot) {

            constexpr int NFW=14, DFW=10, PFW=8, CW=80, TW=2;
	    auto _cnt_string=[](const auto w, const auto s) {
	      auto s2{0};
              if constexpr (std::is_same_v<decltype(s),const std::string>) {
                s2=(w-std::size(s))/2;
	      } else {
                s2=(w-std::strlen(s))/2;
	      }
	      return std::string(s2,' ')+s+std::string(s2,' ');
            };

	    const std::string tab(TW,' ');
             if (tot>0) {
		        std::cout << std::string(ts,' ') << std::left << std::setfill('.')
                << std::setw(NFW-1) << name << ":" << tab
                << std::setw(PFW) << std::setfill(' ') << _cnt_string(PFW,std::to_string(rec._cnt)) << tab
                << std::setw(DFW) << std::scientific << std::setprecision(4) << rec._dur.count() << tab 
                << std::setw(PFW) << std::defaultfloat << rec._dur.count()/tot << tab
                << std::setw(PFW) << rec._dur.count()/root.second._dur.count();
                if constexpr (EnableStats) {
                    if (name!="total") {
                        const auto t_ave=rec._dur.count()/rec._cnt;
                        const auto t_rms=(rec._cnt>0 ? std::sqrt(rec._stats._rms/rec._cnt-t_ave*t_ave) : 0);

                        std::cout << std::scientific << std::setprecision(3) 
                                  << std::setw(PFW) << t_ave << tab << std::setw(PFW) << t_rms << tab
                                  << std::setw(PFW) << rec._stats._max << tab << std::setw(PFW) << rec._stats._min;
                    }
                }
                std::cout <<"\n";
             }
             else {
                 std::cout << std::string(CW,'=') << "\n"
                 << name << ": call-cnt: " << rec._cnt 
                 << ", time: "<< std::scientific << rec._dur.count() << " s\n"
                 << std::string(CW,'-') << "\n";
                 std::cout << std::setw(ts) << std::setfill(' ') << std::left << "L-"+std::to_string(ts/tabsize)
                 << _cnt_string(NFW,"name") << tab << _cnt_string(PFW,"call-cnt") << tab << _cnt_string(DFW,"t[s]") << tab 
                 << _cnt_string(PFW,"% ") << tab << _cnt_string(PFW,"%["+root.first+"]");
                 if constexpr (EnableStats) {
                     std::cout << tab << _cnt_string(PFW,"t[s]/cnt") << tab << _cnt_string(PFW,"t_rms[s]")
                               << tab << _cnt_string(PFW,"t_max[s]") << tab << _cnt_string(PFW,"t_min[s]");
                 }
                 std::cout << "\n";
             }
        };
	if (_records.size()==1) {
	    // special case of only one entry
            const auto& rec=recs.begin();
            prnt_rec (rec->first,rec->second,0);
	}
	else {
		if (a_tr._cnt>0 && recs.size()>0) {
            // print only if function exists and contains other timers
            prnt_rec (a_seq.substr(0,a_seq.size()-2),a_tr,0);

            // print finer timer-mesurementes and total
            T total{};
            for (const auto& [name,rec] : recs) {
                prnt_rec (name,rec,a_tr._dur.count());
                total._cnt+=rec._cnt;
                total._dur+=rec._dur;
            }
            prnt_rec("total",total,a_tr._dur.count());
        }
        // analyse sub-timers
        for (const auto& [name,rec] : recs) {
            if (ts==0) root={name,rec};
            print_record(a_seq+name+"::",rec,ts+tabsize);
        }
	}
        if (ts==0) std::cout<<std::string(80,'-')<<"\n";
	}
};

template <typename T, typename C>
std::unordered_map<std::string,T> fm_profile::Timer<fm_profile::onduty,T,C>::_records{};
template <typename T, typename C>
std::string fm_profile::Timer<fm_profile::onduty,T,C>::_seq{};

