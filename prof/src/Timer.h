// simple timer obj
#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <algorithm>
#include <functional>
#include <map>
#include <utility>

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
            std::string spt, spr;
            if (tot>0) {
                std::cout << std::string(ts+2,' ') 
                << std::internal << std::setw(14) << "name" << std::setw(12) << "time [s]"
                << std::setw(8) << "%" << "% of " << root.first <<"\n"
                << "L" << ts/tabsize << std::string(ts,' ') 
                << std::setw(14) << std::left << std::setfill('.');
                spt=std::to_string(rec._dur.count()/tot);
                spr=std::to_string(rec._dur.count()/root.second._dur.count());
            }
            std::cout << name << rec._cnt << std::scientific << rec._dur.count()
            << (tot>0?std::to_string(rec._dur.count()/tot):"") 
            << (tot>0)
//            std:: cout << name << ": call-cnt: " << rec._cnt 
//            << ", time: "<< std::scientific << rec._dur.count() << " s"
//            << (tot>0?", %: "+std::to_string(rec._dur.count()/tot):"") <<"\n";
        };
        // print only if function exists and contains other timers
        if (a_tr._cnt>0 && recs.size()>0) {
            std::cout << std::string(80,'=') <<"\n";
            prnt_rec (a_seq.substr(0,a_seq.size()-2),a_tr,0);
            std::cout << std::string(80,'-') <<"\n";

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
