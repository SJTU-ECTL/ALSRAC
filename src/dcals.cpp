#include "dcals.h"


using namespace std;


Dcals_Man_t::Dcals_Man_t(Abc_Ntk_t * pNtk, int nFrame, int cutSize, double metricBound, int metricType, int mapType)
{
    this->pOriNtk = pNtk;
    this->pAppNtk = Abc_NtkDup(this->pOriNtk);
    this->metricType = metricType;
    this->mapType = mapType;
    this->nFrame = nFrame;
    this->cutSize = cutSize;
    this->metric = 0;
    this->metricBound = metricBound;
    if (Abc_NtkHasMapping(this->pOriNtk))
        this->maxDelay = Ckt_GetDelay(this->pOriNtk);
    else
        this->maxDelay = DBL_MAX;
    this->pPars = InitMfsPars();
    DASSERT(nFrame > 0);
    DASSERT(pOriNtk != nullptr);
    DASSERT(Abc_NtkIsLogic(pOriNtk));
    DASSERT(Abc_NtkToAig(pOriNtk));
    DASSERT(Abc_NtkHasAig(pOriNtk));
}


Dcals_Man_t::~Dcals_Man_t()
{
    Abc_NtkDelete(pAppNtk);
    pAppNtk = nullptr;
    delete pPars;
}


Mfs_Par_t * Dcals_Man_t::InitMfsPars()
{
    Mfs_Par_t * pPars = new Mfs_Par_t;
    memset(pPars, 0, sizeof(Mfs_Par_t));
    pPars->nWinTfoLevs  =    2;
    pPars->nFanoutsMax  =   30;
    pPars->nDepthMax    =   20;
    pPars->nWinMax      =  300;
    pPars->nGrowthLevel =    0;
    pPars->nBTLimit     = 5000;
    pPars->fRrOnly      =    0;
    pPars->fResub       =    1;
    pPars->fArea        =    0;
    pPars->fMoreEffort  =    0;
    pPars->fSwapEdge    =    0;
    pPars->fOneHotness  =    0;
    pPars->fVerbose     =    0;
    pPars->fVeryVerbose =    0;
    return pPars;
}


void Dcals_Man_t::DCALS()
{
    if (mapType == 1) {
        cout << "Original circuit: size = " << Abc_NtkNodeNum(pOriNtk)
        << ", depth = " << Abc_NtkLevel(pOriNtk) << endl;
    }
    clock_t st = clock();
    while (metric < metricBound) {
        if (nFrame != 0)
            LocalAppChange();
        else
            ConstReplace();
        cout << "time = " << clock() - st << endl << endl;
    }
}


