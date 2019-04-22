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
    assert(system("if [ ! -d mapped ]; then mkdir mapped; fi") != -1);
    Command = string("write_blif mapped/");
    Command += fileName;
    assert( !Cmd_CommandExecute(pAbc, Command.c_str()) );

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
