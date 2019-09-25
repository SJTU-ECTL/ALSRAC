#include <bits/stdc++.h>
#include "cmdline.h"
#include "dcals.h"


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

    Abc_Start();
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    ostringstream command("");
    command << "read_genlib " << genlib << ";read_blif " << input << "; aig;";
    DASSERT(!Cmd_CommandExecute(pAbc, command.str().c_str()));
    Abc_Ntk_t * pNtk = Abc_NtkDup(Abc_FrameReadNtk(pAbc));

    // Dcals_Man_t alsEng(pNtk, nFrame, nCut, aem);
    // alsEng.LocalAppChange();

    command.str("");
    command << "read_blif " << approx << "; aig;";
    DASSERT(!Cmd_CommandExecute(pAbc, command.str().c_str()));
    Abc_Ntk_t * pNtk2 = Abc_NtkDup(Abc_FrameReadNtk(pAbc));
    // cout << MeasureER(pNtk, pNtk2, nFrame, 314) << endl;

    Simulator_t smlt1(pNtk, nFrame);
    smlt1.Input();
    smlt1.Simulate();
    Simulator_t smlt2(pNtk2, nFrame);
    smlt2.Input();
    smlt2.Simulate();
    int ret = 0;
    for (int k = 0; k < smlt1.GetFrameNum(); ++k) {
        int nPo = Abc_NtkPoNum(pNtk);
        cout << smlt1.GetOutput(0, nPo - 1, k) << "," << smlt2.GetOutput(0, nPo - 1, k) << endl;
        if (smlt1.GetOutput(0, nPo - 1, k) != smlt2.GetOutput(0, nPo - 1, k))
            ++ret;
    }
    cout << ret << endl;

    smlt1.Stop();
    smlt2.Stop();

    Abc_NtkDelete(pNtk2);

    Abc_NtkDelete(pNtk);
    Abc_Stop();

    return 0;
}
