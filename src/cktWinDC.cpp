#include "cktWinDC.h"


using namespace std;
using namespace abc;


int Ckt_WinMfsTest( Abc_Ntk_t * pNtk, int nWinTfiLevs)
{
    // set parameters
    Mfs_Par_t mfsPars, * pPars = &mfsPars;
    Ckt_WinSetMfsPars(pPars);
    DEBUG_ASSERT(pPars->fResub == 1, module_a{}, "pPars->fResub = 0");
    DEBUG_ASSERT(pPars->fPower == 0, module_a{}, "pPars->fPower = 1");
    DEBUG_ASSERT(pNtk->pExcare == nullptr, module_a{}, "pNtk->pExcare not empty");

    Bdc_Par_t Pars = {0}, * pDecPars = &Pars;
    ProgressBar * pProgress;
    Mfs_Man_t * p;
    Abc_Obj_t * pObj;
    int i, nFaninMax;
    abctime clk = Abc_Clock();
    int nTotalNodesBeg = Abc_NtkNodeNum(pNtk);
    int nTotalEdgesBeg = Abc_NtkGetTotalFanins(pNtk);

    assert( Abc_NtkIsLogic(pNtk) );
    nFaninMax = Abc_NtkGetFaninMax(pNtk);
    if ( pPars->fResub )
    {
        if ( nFaninMax > 8 )
        {
            printf( "Nodes with more than %d fanins will not be processed.\n", 8 );
            nFaninMax = 8;
        }
    }
    else
        DEBUG_ASSERT(0, module_a{}, "pPars->fResub = 0");
    // convert into the AIG
    if ( !Abc_NtkToAig(pNtk) )
    {
        fprintf( stdout, "Converting to AIGs has failed.\n" );
        return 0;
    }
    assert( Abc_NtkHasAig(pNtk) );

    // start the manager
    p = Mfs_ManAlloc( pPars );
    p->pNtk = pNtk;
    p->nFaninMax = nFaninMax;

    // prepare the BDC manager
    if ( !pPars->fResub )
    {
        pDecPars->nVarsMax = (nFaninMax < 3) ? 3 : nFaninMax;
        pDecPars->fVerbose = pPars->fVerbose;
        p->vTruth = Vec_IntAlloc( 0 );
        p->pManDec = Bdc_ManAlloc( pDecPars );
    }
    // compute levels
    Abc_NtkLevel( pNtk );
    Abc_NtkStartReverseLevels( pNtk, pPars->nGrowthLevel );

    // compute don't-cares for each node
    p->nTotalNodesBeg = nTotalNodesBeg;
    p->nTotalEdgesBeg = nTotalEdgesBeg;
    if ( pPars->fResub )
    {
        pProgress = Extra_ProgressBarStart( stdout, Abc_NtkObjNumMax(pNtk) );
        Abc_NtkForEachNode( pNtk, pObj, i )
        {
            if ( p->pPars->nDepthMax && (int)pObj->Level > p->pPars->nDepthMax )
                continue;
            if ( Abc_ObjFaninNum(pObj) < 2 || Abc_ObjFaninNum(pObj) > nFaninMax )
                continue;
            if ( !p->pPars->fVeryVerbose )
                Extra_ProgressBarUpdate( pProgress, i, NULL );
            if ( pPars->fResub )
                Ckt_WinMfsResub(p, pObj, nWinTfiLevs);
            else {
                DEBUG_ASSERT(0, module_a{}, "pPars->fResub = 0");
            }
        }
        Extra_ProgressBarStop( pProgress );
    } else
    {
        DEBUG_ASSERT(0, module_a{}, "pPars->fResub = 0");
    }
    Abc_NtkStopReverseLevels( pNtk );

    // perform the sweeping
    if ( !pPars->fResub )
    {
        extern void Abc_NtkBidecResyn( Abc_Ntk_t * pNtk, int fVerbose );
//        Abc_NtkSweep( pNtk, 0 );
//        Abc_NtkBidecResyn( pNtk, 0 );
    }

    p->nTotalNodesEnd = Abc_NtkNodeNum(pNtk);
    p->nTotalEdgesEnd = Abc_NtkGetTotalFanins(pNtk);

    // free the manager
    p->timeTotal = Abc_Clock() - clk;
    Mfs_ManStop( p );
    return 1;
}


