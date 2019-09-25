#include "appMfs.h"


using namespace std;
using namespace abc;


static unsigned seed;


int App_CommandMfs(Abc_Ntk_t * pNtk, shared_ptr <Ckt_Ntk_t> pNtkRef, int & frameNumber, float & error, int & nLocalPI, float errorThreshold)
{
    DASSERT(frameNumber > 0);
    Mfs_Par_t Pars, * pPars = &Pars;
    // set defaults
    App_NtkMfsParsDefault(pPars);
    Extra_UtilGetoptReset();
    if (pNtk == NULL)
    {
        Abc_Print(-1, "Empty network.\n");
        return 1;
    }
    if (!Abc_NtkIsLogic(pNtk))
    {
        Abc_Print(-1, "This command can only be applied to a logic network.\n");
        return 1;
    }
    // modify the current network
    if (!App_NtkMfs(pNtk, pPars, pNtkRef, frameNumber, error, nLocalPI, errorThreshold))
    {
        Abc_Print(-1, "Resynthesis has failed.\n");
        return 1;
    }
    return 0;
}


void App_NtkMfsParsDefault(Mfs_Par_t * pPars)
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
    pPars->fVerbose     =    0;
    pPars->fVeryVerbose =    0;
}


int App_NtkMfs(Abc_Ntk_t * pNtk, Mfs_Par_t * pPars, shared_ptr <Ckt_Ntk_t> pNtkRef, int & frameNumber, float & error, int & nLocalPI, float errorThreshold)
{
    static int patienceN;
    static int patienceM;

    Bdc_Par_t Pars = {0}, * pDecPars = &Pars;
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
    {
        DASSERT(0);
    }
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

    if ( pNtk->pExcare )
        DASSERT(0);
    if ( p->pCare != NULL )
        DASSERT(0);
    // prepare the BDC manager
    if ( !pPars->fResub )
    {
        pDecPars->nVarsMax = (nFaninMax < 3) ? 3 : nFaninMax;
        pDecPars->fVerbose = pPars->fVerbose;
        p->vTruth = Vec_IntAlloc( 0 );
        p->pManDec = Bdc_ManAlloc( pDecPars );
    }

    if ( p->pCare )
        DASSERT(0);

    // compute levels
    Abc_NtkLevel( pNtk );
    Abc_NtkStartReverseLevels( pNtk, pPars->nGrowthLevel );

    // compute don't-cares for each node
    p->nTotalNodesBeg = nTotalNodesBeg;
    p->nTotalEdgesBeg = nTotalEdgesBeg;
    DASSERT(pPars->fResub);
    int nObjNum = Abc_NtkObjNum(pNtk);
    int bestId = -1;
    float bestError = 1.0;
    cout << "seed = " << seed << " patienceN = " << patienceN << " patienceM = " << patienceM
        << " frameNumber = " << frameNumber << " nLocalPI = " << nLocalPI << endl;
    boost::progress_display pd(nObjNum);
    seed = time(unsigned(NULL));
    for (i = 0; i < nObjNum; ++i) {
        ++pd;
        Abc_Ntk_t * pNtkTest = Abc_NtkDup(pNtk);
        DASSERT(Abc_NtkObjNum(pNtk) == Abc_NtkObjNum(pNtkTest));
        p->pNtk = pNtkTest;
        pObj = Abc_NtkObj(pNtkTest, i);
        if (pObj != nullptr && Abc_ObjIsNode(pObj)) {
            Abc_NtkLevel(pNtkTest);
            Abc_NtkStartReverseLevels(pNtkTest, pPars->nGrowthLevel);
            if (!(Abc_ObjFaninNum(pObj) < 2 || Abc_ObjFaninNum(pObj) > nFaninMax)) {
                int isUpdated = 0;
                App_NtkMfsResub(p, pObj, isUpdated, frameNumber, nLocalPI);
                if (isUpdated) {
                    shared_ptr <Ckt_Ntk_t> pCktNtk = make_shared <Ckt_Ntk_t> (pNtkTest);
                    pCktNtk->Init(102400);
                    float tmpError = pCktNtk->MeasureError(pNtkRef, 100);
                    // cout << tmpError << "\t" << bestError << endl;
                    if (tmpError < bestError) {
                        // cout << "updated" << endl;
                        bestError = tmpError;
                        bestId = i;
                    }
                }
            }
        }
        Abc_NtkDelete(pNtkTest);
    }
    if (bestId == -1) {
        ++patienceN;
        if (patienceN > 5) {
            patienceM = patienceN = 0;
            frameNumber /= 2;
        }
        cout << "change patienceN" << endl;
    }
    else {
        if (bestError > errorThreshold) {
            ++patienceM;
            cout << "change patienceM" << endl;
            if (patienceM > 5 ) {
                cout << "best node " << Abc_ObjName(Abc_NtkObj(pNtk, bestId)) << " best error " << bestError << endl;
                error = bestError;
                p->pNtk = pNtk;
                int isUpdated = 0;
                App_NtkMfsResub(p, Abc_NtkObj(pNtk, bestId), isUpdated, frameNumber, nLocalPI);
                DASSERT(isUpdated);
            }
        }
        else {
            patienceN = patienceM = 0;
            cout << "best node " << Abc_ObjName(Abc_NtkObj(pNtk, bestId)) << " best error " << bestError << endl;
            error = bestError;
            p->pNtk = pNtk;
            int isUpdated = 0;
            App_NtkMfsResub(p, Abc_NtkObj(pNtk, bestId), isUpdated, frameNumber, nLocalPI);
            DASSERT(isUpdated);
        }
    }

    Abc_NtkStopReverseLevels( pNtk );
    Abc_NtkSweep( pNtk, 0 );

    // perform the sweeping
    if ( !pPars->fResub )
        DASSERT(0);

    p->nTotalNodesEnd = Abc_NtkNodeNum(pNtk);
    p->nTotalEdgesEnd = Abc_NtkGetTotalFanins(pNtk);

    // undo labesl
    if ( p->pCare )
        DASSERT(0);

    // free the manager
    p->timeTotal = Abc_Clock() - clk;
    Mfs_ManStop( p );
    return 1;
}


