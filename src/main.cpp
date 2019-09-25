#include <bits/stdc++.h>
#include "cmdline.h"
#include "dcals.h"
#include "cktNtk.h"


using namespace std;
using namespace abc;
using namespace cmdline;


parser Cmdline_Parser(int argc, char * argv[])
{
    parser option;
    option.add <string> ("input", 'i', "Original Circuit file", true);
    option.add <string> ("approx", 'x', "Approximate Circuit file", false, "");
    option.add <string> ("genlib", 'l', "Standard Cell Library", false, "data/genlib/mcnc.genlib");
    option.add <int> ("nFrame", 'f', "Initial Simulation Round", false, 64, range(1, INT_MAX));
    option.add <int> ("nCut", 'c', "Initial Cut Size", false, 30, range(1, INT_MAX));
    option.add <double> ("aem", 'm', "Average Error Magnitude Constraint", false, 0.01, range(0.0, 1.0));
    option.add <double> ("er", 'r', "Error Rate Constraint", false, 0.01, range(0.0, 1.0));
    option.parse_check(argc, argv);
    return option;
}


int main(int argc, char * argv[])
{
    parser option = Cmdline_Parser(argc, argv);
    string input = option.get <string> ("input");
    string approx = option.get <string> ("approx");
    string genlib = option.get <string> ("genlib");
    int nFrame = option.get <int> ("nFrame");
    int nCut = option.get <int> ("nCut");
    double aem = option.get <double> ("aem");
    double er = option.get <double> ("er");

    Abc_Start();
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    ostringstream command("");
    command << "read_genlib " << genlib << ";read_blif " << input << "; aig;";
    DASSERT(!Cmd_CommandExecute(pAbc, command.str().c_str()));
    Abc_Ntk_t * pNtk = Abc_NtkDup(Abc_FrameReadNtk(pAbc));

    Dcals_Man_t alsEng(pNtk, nFrame, nCut, er);
    alsEng.DCALS();

    Abc_NtkDelete(pNtk);
    Abc_Stop();

    return 0;
}