int Ckt_WinMfsResub(Mfs_Man_t * p, Abc_Obj_t * pNode, int nWinTfiLevs)
{
    // perform logic simulation
    shared_ptr <Ckt_Ntk_t> pCktNtk = make_shared <Ckt_Ntk_t> (pNode->pNtk, false);
    pCktNtk->Init(64);
    // pCktNtk->PrintObjs();
    pCktNtk->LogicSim(false);

    abctime clk;
    p->nNodesTried++;
    // prepare data structure for this node
    Mfs_ManClean( p );
    // compute window roots, window support, and window nodes
clk = Abc_Clock();
    p->vRoots = Abc_MfsComputeRoots(pNode, p->pPars->nWinTfoLevs, p->pPars->nFanoutsMax);
    p->vSupp  = Abc_NtkNodeSupport(p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots));
    p->vNodes = Abc_NtkDfsNodes(p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots));
    Vec_Ptr_t * vWinPIs = Ckt_WinNtkNodeSupport(p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots), Abc_ObjLevel(pNode) - nWinTfiLevs);

p->timeWin += Abc_Clock() - clk;
    if ( p->pPars->nWinMax && Vec_PtrSize(p->vNodes) > p->pPars->nWinMax )
    {
        p->nMaxDivs++;
        return 1;
    }
    // compute the divisors of the window
clk = Abc_Clock();
    p->vDivs  = Abc_MfsComputeDivisors( p, pNode, Abc_ObjRequiredLevel(pNode) - 1 );
    p->nTotalDivs += Vec_PtrSize(p->vDivs) - Abc_ObjFaninNum(pNode);

    {
        // Abc_Obj_t * pObj;
        // int i;
        cout << Abc_ObjName(pNode) << " : ";
        cout << "vWinPIs (" << Vec_PtrSize(vWinPIs) << ")\n";
        // cout << "vRoots (" << Vec_PtrSize(p->vRoots) << ")\t";
        // Vec_PtrForEachEntry(Abc_Obj_t *, p->vRoots, pObj, i)
        //     cout << Abc_ObjName(pObj) << "(" << Abc_ObjLevel(pObj) << ")" << "\t";
        // cout << endl;
        // cout << "vSupp (" << Vec_PtrSize(p->vSupp) << ")\t";
        // Vec_PtrForEachEntry(Abc_Obj_t *, p->vSupp, pObj, i)
        //     cout << Abc_ObjName(pObj) << "(" << Abc_ObjLevel(pObj) << ")" << "\t";
        // cout << endl;
        // cout << "vNodes (" << Vec_PtrSize(p->vNodes) << ")\t";
        // Vec_PtrForEachEntry(Abc_Obj_t *, p->vNodes, pObj, i)
        //     cout << Abc_ObjName(pObj) << "(" << Abc_ObjLevel(pObj) << ")" << "\t";
        // cout << endl;
        // cout << "vWinPIs (" << Vec_PtrSize(vWinPIs) << ")\t";
        // Vec_PtrForEachEntry(Abc_Obj_t *, vWinPIs, pObj, i)
        //     cout << Abc_ObjName(pObj) << "(" << Abc_ObjLevel(pObj) << ")" << "\t";
        // cout << endl;
        // cout << "vDivs (" << Vec_PtrSize(p->vDivs) << ")" << endl;
    }

p->timeDiv += Abc_Clock() - clk;
    // construct AIG for the window
clk = Abc_Clock();
    p->pAigWin = Ckt_WinConstructAppAig(p, pNode, vWinPIs, pCktNtk);
p->timeAig += Abc_Clock() - clk;

    // translate it into CNF
clk = Abc_Clock();
    p->pCnf = Cnf_DeriveSimple( p->pAigWin, 1 + Vec_PtrSize(p->vDivs) );
p->timeCnf += Abc_Clock() - clk;
    // create the SAT problem
