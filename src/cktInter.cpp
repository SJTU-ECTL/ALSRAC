#include "cktInter.h"


using namespace std;
using namespace abc;


Ckt_DC_t::Ckt_DC_t(Abc_Ntk_t * p_abc_ntk)
    : pAbcNtk(p_abc_ntk)
{
    Abc_Obj_t * pAbcObj;
    int i;
    patLen = Abc_NtkPiNum(pAbcNtk);
    Abc_NtkForEachPi(pAbcNtk, pAbcObj, i)
        abc2DC[pAbcObj->Id] = i;
    DEBUG_ASSERT(i == patLen, module_a{}, "# pi != patLen");
}


Ckt_DC_t::~Ckt_DC_t (void)
{
}


void Ckt_DC_t::AddPattern(const std::vector <bool> & pattern)
{
    DEBUG_ASSERT(static_cast <int> (pattern.size()) == patLen, module_a{}, "pattern size != patLen");
    patterns.emplace_back(pattern);
}


void Ckt_DC_t::AddPatternR()
{
    static unsigned seed;
    seed += 10;
    boost::random::mt19937 gen(seed);
    boost::uniform_int <> dist(0, 1);
    boost::variate_generator <boost::mt19937 &, boost::uniform_int <> > coin(gen, dist);

    vector <bool> pattern;
    pattern.reserve(patLen);
    for (int i = 0; i < patLen; ++i)
        pattern.emplace_back(static_cast <bool> (coin()));
    patterns.emplace_back(pattern);
}


ostream & operator <<(ostream & os, const Ckt_DC_t & dc)
{
    Abc_Obj_t * pAbcObj;
    int i;
    Abc_NtkForEachPi(dc.pAbcNtk, pAbcObj, i)
        cout << Abc_ObjName(pAbcObj) << "\t";
    cout << endl;
    for (auto & pattern : dc.patterns)
        cout << pattern;
    return os;
}


ostream & operator <<(ostream & os, const vector <bool> & pattern)
{
    for (auto val : pattern)
        cout << val;
    cout << endl;
    return os;
}


