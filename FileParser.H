
#ifndef FILE_PARSER_H
#define FILE_PARSER_H

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace fm {
 
    class FileParser {
        
        public:
        // destructor
        ~FileParser() {}

        // constructor
        FileParser(const std::string a_file_name) {

            // open and parse file
            if (std::ifstream file(a_file_name); file) {

                std::string s, key;
                while ( getline(file,s) ) {
 
                    // first non-blank char
                    if (const auto pos=s.find_first_not_of(" "); 
                        // ignore comments
                        pos!=std::string::npos && s.at(pos)!='#') {

                        if (const auto pos=s.find("="); pos!=std::string::npos) {

	                        const std::istringstream ikey(s.substr(0,pos));
                            const std::string  entry(s.substr(pos+1,s.size()-1));

                            // remove unwanted blanks
                            ikey >> key;

                            m_entries[key] = entry;
                        }
                        else {

                            // add to previous key
                            m_entries[key].append(" "+s);
                        }
                    }
                    else {

      	                // reset key
                        key=" ";
                    }
                }
                // close file
                file.close();
            }
            else {
                std::cerr << "FileParser::define: Error while opening file!\n";
                std::abort();
            }
        }

        // get item value of entry a_name
        template <class T> void get_item(T& a_val, const std::string a_name) {

        // search object a_name
        if (const auto it=m_entries.find(a_name); it!=m_entries.end()) {
            try {
                 std::istringstream line(it->second);
                 line >> a_val;
            }
            catch (std::ios_base::failure&) {
                std::cout << "FileParser::get_item: error: entry was : "
                          << it->second << "\n";
            }
        }
        else {
            std::cerr << "FileParser::get_item: item " << a_name << " not found!\n";
            std::abort();
        }
    }

    // fill a_c container with (a_c.size) items from entry a_name
    template <class T> void get_items(T& a_c, const std::string a_name) {
        static_assert(a_c.size()>0,"FileParser::get_items: size of "+a_name" must be >0");

        // search object a_name
        if (const auto it=m_entries.find(a_name.c_str()); it!=m_entries.end()) {
            try {
	            std::istringstream line(it->second);
                for (auto& c : a_c) line >> c;
            }
            catch (std::ios_base::failure&) {
                std::cout << "FileParser::get_item: error: entry was : "
                          << it->second << "\n";
            }
        }
        else {
            std::cerr << "FileParser::get_item: item " << a_name << " not found!\n";
            std::abort();
        }
    }

    protected:
    std::unordered_map<std::string,std::string> m_entries;
};

#endif
