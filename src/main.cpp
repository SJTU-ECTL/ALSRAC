#include <bits/stdc++.h>
#include "cmdline.h"
#include "cktNtk.h"
#include "cktInter.h"

using namespace std;
using namespace abc;
using namespace cmdline;


parser Cmdline_Parser(int argc, char * argv[])
{
    parser option;
    option.add <string> ("file",    'f', "Original Circuit file",    false, "data/sop/alu4.blif");
    option.add <string> ("approx",  'a', "Approxiamte Circuit file", false, "approx/alu4_0.0475.blif");
    option.add <string> ("genlib",  'g', "Map libarary file",        false, "data/genlib/mcnc.genlib");
    // option.add <float>  ("error",   'e', "Error rate",               false, 0.05f, range(0.0f, 1.0f));
    option.add <int>    ("nFrame",  'n', "Frame nFrame",             false, 10000, range(1, INT_MAX));
    option.parse_check(argc, argv);
    return option;
}


void MeasureErrorRate(string file, string approx, int nFrame)
{
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    string command = "read_blif " + file;
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "read_blif failed");
    shared_ptr <Ckt_Ntk_t> pCktNtkOri = make_shared <Ckt_Ntk_t> (Abc_FrameReadNtk(pAbc));
    pCktNtkOri->Init(nFrame);

    command = "read_blif " + approx;
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "read_blif failed");
    shared_ptr <Ckt_Ntk_t> pCktNtkApp = make_shared <Ckt_Ntk_t> (Abc_FrameReadNtk(pAbc));
    pCktNtkApp->Init(nFrame);

    cout << "error rate = " << pCktNtkApp->MeasureError(pCktNtkOri) << endl;
}


int main(int argc, char * argv[])
{
    parser option = Cmdline_Parser(argc, argv);
    string file = option.get <string> ("file");
    string approx = option.get <string> ("approx");
    string genlib = option.get <string> ("genlib");
    int nFrame = option.get <int> ("nFrame");
    // float ERThres = option.get <float> ("error");

    Abc_Start();
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    string command = "read_genlib -v " + genlib;
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "read_genlib failed" );

    // if (approx != "")
    //     MeasureErrorRate(file, approx, nFrame);

    command = "read_blif " + file;
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "read_blif failed");

    Ckt_DC_t dc(Abc_FrameReadNtk(pAbc));
    cout << "# po = " << Abc_NtkPoNum(Abc_FrameReadNtk(pAbc)) << endl;
    for (int i = 0; i < 1000; ++i)
        dc.AddPatternR();
    cout << dc;
    Ckt_MfsTest(Abc_FrameReadNtk(pAbc), dc);

    command = "map -a; print_stats";
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "read_blif failed");

    Abc_Stop();

    return 0;
}
