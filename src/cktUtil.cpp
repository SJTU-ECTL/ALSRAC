#include "cktUtil.h"


using namespace std;


// evaluate
void Ckt_EvalASIC(Abc_Ntk_t * pNtk, string fileName, double maxDelay, bool isOutput)
{
    string Command;
    string resyn2 = "strash; balance; rewrite; refactor; balance; rewrite; rewrite -z; balance; refactor -z; rewrite -z; balance;";
    float area, delay;

    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    Abc_FrameReplaceCurrentNetwork(pAbc, Abc_NtkDup(pNtk));

    Command = resyn2 + string("amap;");
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));

    ostringstream ss;
    ss.str("");
    ss << maxDelay;
    area = Ckt_GetArea(Abc_FrameReadNtk(pAbc));
    delay = Ckt_GetDelay(Abc_FrameReadNtk(pAbc));
    for (int i = 0; i < 10; ++i) {
        if (delay < maxDelay) {
            Command = resyn2 + string("amap;");
            DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
        }
        else {
            Command = resyn2 + string("map -D ") + ss.str() + string(";");
            DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
        }
        area = Ckt_GetArea(Abc_FrameReadNtk(pAbc));
        delay = Ckt_GetDelay(Abc_FrameReadNtk(pAbc));
    }

    if (delay >= maxDelay + 0.1) {
        Command = resyn2 + string("map -D ") + ss.str() + string(";");
        DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
        area = Ckt_GetArea(Abc_FrameReadNtk(pAbc));
        delay = Ckt_GetDelay(Abc_FrameReadNtk(pAbc));
    }
    if (delay >= maxDelay + 0.1)
        cout << "Warning: exceed required delay" << endl;
    cout << "area = " << area << endl;
    cout << "delay = " << delay << endl;
    if (isOutput) {
        ss.str("");
        ss << "write_blif " << fileName << "_" << area << "_" << delay << ".blif";
        DASSERT(!Cmd_CommandExecute(pAbc, ss.str().c_str()));
    }
}


void Ckt_EvalFPGA(Abc_Ntk_t * pNtk, string fileName, string map)
{
    const string resyn2 = "strash; balance; rewrite; refactor; balance; rewrite; rewrite -z; balance; refactor -z; rewrite -z; balance;";

    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    Abc_FrameReplaceCurrentNetwork(pAbc, Abc_NtkDup(pNtk));
    string Command = resyn2 + map;

    // synthesis and mapping
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
    int size = Abc_NtkNodeNum(Abc_FrameReadNtk(pAbc));
    int depth = Abc_NtkLevel(Abc_FrameReadNtk(pAbc));
    int bestSize = size;
    int bestDepth = depth;
    bool terminate = false;
    int nRounds = 0;

    // repeat until no improvement
    while (!terminate) {
        terminate = true;
        ++nRounds;

        DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
        size = Abc_NtkNodeNum(Abc_FrameReadNtk(pAbc));
        depth = Abc_NtkLevel(Abc_FrameReadNtk(pAbc));
        if (size < bestSize) {
            bestSize = size;
            bestDepth = depth;
            terminate = false;
        }
        else if (size == bestSize && depth < bestDepth) {
            bestDepth = depth;
            terminate = false;
        }
    }

    // output
    cout << "size = " << size << endl;
    cout << "depth = " << depth << endl;
    Command = string("write_blif ");
    ostringstream ss("");
    ss << fileName << "_" << size << "_" << depth << ".blif";
    string str = ss.str();
    Command += str;
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
}


float Ckt_GetArea(Abc_Ntk_t * pNtk)
{
    DASSERT(Abc_NtkHasMapping(pNtk));
    Vec_Ptr_t * vNodes = Abc_NtkDfs(pNtk, 0);
    float area = 0.0;
    Abc_Obj_t * pObj;
    int i;
    Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pObj, i)
        area += Mio_GateReadArea( (Mio_Gate_t *)pObj->pData );
    Vec_PtrFree(vNodes);
    return area;
}


float Ckt_GetDelay(Abc_Ntk_t * pNtk)
{
    DASSERT(Abc_NtkHasMapping(pNtk));
    return Abc_GetArrivalTime(pNtk);
}


// timing
static void Abc_NtkDelayTraceSetSlack(Vec_Int_t * vSlacks, Abc_Obj_t * pObj, int iFanin, float Num)
{
    Vec_IntWriteEntry( vSlacks, Vec_IntEntry(vSlacks, Abc_ObjId(pObj)) + iFanin, Abc_Float2Int(Num) );
}


static Abc_Time_t * Abc_NodeArrival(Abc_Obj_t * pNode)
{
    return (Abc_Time_t *)pNode->pNtk->pManTime->vArrs->pArray[pNode->Id];
}