int App_NtkMfsResub(Mfs_Man_t * p, Abc_Obj_t * pNode, int & isUpdated, int & frameNumber, int & nLocalPI)
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
        // cout << p->nMaxDivs << endl;
        isUpdated = 0;
        return 1;
    }
    // compute the divisors of the window
clk = Abc_Clock();
    p->vDivs  = Abc_MfsComputeDivisors( p, pNode, Abc_ObjRequiredLevel(pNode) - 1 );
    p->nTotalDivs += Vec_PtrSize(p->vDivs) - Abc_ObjFaninNum(pNode);
p->timeDiv += Abc_Clock() - clk;

    // construct AIG for the window
clk = Abc_Clock();
    p->pAigWin = App_NtkConstructAig(p, pNode, frameNumber, nLocalPI);
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
        isUpdated = 0;
        return 1;
    }
    // solve the SAT problem
    if ( p->pPars->fPower )
        DASSERT(0);
    else if ( p->pPars->fSwapEdge )
        DASSERT(0);
    else
    {
        isUpdated = App_NtkMfsResubNode(p, pNode);
        if ( p->pPars->fMoreEffort )
            DASSERT(0);
    }
p->timeSat += Abc_Clock() - clk;

    return 1;
}


int App_NtkMfsResubNode(Mfs_Man_t * p, Abc_Obj_t * pNode)
{
    Abc_Obj_t * pFanin;
    int i;
    // try replacing area critical fanins
    Abc_ObjForEachFanin( pNode, pFanin, i )
        if ( !Abc_ObjIsCi(pFanin) && Abc_ObjFanoutNum(pFanin) == 1 )
        {
            if ( App_NtkMfsSolveSatResub( p, pNode, i, 0, 0 ) ) {
                return 1;
            }
        }
    // try removing redundant edges
    if ( !p->pPars->fArea ) {
        Abc_ObjForEachFanin( pNode, pFanin, i )
            if ( Abc_ObjIsCi(pFanin) || Abc_ObjFanoutNum(pFanin) != 1 )
            {
                if ( App_NtkMfsSolveSatResub( p, pNode, i, 1, 0 ) )
                    return 1;
            }
    }
    if ( Abc_ObjFaninNum(pNode) == p->nFaninMax )
        return 0;
    return 0;
}


