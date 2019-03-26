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
    option.add <string>    ("file",    'f', "Original Circuit file",    true);
    option.add <string>    ("approx",  'a', "Approximate Circuit file", false);
    option.add <string>    ("genlib",  'g', "Map libarary file",        false, "data/genlib/mcnc.genlib");
    option.add <uint64_t>  ("error",   'e', "Approximate EXDC number",  false, 100, range(static_cast<uint64_t>(0), static_cast<uint64_t>(ULLONG_MAX)));
    option.add <int>       ("nFrame",  'n', "Simulation Frame number",  false, 10240, range(1, INT_MAX));
    option.add             ("measure", 'm', "enable measuring the ER");
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

    cout << "error rate = " << pCktNtkApp->MeasureError(pCktNtkOri, 666) << endl;
}


void TestSimulator(string file, int nFrame)
{
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    string command = "read_blif " + file;
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "read_blif failed");
    shared_ptr <Ckt_Ntk_t> pCktNtk = make_shared <Ckt_Ntk_t> (Abc_FrameReadNtk(pAbc));
    pCktNtk->Init(nFrame);
    pCktNtk->TestSimSpeed();
    // pCktNtk->CheckSim();
}


void ALS_DC(string file, string approx, uint64_t error)
{
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    string command = "read_blif " + file;
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "read_blif failed");

    Ckt_Set_t dcs(Abc_FrameReadNtk(pAbc), 0);
    cout << "# pi = " << Abc_NtkPiNum(Abc_FrameReadNtk(pAbc)) << endl;
    for (uint64_t i = 0; i < error; ++i)
        dcs.AddPatternR();
    // cout << dcs;
    Ckt_MfsTest(Abc_FrameReadNtk(pAbc), dcs);

    command = "map -a; print_stats";
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{});

    DEBUG_ASSERT(system("if [ ! -d approx ]; then mkdir approx; fi") != -1, module_a{}, "mkdir failed");
    if (approx == "") {
        string fileName("approx/" + string(Abc_FrameReadNtk(pAbc)->pName) + "-");
        stringstream ss;
        string str;
        ss << error;
        ss >> str;
        fileName += str;
        fileName += ".blif";
        command = "write_blif " + fileName;
    }
    else
        command = "write_blif " + approx;
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "write_blif failed");
}


void ALS_CR(string file, string approx, int nFrame)
{
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    string command = "read_blif " + file;
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "read_blif failed");

    Ckt_Set_t cs(Abc_FrameReadNtk(pAbc), 1);
    cout << "# pi = " << Abc_NtkPiNum(Abc_FrameReadNtk(pAbc)) << endl;
    for (int i = 0; i < nFrame; ++i)
        cs.AddPatternR();
    // cout << cs;
    Ckt_MfsTest(Abc_FrameReadNtk(pAbc), cs);

    command = "map -a; print_stats";
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{});

    DEBUG_ASSERT(system("if [ ! -d approx ]; then mkdir approx; fi") != -1, module_a{}, "mkdir failed");
    if (approx == "") {
        string fileName("approx/" + string(Abc_FrameReadNtk(pAbc)->pName) + "-");
        stringstream ss;
        string str;
        ss << nFrame;
        ss >> str;
        fileName += str;
        fileName += ".blif";
        command = "write_blif " + fileName;
        cout << command << endl;
    }
    else
        command = "write_blif " + approx;
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "write_blif failed");
}


int main(int argc, char * argv[])
{
    parser option = Cmdline_Parser(argc, argv);
    string file = option.get <string> ("file");
    string approx = option.get <string> ("approx");
    string genlib = option.get <string> ("genlib");
    int nFrame = option.get <int> ("nFrame");
    bool isMeasure = option.exist("measure");
    // uint64_t error = option.get <uint64_t> ("error");

    Abc_Start();
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    string command = "read_genlib -v " + genlib;
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "read_genlib failed" );

    if (isMeasure) {
        DEBUG_ASSERT(approx != "" && file != "", module_a{}, "invalid file name");
        MeasureErrorRate(file, approx, nFrame);
    }
    else {
        // ALS_Sim(file, approx, nFrame);
        ALS_CR(file, approx, nFrame);
        // ALS_DC(file, approx, nFrame);
    }

    Abc_Stop();

    return 0;
}