void Dcals_Man_t::LocalAppChange()
{
    static int pat1, pat2;

    DASSERT(Abc_NtkIsLogic(pAppNtk));
    DASSERT(Abc_NtkToAig(pAppNtk));
    DASSERT(Abc_NtkHasAig(pAppNtk));
    DASSERT(pAppNtk->pExcare == nullptr);

    // new seed for logic simulation
    random_device rd;
    seed = static_cast <unsigned>(rd());
    cout << "nframe = " << nFrame << " seed = " << seed << endl;
    cout << "patience = " << pat1 << "," << pat2 << endl;

    // maximum fanin limitation
    int nFaninMax = Abc_NtkGetFaninMax(pAppNtk);
    if ( nFaninMax > 8 )
    {
        printf( "Nodes with more than %d fanins will not be processed.\n", 8 );
        nFaninMax = 8;
    }

    // start the manager
    Mfs_Man_t * pMfsMan = Mfs_ManAlloc(pPars);
    DASSERT(pMfsMan->pCare == nullptr);
    pMfsMan->pNtk = pAppNtk;
    pMfsMan->nFaninMax = nFaninMax;

    // compute level
    Abc_NtkLevel(pAppNtk);
    Abc_NtkStartReverseLevels(pAppNtk, pPars->nGrowthLevel);

    // generate candidates
    double bestEr = 1.0;
    int bestId = -1;
    Abc_Obj_t * pObjApp = nullptr;
    int i = 0;
    boost::progress_display pd(Abc_NtkNodeNum(pAppNtk));
    clock_t st;
    uint64_t t1 = 0, t2 = 0, t3 = 0;
    Abc_NtkForEachNode(pAppNtk, pObjApp, i) {
        // process bar
        ++pd;
        // // skip nodes with deep levels and too much fanins
        // if ( pMfsMan->pPars->nDepthMax && (int)pObjApp->Level > pMfsMan->pPars->nDepthMax )
        //     continue;
        // if ( Abc_ObjFaninNum(pObjApp) < 2 || Abc_ObjFaninNum(pObjApp) > nFaninMax )
        //     continue;

        // print node function
        if ( pMfsMan->pPars->fVeryVerbose )
            Ckt_PrintNodeFunc(pObjApp);
        // evaluate a candidate
        st = clock();
        Hop_Obj_t * pCandNode = LocalAppChangeNode(pMfsMan, pObjApp);
        t2 += clock() - st;
        st = clock();
        if (pCandNode != nullptr) {
            if ( pMfsMan->pPars->fVeryVerbose )
                Ckt_PrintHopFunc(pCandNode, pMfsMan->vMfsFanins);

            Abc_Ntk_t * pNewNtk = Abc_NtkDup(pAppNtk);
            Abc_NtkLevel(pNewNtk);
            Abc_NtkStartReverseLevels(pNewNtk, pPars->nGrowthLevel);
            pMfsMan->pNtk = pNewNtk;

            Hop_Obj_t * pTmp = LocalAppChangeNode(pMfsMan, Abc_NtkObj(pNewNtk, i));
            Ckt_NtkMfsUpdateNetwork(pMfsMan, Abc_NtkObj(pNewNtk, i), pMfsMan->vMfsFanins, pTmp);

            double er = 0;
            if (!metricType)
                er = MeasureER(pOriNtk, pNewNtk, 102400, 100);
            else
                er = MeasureAEMR(pOriNtk, pNewNtk, 10240, 100);
            if (er < bestEr) {
                bestEr = er;
                bestId = i;
            }

            Abc_NtkStopReverseLevels(pNewNtk);
            Abc_NtkDelete(pNewNtk);
            pMfsMan->pNtk = pAppNtk;
        }
        t3 += clock() - st;
    }
    // cout << "t1 = " << t1 << ", t2 = " << t2 << ", t3 = " << t3 << endl;

    // apply local approximate change
    if (bestId != -1) {
        pat1 = 0;
        bool isApply = false;
        if (bestEr <= metricBound) {
            pat1 = pat2 = 0;
            isApply = true;
        }
        else {
            ++pat2;
            isApply = false;
            if (pat2 == 5)
                isApply = true;
        }
        if (isApply) {
            cout << "best node " << Abc_ObjName(Abc_NtkObj(pAppNtk, bestId)) << " best error " << bestEr << endl;
            metric = bestEr;
            Hop_Obj_t * pCandNode = LocalAppChangeNode(pMfsMan, Abc_NtkObj(pAppNtk, bestId));
            Ckt_NtkMfsUpdateNetwork(pMfsMan, Abc_NtkObj(pAppNtk, bestId), pMfsMan->vMfsFanins, pCandNode);
        }
    }
    else {
        ++pat1;
        if (pat1 == 5) {
            nFrame >>= 1;
            pat1 = pat2 = 0;
        }
    }

    // recycle memory
    Abc_NtkStopReverseLevels(pAppNtk);
    Mfs_ManStop(pMfsMan);

    // disturb the network
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    Abc_FrameReplaceCurrentNetwork(pAbc, Abc_NtkDup(pAppNtk));
    string Command = string("strash; balance; rewrite; refactor; balance; rewrite; rewrite -z; balance; refactor -z; rewrite -z; balance");
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
    Command = string("logic; sweep; mfs");
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
    Abc_NtkDelete(pAppNtk);
    pAppNtk = Abc_NtkDup(Abc_FrameReadNtk(pAbc));

    // evaluate the current approximate circuit
    ostringstream fileName("");
    fileName << pAppNtk->pName << "_" << metric;
    if (!mapType)
        Ckt_EvalASIC(pAppNtk, fileName.str(), maxDelay);
    else {
        Ckt_EvalFPGA(pAppNtk, fileName.str(), "strash; if -K 6 -a;");
        Ckt_EvalFPGA(pAppNtk, fileName.str(), "strash; if -K 6;");
    }
}