float Abc_GetArrivalTime(Abc_Ntk_t * pNtk)
{
    Vec_Ptr_t * vNodes;
    Abc_Obj_t * pNode, * pDriver;
    float tArrivalMax, arrivalTime;
    int i;
    // init time manager
    Abc_NtkTimePrepare( pNtk );
    // get all nodes in topological order
    vNodes = Abc_NtkDfs( pNtk, 1 );
    // get arrival time
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pNode, i ) {
        if (!Abc_NodeIsConst(pNode))
            Abc_NodeDelayTraceArrival( pNode, nullptr );
    }
    Vec_PtrFree( vNodes );
    // get the latest arrival times
    tArrivalMax = -ABC_INFINITY;
    Abc_NtkForEachCo( pNtk, pNode, i )
    {
        pDriver = Abc_ObjFanin0(pNode);
        arrivalTime = Abc_NodeArrival(pDriver)->Rise;
        if ( tArrivalMax < arrivalTime )
            tArrivalMax = arrivalTime;
    }
    return tArrivalMax;
}


void Abc_NtkTimePrepare(Abc_Ntk_t * pNtk)
{
    Abc_Obj_t * pObj;
    Abc_Time_t ** ppTimes, * pTime;
    int i;
    // if there is no timing manager, allocate and initialize
    if ( pNtk->pManTime == NULL )
    {
        pNtk->pManTime = Abc_ManTimeStart(pNtk);
        Abc_NtkTimeInitialize( pNtk, NULL );
        return;
    }
    // if timing manager is given, expand it if necessary
    Abc_ManTimeExpand( pNtk->pManTime, Abc_NtkObjNumMax(pNtk), 0 );
    // clean arrivals except for PIs
    ppTimes = (Abc_Time_t **)pNtk->pManTime->vArrs->pArray;
    Abc_NtkForEachNode( pNtk, pObj, i )
    {
        pTime = ppTimes[pObj->Id];
        pTime->Fall = pTime->Rise = Abc_ObjFaninNum(pObj) ? -ABC_INFINITY : 0; // set contant node arrivals to zero
    }
    Abc_NtkForEachCo( pNtk, pObj, i )
    {
        pTime = ppTimes[pObj->Id];
        pTime->Fall = pTime->Rise = -ABC_INFINITY;
    }
    // clean required except for POs
    ppTimes = (Abc_Time_t **)pNtk->pManTime->vReqs->pArray;
    Abc_NtkForEachNode( pNtk, pObj, i )
    {
        pTime = ppTimes[pObj->Id];
        pTime->Fall = pTime->Rise = ABC_INFINITY;
    }
    Abc_NtkForEachCi( pNtk, pObj, i )
    {
        pTime = ppTimes[pObj->Id];
        pTime->Fall = pTime->Rise = ABC_INFINITY;
    }
}


