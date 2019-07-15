#include <bits/stdc++.h>
#include "cmdline.h"
#include "abcApi.h"
#include "appMfs.h"


using namespace std;
using namespace abc;
using namespace cmdline;


parser Cmdline_Parser(int argc, char * argv[])
{
    parser option;
    option.add <string>    ("input",   'i', "Original Circuit file",    true);
    option.add <string>    ("output",  'o', "Approximate Circuit file", false);
    option.add <string>    ("genlib",  'g', "Map libarary file",        false, "data/genlib/mcnc.genlib");
    option.add <int>       ("nFrame",  'f', "Simulation Frame number",  false, 10240, range(1, INT_MAX));
    option.add <int>       ("level",   'l', "TFO level",                false, 3,     range(0, INT_MAX));
    option.add <int>       ("nLocalPI",'m', "Local PI number",          false, 10,    range(1, INT_MAX));
    option.parse_check(argc, argv);
    return option;
}


int main(int argc, char * argv[])
{
    parser option = Cmdline_Parser(argc, argv);
    string input = option.get <string> ("input");

    Abc_Start();
    Abc_Ntk_t * pNtk = Io_Read(const_cast <char *>(input.c_str()), IO_FILE_BLIF, 1, 0);
    App_DontCareSimpl(pNtk);
    Abc_NtkDelete(pNtk);
    Abc_Stop();

    return 0;
}