void Dcals_Man_t::ConstReplace()
{
    DASSERT(Abc_NtkIsLogic(pAppNtk));
    DASSERT(Abc_NtkToAig(pAppNtk));
    DASSERT(Abc_NtkHasAig(pAppNtk));

    double er = 0;
    double bestEr = 1.0;
    int bestId = -1;
    bool isConst0 = false;
    Abc_Obj_t * pObjApp = nullptr;
    int i = 0;
    cout << "nframe = " << nFrame << endl;
    Abc_NtkForEachNode(pAppNtk, pObjApp, i) {
        if (Abc_NodeIsConst(pObjApp))
            continue;
        Abc_Ntk_t * pCandNtk = Abc_NtkDup(pAppNtk);
        Abc_Obj_t * pObjCand = Abc_NtkObj(pCandNtk, i);
        DASSERT(!strcmp(Abc_ObjName(pObjCand), Abc_ObjName(pObjApp)));

        // replace with const 0
        Abc_Obj_t * pConst0 = Ckt_GetConst(pCandNtk, 0);
        Abc_ObjReplace(pObjCand, pConst0);

        // evaluate
        if (!metricType)
            er = MeasureER(pOriNtk, pCandNtk, 102400, 100);
        else
            er = MeasureAEMR(pOriNtk, pCandNtk, 10240, 100);
        if (er < bestEr) {
            bestEr = er;
            bestId = i;
            isConst0 = true;
        }

        // replace with const 1
        Abc_Obj_t * pConst1 = Ckt_GetConst(pCandNtk, 1);
        Abc_ObjReplace(pConst0, pConst1);

        // evaluate
        if (!metricType)
            er = MeasureER(pOriNtk, pCandNtk, 102400, 100);
        else
            er = MeasureAEMR(pOriNtk, pCandNtk, 10240, 100);
        if (er < bestEr) {
            bestEr = er;
            bestId = i;
            isConst0 = false;
        }

        Abc_NtkDelete(pCandNtk);
    }

    // apply the constant replacement
    if (bestId != -1) {
        cout << "best node " << Abc_ObjName(Abc_NtkObj(pAppNtk, bestId)) << " best error " << bestEr << endl;
        if (isConst0)
            Abc_ObjReplace(Abc_NtkObj(pAppNtk, bestId), Ckt_GetConst(pAppNtk, 0));
        else
            Abc_ObjReplace(Abc_NtkObj(pAppNtk, bestId), Ckt_GetConst(pAppNtk, 1));
        metric = bestEr;
    }

    // disturb the network
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    Abc_FrameReplaceCurrentNetwork(pAbc, Abc_NtkDup(pAppNtk));
    string Command = string("strash; balance; rewrite; refactor; balance; rewrite; rewrite -z; balance; refactor -z; rewrite -z; balance");
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
    Command = string("logic; sweep; mfs");
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
    Abc_NtkDelete(pAppNtk);
    pAppNtk = Abc_NtkDup(Abc_FrameReadNtk(pAbc));

    // evaluate the current approximate circuit
    ostringstream fileName("");
    fileName << pAppNtk->pName << "_" << metric;
    if (!mapType)
        Ckt_EvalASIC(pAppNtk, fileName.str(), maxDelay);
    else {
        Ckt_EvalFPGA(pAppNtk, fileName.str(), "strash; if -K 6 -a;");
        Ckt_EvalFPGA(pAppNtk, fileName.str(), "strash; if -K 6;");
    }
}