void Abc_NodeDelayTraceArrival(Abc_Obj_t * pNode, Vec_Int_t * vSlacks)
{
    Abc_Obj_t * pFanin;
    Abc_Time_t * pTimeIn, * pTimeOut;
    float tDelayBlockRise, tDelayBlockFall;
    Mio_PinPhase_t PinPhase;
    Mio_Pin_t * pPin;
    int i;

    // start the arrival time of the node
    pTimeOut = Abc_NodeArrival(pNode);
    pTimeOut->Rise = pTimeOut->Fall = -ABC_INFINITY;
    // consider the buffer
    if ( Abc_ObjIsBarBuf(pNode) )
    {
        pTimeIn = Abc_NodeArrival(Abc_ObjFanin0(pNode));
        *pTimeOut = *pTimeIn;
        return;
    }
    // go through the pins of the gate
    pPin = Mio_GateReadPins((Mio_Gate_t *)pNode->pData);
    Abc_ObjForEachFanin( pNode, pFanin, i )
    {
        pTimeIn = Abc_NodeArrival(pFanin);
        // get the interesting parameters of this pin
        PinPhase = Mio_PinReadPhase(pPin);
        tDelayBlockRise = (float)Mio_PinReadDelayBlockRise( pPin );
        tDelayBlockFall = (float)Mio_PinReadDelayBlockFall( pPin );
        // compute the arrival times of the positive phase
        if ( PinPhase != MIO_PHASE_INV )  // NONINV phase is present
        {
            if ( pTimeOut->Rise < pTimeIn->Rise + tDelayBlockRise )
                pTimeOut->Rise = pTimeIn->Rise + tDelayBlockRise;
            if ( pTimeOut->Fall < pTimeIn->Fall + tDelayBlockFall )
                pTimeOut->Fall = pTimeIn->Fall + tDelayBlockFall;
        }
        if ( PinPhase != MIO_PHASE_NONINV )  // INV phase is present
        {
            if ( pTimeOut->Rise < pTimeIn->Fall + tDelayBlockRise )
                pTimeOut->Rise = pTimeIn->Fall + tDelayBlockRise;
            if ( pTimeOut->Fall < pTimeIn->Rise + tDelayBlockFall )
                pTimeOut->Fall = pTimeIn->Rise + tDelayBlockFall;
        }
        pPin = Mio_PinReadNext(pPin);
    }

    // compute edge slacks
    if ( vSlacks )
    {
        float Slack;
        // go through the pins of the gate
        pPin = Mio_GateReadPins((Mio_Gate_t *)pNode->pData);
        Abc_ObjForEachFanin( pNode, pFanin, i )
        {
            pTimeIn = Abc_NodeArrival(pFanin);
            // get the interesting parameters of this pin
            PinPhase = Mio_PinReadPhase(pPin);
            tDelayBlockRise = (float)Mio_PinReadDelayBlockRise( pPin );
            tDelayBlockFall = (float)Mio_PinReadDelayBlockFall( pPin );
            // compute the arrival times of the positive phase
            Slack = ABC_INFINITY;
            if ( PinPhase != MIO_PHASE_INV )  // NONINV phase is present
            {
                Slack = Abc_MinFloat( Slack, Abc_AbsFloat(pTimeIn->Rise + tDelayBlockRise - pTimeOut->Rise) );
                Slack = Abc_MinFloat( Slack, Abc_AbsFloat(pTimeIn->Fall + tDelayBlockFall - pTimeOut->Fall) );
            }
            if ( PinPhase != MIO_PHASE_NONINV )  // INV phase is present
            {
                Slack = Abc_MinFloat( Slack, Abc_AbsFloat(pTimeIn->Fall + tDelayBlockRise - pTimeOut->Rise) );
                Slack = Abc_MinFloat( Slack, Abc_AbsFloat(pTimeIn->Rise + tDelayBlockFall - pTimeOut->Fall) );
            }
            pPin = Mio_PinReadNext(pPin);
            Abc_NtkDelayTraceSetSlack( vSlacks, pNode, i, Slack );
        }
    }
}


void Abc_ManTimeExpand(Abc_ManTime_t * p, int nSize, int fProgressive)
{
    Vec_Ptr_t * vTimes;
    Abc_Time_t * ppTimes, * ppTimesOld, * pTime;
    int nSizeOld, nSizeNew, i;

    nSizeOld = p->vArrs->nSize;
    if ( nSizeOld >= nSize )
        return;
    nSizeNew = fProgressive? 2 * nSize : nSize;
    if ( nSizeNew < 100 )
        nSizeNew = 100;

    vTimes = p->vArrs;
    Vec_PtrGrow( vTimes, nSizeNew );
    vTimes->nSize = nSizeNew;
    ppTimesOld = ( nSizeOld == 0 )? NULL : (Abc_Time_t *)vTimes->pArray[0];
    ppTimes = ABC_REALLOC( Abc_Time_t, ppTimesOld, nSizeNew );
    for ( i = 0; i < nSizeNew; i++ )
        vTimes->pArray[i] = ppTimes + i;
    for ( i = nSizeOld; i < nSizeNew; i++ )
    {
        pTime = (Abc_Time_t *)vTimes->pArray[i];
        pTime->Rise  = -ABC_INFINITY;
        pTime->Fall  = -ABC_INFINITY;
    }

    vTimes = p->vReqs;
    Vec_PtrGrow( vTimes, nSizeNew );
    vTimes->nSize = nSizeNew;
    ppTimesOld = ( nSizeOld == 0 )? NULL : (Abc_Time_t *)vTimes->pArray[0];
    ppTimes = ABC_REALLOC( Abc_Time_t, ppTimesOld, nSizeNew );
    for ( i = 0; i < nSizeNew; i++ )
        vTimes->pArray[i] = ppTimes + i;
    for ( i = nSizeOld; i < nSizeNew; i++ )
    {
        pTime = (Abc_Time_t *)vTimes->pArray[i];
        pTime->Rise  = ABC_INFINITY;
        pTime->Fall  = ABC_INFINITY;
    }
}