clk = Abc_Clock();
    p->pSat = Abc_MfsCreateSolverResub( p, NULL, 0, 0 );
    if ( p->pSat == NULL )
    {
        p->nNodesBad++;
        return 1;
    }
    // solve the SAT problem
    if ( p->pPars->fPower )
        DEBUG_ASSERT(0, module_a{}, "pPars->fPower = 1");
    else if ( p->pPars->fSwapEdge )
        DEBUG_ASSERT(0, module_a{}, "pPars->fSwapEdge = 1");
    else
    {
        Abc_NtkMfsResubNode( p, pNode );
        if ( p->pPars->fMoreEffort )
            Abc_NtkMfsResubNode2( p, pNode );
    }
p->timeSat += Abc_Clock() - clk;

    Vec_PtrFree(vWinPIs);
    return 1;
}


void Ckt_WinSetMfsPars( Mfs_Par_t * pPars )
{
    memset( pPars, 0, sizeof(Mfs_Par_t) );
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
    pPars->fVerbose     =    1;
    pPars->fVeryVerbose =    0;
    // Abc_Print( -2, "usage: mfs [-WFDMLC <num>] [-draestpgvh]\n" );
    // Abc_Print( -2, "\t           performs don't-care-based optimization of logic networks\n" );
    // Abc_Print( -2, "\t-W <num> : the number of levels in the TFO cone (0 <= num) [default = %d]\n", pPars->nWinTfoLevs );
    // Abc_Print( -2, "\t-F <num> : the max number of fanouts to skip (1 <= num) [default = %d]\n", pPars->nFanoutsMax );
    // Abc_Print( -2, "\t-D <num> : the max depth nodes to try (0 = no limit) [default = %d]\n", pPars->nDepthMax );
    // Abc_Print( -2, "\t-M <num> : the max node count of windows to consider (0 = no limit) [default = %d]\n", pPars->nWinMax );
    // Abc_Print( -2, "\t-L <num> : the max increase in node level after resynthesis (0 <= num) [default = %d]\n", pPars->nGrowthLevel );
    // Abc_Print( -2, "\t-C <num> : the max number of conflicts in one SAT run (0 = no limit) [default = %d]\n", pPars->nBTLimit );
    // Abc_Print( -2, "\t-d       : toggle performing redundancy removal [default = %s]\n", pPars->fRrOnly? "yes": "no" );
    // Abc_Print( -2, "\t-r       : toggle resubstitution and dc-minimization [default = %s]\n", pPars->fResub? "resub": "dc-min" );
    // Abc_Print( -2, "\t-a       : toggle minimizing area or area+edges [default = %s]\n", pPars->fArea? "area": "area+edges" );
    // Abc_Print( -2, "\t-e       : toggle high-effort resubstitution [default = %s]\n", pPars->fMoreEffort? "yes": "no" );
    // Abc_Print( -2, "\t-s       : toggle evaluation of edge swapping [default = %s]\n", pPars->fSwapEdge? "yes": "no" );
    // Abc_Print( -2, "\t-t       : toggle using artificial one-hotness conditions [default = %s]\n", pPars->fOneHotness? "yes": "no" );
    // Abc_Print( -2, "\t-p       : toggle power-aware optimization [default = %s]\n", pPars->fPower? "yes": "no" );
    // Abc_Print( -2, "\t-g       : toggle using new SAT solver [default = %s]\n", pPars->fGiaSat? "yes": "no" );
    // Abc_Print( -2, "\t-v       : toggle printing optimization summary [default = %s]\n", pPars->fVerbose? "yes": "no" );
    // Abc_Print( -2, "\t-w       : toggle printing detailed stats for each node [default = %s]\n", pPars->fVeryVerbose? "yes": "no" );
    // Abc_Print( -2, "\t-h       : print the command usage\n");
}


