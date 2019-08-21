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
    option.add <string>    ("input",   'i', "Original Circuit file",    true);
    option.add <string>    ("genlib",  'g', "Map libarary file",        false, "data/genlib/mcnc.genlib");
    option.add <int>       ("nFrame",  'n', "Simulation Frame number",  false, 8192, range(1, INT_MAX));
    option.add <int>       ("nLocalPI",'m', "Local PI number",          false, 30,    range(1, INT_MAX));
    option.parse_check(argc, argv);
    return option;
}


int main(int argc, char * argv[])
{
    parser option = Cmdline_Parser(argc, argv);
    string input = option.get <string> ("input");
    string genlib = option.get <string> ("genlib");
    int nFrame = option.get <int> ("nFrame");
    int nLocalPI = option.get <int> ("nLocalPI");

    Abc_Start();
    string Command = string("read " + genlib);
    DASSERT( !Cmd_CommandExecute(Abc_FrameGetGlobalFrame(), Command.c_str()) );
    Abc_Ntk_t * pNtk = Io_Read(const_cast <char *>(input.c_str()), IO_FILE_BLIF, 1, 0);
    shared_ptr <Ckt_Ntk_t> pNtkRef = make_shared <Ckt_Ntk_t> (pNtk);
    pNtkRef->Init(102400);
    pNtkRef->LogicSim(false);
    float error = 0.0f;
    for (int i = 0; i < 100; ++i) {
        cout << i << endl;
        App_CommandMfs(pNtk, pNtkRef, nFrame, error, nLocalPI);
        stringstream ss;
        string str;
        ss << pNtk->pName << "_" << i << "_" << error;
        ss >> str;
        cout << str << endl;
        Ckt_Synthesis(pNtk, str);

        Abc_Ntk_t * pNtkTmp = pNtk;
        pNtk = Abc_NtkDup(pNtk);
        Abc_NtkDelete(pNtkTmp);
    }
    Abc_NtkDelete(pNtk);
    Abc_Stop();

    return 0;
}
