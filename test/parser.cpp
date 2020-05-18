
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <array>
#include <list>
#include "FileParser.H"

int main(int argc, char* argv[]) {

    using namespace fm;
    const std::string prog(argv[0]);

    // fila name string
    std::string filename{};
    // read in filename
    for (auto i{1}; i<argc; ++i)
        if (strncmp("-f",argv[i],2)==0) filename=argv[i+1];

    if (filename.size()==0) {
        std::cout<<"\n No input file was provided.\n Run this prog with: "+prog+" -f filename\n\n";
        return 0;
    }

    FileParser parser(filename);

    int vsize{};
    parser.get_item(vsize,"vsize");
    std::vector<double> v(vsize);
    parser.get_items(v,"vector");

    int lsize{};
    parser.get_item(lsize,"lsize");
    std::list<float> l(lsize);
    parser.get_items(l,"list");

    constexpr int asize=5;
    std::array<int,asize> a;
    parser.get_items(a,"array");

    float x{};
    parser.get_item(x,"flt_x");

    std::cout << "\nvector: ";
    for (auto i : v) std::cout << i << " ";
    std::cout <<'\n';
    std::cout << "\nlist: ";
    for (auto i : l) std::cout << i << " ";
    std::cout <<'\n';
    std::cout << "\narray: ";
    for (auto i : a) std::cout << i << " ";
    std::cout <<'\n';
    std::cout <<"\nx="<<x<<'\n';

    std::cout <<"\n parser mapped data is:\n\n";
    parser.flush();
}