Hop_Obj_t * Dcals_Man_t::LocalAppChangeNode(Mfs_Man_t * p, Abc_Obj_t * pNode)
{
    // prepare data structure for this node
    Mfs_ManClean(p);
    // compute window roots, window support, and window nodes
    p->vRoots = Abc_MfsComputeRoots(pNode, p->pPars->nWinTfoLevs, p->pPars->nFanoutsMax);
    p->vSupp  = Abc_NtkNodeSupport(p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots));
    p->vNodes = Abc_NtkDfsNodes(p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots));
    if (p->pPars->nWinMax && Vec_PtrSize(p->vNodes) > p->pPars->nWinMax) {
        // cout << "Warning: window size reaches to the limitation" << endl;
        return nullptr;
    }
    // compute the divisors of the window
    p->vDivs  = Abc_MfsComputeDivisors(p, pNode, Abc_ObjRequiredLevel(pNode) - 1);
    p->nTotalDivs += Vec_PtrSize(p->vDivs) - Abc_ObjFaninNum(pNode);
    // construct AIG for the window
    p->pAigWin = ConstructAppAig(p, pNode);
    // translate it into CNF
    p->pCnf = Cnf_DeriveSimple(p->pAigWin, 1 + Vec_PtrSize(p->vDivs));
    // create the SAT problem
    p->pSat = Abc_MfsCreateSolverResub(p, nullptr, 0, 0);
    if (p->pSat == nullptr)
        return nullptr;
    // solve the SAT problem
    return Ckt_NtkMfsResubNode(p, pNode);
}


Aig_Man_t * Dcals_Man_t::ConstructAppAig(Mfs_Man_t * p, Abc_Obj_t * pNode)
{
    Aig_Obj_t * pObjAig, * pDC, * pRoot;
    // find local pi
    Vec_Ptr_t * vLocalPI = Ckt_FindCut(pNode, cutSize);
    // perform logic simulation
    Simulator_Pro_t smlt(pNode->pNtk, nFrame);
    smlt.Input(seed);
    smlt.Simulate();
    set <string> patterns;
    for (int i = 0; i < smlt.GetBlockNum(); ++i) {
        for (int j = 0; j < 64; ++j) {
            string pattern = "";
            Abc_Obj_t * pObj = nullptr;
            int k = 0;
            Vec_PtrForEachEntry(Abc_Obj_t *, vLocalPI, pObj, k)
                pattern += smlt.GetValue(pObj, i, j)? '1': '0';
            patterns.insert(pattern);
        }
    }

    // start the new manager
    Aig_Man_t * pMan = Aig_ManStart( 1000 );
    // construct the root node's AIG cone
    pObjAig = Abc_NtkConstructAig_rec( p, pNode, pMan );
    Aig_ObjCreateCo( pMan, pObjAig );

    // add approximate care set
    pRoot = Aig_ManConst0(pMan);
    for (auto pattern: patterns) {
        pDC = Aig_ManConst1(pMan);
        int k = 0;
        Abc_Obj_t * pObj = nullptr;
        Vec_PtrForEachEntry(Abc_Obj_t *, vLocalPI, pObj, k) {
            if (pattern[k] == '1')
                pDC = Aig_And(pMan, pDC, (Aig_Obj_t *)pObj->pCopy);
            else
                pDC = Aig_And(pMan, pDC, Aig_Not((Aig_Obj_t *)pObj->pCopy));
        }
        pRoot = Aig_Or(pMan, pRoot, pDC);
    }
    Aig_ObjCreateCo(pMan, pRoot);

    Vec_PtrFree(vLocalPI);

    // construct the node
    pObjAig = (Aig_Obj_t *)pNode->pCopy;
    Aig_ObjCreateCo( pMan, pObjAig );
    // construct the divisors
    Abc_Obj_t * pFanin = nullptr;
    int i = 0;
    Vec_PtrForEachEntry(Abc_Obj_t *, p->vDivs, pFanin, i)
    {
        pObjAig = (Aig_Obj_t *)pFanin->pCopy;
        Aig_ObjCreateCo(pMan, pObjAig);
    }

    Aig_ManCleanup(pMan);
    return pMan;
}