int App_NtkMfsSolveSatResub(Mfs_Man_t * p, Abc_Obj_t * pNode, int iFanin, int fOnlyRemove, int fSkipUpdate)
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
//        printf( "\n" );
        printf( "%5d : Lev =%3d. Leaf =%3d. Node =%3d. Divs =%3d.  Fanin = %4d (%d/%d), MFFC = %d\n",
            pNode->Id, pNode->Level, Vec_PtrSize(p->vSupp), Vec_PtrSize(p->vNodes), Vec_PtrSize(p->vDivs)-Abc_ObjFaninNum(pNode),
            Abc_ObjFaninId(pNode, iFanin), iFanin, Abc_ObjFaninNum(pNode),
            Abc_ObjFanoutNum(Abc_ObjFanin(pNode, iFanin)) == 1 ? Abc_NodeMffcLabel(Abc_ObjFanin(pNode, iFanin)) : 0 );
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
    RetValue = Abc_NtkMfsTryResubOnce( p, pCands, nCands );
    if ( RetValue == -1 )
        return 0;
    if ( RetValue == 1 )
    {
        if ( p->pPars->fVeryVerbose )
            printf( "Node %d: Fanin %d can be removed.\n", pNode->Id, iFanin );
        p->nNodesResub++;
        p->nNodesGainedLevel++;
        if ( fSkipUpdate )
            return 1;
clk = Abc_Clock();
        // derive the function
        pFunc = Abc_NtkMfsInterplate( p, pCands, nCands );
        if ( pFunc == NULL )
            return 0;
        // update the network
        Abc_NtkMfsUpdateNetwork( p, pNode, p->vMfsFanins, pFunc );
p->timeInt += Abc_Clock() - clk;
        p->nRemoves++;
        return 1;
    }

    if ( fOnlyRemove || p->pPars->fRrOnly )
        return 0;

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
            if ( p->pPars->fPower )
                DASSERT(0);
            pData  = (unsigned *)Vec_PtrEntry( p->vDivCexes, iVar );
            for ( w = 0; w < nWords; w++ )
                if ( pData[w] != static_cast <unsigned> (~0) )
                    break;
            if ( w == nWords )
                break;
        }
        if ( iVar == Vec_PtrSize(p->vDivs)-Abc_ObjFaninNum(pNode) )
            return 0;

        pCands[nCands] = toLitCond( Vec_IntEntry(p->vProjVarsSat, iVar), 1 );
        RetValue = Abc_NtkMfsTryResubOnce( p, pCands, nCands+1 );
        if ( RetValue == -1 )
            return 0;
        if ( RetValue == 1 )
        {
            if ( p->pPars->fVeryVerbose )
                printf( "Node %d: Fanin %d can be replaced by divisor %d.\n", pNode->Id, iFanin, iVar );
            p->nNodesResub++;
            p->nNodesGainedLevel++;
            if ( fSkipUpdate )
                return 1;
clk = Abc_Clock();
            // derive the function
            pFunc = Abc_NtkMfsInterplate( p, pCands, nCands+1 );
            if ( pFunc == NULL )
                return 0;
            // update the network
            Vec_PtrPush( p->vMfsFanins, Vec_PtrEntry(p->vDivs, iVar) );
            Abc_NtkMfsUpdateNetwork( p, pNode, p->vMfsFanins, pFunc );
p->timeInt += Abc_Clock() - clk;
            p->nResubs++;
            return 1;
        }
        if ( p->nCexes >= p->pPars->nWinMax )
            break;
    }
    if ( p->pPars->fVeryVerbose )
        printf( "Node %d: Cannot find replacement for fanin %d.\n", pNode->Id, iFanin );
    return 0;
}


