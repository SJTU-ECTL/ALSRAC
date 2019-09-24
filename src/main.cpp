#include <bits/stdc++.h>
#include "cmdline.h"
#include "simulator.h"
#include "cktNtk.h"


using namespace std;
using namespace abc;
using namespace cmdline;


parser Cmdline_Parser(int argc, char * argv[])
{
    parser option;
    option.add <string> ("input", 'i', "Original Circuit file", true);
    option.add <int> ("nFrame", 'n', "Simulation Frame number", false, 64, range(1, INT_MAX));
    option.parse_check(argc, argv);
    return option;
}


int main(int argc, char * argv[])
{
    parser option = Cmdline_Parser(argc, argv);
    string input = option.get <string> ("input");
    int nFrame = option.get <int> ("nFrame");

    Abc_Start();
    ostringstream command("");
    command << "read_blif " << input << "; aig;";
    Abc_Frame_t * pFrame = Abc_FrameGetGlobalFrame();
    DASSERT(!Cmd_CommandExecute(pFrame, command.str().c_str()), "read_blif failed");

    Abc_Ntk_t * pCurCkt1 = Abc_NtkDup(Abc_FrameReadNtk(pFrame));
    Abc_Ntk_t * pCurCkt2 = Abc_NtkDup(Abc_FrameReadNtk(pFrame));
    cout << MeasureAEM(pCurCkt1, pCurCkt2, nFrame) << endl;
    Abc_NtkDelete(pCurCkt1);
    Abc_NtkDelete(pCurCkt2);

    Abc_Stop();

    return 0;
}