Aig_Obj_t * Abc_NtkConstructAig_rec( Mfs_Man_t * p, Abc_Obj_t * pNode, Aig_Man_t * pMan )
{
    Aig_Obj_t * pRoot, * pExor;
    Abc_Obj_t * pObj;
    int i;
    // assign AIG nodes to the leaves
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vSupp, pObj, i )
        pObj->pCopy = pObj->pNext = (Abc_Obj_t *)Aig_ObjCreateCi( pMan );
    // strash intermediate nodes
    Abc_NtkIncrementTravId( pNode->pNtk );
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vNodes, pObj, i )
    {
        Abc_MfsConvertHopToAig( pObj, pMan );
        if ( pObj == pNode )
            pObj->pNext = Abc_ObjNot(pObj->pNext);
    }
    // create the observability condition
    pRoot = Aig_ManConst0(pMan);
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vRoots, pObj, i )
    {
        pExor = Aig_Exor( pMan, (Aig_Obj_t *)pObj->pCopy, (Aig_Obj_t *)pObj->pNext );
        pRoot = Aig_Or( pMan, pRoot, pExor );
    }
    return pRoot;
}


void Abc_MfsConvertHopToAig( Abc_Obj_t * pObjOld, Aig_Man_t * pMan )
{
    Hop_Man_t * pHopMan;
    Hop_Obj_t * pRoot;
    Abc_Obj_t * pFanin;
    int i;
    // get the local AIG
    pHopMan = (Hop_Man_t *)pObjOld->pNtk->pManFunc;
    pRoot = (Hop_Obj_t *)pObjOld->pData;
    // check the case of a constant
    if ( Hop_ObjIsConst1( Hop_Regular(pRoot) ) )
    {
        pObjOld->pCopy = (Abc_Obj_t *)Aig_NotCond( Aig_ManConst1(pMan), Hop_IsComplement(pRoot) );
        pObjOld->pNext = pObjOld->pCopy;
        return;
    }
    // assign the fanin nodes
    Abc_ObjForEachFanin( pObjOld, pFanin, i )
        Hop_ManPi(pHopMan, i)->pData = pFanin->pCopy;
    // construct the AIG
    Abc_MfsConvertHopToAig_rec( Hop_Regular(pRoot), pMan );
    pObjOld->pCopy = (Abc_Obj_t *)Aig_NotCond( (Aig_Obj_t *)Hop_Regular(pRoot)->pData, Hop_IsComplement(pRoot) );
    Hop_ConeUnmark_rec( Hop_Regular(pRoot) );
    // assign the fanin nodes
    Abc_ObjForEachFanin( pObjOld, pFanin, i )
        Hop_ManPi(pHopMan, i)->pData = pFanin->pNext;
    // construct the AIG
    Abc_MfsConvertHopToAig_rec( Hop_Regular(pRoot), pMan );
    pObjOld->pNext = (Abc_Obj_t *)Aig_NotCond( (Aig_Obj_t *)Hop_Regular(pRoot)->pData, Hop_IsComplement(pRoot) );
    Hop_ConeUnmark_rec( Hop_Regular(pRoot) );
}