int Ckt_MfsTest( Abc_Ntk_t * pNtk, Ckt_DC_t & dc )
{
    // set parameters
    Mfs_Par_t mfsPars, * pPars = &mfsPars;
    Ckt_SetMfsPars(pPars);
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
                Ckt_MfsResub( p, pObj, dc );
            else {
                DEBUG_ASSERT(0, module_a{}, "pPars->fResub = 0");
                // Abc_NtkMfsNode( p, pObj );
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


int Ckt_MfsResub( Mfs_Man_t * p, Abc_Obj_t * pNode, Ckt_DC_t & dc )
{
    abctime clk;
    p->nNodesTried++;
    // prepare data structure for this node
    Mfs_ManClean( p );
    // compute window roots, window support, and window nodes
clk = Abc_Clock();
    p->vRoots = Abc_MfsComputeRoots( pNode, p->pPars->nWinTfoLevs, p->pPars->nFanoutsMax );
    p->vSupp  = Abc_NtkNodeSupport( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );
    p->vNodes = Abc_NtkDfsNodes( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );
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
p->timeDiv += Abc_Clock() - clk;
    // construct AIG for the window
clk = Abc_Clock();
    // p->pAigWin = Abc_NtkConstructAig( p, pNode );
    p->pAigWin = Ckt_ConstructAppAig( p, pNode, dc );
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
    return 1;
}


void Ckt_SetMfsPars( Mfs_Par_t * pPars )
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
    Abc_Print( -2, "usage: mfs [-WFDMLC <num>] [-draestpgvh]\n" );
    Abc_Print( -2, "\t           performs don't-care-based optimization of logic networks\n" );
    Abc_Print( -2, "\t-W <num> : the number of levels in the TFO cone (0 <= num) [default = %d]\n", pPars->nWinTfoLevs );
    Abc_Print( -2, "\t-F <num> : the max number of fanouts to skip (1 <= num) [default = %d]\n", pPars->nFanoutsMax );
    Abc_Print( -2, "\t-D <num> : the max depth nodes to try (0 = no limit) [default = %d]\n", pPars->nDepthMax );
    Abc_Print( -2, "\t-M <num> : the max node count of windows to consider (0 = no limit) [default = %d]\n", pPars->nWinMax );
    Abc_Print( -2, "\t-L <num> : the max increase in node level after resynthesis (0 <= num) [default = %d]\n", pPars->nGrowthLevel );
    Abc_Print( -2, "\t-C <num> : the max number of conflicts in one SAT run (0 = no limit) [default = %d]\n", pPars->nBTLimit );
    Abc_Print( -2, "\t-d       : toggle performing redundancy removal [default = %s]\n", pPars->fRrOnly? "yes": "no" );
    Abc_Print( -2, "\t-r       : toggle resubstitution and dc-minimization [default = %s]\n", pPars->fResub? "resub": "dc-min" );
    Abc_Print( -2, "\t-a       : toggle minimizing area or area+edges [default = %s]\n", pPars->fArea? "area": "area+edges" );
    Abc_Print( -2, "\t-e       : toggle high-effort resubstitution [default = %s]\n", pPars->fMoreEffort? "yes": "no" );
    Abc_Print( -2, "\t-s       : toggle evaluation of edge swapping [default = %s]\n", pPars->fSwapEdge? "yes": "no" );
    Abc_Print( -2, "\t-t       : toggle using artificial one-hotness conditions [default = %s]\n", pPars->fOneHotness? "yes": "no" );
    Abc_Print( -2, "\t-p       : toggle power-aware optimization [default = %s]\n", pPars->fPower? "yes": "no" );
    Abc_Print( -2, "\t-g       : toggle using new SAT solver [default = %s]\n", pPars->fGiaSat? "yes": "no" );
    Abc_Print( -2, "\t-v       : toggle printing optimization summary [default = %s]\n", pPars->fVerbose? "yes": "no" );
    Abc_Print( -2, "\t-w       : toggle printing detailed stats for each node [default = %s]\n", pPars->fVeryVerbose? "yes": "no" );
    Abc_Print( -2, "\t-h       : print the command usage\n");
}


Aig_Man_t * Ckt_ConstructAppAig( Mfs_Man_t * p, Abc_Obj_t * pNode, Ckt_DC_t & dc )
{
    Aig_Man_t * pMan;
    Abc_Obj_t * pFanin;
    Aig_Obj_t * pObjAig;
    int i;
    // start the new manager
    pMan = Aig_ManStart( 1000 );
    // construct the root node's AIG cone
    pObjAig = Ckt_ConstructAppAig_rec( p, pNode, pMan, dc );
//    assert( Aig_ManConst1(pMan) == pObjAig );
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


Aig_Obj_t * Ckt_ConstructAppAig_rec( Mfs_Man_t * p, Abc_Obj_t * pNode, Aig_Man_t * pMan, Ckt_DC_t & dc )
{
    Aig_Obj_t * pRoot, * pExor, * pDC, * pCi;
    Abc_Obj_t * pObj;
    int i;
    // assign AIG nodes to the leaves
    vector <int> indexes;
    indexes.reserve(Vec_PtrSize(p->vSupp));
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vSupp, pObj, i ) {
        pObj->pCopy = pObj->pNext = (Abc_Obj_t *)Aig_ObjCreateCi( pMan );
        indexes.emplace_back(dc.abc2DC[pObj->Id]);
    }
    DEBUG_ASSERT(Aig_ManCiNum(pMan) == Vec_PtrSize(p->vSupp), module_a{}, "# aig ci != # p->vSupp");
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
    // add don't cares constraint
    for (auto & pattern : dc.patterns) {
        pDC = Aig_ManConst0(pMan);
        Aig_ManForEachCi(pMan, pCi, i) {
            if (pattern[indexes[i]]) {
                pDC = Aig_Or(pMan, pDC, Aig_Not(pCi));
            }
            else {
                pDC = Aig_Or(pMan, pDC, pCi);
            }
        }
        pRoot = Aig_And(pMan, pRoot, pDC);
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