Abc_ManTime_t * Abc_ManTimeStart(Abc_Ntk_t * pNtk)
{
    int fUseZeroDefaultOutputRequired = 1;
    Abc_ManTime_t * p;
    Abc_Obj_t * pObj; int i;
    p = pNtk->pManTime = ABC_ALLOC( Abc_ManTime_t, 1 );
    memset( p, 0, sizeof(Abc_ManTime_t) );
    p->vArrs = Vec_PtrAlloc( 0 );
    p->vReqs = Vec_PtrAlloc( 0 );
    // set default default input=arrivals (assumed to be 0)
    // set default default output-requireds (can be either 0 or +infinity, based on the flag)
    p->tReqDef.Rise = fUseZeroDefaultOutputRequired ? 0 : ABC_INFINITY;
    p->tReqDef.Fall = fUseZeroDefaultOutputRequired ? 0 : ABC_INFINITY;
    // extend manager
    Abc_ManTimeExpand( p, Abc_NtkObjNumMax(pNtk) + 1, 0 );
    // set the default timing for CIs
    Abc_NtkForEachCi( pNtk, pObj, i )
        Abc_NtkTimeSetArrival( pNtk, Abc_ObjId(pObj), p->tArrDef.Rise, p->tArrDef.Rise );
    // set the default timing for COs
    Abc_NtkForEachCo( pNtk, pObj, i )
        Abc_NtkTimeSetRequired( pNtk, Abc_ObjId(pObj), p->tReqDef.Rise, p->tReqDef.Rise );
    return p;
}


// misc
void Ckt_NtkRename(Abc_Ntk_t * pNtk, const char * name)
{
    free(pNtk->pName);
    pNtk->pName = (char *)malloc(strlen(name) + 1);
    memcpy(pNtk->pName, name, strlen(name) + 1);
}


Abc_Obj_t * Ckt_GetConst(Abc_Ntk_t * pNtk, bool isConst1)
{
    Abc_Obj_t * pNode = nullptr;
    int i = 0;
    Abc_Obj_t * pConst = nullptr;
    if (!isConst1) {
        Abc_NtkForEachNode(pNtk, pNode, i) {
            if (Abc_NodeIsConst0(pNode)) {
                pConst = pNode;
                break;
            }
        }
    }
    else {
        Abc_NtkForEachNode(pNtk, pNode, i) {
            if (Abc_NodeIsConst1(pNode)) {
                pConst = pNode;
                break;
            }
        }
    }
    if (pConst == nullptr) {
        if (!isConst1)
            pConst = Abc_NtkCreateNodeConst0(pNtk);
        else
            pConst = Abc_NtkCreateNodeConst1(pNtk);
    }
    return pConst;
}


void Ckt_PrintNodeFunc(Abc_Obj_t * pNode)
{
    DASSERT(pNode != nullptr);
    DASSERT(Abc_NtkIsAigLogic(pNode->pNtk));
    Vec_Vec_t * vLevels = Vec_VecAlloc( 10 );
    // set the input names
    Abc_Obj_t * pFanin = nullptr;
    int k = 0;
    Abc_ObjForEachFanin( pNode, pFanin, k )
        Hop_IthVar((Hop_Man_t *)pNode->pNtk->pManFunc, k)->pData = Abc_ObjName(pFanin);
    // write the formula
    cout << Abc_ObjName(pNode) << " = ";
    Hop_ObjPrintEqn( stdout, (Hop_Obj_t *)pNode->pData, vLevels, 0 );
    cout << endl;
    Vec_VecFree( vLevels );
}


void Ckt_PrintHopFunc(Hop_Obj_t * pHopObj, Vec_Ptr_t * vFanins)
{
    DASSERT(pHopObj != nullptr);
    DASSERT(vFanins != nullptr);
    Vec_Vec_t * vLevels = Vec_VecAlloc( 10 );
    // set the input names
    int k = 0;
    Abc_Obj_t * pFanin = nullptr;
    Vec_PtrForEachEntry(Abc_Obj_t *, vFanins, pFanin, k)
        Hop_IthVar((Hop_Man_t *)pFanin->pNtk->pManFunc, k)->pData = Abc_ObjName(pFanin);
    // write the formula
    cout << "F = ";
    Hop_ObjPrintEqn( stdout, pHopObj, vLevels, 0 );
    cout << endl;
    // cout << "(";
    // Vec_PtrForEachEntry(Abc_Obj_t *, vFanins, pFanin, k)
    //     cout << Abc_ObjName(pFanin) << ",";
    // cout << ")" << endl;
    Vec_VecFree( vLevels );
}


void Ckt_WriteBlif(Abc_Ntk_t * pNtk, string fileName)
{
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    Abc_FrameReplaceCurrentNetwork(pAbc, Abc_NtkDup(pNtk));
    string Command = "write_blif " + fileName;
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
}


void Ckt_PrintSop(std::string sop)
{
    for (auto &ch: sop) {
        if (ch == '\n')
            cout << ";";
        else
            cout << ch;
    }
}


void Ckt_PrintNodes(Vec_Ptr_t * vFanins)
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Vec_PtrForEachEntry(Abc_Obj_t *, vFanins, pObj, i)
        cout << Abc_ObjName(pObj) << ",";
}


void Ckt_PrintFanins(Abc_Obj_t * pObj)
{
    Abc_Obj_t * pFanin = nullptr;
    int i = 0;
    Abc_ObjForEachFanin(pObj, pFanin, i)
        cout << Abc_ObjName(pFanin) << ",";
}