void Abc_MfsConvertHopToAig_rec( Hop_Obj_t * pObj, Aig_Man_t * pMan )
{
    assert( !Hop_IsComplement(pObj) );
    if ( !Hop_ObjIsNode(pObj) || Hop_ObjIsMarkA(pObj) )
        return;
    Abc_MfsConvertHopToAig_rec( Hop_ObjFanin0(pObj), pMan );
    Abc_MfsConvertHopToAig_rec( Hop_ObjFanin1(pObj), pMan );
    pObj->pData = Aig_And( pMan, (Aig_Obj_t *)Hop_ObjChild0Copy(pObj), (Aig_Obj_t *)Hop_ObjChild1Copy(pObj) );
    assert( !Hop_ObjIsMarkA(pObj) ); // loop detection
    Hop_ObjSetMarkA( pObj );
}


Vec_Ptr_t * Ckt_FindCut(Abc_Obj_t * pNode, int nMax)
{
    deque <Abc_Obj_t *> fringe;
    Abc_Obj_t * pObj;
    int i;
    // set the traversal ID
    Abc_NtkIncrementTravId(pNode->pNtk);
    // start the array of nodes
    Vec_Ptr_t * vNodes = Vec_PtrAlloc(20);
    fringe.emplace_back(pNode);
    while (fringe.size() && static_cast <int>(fringe.size()) < nMax) {
        // get the front node
        Abc_Obj_t * pFrontNode = fringe.front();
        // expand the front node
        Abc_ObjForEachFanin(pFrontNode, pObj, i) {
            if (!Abc_NodeIsTravIdCurrent(pObj)) {
                Abc_NodeSetTravIdCurrent(pObj);
                fringe.emplace_back(pObj);
            }
        }
        // if the front node is PI or const, add it to vNodes
        if (Abc_ObjFaninNum(pFrontNode) == 0) {
            Vec_PtrPush(vNodes, pFrontNode);
            --nMax;
        }
        // pop the front node
        fringe.pop_front();
    }
    for (auto & pObj : fringe)
        Vec_PtrPush(vNodes, pObj);
    return vNodes;
}


Hop_Obj_t * Ckt_NtkMfsResubNode(Mfs_Man_t * p, Abc_Obj_t * pNode)
{
    Hop_Obj_t * pCand = nullptr;
    Abc_Obj_t * pFanin = nullptr;
    int i = 0;
    // try replacing area critical fanins
    Abc_ObjForEachFanin( pNode, pFanin, i )
        if ( !Abc_ObjIsCi(pFanin) && Abc_ObjFanoutNum(pFanin) == 1 )
        {
            pCand = Ckt_NtkMfsSolveSatResub( p, pNode, i, 0 );
            if (pCand != nullptr)
                return pCand;
        }
    // try removing redundant edges
    if ( !p->pPars->fArea )
    {
        Abc_ObjForEachFanin( pNode, pFanin, i )
            if ( Abc_ObjIsCi(pFanin) || Abc_ObjFanoutNum(pFanin) != 1 )
            {
                pCand = Ckt_NtkMfsSolveSatResub( p, pNode, i, 1 );
                if (pCand != nullptr)
                    return pCand;
            }
    }
    if ( Abc_ObjFaninNum(pNode) == p->nFaninMax )
        return nullptr;
    return nullptr;
}