Aig_Man_t * Ckt_WinConstructAppAig(Mfs_Man_t * p, Abc_Obj_t * pNode, Vec_Ptr_t * vWinPIs, shared_ptr <Ckt_Ntk_t> pCktNtk)
{
    Aig_Man_t * pMan;
    Abc_Obj_t * pFanin;
    Aig_Obj_t * pObjAig;
    int i;
    // start the new manager
    pMan = Aig_ManStart( 1000 );
    // construct the root node's AIG cone
    // pObjAig = Ckt_WinConstructAppAig_rec(p, pNode, pMan, vWinPIs, pCktNtk);
    pObjAig = Ckt_WinConstructAppAig2_rec(p, pNode, pMan, vWinPIs, pCktNtk);
    Aig_ObjCreateCo( pMan, pObjAig );
    if ( p->pCare )
    {
        DEBUG_ASSERT(0, module_a{}, "p->pCare not empty");
    }
    if ( p->pPars->fResub )
    {
        // construct the node
        pObjAig = (Aig_Obj_t *)pNode->pCopy;
        Aig_ObjCreateCo( pMan, pObjAig );

        // construct the divisors
        Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs, pFanin, i )
        {
            pObjAig = (Aig_Obj_t *)pFanin->pCopy;
            Aig_ObjCreateCo( pMan, pObjAig );
        }
    }
    else
    {
        DEBUG_ASSERT(0, module_a{}, "pPars->fResub = 0");
    }
    Aig_ManCleanup( pMan );
    return pMan;
}


Aig_Obj_t * Ckt_WinConstructAppAig_rec( Mfs_Man_t * p, Abc_Obj_t * pNode, Aig_Man_t * pMan, Vec_Ptr_t * vWinPIs, shared_ptr <Ckt_Ntk_t> pCktNtk)
{
    Aig_Obj_t * pRoot, * pExor, * pDC;
    Abc_Obj_t * pObj;
    int i, k;
    // assign AIG nodes to the leaves
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vSupp, pObj, i )
        pObj->pCopy = pObj->pNext = (Abc_Obj_t *)Aig_ObjCreateCi( pMan );
    // strash intermediate nodes
    Abc_NtkIncrementTravId( pNode->pNtk );
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vNodes, pObj, i )
    {
        Ckt_WinMfsConvertHopToAig( pObj, pMan );
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
    // add approximate SDCs
    // for (int i = 0; i < pCktNtk->GetSimNum(); ++i) {
    //     for (int j = 0; j < 64; ++j) {
    //         pDC = Aig_ManConst0(pMan);
    //         Vec_PtrForEachEntry(Abc_Obj_t *, vWinPIs, pObj, k) {
    //             shared_ptr <Ckt_Obj_t> pCktObj = pCktNtk->GetCktObj(pObj->Id);
    //             DEBUG_ASSERT(pCktObj->GetAbcObj() == pObj, module_a{}, "object does not match");
    //             if (pCktObj->GetSimVal(i, j))
    //                 pDC = Aig_Or(pMan, pDC, Aig_Not((Aig_Obj_t *)pObj->pCopy));
    //             else
    //                 pDC = Aig_Or(pMan, pDC, (Aig_Obj_t *)pObj->pCopy);
    //         }
    //         pRoot = Aig_And(pMan, pRoot, pDC);
    //     }
    // }
    return pRoot;
}


Aig_Obj_t * Ckt_WinConstructAppAig2_rec( Mfs_Man_t * p, Abc_Obj_t * pNode, Aig_Man_t * pMan, Vec_Ptr_t * vWinPIs, shared_ptr <Ckt_Ntk_t> pCktNtk)
{
    Aig_Obj_t * pRoot, * pCare;
    Abc_Obj_t * pObj;
    int i, k;

    // assign AIG nodes to the leaves
    Abc_NtkForEachPi(pNode->pNtk, pObj, i)
        pObj->pCopy = (Abc_Obj_t *)Aig_ObjCreateCi( pMan );

    // strash intermediate nodes
    Abc_NtkIncrementTravId( pNode->pNtk );
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vNodes, pObj, i )
        Ckt_WinMfsConvertHopToAig2( pObj, pMan );

    pRoot = Aig_ManConst0(pMan);
    for (int i = 0; i < pCktNtk->GetSimNum(); ++i) {
        for (int j = 0; j < 64; ++j) {
            pCare = Aig_ManConst1(pMan);
            Vec_PtrForEachEntry(Abc_Obj_t *, vWinPIs, pObj, k) {
                shared_ptr <Ckt_Obj_t> pCktObj = pCktNtk->GetCktObj(pObj->Id);
                DEBUG_ASSERT(pCktObj->GetAbcObj() == pObj, module_a{}, "object does not match");
                if (!pCktObj->GetSimVal(i, j))
                    pCare = Aig_And(pMan, pCare, Aig_Not((Aig_Obj_t *)pObj->pCopy));
                else
                    pCare = Aig_And(pMan, pCare, (Aig_Obj_t *)pObj->pCopy);
            }
            pRoot = Aig_Or(pMan, pRoot, pCare);
        }
    }
    return pRoot;
}


