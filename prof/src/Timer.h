// simple timer obj
#include <ios>
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>
#include <cstring>

namespace fm_profile {

    // time record
    template <typename I=size_t, typename R=double>
    struct TimeRecord {
        I _cnt;
        std::chrono::duration<R> _dur;
    };
    
    template <typename T=TimeRecord<>, typename Clock=std::chrono::high_resolution_clock>
    class Timer {
        // print out
        void static print_record(const std::string s, const T r, const auto i);
        // static member data
        static std::unordered_map<std::string,T> _records;
        static std::string _seq;
        // member data
        std::string _name;
        std::chrono::time_point<Clock> _t_up;
        bool _stop{true};

        public:
        Timer(const std::string a_name)
        : _name((_seq.size()>0?"::":"")+a_name), _t_up(Clock::now()) {
            _seq=_seq+_name;
        }

        void stop() {
            ++_records[_seq]._cnt;
            _records[_seq]._dur += Clock::now() - _t_up;
            _seq.resize(_seq.size()-_name.size());
            _stop=false;
        }

        static void print_record() {
            print_record("",T{},0);
        }

        ~Timer() {
            if (_stop) stop();
	    //std::cout << "calling "<<_name<<"'s destructor\n";
        }
    };

    template <typename T, typename C>
	void Timer<T,C>::print_record(const std::string a_seq, const T a_tr, const auto ts) {

        constexpr auto tabsize=3;
        const auto& _records=Timer<T,C>::_records;
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
                << std::setw(DFW) << std::setfill(' ') << std::scientific << std::setprecision(4) << rec._dur.count() << tab
                << std::setw(PFW) << std::defaultfloat << rec._dur.count()/tot << tab
                << std::setw(PFW) << rec._dur.count()/root.second._dur.count() <<"\n";
             }
             else {
                 std::cout << std::string(CW,'=') << "\n"
                 << name << ": call-cnt: " << rec._cnt 
                 << ", time: "<< std::scientific << rec._dur.count() << " s\n"
                 << std::string(CW,'-') << "\n";
                 std::cout << std::setw(ts) << std::setfill(' ') << std::left << "L-"+std::to_string(ts/tabsize)
                 << _cnt_string(NFW,"name") << tab << _cnt_string(DFW,"time[s]") << tab 
                 << _cnt_string(PFW,"% ") << tab << _cnt_string(PFW,"%["+root.first+"]") << "\n";
             }
        };
        // print only if function exists and contains other timers
        if (a_tr._cnt>0 && recs.size()>0) {
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
        if (ts==0) std::cout<<std::string(80,'-')<<"\n";
	}
};

template <typename T, typename C>
std::unordered_map<std::string,T> fm_profile::Timer<T,C>::_records{};
template <typename T, typename C>
std::string fm_profile::Timer<T,C>::_seq{};