Hop_Obj_t * Ckt_NtkMfsSolveSatResub( Mfs_Man_t * p, Abc_Obj_t * pNode, int iFanin, int fOnlyRemove)
{
    int fVeryVerbose = 0;
    unsigned * pData;
    int pCands[MFS_FANIN_MAX];
    int RetValue, iVar, i, nCands, nWords, w;
    abctime clk;
    Abc_Obj_t * pFanin;
    Hop_Obj_t * pFunc;
    assert( iFanin >= 0 );
    p->nTryRemoves++;

    // clean simulation info
    Vec_PtrFillSimInfo( p->vDivCexes, 0, p->nDivWords );
    p->nCexes = 0;
    if ( p->pPars->fVeryVerbose )
    {
        // printf( "%5d : Lev =%3d. Leaf =%3d. Node =%3d. Divs =%3d.  Fanin = %4d (%d/%d), MFFC = %d\n",
        //     pNode->Id, pNode->Level, Vec_PtrSize(p->vSupp), Vec_PtrSize(p->vNodes), Vec_PtrSize(p->vDivs)-Abc_ObjFaninNum(pNode),
        //     Abc_ObjFaninId(pNode, iFanin), iFanin, Abc_ObjFaninNum(pNode),
        //     Abc_ObjFanoutNum(Abc_ObjFanin(pNode, iFanin)) == 1 ? Abc_NodeMffcLabel(Abc_ObjFanin(pNode, iFanin)) : 0 );
    }

    // try fanins without the critical fanin
    nCands = 0;
    Vec_PtrClear( p->vMfsFanins );
    Abc_ObjForEachFanin( pNode, pFanin, i )
    {
        if ( i == iFanin )
            continue;
        Vec_PtrPush( p->vMfsFanins, pFanin );
        iVar = Vec_PtrSize(p->vDivs) - Abc_ObjFaninNum(pNode) + i;
        pCands[nCands++] = toLitCond( Vec_IntEntry( p->vProjVarsSat, iVar ), 1 );
    }
    RetValue = Ckt_NtkMfsTryResubOnce( p, pCands, nCands );
    // time out
    if ( RetValue == -1 )
        return nullptr;
    // Can be resubstituted by only (nFaninNum - 1) fanins
    if ( RetValue == 1 )
    {
        if ( p->pPars->fVeryVerbose )
            printf( "Node %s: Fanin %d(%s) can be removed.\n", Abc_ObjName(pNode), iFanin, Abc_ObjName(Abc_ObjFanin(pNode, iFanin)) );
        p->nNodesResub++;
        p->nNodesGainedLevel++;
clk = Abc_Clock();
        // derive the function
        pFunc = Abc_NtkMfsInterplate( p, pCands, nCands );
        if ( pFunc == nullptr )
            return nullptr;
        // update the network
        // Ckt_NtkMfsUpdateNetwork( p, pNode, p->vMfsFanins, pFunc );
p->timeInt += Abc_Clock() - clk;
        p->nRemoves++;
        return pFunc;
    }

    if ( fOnlyRemove || p->pPars->fRrOnly )
        return nullptr;

    // Possibly resubstituted by (nFaninNum - 1) fanins and one divisor
    p->nTryResubs++;
    if ( fVeryVerbose )
    {
        for ( i = 0; i < 9; i++ )
            printf( " " );
        for ( i = 0; i < Vec_PtrSize(p->vDivs)-Abc_ObjFaninNum(pNode); i++ )
            printf( "%d", i % 10 );
        for ( i = 0; i < Abc_ObjFaninNum(pNode); i++ )
            if ( i == iFanin )
                printf( "*" );
            else
                printf( "%c", 'a' + i );
        printf( "\n" );
    }
    iVar = -1;
    while ( 1 )
    {
        if ( fVeryVerbose )
        {
            printf( "%3d: %3d ", p->nCexes, iVar );
            for ( i = 0; i < Vec_PtrSize(p->vDivs); i++ )
            {
                pData = (unsigned *)Vec_PtrEntry( p->vDivCexes, i );
                printf( "%d", Abc_InfoHasBit(pData, p->nCexes-1) );
            }
            printf( "\n" );
        }

        // find the next divisor to try
        nWords = Abc_BitWordNum(p->nCexes);
        assert( nWords <= p->nDivWords );
        for ( iVar = 0; iVar < Vec_PtrSize(p->vDivs)-Abc_ObjFaninNum(pNode); iVar++ )
        {
            DASSERT(!p->pPars->fPower);
            pData  = (unsigned *)Vec_PtrEntry( p->vDivCexes, iVar );
            for ( w = 0; w < nWords; w++ )
                if ( pData[w] != (unsigned)(~0) )
                    break;
            if ( w == nWords )
                break;
        }
        if ( iVar == Vec_PtrSize(p->vDivs)-Abc_ObjFaninNum(pNode) )
            return nullptr;

        pCands[nCands] = toLitCond( Vec_IntEntry(p->vProjVarsSat, iVar), 1 );
        RetValue = Ckt_NtkMfsTryResubOnce( p, pCands, nCands+1 );
        // time out
        if ( RetValue == -1 )
            return nullptr;
        // Can be resubstituted by (nFaninNum - 1) fanins and one divisor
        if ( RetValue == 1 )
        {
            if ( p->pPars->fVeryVerbose )
                printf( "Node %s: Fanin %d(%s) can be replaced by divisor %d(%s).\n",
                        Abc_ObjName(pNode), iFanin, Abc_ObjName(Abc_ObjFanin(pNode, iFanin)),
                        iVar, Abc_ObjName((Abc_Obj_t *)Vec_PtrEntry(p->vDivs, iVar)));
            p->nNodesResub++;
            p->nNodesGainedLevel++;
clk = Abc_Clock();
            // derive the function
            pFunc = Abc_NtkMfsInterplate( p, pCands, nCands+1 );
            if ( pFunc == nullptr )
                return nullptr;
            // update the network
            Vec_PtrPush( p->vMfsFanins, Vec_PtrEntry(p->vDivs, iVar) );
            // Ckt_NtkMfsUpdateNetwork( p, pNode, p->vMfsFanins, pFunc );
p->timeInt += Abc_Clock() - clk;
            p->nResubs++;
            return pFunc;
        }
        if ( p->nCexes >= p->pPars->nWinMax )
            break;
    }
    if ( p->pPars->fVeryVerbose )
        printf( "Node %d: Cannot find replacement for fanin %d.\n", pNode->Id, iFanin );
    return nullptr;
}