void Ckt_WinMfsConvertHopToAig( Abc_Obj_t * pObjOld, Aig_Man_t * pMan )
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
    Ckt_WinMfsConvertHopToAig_rec( Hop_Regular(pRoot), pMan );
    pObjOld->pCopy = (Abc_Obj_t *)Aig_NotCond( (Aig_Obj_t *)Hop_Regular(pRoot)->pData, Hop_IsComplement(pRoot) );
    Hop_ConeUnmark_rec( Hop_Regular(pRoot) );

    // assign the fanin nodes
    Abc_ObjForEachFanin( pObjOld, pFanin, i )
        Hop_ManPi(pHopMan, i)->pData = pFanin->pNext;
    // construct the AIG
    Ckt_WinMfsConvertHopToAig_rec( Hop_Regular(pRoot), pMan );
    pObjOld->pNext = (Abc_Obj_t *)Aig_NotCond( (Aig_Obj_t *)Hop_Regular(pRoot)->pData, Hop_IsComplement(pRoot) );
    Hop_ConeUnmark_rec( Hop_Regular(pRoot) );
}


void Ckt_WinMfsConvertHopToAig2( Abc_Obj_t * pObjOld, Aig_Man_t * pMan )
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
        return;
    }

    // assign the fanin nodes
    Abc_ObjForEachFanin( pObjOld, pFanin, i )
        Hop_ManPi(pHopMan, i)->pData = pFanin->pCopy;
    // construct the AIG
    Ckt_WinMfsConvertHopToAig_rec( Hop_Regular(pRoot), pMan );
    pObjOld->pCopy = (Abc_Obj_t *)Aig_NotCond( (Aig_Obj_t *)Hop_Regular(pRoot)->pData, Hop_IsComplement(pRoot) );
    Hop_ConeUnmark_rec( Hop_Regular(pRoot) );
}


void Ckt_WinMfsConvertHopToAig_rec( Hop_Obj_t * pObj, Aig_Man_t * pMan )
{
    assert( !Hop_IsComplement(pObj) );
    if ( !Hop_ObjIsNode(pObj) || Hop_ObjIsMarkA(pObj) )
        return;
    Ckt_WinMfsConvertHopToAig_rec( Hop_ObjFanin0(pObj), pMan );
    Ckt_WinMfsConvertHopToAig_rec( Hop_ObjFanin1(pObj), pMan );
    pObj->pData = Aig_And( pMan, (Aig_Obj_t *)Hop_ObjChild0Copy(pObj), (Aig_Obj_t *)Hop_ObjChild1Copy(pObj) );
    assert( !Hop_ObjIsMarkA(pObj) ); // loop detection
    Hop_ObjSetMarkA( pObj );
}


Vec_Ptr_t * Ckt_WinNtkNodeSupport(Abc_Ntk_t * pNtk, Abc_Obj_t ** ppNodes, int nNodes, int minLevel)
{
    Vec_Ptr_t * vNodes;
    int i;
    // set the traversal ID
    Abc_NtkIncrementTravId( pNtk );
    // start the array of nodes
    vNodes = Vec_PtrAlloc( 100 );
    // go through the PO nodes and call for each of them
    for ( i = 0; i < nNodes; i++ )
        if ( Abc_ObjIsCo(ppNodes[i]) )
            Ckt_WinNtkNodeSupport_rec( Abc_ObjFanin0(ppNodes[i]), vNodes, minLevel );
        else
            Ckt_WinNtkNodeSupport_rec( ppNodes[i], vNodes, minLevel );
    return vNodes;
}