void Abc_NtkMfsUpdateNetwork(Mfs_Man_t * p, Abc_Obj_t * pObj, Vec_Ptr_t * vMfsFanins, Hop_Obj_t * pFunc)
{
    Abc_Obj_t * pObjNew, * pFanin;
    int k;
    // create the new node
    pObjNew = Abc_NtkCreateNode( pObj->pNtk );
    pObjNew->pData = pFunc;
    Vec_PtrForEachEntry( Abc_Obj_t *, vMfsFanins, pFanin, k )
        Abc_ObjAddFanin( pObjNew, pFanin );
    // replace the old node by the new node
//printf( "Replacing node " ); Abc_ObjPrint( stdout, pObj );
//printf( "Inserting node " ); Abc_ObjPrint( stdout, pObjNew );
    // update the level of the node
    Abc_NtkUpdate( pObj, pObjNew, p->vLevels );
}


int Abc_NtkMfsTryResubOnce(Mfs_Man_t * p, int * pCands, int nCands)
{
    int fVeryVerbose = 0;
    unsigned * pData;
    int RetValue, RetValue2 = -1, iVar, i;//, clk = Abc_Clock();

    // cout << "Abc_NtkMfsTryResubOnce, pCands ";
    // for (i = 0; i < nCands; ++i)
    //     printf( "%s%d ", pCands[i]&1 ? "!":"", pCands[i]>>1 );
    // cout << endl;
/*
    if ( p->pPars->fGiaSat )
    {
        RetValue2 = Abc_NtkMfsTryResubOnceGia( p, pCands, nCands );
p->timeGia += Abc_Clock() - clk;
        return RetValue2;
    }
*/
    p->nSatCalls++;
    RetValue = sat_solver_solve( p->pSat, pCands, pCands + nCands, (ABC_INT64_T)p->pPars->nBTLimit, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );
//    assert( RetValue == l_False || RetValue == l_True );

    if ( RetValue != l_Undef && RetValue2 != -1 )
    {
        assert( (RetValue == l_False) == (RetValue2 == 1) );
    }

    if ( RetValue == l_False )
    {
        if ( fVeryVerbose )
        printf( "U\n" );
        return 1;
    }
    if ( RetValue != l_True )
    {
        if ( fVeryVerbose )
        printf( "T\n" );
        p->nTimeOuts++;
        return -1;
    }
    if ( fVeryVerbose )
    printf( "S\n" );
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


Aig_Man_t * Abc_NtkToDar(Abc_Ntk_t * pNtk, int fExors, int fRegisters)
{
    Vec_Ptr_t * vNodes;
    Aig_Man_t * pMan;
    Aig_Obj_t * pObjNew;
    Abc_Obj_t * pObj;
    int i, nNodes, nDontCares;
    // make sure the latches follow PIs/POs
    if ( fRegisters )
    {
        assert( Abc_NtkBoxNum(pNtk) == Abc_NtkLatchNum(pNtk) );
        Abc_NtkForEachCi( pNtk, pObj, i )
            if ( i < Abc_NtkPiNum(pNtk) )
            {
                assert( Abc_ObjIsPi(pObj) );
                if ( !Abc_ObjIsPi(pObj) )
                    Abc_Print( 1, "Abc_NtkToDar(): Temporary bug: The PI ordering is wrong!\n" );
            }
            else
                assert( Abc_ObjIsBo(pObj) );
        Abc_NtkForEachCo( pNtk, pObj, i )
            if ( i < Abc_NtkPoNum(pNtk) )
            {
                assert( Abc_ObjIsPo(pObj) );
                if ( !Abc_ObjIsPo(pObj) )
                    Abc_Print( 1, "Abc_NtkToDar(): Temporary bug: The PO ordering is wrong!\n" );
            }
            else
                assert( Abc_ObjIsBi(pObj) );
        // print warning about initial values
        nDontCares = 0;
        Abc_NtkForEachLatch( pNtk, pObj, i )
            if ( Abc_LatchIsInitDc(pObj) )
            {
                Abc_LatchSetInit0(pObj);
                nDontCares++;
            }
        if ( nDontCares )
        {
            Abc_Print( 1, "Warning: %d registers in this network have don't-care init values.\n", nDontCares );
            Abc_Print( 1, "The don't-care are assumed to be 0. The result may not verify.\n" );
            Abc_Print( 1, "Use command \"print_latch\" to see the init values of registers.\n" );
            Abc_Print( 1, "Use command \"zero\" to convert or \"init\" to change the values.\n" );
        }
    }
    // create the manager
    pMan = Aig_ManStart( Abc_NtkNodeNum(pNtk) + 100 );
    pMan->fCatchExor = fExors;
    pMan->nConstrs = pNtk->nConstrs;
    pMan->nBarBufs = pNtk->nBarBufs;
    pMan->pName = Extra_UtilStrsav( pNtk->pName );
    pMan->pSpec = Extra_UtilStrsav( pNtk->pSpec );
    // transfer the pointers to the basic nodes
    Abc_AigConst1(pNtk)->pCopy = (Abc_Obj_t *)Aig_ManConst1(pMan);
    Abc_NtkForEachCi( pNtk, pObj, i )
    {
        pObj->pCopy = (Abc_Obj_t *)Aig_ObjCreateCi(pMan);
        // initialize logic level of the CIs
        ((Aig_Obj_t *)pObj->pCopy)->Level = pObj->Level;
    }

    // complement the 1-values registers
    if ( fRegisters ) {
        Abc_NtkForEachLatch( pNtk, pObj, i )
            if ( Abc_LatchIsInit1(pObj) )
                Abc_ObjFanout0(pObj)->pCopy = Abc_ObjNot(Abc_ObjFanout0(pObj)->pCopy);
    }
    // perform the conversion of the internal nodes (assumes DFS ordering)
//    pMan->fAddStrash = 1;
    vNodes = Abc_NtkDfs( pNtk, 0 );
    Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pObj, i )
//    Abc_NtkForEachNode( pNtk, pObj, i )
    {
        pObj->pCopy = (Abc_Obj_t *)Aig_And( pMan, (Aig_Obj_t *)Abc_ObjChild0Copy(pObj), (Aig_Obj_t *)Abc_ObjChild1Copy(pObj) );
//        Abc_Print( 1, "%d->%d ", pObj->Id, ((Aig_Obj_t *)pObj->pCopy)->Id );
    }
    Vec_PtrFree( vNodes );
    pMan->fAddStrash = 0;
    // create the POs
    Abc_NtkForEachCo( pNtk, pObj, i )
        Aig_ObjCreateCo( pMan, (Aig_Obj_t *)Abc_ObjChild0Copy(pObj) );
    // complement the 1-valued registers
    Aig_ManSetRegNum( pMan, Abc_NtkLatchNum(pNtk) );
    if ( fRegisters )
        Aig_ManForEachLiSeq( pMan, pObjNew, i )
            if ( Abc_LatchIsInit1(Abc_ObjFanout0(Abc_NtkCo(pNtk,i))) )
                pObjNew->pFanin0 = Aig_Not(pObjNew->pFanin0);
    // remove dangling nodes
    nNodes = (Abc_NtkGetChoiceNum(pNtk) == 0)? Aig_ManCleanup( pMan ) : 0;
    if ( !fExors && nNodes )
        Abc_Print( 1, "Abc_NtkToDar(): Unexpected %d dangling nodes when converting to AIG!\n", nNodes );
//Aig_ManDumpVerilog( pMan, "test.v" );
    // save the number of registers
    if ( fRegisters )
    {
        Aig_ManSetRegNum( pMan, Abc_NtkLatchNum(pNtk) );
        pMan->vFlopNums = Vec_IntStartNatural( pMan->nRegs );
//        pMan->vFlopNums = NULL;
//        pMan->vOnehots = Abc_NtkConverLatchNamesIntoNumbers( pNtk );
        if ( pNtk->vOnehots )
            pMan->vOnehots = (Vec_Ptr_t *)Vec_VecDupInt( (Vec_Vec_t *)pNtk->vOnehots );
    }
    if ( !Aig_ManCheck( pMan ) )
    {
        Abc_Print( 1, "Abc_NtkToDar: AIG check has failed.\n" );
        Aig_ManStop( pMan );
        return NULL;
    }
    return pMan;
}