int Ckt_NtkMfsTryResubOnce(Mfs_Man_t * p, int * pCands, int nCands)
{
    int fVeryVerbose = 0;
    unsigned * pData;
    int RetValue, iVar, i;

    p->nSatCalls++;
    RetValue = sat_solver_solve( p->pSat, pCands, pCands + nCands, (ABC_INT64_T)p->pPars->nBTLimit, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );

    // UNSAT
    if ( RetValue == l_False )
    {
        if ( fVeryVerbose )
            printf( "U " );
        return 1;
    }
    // time out
    if ( RetValue != l_True )
    {
        if ( fVeryVerbose )
            printf( "T " );
        p->nTimeOuts++;
        DASSERT(0);
        return -1;
    }
    // SAT
    if ( fVeryVerbose )
        printf( "S " );
    p->nSatCexes++;
    // store the counter-example
    Vec_IntForEachEntry( p->vProjVarsSat, iVar, i )
    {
        pData = (unsigned *)Vec_PtrEntry( p->vDivCexes, i );
        if ( !sat_solver_var_value( p->pSat, iVar ) ) // remove 0s!!!
        {
            assert( Abc_InfoHasBit(pData, p->nCexes) );
            Abc_InfoXorBit( pData, p->nCexes );
        }
    }
    p->nCexes++;
    return 0;
}


void Ckt_NtkMfsUpdateNetwork(Mfs_Man_t * p, Abc_Obj_t * pObj, Vec_Ptr_t * vMfsFanins, Hop_Obj_t * pFunc)
{
    Abc_Obj_t * pObjNew, * pFanin;
    int k;
    // create the new node
    pObjNew = Abc_NtkCreateNode( pObj->pNtk );
    pObjNew->pData = pFunc;
    Vec_PtrForEachEntry( Abc_Obj_t *, vMfsFanins, pFanin, k )
        Abc_ObjAddFanin( pObjNew, pFanin );
    // replace the old node by the new node
    // update the level of the node
    Abc_NtkUpdate( pObj, pObjNew, p->vLevels );
}
