// simple timer obj
#include <ios>
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <algorithm>
#include <functional>
#include <map>
#include <utility>
#include <cstring>

namespace fm_timer {

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
        static std::map<std::string,T> _records;
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
        }
    };

    template <typename T, typename C>
	void Timer<T,C>::print_record(const std::string a_seq, const T a_tr, const auto ts) {

        constexpr auto tabsize=3;
        const auto& _records=Timer<T,C>::_records;
        static std::pair<std::string,T> root;

        std::vector<std::pair<std::string,T>> recs;
        for (auto r : _records) {
            if (a_seq==r.first.substr(0,a_seq.size()) && r.first.find("::",a_seq.size())==std::string::npos) {
                recs.push_back(std::make_pair(r.first.substr(a_seq.size()),r.second) );
            }
        }
        std::sort(recs.begin(),recs.end(),[](const auto& a, const auto& b){return a.second._dur>b.second._dur;});

        auto prnt_rec=[ts](const auto name, const auto rec, const auto tot) {

            constexpr int FW=10, CW=80;
            auto _cnt_string=[](const auto w, const std::string s) {
                const auto s2=(w-std::size(s))/2;
                return std::string(s2,' ')+s+std::string(s2,' ');
            };

             if (tot>0) {
		        std::cout << std::string(ts,' ') << std::left << std::setfill('.')
                << std::setw(FW-1) << name << ":"
                << std::setw(FW) << std::setfill(' ') << std::scientific << rec._dur.count()
                << std::setw(FW) << std::defaultfloat << rec._dur.count()/tot
                << std::setw(FW) << rec._dur.count()/root.second._dur.count() <<"\n";
             }
             else {
                 std::cout << std::string(CW,'=') << "\n"
                 << name <<" "<< rec._cnt <<" "<< std::scientific << rec._dur.count() <<"\n"
                 << std::string(CW,'-') << "\n";
                 std::cout << std::setw(ts) << std::setfill(' ') << std::left << "L-" << ts/tabsize
                 << _cnt_string(FW,"name") << _cnt_string(FW,"time[s]") 
                 << _cnt_string(FW,"  %") << _cnt_string(FW,"%["+root.first+"]") << "\n";
             }
        };
        // print only if function exists and contains other timers
        if (a_tr._cnt>0 && recs.size()>0) {
            prnt_rec (a_seq.substr(0,a_seq.size()-2),a_tr,0);

            // print finer timer-mesurementes and total
            T total{};
            for (const auto& r : recs) {
                prnt_rec (r.first,r.second,a_tr._dur.count());
                total._cnt+=r.second._cnt;
                total._dur+=r.second._dur;
            }
            prnt_rec("total",total,a_tr._dur.count());
        }
        // analyse sub-timers
        for (const auto& r : recs) {
            if (ts==0) root=r;
            print_record(a_seq+r.first+"::",r.second,ts+tabsize);
        }
        if (ts==0) std::cout<<std::string(80,'-')<<"\n";
	}
};

template <typename T, typename C>
std::map<std::string,T> fm_timer::Timer<T,C>::_records{};
template <typename T, typename C>
std::string fm_timer::Timer<T,C>::_seq{};