Vec_Ptr_t * App_FindLocalInput(Abc_Obj_t * pNode, int nMax)
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
        // check the number of unvisited fanins
        // int nUnvisited = 0;
        // Abc_ObjForEachFanin(pFrontNode, pObj, i) {
        //     if (!Abc_NodeIsTravIdCurrent(pObj)) {
        //         ++nUnvisited;
        //     }
        // }
        // // expand the front node
        // if (fringe.size() + nUnvisited <= nMax ) {
            Abc_ObjForEachFanin(pFrontNode, pObj, i) {
                if (!Abc_NodeIsTravIdCurrent(pObj)) {
                    Abc_NodeSetTravIdCurrent(pObj);
                    fringe.emplace_back(pObj);
                }
            }
        // }
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


Aig_Man_t * App_NtkConstructAig(Mfs_Man_t * p, Abc_Obj_t * pNode, int & frameNumber, int & nLocalPI)
{
    // perform logic simulation
    shared_ptr <Ckt_Ntk_t> pCktNtk = make_shared <Ckt_Ntk_t> (pNode->pNtk);
    if (frameNumber >= 64)
        pCktNtk->Init(frameNumber);
    else
        pCktNtk->Init(64);
    pCktNtk->LogicSim(false, seed);
    // find local pi
    Vec_Ptr_t * vLocalPI = App_FindLocalInput(pNode, nLocalPI);

    Aig_Man_t * pMan;
    Abc_Obj_t * pFanin, * pObj;
    Aig_Obj_t * pObjAig, * pDC, * pRoot;
    int i, k;
    // start the new manager
    pMan = Aig_ManStart( 1000 );
    // construct the root node's AIG cone
    pObjAig = Abc_NtkConstructAig_rec( p, pNode, pMan );
//    assert( Aig_ManConst1(pMan) == pObjAig );
    Aig_ObjCreateCo( pMan, pObjAig );

    if ( p->pCare )
        DASSERT(0);

    // add approximate care set
    set <string> patterns;
    int nCluster = min(frameNumber, 64);
    for (int i = 0; i < pCktNtk->GetSimNum(); ++i) {
        for (int j = 0; j < nCluster; ++j) {
            string pattern = "";
            Vec_PtrForEachEntry(Abc_Obj_t *, vLocalPI, pObj, k) {
                shared_ptr <Ckt_Obj_t> pCktObj = pCktNtk->GetCktObj(string(Abc_ObjName(pObj)));
                DEBUG_ASSERT(pCktObj->GetName() == string(Abc_ObjName(pObj)), module_a{}, "object does not match");
                pattern +=  pCktObj->GetSimVal(i, j)? '1': '0';
            }
            DASSERT(static_cast <int>(pattern.length()) == Vec_PtrSize(vLocalPI));
            patterns.insert(pattern);
        }
    }
    pRoot = Aig_ManConst0(pMan);
    for (auto pattern: patterns) {
        pDC = Aig_ManConst1(pMan);
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
        DASSERT(0);
    }

    Aig_ManCleanup( pMan );
    return pMan;
}


Aig_Obj_t * Abc_NtkConstructAig_rec(Mfs_Man_t * p, Abc_Obj_t * pNode, Aig_Man_t * pMan)
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
