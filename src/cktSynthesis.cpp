#include "cktSynthesis.h"


using namespace std;
using namespace abc;


float Ckt_Synthesis(Abc_Ntk_t * pNtk, string fileName)
{
    string Command;
    string ntkName(pNtk->pName);
    float area, delay;

    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    Abc_FrameReplaceCurrentNetwork(pAbc, Abc_NtkDup(pNtk));

    for (int i = 0; i < 10; ++i) {
        Command = string("strash; balance; rewrite; refactor; balance; rewrite; rewrite -z; balance; refactor -z; rewrite -z; balance");
        assert( !Cmd_CommandExecute(pAbc, Command.c_str()) );
        Command = string("map -a;");
        assert( !Cmd_CommandExecute(pAbc, Command.c_str()) );
    }
    area = Ckt_GetArea(Abc_FrameReadNtk(pAbc));
    delay = Abc_GetArrivalTime(Abc_FrameReadNtk(pAbc));
    cout << "area = " << area << endl << "delay = " << delay << endl;
    DASSERT(system("if [ ! -d appntk ]; then mkdir appntk; fi") != -1);
    Command = string("write_blif ");
    ostringstream ss;
    ss << "appntk/" << fileName << "_" << area << "_" << delay << ".blif";
    string str = ss.str();
    Command += str;
    cout << Command << endl;
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));

    ss.str("");
    ss << "sis -x -c \"read_library data/genlib/mcnc.genlib;read_blif " << str << ";print_map_stats\" | grep -e \"Most Negative Slack\" -e \"Total Area\"";
    cout << ss.str() << endl;
    DASSERT(system(const_cast <char*>(ss.str().c_str())) != -1);

    return area;
}


float Ckt_GetArea(Abc_Ntk_t * pNtk)
{
    assert(Abc_NtkHasMapping(pNtk));
    Vec_Ptr_t * vNodes = Abc_NtkDfs(pNtk, 0);
    float area = 0.0;
    Abc_Obj_t * pObj;
    int i;
    Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pObj, i)
        area += Mio_GateReadArea( (Mio_Gate_t *)pObj->pData );
    Vec_PtrFree(vNodes);
    return area;
}
