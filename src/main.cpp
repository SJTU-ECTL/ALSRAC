#include "headers.h"
#include "cmdline.h"
#include "dcals.h"


using namespace std;
using namespace cmdline;


parser Cmdline_Parser(int argc, char * argv[])
{
    parser option;
    option.add <string> ("input", 'i', "Original Circuit file", true);
    option.add <string> ("approx", 'x', "Approximate Circuit file", false, "");
    option.add <string> ("genlib", 'l', "Standard Cell Library", false, "data/genlib/mcnc.genlib");
    option.add <int> ("select", 's', "Mode Selection, 0 = dcals, 1 = measure", false, 0, range(0, 1));
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
    int select = option.get <int> ("select");
    int nFrame = option.get <int> ("nFrame");
    int nCut = option.get <int> ("nCut");
    double aem = option.get <double> ("aem");
    double er = option.get <double> ("er");

    Abc_Start();
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    ostringstream command("");
    command << "read_genlib " << genlib;
    DASSERT(!Cmd_CommandExecute(pAbc, command.str().c_str()));

    // dcals
    if (select == 0) {
        command.str("");
        command << "read_blif " << input;
        DASSERT(!Cmd_CommandExecute(pAbc, command.str().c_str()));
        Abc_Ntk_t * pNtk = Abc_NtkDup(Abc_FrameReadNtk(pAbc));
        DASSERT(Abc_NtkHasMapping(pNtk));

        Dcals_Man_t alsEng(pNtk, nFrame, nCut, er);
        alsEng.DCALS();

        Abc_NtkDelete(pNtk);
    }

    // measure
    if (select == 1) {
        command.str("");
        command << "read_blif " << input << ";aig;";
        DASSERT(!Cmd_CommandExecute(pAbc, command.str().c_str()));
        Abc_Ntk_t * pNtk1 = Abc_NtkDup(Abc_FrameReadNtk(pAbc));
        command << "read_blif " << approx;
        DASSERT(!Cmd_CommandExecute(pAbc, command.str().c_str()));
        Abc_Ntk_t * pNtk2 = Abc_NtkDup(Abc_FrameReadNtk(pAbc));
        cout << "area = " << Ckt_GetArea(pNtk2) << endl;
        cout << "delay = " << Ckt_GetDelay(pNtk2) << endl;
        DASSERT(Abc_NtkToAig(pNtk2));
        cout << "error rate = " << MeasureER(pNtk1, pNtk2, nFrame) << endl;
        cout << "average error magnitude = " << MeasureAEM(pNtk1, pNtk2, nFrame) << endl;
        Abc_NtkDelete(pNtk1);
        Abc_NtkDelete(pNtk2);
    }

    // recycle memory
    Abc_Stop();

    return 0;
}
