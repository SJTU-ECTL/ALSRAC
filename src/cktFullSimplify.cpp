#include "cktFullSimplify.h"

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


void Ckt_FullSimplifyTest(int argc, char * argv[])
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
    DASSERT(Cmd_CommandExecute(pAbc, command.c_str()) == 0);
    command = "read_blif " + file;
    DASSERT(Cmd_CommandExecute(pAbc, command.c_str()) == 0);
    shared_ptr <Ckt_Ntk_t> pCktNtk = make_shared <Ckt_Ntk_t> (Abc_FrameReadNtk(pAbc), false);

    pCktNtk->Init(nFrame);
    pCktNtk->LogicSim(false);

    vector < shared_ptr <Ckt_Obj_t> > vRoots;
    for (int i = 0; i < pCktNtk->GetObjNum(); ++i) {
        if (!pCktNtk->GetObj(i)->IsPO()) {
            cout << pCktNtk->GetObj(i) << endl;
            Ckt_ComputeRoot(pCktNtk->GetObj(i), vRoots);
        }
    }

    Abc_Stop();
}


void Ckt_ComputeRoot(shared_ptr <Ckt_Obj_t> pCktObj, vector < shared_ptr <Ckt_Obj_t> > & vRoots, int nTfoLevel)
{
    vRoots.clear();

    DASSERT(!pCktObj->IsPO(), "Error: cannot traverse the fanout cone of PO");

    pCktObj->GetCktNtk()->SetUnvisited();
    Ckt_ComputeRoot_Rec(pCktObj, vRoots, nTfoLevel - 1);
}


void Ckt_ComputeRoot_Rec(std::shared_ptr <Ckt_Obj_t> pCktObj, std::vector < std::shared_ptr <Ckt_Obj_t> > & vRoots, int nTfoLevel)
{
}
