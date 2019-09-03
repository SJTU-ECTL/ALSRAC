#include <bits/stdc++.h>
#include "cmdline.h"
#include "abcApi.h"
#include "appMfs.h"
#include "cktSynthesis.h"


using namespace std;
using namespace abc;
using namespace cmdline;


parser Cmdline_Parser(int argc, char * argv[])
{
    parser option;
    option.add <string> ("input",   'i', "Original Circuit file",    true);
    option.add <string> ("genlib",  'g', "Map libarary file",        false, "data/genlib/mcnc.genlib");
    option.add <int>    ("nFrame",  'n', "Simulation Frame number",  false, 64,      range(1, INT_MAX));
    option.add <int>    ("nLocalPI",'m', "Local PI number",          false, 30,      range(1, INT_MAX));
    option.add <float>  ("delay",   'd', "Required delay",           false, FLT_MAX, range(0.0f, FLT_MAX));
    option.add <float>  ("error",   'e', "Error rate threshold",     false, 0.05,    range(0.0f, 1.0f));
    option.parse_check(argc, argv);
    return option;
}


int main(int argc, char * argv[])
{
    clock_t st = clock();
    parser option = Cmdline_Parser(argc, argv);
    string input = option.get <string> ("input");
    string genlib = option.get <string> ("genlib");
    int nFrame = option.get <int> ("nFrame");
    int nLocalPI = option.get <int> ("nLocalPI");
    float reqDelay = option.get <float> ("delay");
    float errorThreshold = option.get <float> ("error");

    Abc_Start();
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    string Command = string("read " + genlib);
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
    Command = string("read " + input);
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
    Abc_Ntk_t * pNtk = Abc_NtkDup(Abc_FrameReadNtk(pAbc));

    DASSERT(Abc_NtkHasMapping(Abc_FrameReadNtk(pAbc)));
    shared_ptr <Ckt_Ntk_t> pNtkRef = make_shared <Ckt_Ntk_t> (Abc_FrameReadNtk(pAbc));
    pNtkRef->Init(102400);
    reqDelay = Ckt_GetDelay(pNtkRef->GetAbcNtk());
    cout << "Original area = " << Ckt_GetArea(pNtkRef->GetAbcNtk()) << ", " <<
    "original delay = " << Ckt_GetDelay(pNtkRef->GetAbcNtk()) << endl;

    nLocalPI = min(nLocalPI, Abc_NtkPiNum(pNtk));
    float error = 0.0f;
    ostringstream ss;
    for (int i = 0; ; ++i) {
        cout << endl << "round " << i << endl;
        App_CommandMfs(pNtk, pNtkRef, nFrame, error, nLocalPI, errorThreshold);
        cout  << "time = " << clock() - st << endl;
        if (error > errorThreshold)
            break;
        ss.str("");
        ss << pNtk->pName << "_" << i << "_" << error;
        Ckt_Synthesis3(pNtk, ss.str(), reqDelay);

        Abc_FrameReplaceCurrentNetwork(pAbc, Abc_NtkDup(pNtk));
        Command = string("strash; balance; rewrite; refactor; balance; rewrite; rewrite -z; balance; refactor -z; rewrite -z; balance");
        DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
        Command = string("logic; sweep; mfs");
        DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
        pNtk = Abc_NtkDup(Abc_FrameReadNtk(pAbc));
    }
    Abc_NtkDelete(pNtk);
    Abc_Stop();

    return 0;
}