void Ckt_WinNtkNodeSupport_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vNodes, int minLevel )
{
    Abc_Obj_t * pFanin;
    int i;
    assert( !Abc_ObjIsNet(pNode) );
    // if this node is already visited, skip
    if ( Abc_NodeIsTravIdCurrent( pNode ) )
        return;
    // mark the node as visited
    Abc_NodeSetTravIdCurrent( pNode );
    // collect the CI
    if ( Abc_ObjIsCi(pNode) || (Abc_NtkIsStrash(pNode->pNtk) && Abc_ObjFaninNum(pNode) == 0) || Abc_ObjLevel(pNode) <= minLevel )
    // if ( Abc_ObjIsCi(pNode) || (Abc_NtkIsStrash(pNode->pNtk) && Abc_ObjFaninNum(pNode) == 0) )
    {
        Vec_PtrPush( vNodes, pNode );
        return;
    }
    assert( Abc_ObjIsNode( pNode ) );
    // visit the transitive fanin of the node
    Abc_ObjForEachFanin( pNode, pFanin, i )
        Ckt_WinNtkNodeSupport_rec( Abc_ObjFanin0Ntk(pFanin), vNodes, minLevel );
}


Vec_Ptr_t * Ckt_WinNtkDfsNodes( Abc_Ntk_t * pNtk, Abc_Obj_t ** ppNodes, int nNodes, int minLevel )
{
    Vec_Ptr_t * vNodes;
    int i;
    // set the traversal ID
    Abc_NtkIncrementTravId( pNtk );
    // start the array of nodes
    vNodes = Vec_PtrAlloc( 100 );
    // go through the PO nodes and call for each of them
    for ( i = 0; i < nNodes; i++ )
    {
        if ( Abc_NtkIsStrash(pNtk) && Abc_AigNodeIsConst(ppNodes[i]) )
            continue;
        if ( Abc_ObjIsCo(ppNodes[i]) )
        {
            Abc_NodeSetTravIdCurrent(ppNodes[i]);
            Ckt_WinNtkDfs_rec( Abc_ObjFanin0Ntk(Abc_ObjFanin0(ppNodes[i])), vNodes, minLevel );
        }
        else if ( Abc_ObjIsNode(ppNodes[i]) || Abc_ObjIsCi(ppNodes[i]) )
            Ckt_WinNtkDfs_rec( ppNodes[i], vNodes, minLevel );
    }
    return vNodes;
}


void Ckt_WinNtkDfs_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vNodes, int minLevel )
{
    Abc_Obj_t * pFanin;
    int i;
    assert( !Abc_ObjIsNet(pNode) );
    // if this node is already visited, skip
    if ( Abc_NodeIsTravIdCurrent( pNode ) )
        return;
    // mark the node as visited
    Abc_NodeSetTravIdCurrent( pNode );
    // skip the CI
    // if ( Abc_ObjIsCi(pNode) || (Abc_NtkIsStrash(pNode->pNtk) && Abc_AigNodeIsConst(pNode)) || Abc_ObjLevel(pNode) <= minLevel )
    if ( Abc_ObjIsCi(pNode) || (Abc_NtkIsStrash(pNode->pNtk) && Abc_AigNodeIsConst(pNode)) )
        return;
    assert( Abc_ObjIsNode( pNode ) || Abc_ObjIsBox( pNode ) );
    // visit the transitive fanin of the node
    Abc_ObjForEachFanin( pNode, pFanin, i )
    {
//        pFanin = Abc_ObjFanin( pNode, Abc_ObjFaninNum(pNode)-1-i );
        Ckt_WinNtkDfs_rec( Abc_ObjFanin0Ntk(pFanin), vNodes, minLevel );
    }
    // add the node after the fanins have been added
    Vec_PtrPush( vNodes, pNode );
}
