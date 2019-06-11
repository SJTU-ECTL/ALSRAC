#include <bits/stdc++.h>
#include <gtest/gtest.h>
#include "cmdline.h"
#include "cktNtk.h"

using namespace std;
using namespace abc;
using namespace cmdline;


parser Cmdline_Parser(int argc, char * argv[])
{
    parser option;
    option.add <string>    ("file",    'f', "Original Circuit file",    true);
    option.add <string>    ("approx",  'a', "Approximate Circuit file", false);
    option.add <string>    ("genlib",  'g', "Map libarary file",        false, "data/genlib/mcnc.genlib");
    option.add <int>       ("nFrame",  'n', "Simulation Frame number",  false, 10240, range(1, INT_MAX));
    option.add <int>       ("level",   'l', "Window level",             false, 3,     range(1, INT_MAX));
    option.add             ("measure", 'm', "Enable measuring the ER");
    option.parse_check(argc, argv);
    return option;
}


int main(int argc, char * argv[])
{
    parser option = Cmdline_Parser(argc, argv);
    string file = option.get <string> ("file");
    string approx = option.get <string> ("approx");
    string genlib = option.get <string> ("genlib");
    int nFrame = option.get <int> ("nFrame");
    int level = option.get <int> ("level");
    bool isMeasure = option.exist("measure");

    Abc_Start();
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    string command = "read_genlib -v " + genlib;
    ASSERT_EQ(Cmd_CommandExecute(pAbc, command.c_str()), 0);

    Abc_Stop();
    return 0;
}
