#include "appMfs.h"


using namespace abc;
using namespace std;

static const lit    LIT_UNDEF = 0xffffffff;

static inline unsigned * Int_ManTruthRead( Int_Man_t * p, Sto_Cls_t * pCls )          { return p->pInters + pCls->Id * p->nWords;                 }
static inline void       Int_ManTruthClear( unsigned * p, int nWords )                { int i; for ( i = nWords - 1; i >= 0; i-- ) p[i]  =  0;    }
static inline void       Int_ManTruthFill( unsigned * p, int nWords )                 { int i; for ( i = nWords - 1; i >= 0; i-- ) p[i]  = ~0;    }
static inline void       Int_ManTruthCopy( unsigned * p, unsigned * q, int nWords )   { int i; for ( i = nWords - 1; i >= 0; i-- ) p[i]  =  q[i]; }
static inline void       Int_ManTruthAnd( unsigned * p, unsigned * q, int nWords )    { int i; for ( i = nWords - 1; i >= 0; i-- ) p[i] &=  q[i]; }
static inline void       Int_ManTruthOr( unsigned * p, unsigned * q, int nWords )     { int i; for ( i = nWords - 1; i >= 0; i-- ) p[i] |=  q[i]; }
static inline void       Int_ManTruthOrNot( unsigned * p, unsigned * q, int nWords )  { int i; for ( i = nWords - 1; i >= 0; i-- ) p[i] |= ~q[i]; }
static inline int        Int_ManProofGet( Int_Man_t * p, Sto_Cls_t * pCls )           { return p->pProofNums[pCls->Id];  }
static inline void       Int_ManProofSet( Int_Man_t * p, Sto_Cls_t * pCls, int n )    { p->pProofNums[pCls->Id] = n;     }

static inline void Int_ManWatchClause( Int_Man_t * p, Sto_Cls_t * pClause, lit Lit )
{
    assert( lit_check(Lit, p->pCnf->nVars) );
    if ( pClause->pLits[0] == Lit )
        pClause->pNext0 = p->pWatches[lit_neg(Lit)];
    else
    {
        assert( pClause->pLits[1] == Lit );
        pClause->pNext1 = p->pWatches[lit_neg(Lit)];
    }
    p->pWatches[lit_neg(Lit)] = pClause;
}

static inline int Int_ManEnqueue( Int_Man_t * p, lit Lit, Sto_Cls_t * pReason )
{
    int Var = lit_var(Lit);
    if ( p->pAssigns[Var] != LIT_UNDEF )
        return p->pAssigns[Var] == Lit;
    p->pAssigns[Var] = Lit;
    p->pReasons[Var] = pReason;
    p->pTrail[p->nTrailSize++] = Lit;
//printf( "assigning var %d value %d\n", Var, !lit_sign(Lit) );
    return 1;
}

static inline void Int_ManCancelUntil( Int_Man_t * p, int Level )
{
    lit Lit;
    int i, Var;
    for ( i = p->nTrailSize - 1; i >= Level; i-- )
    {
        Lit = p->pTrail[i];
        Var = lit_var( Lit );
        p->pReasons[Var] = NULL;
        p->pAssigns[Var] = LIT_UNDEF;
//printf( "cancelling var %d\n", Var );
    }
    p->nTrailSize = Level;
}

static inline Hop_Obj_t * Bdc_FunCopyHop( Bdc_Fun_t * pObj )  { return Hop_NotCond( (Hop_Obj_t *)Bdc_FuncCopy(Bdc_Regular(pObj)), Bdc_IsComplement(pObj) );  }
static inline int Gia_ObjChild0Copy( Aig_Obj_t * pObj )  { return Abc_LitNotCond( Aig_ObjFanin0(pObj)->iData, Aig_ObjFaninC0(pObj) ); }
static inline int Gia_ObjChild1Copy( Aig_Obj_t * pObj )  { return Abc_LitNotCond( Aig_ObjFanin1(pObj)->iData, Aig_ObjFaninC1(pObj) ); }

static inline int     var_level     (sat_solver* s, int v)            { return s->levels[v];   }
static inline int     var_value     (sat_solver* s, int v)            { return s->assigns[v];  }
static inline int     var_polar     (sat_solver* s, int v)            { return s->polarity[v]; }

static inline void    var_set_level (sat_solver* s, int v, int lev)   { s->levels[v] = lev;    }
static inline void    var_set_value (sat_solver* s, int v, int val)   { s->assigns[v] = val;   }
static inline void    var_set_polar (sat_solver* s, int v, int pol)   { s->polarity[v] = pol;  }

static const int var0  = 1;
static const int var1  = 0;
static const int varX  = 3;

static inline int      sat_solver_dl(sat_solver* s)                { return veci_size(&s->trail_lim); }

static inline void order_assigned(sat_solver* s, int v)
{
}

static inline int sat_solver_enqueue(sat_solver* s, lit l, int from)
{
    int v  = lit_var(l);
    if ( s->pFreqs[v] == 0 )
//    {
        s->pFreqs[v] = 1;
//        s->nVarUsed++;
//    }

#ifdef VERBOSEDEBUG
    printf(L_IND"enqueue("L_LIT")\n", L_ind, L_lit(l));
#endif
    if (var_value(s, v) != varX)
        return var_value(s, v) == lit_sign(l);
    else{
/*
        if ( s->pCnfFunc )
        {
            if ( lit_sign(l) )
            {
                if ( (s->loads[v] & 1) == 0 )
                {
                    s->loads[v] ^= 1;
                    s->pCnfFunc( s->pCnfMan, l );
                }
            }
            else
            {
                if ( (s->loads[v] & 2) == 0 )
                {
                    s->loads[v] ^= 2;
                    s->pCnfFunc( s->pCnfMan, l );
                }
            }
        }
*/
        // New fact -- store it.
#ifdef VERBOSEDEBUG
        printf(L_IND"bind("L_LIT")\n", L_ind, L_lit(l));
#endif
        var_set_value(s, v, lit_sign(l));
        var_set_level(s, v, sat_solver_dl(s));
        s->reasons[v] = from;
        s->trail[s->qtail++] = l;
        order_assigned(s, v);
        return true;
    }
}


void App_DontCareSimpl(Abc_Ntk_t * pNtk)
{
}


int Abc_CommandMfs_Test( Abc_Ntk_t * pNtk )
{
    Mfs_Par_t Pars, * pPars = &Pars;
    // set defaults
    Abc_NtkMfsParsDefault_Test( pPars );
    Extra_UtilGetoptReset();

    if ( pNtk == NULL )
    {
        Abc_Print( -1, "Empty network.\n" );
        return 1;
    }
    if ( !Abc_NtkIsLogic(pNtk) )
    {
        Abc_Print( -1, "This command can only be applied to a logic network.\n" );
        return 1;
    }

    // modify the current network
    if ( !Abc_NtkMfs_Test( pNtk, pPars ) )
    {
        Abc_Print( -1, "Resynthesis has failed.\n" );
        return 1;
    }
    return 0;
}


void Abc_NtkMfsParsDefault_Test( Mfs_Par_t * pPars )
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
    pPars->fVeryVerbose =    1;
}


int Abc_NtkMfs_Test( Abc_Ntk_t * pNtk, Mfs_Par_t * pPars )
{
    extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );

    Bdc_Par_t Pars = {0}, * pDecPars = &Pars;
    ProgressBar * pProgress;
    Mfs_Man_t * p;
    Abc_Obj_t * pObj;
    Vec_Vec_t * vLevels;
    Vec_Ptr_t * vNodes;
    int i, k, nNodes, nFaninMax;
    abctime clk = Abc_Clock(), clk2;
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
        if ( nFaninMax > MFS_FANIN_MAX )
        {
            printf( "Nodes with more than %d fanins will not be processed.\n", MFS_FANIN_MAX );
            nFaninMax = MFS_FANIN_MAX;
        }
    }
    // perform the network sweep
//    Abc_NtkSweep( pNtk, 0 );
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

    // precomputer power-aware metrics
    if ( pPars->fPower )
    {
        extern Vec_Int_t * Abc_NtkPowerEstimate( Abc_Ntk_t * pNtk, int fProbOne );
        if ( pPars->fResub )
            p->vProbs = Abc_NtkPowerEstimate( pNtk, 0 );
        else
            p->vProbs = Abc_NtkPowerEstimate( pNtk, 1 );
#if 0
        printf( "Total switching before = %7.2f.\n", Abc_NtkMfsTotalSwitching(pNtk) );
#else
        p->TotalSwitchingBeg = Abc_NtkMfsTotalSwitching(pNtk);
#endif
    }

    if ( pNtk->pExcare )
    {
        Abc_Ntk_t * pTemp;
        if ( Abc_NtkPiNum((Abc_Ntk_t *)pNtk->pExcare) != Abc_NtkCiNum(pNtk) )
            printf( "The PI count of careset (%d) and logic network (%d) differ. Careset is not used.\n",
                Abc_NtkPiNum((Abc_Ntk_t *)pNtk->pExcare), Abc_NtkCiNum(pNtk) );
        else
        {
            pTemp = Abc_NtkStrash( (Abc_Ntk_t *)pNtk->pExcare, 0, 0, 0 );
            p->pCare = Abc_NtkToDar( pTemp, 0, 0 );
            Abc_NtkDelete( pTemp );
            p->vSuppsInv = Aig_ManSupportsInverse( p->pCare );
        }
    }
    if ( p->pCare != NULL )
        printf( "Performing optimization with %d external care clauses.\n", Aig_ManCoNum(p->pCare) );
    // prepare the BDC manager
    if ( !pPars->fResub )
    {
        pDecPars->nVarsMax = (nFaninMax < 3) ? 3 : nFaninMax;
        pDecPars->fVerbose = pPars->fVerbose;
        p->vTruth = Vec_IntAlloc( 0 );
        p->pManDec = Bdc_ManAlloc( pDecPars );
    }

    // label the register outputs
    if ( p->pCare )
    {
        Abc_NtkForEachCi( pNtk, pObj, i )
            pObj->pData = (void *)(ABC_PTRUINT_T)i;
    }

    // compute levels
    Abc_NtkLevel( pNtk );
    Abc_NtkStartReverseLevels( pNtk, pPars->nGrowthLevel );

    // compute don't-cares for each node
    nNodes = 0;
    p->nTotalNodesBeg = nTotalNodesBeg;
    p->nTotalEdgesBeg = nTotalEdgesBeg;
    if ( pPars->fResub )
    {
#if 0
        printf( "TotalSwitching (%7.2f --> ", Abc_NtkMfsTotalSwitching(pNtk) );
#endif
        if (pPars->fPower)
        {
            Abc_NtkMfsPowerResub( p, pPars);
        } else
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
                Abc_NtkMfsResub( p, pObj );
            else
                Abc_NtkMfsNode( p, pObj );
        }
        Extra_ProgressBarStop( pProgress );
#if 0
        printf( " %7.2f )\n", Abc_NtkMfsTotalSwitching(pNtk) );
#endif
    }
    } else
    {
#if 0
        printf( "Total switching before  = %7.2f,  ----> ", Abc_NtkMfsTotalSwitching(pNtk) );
#endif
        pProgress = Extra_ProgressBarStart( stdout, Abc_NtkNodeNum(pNtk) );
        vLevels = Abc_NtkLevelize( pNtk );
        Vec_VecForEachLevelStart( vLevels, vNodes, k, 1 )
        {
            if ( !p->pPars->fVeryVerbose )
                Extra_ProgressBarUpdate( pProgress, nNodes, NULL );
            p->nNodesGainedLevel = 0;
            p->nTotConfLevel = 0;
            p->nTimeOutsLevel = 0;
            clk2 = Abc_Clock();
            Vec_PtrForEachEntry( Abc_Obj_t *, vNodes, pObj, i )
            {
                if ( p->pPars->nDepthMax && (int)pObj->Level > p->pPars->nDepthMax )
                    break;
                if ( Abc_ObjFaninNum(pObj) < 2 || Abc_ObjFaninNum(pObj) > nFaninMax )
                    continue;
                if ( pPars->fResub )
                    Abc_NtkMfsResub( p, pObj );
                else
                    Abc_NtkMfsNode( p, pObj );
            }
            nNodes += Vec_PtrSize(vNodes);
            if ( pPars->fVerbose )
            {
                /*
            printf( "Lev = %2d. Node = %5d. Ave gain = %5.2f. Ave conf = %5.2f. T/o = %6.2f %%  ",
                k, Vec_PtrSize(vNodes),
                1.0*p->nNodesGainedLevel/Vec_PtrSize(vNodes),
                1.0*p->nTotConfLevel/Vec_PtrSize(vNodes),
                100.0*p->nTimeOutsLevel/Vec_PtrSize(vNodes) );
            ABC_PRT( "Time", Abc_Clock() - clk2 );
            */
            }
        }
        Extra_ProgressBarStop( pProgress );
        Vec_VecFree( vLevels );
#if 0
        printf( " %7.2f.\n", Abc_NtkMfsTotalSwitching(pNtk) );
#endif
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

    // undo labesl
    if ( p->pCare )
    {
        Abc_NtkForEachCi( pNtk, pObj, i )
            pObj->pData = NULL;
    }

    if ( pPars->fPower )
    {
#if 1
        p->TotalSwitchingEnd = Abc_NtkMfsTotalSwitching(pNtk);
//        printf( "Total switching after  = %7.2f.\n", Abc_NtkMfsTotalSwitching(pNtk) );
#else
        printf( "Total switching after  = %7.2f.\n", Abc_NtkMfsTotalSwitching(pNtk) );
#endif
    }

    // free the manager
    p->timeTotal = Abc_Clock() - clk;
    Mfs_ManStop( p );
    return 1;
}


void Abc_NtkMfsPowerResub( Mfs_Man_t * p, Mfs_Par_t * pPars)
{
    int i, k;
    Abc_Obj_t *pFanin, *pNode;
    Abc_Ntk_t *pNtk = p->pNtk;
    int nFaninMax = Abc_NtkGetFaninMax(p->pNtk);

    Abc_NtkForEachNode( pNtk, pNode, k )
    {
        if ( p->pPars->nDepthMax && (int)pNode->Level > p->pPars->nDepthMax )
            continue;
        if ( Abc_ObjFaninNum(pNode) < 2 || Abc_ObjFaninNum(pNode) > nFaninMax )
            continue;
        if (Abc_WinNode(p, pNode) )  // something wrong
            continue;

        // solve the SAT problem
        // Abc_NtkMfsEdgePower( p, pNode );
        // try replacing area critical fanins
        Abc_ObjForEachFanin( pNode, pFanin, i )
            if ( Abc_MfsObjProb(p, pFanin) >= 0.35 && Abc_NtkMfsSolveSatResub( p, pNode, i, 0, 0 ) )
                continue;
    }

    Abc_NtkForEachNode( pNtk, pNode, k )
    {
        if ( p->pPars->nDepthMax && (int)pNode->Level > p->pPars->nDepthMax )
            continue;
        if ( Abc_ObjFaninNum(pNode) < 2 || Abc_ObjFaninNum(pNode) > nFaninMax )
            continue;
        if (Abc_WinNode(p, pNode) )  // something wrong
            continue;

        // solve the SAT problem
        // Abc_NtkMfsEdgePower( p, pNode );
        // try replacing area critical fanins
        Abc_ObjForEachFanin( pNode, pFanin, i )
            if ( Abc_MfsObjProb(p, pFanin) >= 0.35 && Abc_NtkMfsSolveSatResub( p, pNode, i, 0, 0 ) )
                continue;
    }

    Abc_NtkForEachNode( pNtk, pNode, k )
    {
        if ( p->pPars->nDepthMax && (int)pNode->Level > p->pPars->nDepthMax )
            continue;
        if ( Abc_ObjFaninNum(pNode) < 2 || Abc_ObjFaninNum(pNode) > nFaninMax )
            continue;
        if (Abc_WinNode(p, pNode) ) // something wrong
            continue;

        Abc_ObjForEachFanin( pNode, pFanin, i )
            if ( Abc_MfsObjProb(p, pFanin) >= 0.2 && Abc_NtkMfsSolveSatResub( p, pNode, i, 1, 0 ) )
                continue;
    }
/*
    Abc_NtkForEachNode( pNtk, pNode, k )
    {
        if ( p->pPars->nDepthMax && (int)pNode->Level > p->pPars->nDepthMax )
            continue;
        if ( Abc_ObjFaninNum(pNode) < 2 || Abc_ObjFaninNum(pNode) > nFaninMax - 2)
            continue;
        if (Abc_WinNode(p, pNode) ) // something wrong
            continue;

        Abc_ObjForEachFanin( pNode, pFanin, i )
            if ( Abc_MfsObjProb(p, pFanin) >= 0.37 && Abc_NtkMfsSolveSatResub2( p, pNode, i, -1 ) )
                continue;
    }
*/
}


int Abc_NtkMfsResub( Mfs_Man_t * p, Abc_Obj_t * pNode )
{
    cout << "Abc_NtkMfsResub " << Abc_ObjId(pNode) << endl;
    abctime clk;
    p->nNodesTried++;
    // prepare data structure for this node
    Mfs_ManClean( p );
    // compute window roots, window support, and window nodes
clk = Abc_Clock();
    p->vRoots = Abc_MfsComputeRoots( pNode, p->pPars->nWinTfoLevs, p->pPars->nFanoutsMax );
    p->vSupp  = Abc_NtkNodeSupport( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );
    p->vNodes = Abc_NtkDfsNodes( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );

    Abc_Obj_t * pObj;
    int i;
    cout << "vRoots:";
    Vec_PtrForEachEntry(Abc_Obj_t *, p->vRoots, pObj, i)
        cout << Abc_ObjId(pObj) << ",";
    cout << endl;
    cout << "vSupp:";
    Vec_PtrForEachEntry(Abc_Obj_t *, p->vSupp, pObj, i)
        cout << Abc_ObjId(pObj) << ",";
    cout << endl;
    cout << "vNodes:";
    Vec_PtrForEachEntry(Abc_Obj_t *, p->vNodes, pObj, i)
        cout << Abc_ObjId(pObj) << ",";
    cout << endl;

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

    cout << "vDivs:";
    Vec_PtrForEachEntry(Abc_Obj_t *, p->vDivs, pObj, i)
        cout << Abc_ObjId(pObj) << ",";
    cout << endl;

    // construct AIG for the window
clk = Abc_Clock();
    p->pAigWin = Abc_NtkConstructAig_Test( p, pNode );
p->timeAig += Abc_Clock() - clk;

    // translate it into CNF
clk = Abc_Clock();
    p->pCnf = Cnf_DeriveSimple_Test( p->pAigWin, 1 + Vec_PtrSize(p->vDivs) );
p->timeCnf += Abc_Clock() - clk;
    // create the SAT problem
clk = Abc_Clock();
    p->pSat = Abc_MfsCreateSolverResub_Test( p, NULL, 0, 0 );
    if ( p->pSat == NULL )
    {
        p->nNodesBad++;
        return 1;
    }
//clk = Abc_Clock();
//    if ( p->pPars->fGiaSat )
//        Abc_NtkMfsConstructGia( p );
//p->timeGia += Abc_Clock() - clk;
    // solve the SAT problem
    if ( p->pPars->fPower )
        Abc_NtkMfsEdgePower( p, pNode );
    else if ( p->pPars->fSwapEdge )
        Abc_NtkMfsEdgeSwapEval( p, pNode );
    else
    {
        Abc_NtkMfsResubNode_Test( p, pNode );
        if ( p->pPars->fMoreEffort )
            Abc_NtkMfsResubNode2( p, pNode );
    }
p->timeSat += Abc_Clock() - clk;
//    if ( p->pPars->fGiaSat )
//        Abc_NtkMfsDeconstructGia( p );
    return 1;
}


int Abc_NtkMfsNode( Mfs_Man_t * p, Abc_Obj_t * pNode )
{
    Hop_Obj_t * pObj;
    int RetValue;
    float dProb;
    extern Hop_Obj_t * Abc_NodeIfNodeResyn( Bdc_Man_t * p, Hop_Man_t * pHop, Hop_Obj_t * pRoot, int nVars, Vec_Int_t * vTruth, unsigned * puCare, float dProb );

    int nGain;
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
    // count the number of patterns
//    p->dTotalRatios += Abc_NtkConstraintRatio( p, pNode );
    // construct AIG for the window
clk = Abc_Clock();
    p->pAigWin = Abc_NtkConstructAig( p, pNode );
p->timeAig += Abc_Clock() - clk;
    // translate it into CNF
clk = Abc_Clock();
    p->pCnf = Cnf_DeriveSimple( p->pAigWin, Abc_ObjFaninNum(pNode) );
p->timeCnf += Abc_Clock() - clk;
    // create the SAT problem
clk = Abc_Clock();
    p->pSat = (sat_solver *)Cnf_DataWriteIntoSolver( p->pCnf, 1, 0 );
    if ( p->pSat && p->pPars->fOneHotness )
        Abc_NtkAddOneHotness( p );
    if ( p->pSat == NULL )
        return 0;
    // solve the SAT problem
    RetValue = Abc_NtkMfsSolveSat( p, pNode );
    p->nTotConfLevel += p->pSat->stats.conflicts;
p->timeSat += Abc_Clock() - clk;
    if ( RetValue == 0 )
    {
        p->nTimeOutsLevel++;
        p->nTimeOuts++;
        return 0;
    }
    // minimize the local function of the node using bi-decomposition
    assert( p->nFanins == Abc_ObjFaninNum(pNode) );
    dProb = p->pPars->fPower? ((float *)p->vProbs->pArray)[pNode->Id] : -1.0;
    pObj = Abc_NodeIfNodeResyn( p->pManDec, (Hop_Man_t *)pNode->pNtk->pManFunc, (Hop_Obj_t *)pNode->pData, p->nFanins, p->vTruth, p->uCare, dProb );
    nGain = Hop_DagSize((Hop_Obj_t *)pNode->pData) - Hop_DagSize(pObj);
    if ( nGain >= 0 )
    {
        p->nNodesDec++;
        p->nNodesGained += nGain;
        p->nNodesGainedLevel += nGain;
        pNode->pData = pObj;
    }
    return 1;
}


int Abc_WinNode(Mfs_Man_t * p, Abc_Obj_t *pNode)
{
//    abctime clk;
//    Abc_Obj_t * pFanin;
//    int i;

    p->nNodesTried++;
    // prepare data structure for this node
    Mfs_ManClean( p );
    // compute window roots, window support, and window nodes
    p->vRoots = Abc_MfsComputeRoots( pNode, p->pPars->nWinTfoLevs, p->pPars->nFanoutsMax );
    p->vSupp  = Abc_NtkNodeSupport( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );
    p->vNodes = Abc_NtkDfsNodes( p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots) );
    if ( p->pPars->nWinMax && Vec_PtrSize(p->vNodes) > p->pPars->nWinMax )
        return 1;
    // compute the divisors of the window
    p->vDivs  = Abc_MfsComputeDivisors( p, pNode, Abc_ObjRequiredLevel(pNode) - 1 );
    p->nTotalDivs += Vec_PtrSize(p->vDivs) - Abc_ObjFaninNum(pNode);
    // construct AIG for the window
    p->pAigWin = Abc_NtkConstructAig( p, pNode );
    // translate it into CNF
    p->pCnf = Cnf_DeriveSimple( p->pAigWin, 1 + Vec_PtrSize(p->vDivs) );
    // create the SAT problem
    p->pSat = Abc_MfsCreateSolverResub_Test( p, NULL, 0, 0 );
    if ( p->pSat == NULL )
    {
        p->nNodesBad++;
        return 1;
    }
    return 0;
}


int Abc_NtkMfsSolveSatResub( Mfs_Man_t * p, Abc_Obj_t * pNode, int iFanin, int fOnlyRemove, int fSkipUpdate )
{
    cout << "Abc_NtkMfsSolveSatResub, pNode->Id " << pNode->Id << " iFanin " << iFanin << " fOnlyRemove " << fOnlyRemove << endl;

    int fVeryVerbose = 1;//p->pPars->fVeryVerbose && Vec_PtrSize(p->vDivs) < 200;// || pNode->Id == 556;
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
        pFunc = Abc_NtkMfsInterplate_Test( p, pCands, nCands );
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
            {
                Abc_Obj_t * pDiv = (Abc_Obj_t *)Vec_PtrEntry(p->vDivs, iVar);
                // only accept the divisor if it is "cool"
                if ( Abc_MfsObjProb(p, pDiv) >= 0.15 )
                    continue;
            }
            pData  = (unsigned *)Vec_PtrEntry( p->vDivCexes, iVar );
            for ( w = 0; w < nWords; w++ )
                if ( pData[w] != ~0 )
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
            pFunc = Abc_NtkMfsInterplate_Test( p, pCands, nCands+1 );
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


void Abc_NtkMfsUpdateNetwork( Mfs_Man_t * p, Abc_Obj_t * pObj, Vec_Ptr_t * vMfsFanins, Hop_Obj_t * pFunc )
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


int Abc_NtkMfsTryResubOnce( Mfs_Man_t * p, int * pCands, int nCands )
{
    int fVeryVerbose = 1;
    unsigned * pData;
    int RetValue, RetValue2 = -1, iVar, i;//, clk = Abc_Clock();

    cout << "Abc_NtkMfsTryResubOnce, pCands ";
    for (i = 0; i < nCands; ++i)
        printf( "%s%d ", pCands[i]&1 ? "!":"", pCands[i]>>1 );
    cout << endl;
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


Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters )
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


Vec_Int_t * Abc_NtkPowerEstimate( Abc_Ntk_t * pNtk, int fProbOne )
{
    extern Aig_Man_t * Abc_NtkToDar( Abc_Ntk_t * pNtk, int fExors, int fRegisters );
    extern Vec_Int_t * Saig_ManComputeSwitchProbs( Aig_Man_t * p, int nFrames, int nPref, int fProbOne );
    Vec_Int_t * vProbs;
    Vec_Int_t * vSwitching;
    float * pProbability;
    float * pSwitching;
    Abc_Ntk_t * pNtkStr;
    Aig_Man_t * pAig;
    Aig_Obj_t * pObjAig;
    Abc_Obj_t * pObjAbc, * pObjAbc2;
    int i;
    // start the resulting array
    vProbs = Vec_IntStart( Abc_NtkObjNumMax(pNtk) );
    pProbability = (float *)vProbs->pArray;
    // strash the network
    pNtkStr = Abc_NtkStrash( pNtk, 0, 1, 0 );
    Abc_NtkForEachObj( pNtk, pObjAbc, i )
        if ( Abc_ObjRegular((Abc_Obj_t *)pObjAbc->pTemp)->Type == ABC_FUNC_NONE )
            pObjAbc->pTemp = NULL;
    // map network into an AIG
    pAig = Abc_NtkToDar( pNtkStr, 0, (int)(Abc_NtkLatchNum(pNtk) > 0) );
    vSwitching = Saig_ManComputeSwitchProbs( pAig, 48, 16, fProbOne );
    pSwitching = (float *)vSwitching->pArray;
    Abc_NtkForEachObj( pNtk, pObjAbc, i )
    {
        if ( (pObjAbc2 = Abc_ObjRegular((Abc_Obj_t *)pObjAbc->pTemp)) && (pObjAig = Aig_Regular((Aig_Obj_t *)pObjAbc2->pTemp)) )
            pProbability[pObjAbc->Id] = pSwitching[pObjAig->Id];
    }
    Vec_IntFree( vSwitching );
    Aig_ManStop( pAig );
    Abc_NtkDelete( pNtkStr );
    return vProbs;
}


Hop_Obj_t * Abc_NodeIfNodeResyn( Bdc_Man_t * p, Hop_Man_t * pHop, Hop_Obj_t * pRoot, int nVars, Vec_Int_t * vTruth, unsigned * puCare, float dProb )
{
    unsigned * pTruth;
    Bdc_Fun_t * pFunc;
    int nNodes, i;
    assert( nVars <= 16 );
    // derive truth table
    pTruth = Hop_ManConvertAigToTruth( pHop, Hop_Regular(pRoot), nVars, vTruth, 0 );
    if ( Hop_IsComplement(pRoot) )
        Extra_TruthNot( pTruth, pTruth, nVars );
    // perform power-aware decomposition
    if ( dProb >= 0.0 )
    {
        float Prob = (float)2.0 * dProb * (1.0 - dProb);
        assert( Prob >= 0.0 && Prob <= 0.5 );
        if ( Prob >= 0.4 )
        {
            Extra_TruthNot( puCare, puCare, nVars );
            if ( dProb > 0.5 ) // more 1s than 0s
                Extra_TruthOr( pTruth, pTruth, puCare, nVars );
            else
                Extra_TruthSharp( pTruth, pTruth, puCare, nVars );
            Extra_TruthNot( puCare, puCare, nVars );
            // decompose truth table
            Bdc_ManDecompose( p, pTruth, NULL, nVars, NULL, 1000 );
        }
        else
        {
            // decompose truth table
            Bdc_ManDecompose( p, pTruth, puCare, nVars, NULL, 1000 );
        }
    }
    else
    {
        // decompose truth table
        Bdc_ManDecompose( p, pTruth, puCare, nVars, NULL, 1000 );
    }
    // convert back into HOP
    Bdc_FuncSetCopy( Bdc_ManFunc( p, 0 ), Hop_ManConst1( pHop ) );
    for ( i = 0; i < nVars; i++ )
        Bdc_FuncSetCopy( Bdc_ManFunc( p, i+1 ), Hop_ManPi( pHop, i ) );
    nNodes = Bdc_ManNodeNum(p);
    for ( i = nVars + 1; i < nNodes; i++ )
    {
        pFunc = Bdc_ManFunc( p, i );
        Bdc_FuncSetCopy( pFunc, Hop_And( pHop, Bdc_FunCopyHop(Bdc_FuncFanin0(pFunc)), Bdc_FunCopyHop(Bdc_FuncFanin1(pFunc)) ) );
    }
    return Bdc_FunCopyHop( Bdc_ManRoot(p) );
}


Vec_Int_t * Saig_ManComputeSwitchProbs( Aig_Man_t * pAig, int nFrames, int nPref, int fProbOne )
{
    Vec_Int_t * vSwitching, * vResult;
    Gia_Man_t * p;
    Aig_Obj_t * pObj;
    int i;
    // translate AIG into the intermediate form (takes care of choices if present!)
    p = Gia_ManFromAigSwitch( pAig );
    // perform the computation of switching activity
    vSwitching = Gia_ManComputeSwitchProbs( p, nFrames, nPref, fProbOne );
    // transfer the computed result to the original AIG
    vResult = Vec_IntStart( Aig_ManObjNumMax(pAig) );
    Aig_ManForEachObj( pAig, pObj, i )
    {
//        if ( Aig_ObjIsCo(pObj) )
//            printf( "%d=%f\n", i, Abc_Int2Float( Vec_IntEntry(vSwitching, Abc_Lit2Var(pObj->iData)) ) );
        Vec_IntWriteEntry( vResult, i, Vec_IntEntry(vSwitching, Abc_Lit2Var(pObj->iData)) );
    }
    // delete intermediate results
    Vec_IntFree( vSwitching );
    Gia_ManStop( p );
    return vResult;
}


Gia_Man_t * Gia_ManFromAigSwitch( Aig_Man_t * p )
{
    Gia_Man_t * pNew;
    Aig_Obj_t * pObj;
    int i;
    // create the new manager
    pNew = Gia_ManStart( Aig_ManObjNum(p) );
    pNew->pName = Abc_UtilStrsav( p->pName );
    pNew->pSpec = Abc_UtilStrsav( p->pSpec );
    pNew->nConstrs = p->nConstrs;
    // create the PIs
    Aig_ManCleanData( p );
    Aig_ManConst1(p)->iData = 1;
    Aig_ManForEachCi( p, pObj, i )
        pObj->iData = Gia_ManAppendCi( pNew );
    // add POs corresponding to the nodes with choices
    Aig_ManForEachNode( p, pObj, i )
        if ( Aig_ObjRefs(pObj) == 0 )
        {
            Gia_ManFromAig_rec( pNew, p, pObj );
            Gia_ManAppendCo( pNew, pObj->iData );
        }
    // add logic for the POs
    Aig_ManForEachCo( p, pObj, i )
        Gia_ManFromAig_rec( pNew, p, Aig_ObjFanin0(pObj) );
    Aig_ManForEachCo( p, pObj, i )
        pObj->iData = Gia_ManAppendCo( pNew, Gia_ObjChild0Copy(pObj) );
    Gia_ManSetRegNum( pNew, Aig_ManRegNum(p) );
    return pNew;
}


void Gia_ManFromAig_rec( Gia_Man_t * pNew, Aig_Man_t * p, Aig_Obj_t * pObj )
{
    Aig_Obj_t * pNext;
    if ( pObj->iData )
        return;
    assert( Aig_ObjIsNode(pObj) );
    Gia_ManFromAig_rec( pNew, p, Aig_ObjFanin0(pObj) );
    Gia_ManFromAig_rec( pNew, p, Aig_ObjFanin1(pObj) );
    pObj->iData = Gia_ManAppendAnd( pNew, Gia_ObjChild0Copy(pObj), Gia_ObjChild1Copy(pObj) );
    if ( p->pEquivs && (pNext = Aig_ObjEquiv(p, pObj)) )
    {
        int iObjNew, iNextNew;
        Gia_ManFromAig_rec( pNew, p, pNext );
        iObjNew  = Abc_Lit2Var(pObj->iData);
        iNextNew = Abc_Lit2Var(pNext->iData);
        if ( pNew->pNexts )
            pNew->pNexts[iObjNew] = iNextNew;
    }
}


Aig_Man_t * Abc_NtkConstructAig_Test( Mfs_Man_t * p, Abc_Obj_t * pNode )
{
    cout << "Abc_NtkConstructAig_Test " << Abc_ObjId(pNode) << endl;
    Aig_Man_t * pMan;
    Abc_Obj_t * pFanin;
    Aig_Obj_t * pObjAig, * pPi, * pPo;
    Vec_Int_t * vOuts;
    int i, k, iOut;
    // start the new manager
    pMan = Aig_ManStart( 1000 );
    // construct the root node's AIG cone
    pObjAig = Abc_NtkConstructAig_rec( p, pNode, pMan );
//    assert( Aig_ManConst1(pMan) == pObjAig );
    Aig_ObjCreateCo( pMan, pObjAig );

    if ( p->pCare )
    {
        // mark the care set
        Aig_ManIncrementTravId( p->pCare );
        Vec_PtrForEachEntry( Abc_Obj_t *, p->vSupp, pFanin, i )
        {
            pPi = Aig_ManCi( p->pCare, (int)(ABC_PTRUINT_T)pFanin->pData );
            Aig_ObjSetTravIdCurrent( p->pCare, pPi );
            pPi->pData = pFanin->pCopy;
        }
        // construct the constraints
        Vec_PtrForEachEntry( Abc_Obj_t *, p->vSupp, pFanin, i )
        {
            vOuts = (Vec_Int_t *)Vec_PtrEntry( p->vSuppsInv, (int)(ABC_PTRUINT_T)pFanin->pData );
            Vec_IntForEachEntry( vOuts, iOut, k )
            {
                pPo = Aig_ManCo( p->pCare, iOut );
                if ( Aig_ObjIsTravIdCurrent( p->pCare, pPo ) )
                    continue;
                Aig_ObjSetTravIdCurrent( p->pCare, pPo );
                if ( Aig_ObjFanin0(pPo) == Aig_ManConst1(p->pCare) )
                    continue;
                pObjAig = Abc_NtkConstructCare_rec( p->pCare, Aig_ObjFanin0(pPo), pMan );
                if ( pObjAig == NULL )
                    continue;
                pObjAig = Aig_NotCond( pObjAig, Aig_ObjFaninC0(pPo) );
                Aig_ObjCreateCo( pMan, pObjAig );
            }
        }
/*
        Aig_ManForEachCo( p->pCare, pPo, i )
        {
//            assert( Aig_ObjFanin0(pPo) != Aig_ManConst1(p->pCare) );
            if ( Aig_ObjFanin0(pPo) == Aig_ManConst1(p->pCare) )
                continue;
            pObjAig = Abc_NtkConstructCare_rec( p->pCare, Aig_ObjFanin0(pPo), pMan );
            if ( pObjAig == NULL )
                continue;
            pObjAig = Aig_NotCond( pObjAig, Aig_ObjFaninC0(pPo) );
            Aig_ObjCreateCo( pMan, pObjAig );
        }
*/
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
        // construct the fanins
        Abc_ObjForEachFanin( pNode, pFanin, i )
        {
            pObjAig = (Aig_Obj_t *)pFanin->pCopy;
            Aig_ObjCreateCo( pMan, pObjAig );
        }
    }

    Aig_ManCleanup( pMan );
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
    cout << "Abc_MfsConvertHopToAig " << Abc_ObjId(pObjOld) << endl;
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


Aig_Obj_t * Abc_NtkConstructCare_rec( Aig_Man_t * pCare, Aig_Obj_t * pObj, Aig_Man_t * pMan )
{
    Aig_Obj_t * pObj0, * pObj1;
    if ( Aig_ObjIsTravIdCurrent( pCare, pObj ) )
        return (Aig_Obj_t *)pObj->pData;
    Aig_ObjSetTravIdCurrent( pCare, pObj );
    if ( Aig_ObjIsCi(pObj) )
        return (Aig_Obj_t *)(pObj->pData = NULL);
    pObj0 = Abc_NtkConstructCare_rec( pCare, Aig_ObjFanin0(pObj), pMan );
    if ( pObj0 == NULL )
        return (Aig_Obj_t *)(pObj->pData = NULL);
    pObj1 = Abc_NtkConstructCare_rec( pCare, Aig_ObjFanin1(pObj), pMan );
    if ( pObj1 == NULL )
        return (Aig_Obj_t *)(pObj->pData = NULL);
    pObj0 = Aig_NotCond( pObj0, Aig_ObjFaninC0(pObj) );
    pObj1 = Aig_NotCond( pObj1, Aig_ObjFaninC1(pObj) );
    return (Aig_Obj_t *)(pObj->pData = Aig_And( pMan, pObj0, pObj1 ));
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


Cnf_Dat_t * Cnf_DeriveSimple_Test( Aig_Man_t * p, int nOutputs )
{
    cout << "Cnf_DeriveSimple_Test" << endl;
    Aig_Obj_t * pObj;
    Cnf_Dat_t * pCnf;
    int OutVar, PoVar, pVars[32], * pLits, ** pClas;
    int i, nLiterals, nClauses, Number;


    Aig_Obj_t * pAigObj;
    cout << "aig network" << endl;
    Aig_ManForEachObj(p, pAigObj, i) {
        cout << pAigObj->Id << "," <<
            pAigObj->Type << "," <<
            Aig_ObjFaninId0(pAigObj) << "(" <<
            (Aig_ObjFaninC0(pAigObj)?"neg":"pos") << ")," <<
            Aig_ObjFaninId1(pAigObj) << "(" <<
            (Aig_ObjFaninC1(pAigObj)?"neg":"pos") << ")" << endl;
    }
    cout << "Aig_ManNodeNum " << Aig_ManNodeNum(p) << endl;
    cout << "Aig_ManCoNum " << Aig_ManCoNum(p) << endl;
    cout << "nOutputs " << nOutputs << endl;


    // count the number of literals and clauses
    nLiterals = 1 + 7 * Aig_ManNodeNum(p) + Aig_ManCoNum( p ) + 3 * nOutputs;
    nClauses = 1 + 3 * Aig_ManNodeNum(p) + Aig_ManCoNum( p ) + nOutputs;

    // allocate CNF
    pCnf = ABC_ALLOC( Cnf_Dat_t, 1 );
    memset( pCnf, 0, sizeof(Cnf_Dat_t) );
    pCnf->pMan = p;
    pCnf->nLiterals = nLiterals;
    pCnf->nClauses = nClauses;
    pCnf->pClauses = ABC_ALLOC( int *, nClauses + 1 );
    pCnf->pClauses[0] = ABC_ALLOC( int, nLiterals );
    pCnf->pClauses[nClauses] = pCnf->pClauses[0] + nLiterals;

    // create room for variable numbers
    pCnf->pVarNums = ABC_ALLOC( int, Aig_ManObjNumMax(p) );
//    memset( pCnf->pVarNums, 0xff, sizeof(int) * Aig_ManObjNumMax(p) );
    for ( i = 0; i < Aig_ManObjNumMax(p); i++ )
        pCnf->pVarNums[i] = -1;
    // assign variables to the last (nOutputs) POs
    Number = 1;
    if ( nOutputs )
    {
//        assert( nOutputs == Aig_ManRegNum(p) );
//        Aig_ManForEachLiSeq( p, pObj, i )
//            pCnf->pVarNums[pObj->Id] = Number++;
        Aig_ManForEachCo( p, pObj, i )
            pCnf->pVarNums[pObj->Id] = Number++;
    }
    // assign variables to the internal nodes
    Aig_ManForEachNode( p, pObj, i )
        pCnf->pVarNums[pObj->Id] = Number++;
    // assign variables to the PIs and constant node
    Aig_ManForEachCi( p, pObj, i )
        pCnf->pVarNums[pObj->Id] = Number++;
    pCnf->pVarNums[Aig_ManConst1(p)->Id] = Number++;
    pCnf->nVars = Number;

    // print CNF numbers
    printf( "SAT numbers of each node:\n" );
    Aig_ManForEachObj( p, pObj, i )
        printf( "%d=%d ", pObj->Id, pCnf->pVarNums[pObj->Id] );
    printf( "\n" );

    // assign the clauses
    pLits = pCnf->pClauses[0];
    pClas = pCnf->pClauses;
    Aig_ManForEachNode( p, pObj, i )
    {
        OutVar   = pCnf->pVarNums[ pObj->Id ];
        pVars[0] = pCnf->pVarNums[ Aig_ObjFanin0(pObj)->Id ];
        pVars[1] = pCnf->pVarNums[ Aig_ObjFanin1(pObj)->Id ];

        // positive phase
        *pClas++ = pLits;
        *pLits++ = 2 * OutVar;
        *pLits++ = 2 * pVars[0] + !Aig_ObjFaninC0(pObj);
        *pLits++ = 2 * pVars[1] + !Aig_ObjFaninC1(pObj);

        // negative phase
        *pClas++ = pLits;
        *pLits++ = 2 * OutVar + 1;
        *pLits++ = 2 * pVars[0] + Aig_ObjFaninC0(pObj);
        *pClas++ = pLits;
        *pLits++ = 2 * OutVar + 1;
        *pLits++ = 2 * pVars[1] + Aig_ObjFaninC1(pObj);
    }

    // write the constant literal
    OutVar = pCnf->pVarNums[ Aig_ManConst1(p)->Id ];
    assert( OutVar <= Aig_ManObjNumMax(p) );
    *pClas++ = pLits;
    *pLits++ = 2 * OutVar;

    // write the output literals
    Aig_ManForEachCo( p, pObj, i )
    {
        OutVar = pCnf->pVarNums[ Aig_ObjFanin0(pObj)->Id ];
        if ( i < Aig_ManCoNum(p) - nOutputs )
        {
            *pClas++ = pLits;
            *pLits++ = 2 * OutVar + Aig_ObjFaninC0(pObj);
        }
        else
        {
            PoVar  = pCnf->pVarNums[ pObj->Id ];
            // first clause
            *pClas++ = pLits;
            *pLits++ = 2 * PoVar;
            *pLits++ = 2 * OutVar + !Aig_ObjFaninC0(pObj);
            // second clause
            *pClas++ = pLits;
            *pLits++ = 2 * PoVar + 1;
            *pLits++ = 2 * OutVar + Aig_ObjFaninC0(pObj);
        }
    }

    // verify that the correct number of literals and clauses was written
    assert( pLits - pCnf->pClauses[0] == nLiterals );
    assert( pClas - pCnf->pClauses == nClauses );
    return pCnf;
}


sat_solver * Abc_MfsCreateSolverResub_Test( Mfs_Man_t * p, int * pCands, int nCands, int fInvert )
{
    sat_solver * pSat;
    Aig_Obj_t * pObjPo;
    int Lits[2], status, iVar, i, c;

    // get the literal for the output of F
    pObjPo = Aig_ManCo( p->pAigWin, Aig_ManCoNum(p->pAigWin) - Vec_PtrSize(p->vDivs) - 1 );
    Lits[0] = toLitCond( p->pCnf->pVarNums[pObjPo->Id], fInvert );

    // collect the outputs of the divisors
    Vec_IntClear( p->vProjVarsCnf );
    Vec_PtrForEachEntryStart( Aig_Obj_t *, p->pAigWin->vCos, pObjPo, i, Aig_ManCoNum(p->pAigWin) - Vec_PtrSize(p->vDivs) )
    {
        assert( p->pCnf->pVarNums[pObjPo->Id] >= 0 );
        Vec_IntPush( p->vProjVarsCnf, p->pCnf->pVarNums[pObjPo->Id] );
    }
    assert( Vec_IntSize(p->vProjVarsCnf) == Vec_PtrSize(p->vDivs) );

    // start the solver
    pSat = sat_solver_new();
    pSat->fVerbose = 1;
    sat_solver_setnvars( pSat, 2 * p->pCnf->nVars + Vec_PtrSize(p->vDivs) );
    if ( pCands )
        sat_solver_store_alloc( pSat );

    // load the first copy of the clauses
    cout << "load the first copy of the clauses" << endl;
    for ( i = 0; i < p->pCnf->nClauses; i++ )
    {
        if ( !sat_solver_addclause_Test( pSat, p->pCnf->pClauses[i], p->pCnf->pClauses[i+1] ) )
        {
            sat_solver_delete( pSat );
            return NULL;
        }
    }
    // add the clause for the first output of F
    cout << "add the clause for the first output of F" << endl;
    if ( !sat_solver_addclause_Test( pSat, Lits, Lits+1 ) )
    {
        sat_solver_delete( pSat );
        return NULL;
    }

    // add one-hotness constraints
    if ( p->pPars->fOneHotness )
    {
        p->pSat = pSat;
        if ( !Abc_NtkAddOneHotness( p ) )
            return NULL;
        p->pSat = NULL;
    }

    // bookmark the clauses of A
    if ( pCands )
        sat_solver_store_mark_clauses_a( pSat );

    // transform the literals
    for ( i = 0; i < p->pCnf->nLiterals; i++ )
        p->pCnf->pClauses[0][i] += 2 * p->pCnf->nVars;
    // load the second copy of the clauses
    cout << "load the second copy of the clauses" << endl;
    for ( i = 0; i < p->pCnf->nClauses; i++ )
    {
        if ( !sat_solver_addclause_Test( pSat, p->pCnf->pClauses[i], p->pCnf->pClauses[i+1] ) )
        {
            sat_solver_delete( pSat );
            return NULL;
        }
    }
    // add one-hotness constraints
    if ( p->pPars->fOneHotness )
    {
        p->pSat = pSat;
        if ( !Abc_NtkAddOneHotness( p ) )
            return NULL;
        p->pSat = NULL;
    }
    // transform the literals
    for ( i = 0; i < p->pCnf->nLiterals; i++ )
        p->pCnf->pClauses[0][i] -= 2 * p->pCnf->nVars;
    // add the clause for the second output of F
    cout << "add the clause for the second output of F" << endl;
    Lits[0] = 2 * p->pCnf->nVars + lit_neg( Lits[0] );
    if ( !sat_solver_addclause_Test( pSat, Lits, Lits+1 ) )
    {
        sat_solver_delete( pSat );
        return NULL;
    }

    if ( pCands )
    {
        // add relevant clauses for EXOR gates
        for ( c = 0; c < nCands; c++ )
        {
            // get the variable number of this divisor
            i = lit_var( pCands[c] ) - 2 * p->pCnf->nVars;
            // get the corresponding SAT variable
            iVar = Vec_IntEntry( p->vProjVarsCnf, i );
            // add the corresponding EXOR gate
            if ( !Abc_MfsSatAddXor( pSat, iVar, iVar + p->pCnf->nVars, 2 * p->pCnf->nVars + i ) )
            {
                sat_solver_delete( pSat );
                return NULL;
            }
            // add the corresponding clause
            if ( !sat_solver_addclause_Test( pSat, pCands + c, pCands + c + 1 ) )
            {
                sat_solver_delete( pSat );
                return NULL;
            }
        }
        // bookmark the roots
        sat_solver_store_mark_roots( pSat );
    }
    else
    {
        // add the clauses for the EXOR gates - and remember their outputs
        cout << "add the clauses for the EXOR gates - and remember their outputs" << endl;
        Vec_IntClear( p->vProjVarsSat );
        Vec_IntForEachEntry( p->vProjVarsCnf, iVar, i )
        {
            if ( !Abc_MfsSatAddXor( pSat, iVar, iVar + p->pCnf->nVars, 2 * p->pCnf->nVars + i ) )
            {
                sat_solver_delete( pSat );
                return NULL;
            }
            Vec_IntPush( p->vProjVarsSat, 2 * p->pCnf->nVars + i );
        }
        assert( Vec_IntSize(p->vProjVarsCnf) == Vec_IntSize(p->vProjVarsSat) );
        cout << "vProjVarsSat: ";
        Vec_IntForEachEntry(p->vProjVarsSat, iVar, i)
            cout << iVar << ",";
        cout << endl;
        cout << "vProjVarsCnf: ";
        Vec_IntForEachEntry(p->vProjVarsCnf, iVar, i)
            cout << iVar << ",";
        cout << endl;
        // simplify the solver
        status = sat_solver_simplify(pSat);
        if ( status == 0 )
        {
//            printf( "Abc_MfsCreateSolverResub(): SAT solver construction has failed. Skipping node.\n" );
            sat_solver_delete( pSat );
            return NULL;
        }
    }
    return pSat;
}


int Abc_MfsSatAddXor( sat_solver * pSat, int iVarA, int iVarB, int iVarC )
{
    lit Lits[3];

    Lits[0] = toLitCond( iVarA, 1 );
    Lits[1] = toLitCond( iVarB, 1 );
    Lits[2] = toLitCond( iVarC, 1 );
    if ( !sat_solver_addclause_Test( pSat, Lits, Lits + 3 ) )
        return 0;

    Lits[0] = toLitCond( iVarA, 1 );
    Lits[1] = toLitCond( iVarB, 0 );
    Lits[2] = toLitCond( iVarC, 0 );
    if ( !sat_solver_addclause_Test( pSat, Lits, Lits + 3 ) )
        return 0;

    Lits[0] = toLitCond( iVarA, 0 );
    Lits[1] = toLitCond( iVarB, 1 );
    Lits[2] = toLitCond( iVarC, 0 );
    if ( !sat_solver_addclause_Test( pSat, Lits, Lits + 3 ) )
        return 0;

    Lits[0] = toLitCond( iVarA, 0 );
    Lits[1] = toLitCond( iVarB, 0 );
    Lits[2] = toLitCond( iVarC, 1 );
    if ( !sat_solver_addclause_Test( pSat, Lits, Lits + 3 ) )
        return 0;

    return 1;
}


int sat_solver_addclause_Test(sat_solver* s, lit* begin, lit* end)
{
    lit *i,*j;
    int maxvar;
    lit last;
    assert( begin < end );
    s->fPrintClause = 1;
    if ( s->fPrintClause )
    {
        for ( i = begin; i < end; i++ )
            printf( "%s%d ", (*i)&1 ? "!":"", (*i)>>1 );
        printf( "\n" );
    }

    veci_resize( &s->temp_clause, 0 );
    for ( i = begin; i < end; i++ )
        veci_push( &s->temp_clause, *i );
    begin = veci_begin( &s->temp_clause );
    end = begin + veci_size( &s->temp_clause );

    // insertion sort
    maxvar = lit_var(*begin);
    for (i = begin + 1; i < end; i++){
        lit l = *i;
        maxvar = lit_var(l) > maxvar ? lit_var(l) : maxvar;
        for (j = i; j > begin && *(j-1) > l; j--)
            *j = *(j-1);
        *j = l;
    }
    sat_solver_setnvars(s,maxvar+1);

    ///////////////////////////////////
    // add clause to internal storage
    if ( s->pStore )
    {
        int RetValue = Sto_ManAddClause( (Sto_Man_t *)s->pStore, begin, end );
        assert( RetValue );
        (void) RetValue;
    }
    ///////////////////////////////////

    // delete duplicates
    last = lit_Undef;
    for (i = j = begin; i < end; i++){
        //printf("lit: "L_LIT", value = %d\n", L_lit(*i), (lit_sign(*i) ? -s->assignss[lit_var(*i)] : s->assignss[lit_var(*i)]));
        if (*i == lit_neg(last) || var_value(s, lit_var(*i)) == lit_sign(*i))
            return true;   // tautology
        else if (*i != last && var_value(s, lit_var(*i)) == varX)
            last = *j++ = *i;
    }
//    j = i;

    if (j == begin)          // empty clause
        return false;

    if (j - begin == 1) // unit clause
        return sat_solver_enqueue(s,*begin,0);

    // create new clause
    sat_solver_clause_new(s,begin,j,0);
    return true;
}


int Abc_NtkMfsResubNode_Test( Mfs_Man_t * p, Abc_Obj_t * pNode )
{
    cout << "Abc_NtkMfsResubNode_Test" << endl;
    Abc_Obj_t * pFanin;
    int i;
    // try replacing area critical fanins
    Abc_ObjForEachFanin( pNode, pFanin, i )
        if ( !Abc_ObjIsCi(pFanin) && Abc_ObjFanoutNum(pFanin) == 1 )
        {
            if ( Abc_NtkMfsSolveSatResub( p, pNode, i, 0, 0 ) )
                return 1;
        }
    // try removing redundant edges
    if ( !p->pPars->fArea )
    {
        Abc_ObjForEachFanin( pNode, pFanin, i )
            if ( Abc_ObjIsCi(pFanin) || Abc_ObjFanoutNum(pFanin) != 1 )
            {
                if ( Abc_NtkMfsSolveSatResub( p, pNode, i, 1, 0 ) )
                    return 1;
            }
    }
    if ( Abc_ObjFaninNum(pNode) == p->nFaninMax )
        return 0;
/*
    // try replacing area critical fanins while adding two new fanins
    Abc_ObjForEachFanin( pNode, pFanin, i )
        if ( !Abc_ObjIsCi(pFanin) && Abc_ObjFanoutNum(pFanin) == 1 )
        {
            if ( Abc_NtkMfsSolveSatResub2( p, pNode, i, -1 ) )
                return 1;
        }
*/
    return 0;
}


Hop_Obj_t * Abc_NtkMfsInterplate_Test( Mfs_Man_t * p, int * pCands, int nCands )
{
    extern Hop_Obj_t * Kit_GraphToHop( Hop_Man_t * pMan, Kit_Graph_t * pGraph );
    int fDumpFile = 1;
    char FileName[32];
    sat_solver * pSat;
    Sto_Man_t * pCnf = NULL;
    unsigned * puTruth;
    Kit_Graph_t * pGraph;
    Hop_Obj_t * pFunc;
    int nFanins, status;
    int c, i, * pGloVars;
//    abctime clk = Abc_Clock();
//    p->nDcMints += Abc_NtkMfsInterplateEval( p, pCands, nCands );

    // derive the SAT solver for interpolation
    cout << "derive the SAT solver for interpolation" << endl;
    pSat = Abc_MfsCreateSolverResub_Test( p, pCands, nCands, 0 );

    // dump CNF file (remember to uncomment two-lit clases in clause_create_new() in 'satSolver.c')
    if ( fDumpFile )
    {
        static int Counter = 0;
        sprintf( FileName, "cnf\\pj1_if6_mfs%03d.cnf", Counter++ );
        Sat_SolverWriteDimacs( pSat, FileName, NULL, NULL, 1 );
    }

    // solve the problem
    status = sat_solver_solve( pSat, NULL, NULL, (ABC_INT64_T)p->pPars->nBTLimit, (ABC_INT64_T)0, (ABC_INT64_T)0, (ABC_INT64_T)0 );
    if ( fDumpFile )
        printf( "File %s has UNSAT problem with %d conflicts.\n", FileName, (int)pSat->stats.conflicts );
    if ( status != l_False )
    {
        p->nTimeOuts++;
        return NULL;
    }
//printf( "%d\n", pSat->stats.conflicts );
//    ABC_PRT( "S", Abc_Clock() - clk );
    // get the learned clauses
    pCnf = (Sto_Man_t *)sat_solver_store_release( pSat );
    sat_solver_delete( pSat );

    // set the global variables
    pGloVars = Int_ManSetGlobalVars( p->pMan, nCands );
    for ( c = 0; c < nCands; c++ )
    {
        // get the variable number of this divisor
        i = lit_var( pCands[c] ) - 2 * p->pCnf->nVars;
        // get the corresponding SAT variable
        pGloVars[c] = Vec_IntEntry( p->vProjVarsCnf, i );
    }

    // derive the interpolant
    p->pMan->fProofWrite = 1;
    nFanins = Int_ManInterpolate_Test( p->pMan, pCnf, 0, &puTruth );
    Sto_ManFree( pCnf );
    assert( nFanins == nCands );

    // transform interpolant into AIG
    pGraph = Kit_TruthToGraph( puTruth, nFanins, p->vMem );
    pFunc = Kit_GraphToHop( (Hop_Man_t *)p->pNtk->pManFunc, pGraph );
    Kit_GraphFree( pGraph );
    return pFunc;
}


Hop_Obj_t * Kit_GraphToHop( Hop_Man_t * pMan, Kit_Graph_t * pGraph )
{
    Kit_Node_t * pNode = NULL;
    int i;
    // collect the fanins
    Kit_GraphForEachLeaf( pGraph, pNode, i )
        pNode->pFunc = Hop_IthVar( pMan, i );
    // perform strashing
    return Kit_GraphToHopInternal( pMan, pGraph );
}


Hop_Obj_t * Kit_GraphToHopInternal( Hop_Man_t * pMan, Kit_Graph_t * pGraph )
{
    Kit_Node_t * pNode = NULL;
    Hop_Obj_t * pAnd0, * pAnd1;
    int i;
    // check for constant function
    if ( Kit_GraphIsConst(pGraph) )
        return Hop_NotCond( Hop_ManConst1(pMan), Kit_GraphIsComplement(pGraph) );
    // check for a literal
    if ( Kit_GraphIsVar(pGraph) )
        return Hop_NotCond( (Hop_Obj_t *)Kit_GraphVar(pGraph)->pFunc, Kit_GraphIsComplement(pGraph) );
    // build the AIG nodes corresponding to the AND gates of the graph
    Kit_GraphForEachNode( pGraph, pNode, i )
    {
        pAnd0 = Hop_NotCond( (Hop_Obj_t *)Kit_GraphNode(pGraph, pNode->eEdge0.Node)->pFunc, pNode->eEdge0.fCompl );
        pAnd1 = Hop_NotCond( (Hop_Obj_t *)Kit_GraphNode(pGraph, pNode->eEdge1.Node)->pFunc, pNode->eEdge1.fCompl );
        pNode->pFunc = Hop_And( pMan, pAnd0, pAnd1 );
    }
    // complement the result if necessary
    return Hop_NotCond( (Hop_Obj_t *)pNode->pFunc, Kit_GraphIsComplement(pGraph) );
}


int Int_ManInterpolate_Test( Int_Man_t * p, Sto_Man_t * pCnf, int fVerbose, unsigned ** ppResult )
{
    Sto_Cls_t * pClause;
    int RetValue = 1;
    abctime clkTotal = Abc_Clock();

    // check that the CNF makes sense
    assert( pCnf->nVars > 0 && pCnf->nClauses > 0 );
    p->pCnf = pCnf;
    p->fVerbose = fVerbose;
    *ppResult = NULL;

    // adjust the manager
    Int_ManResize( p );

    // prepare the interpolant computation
    Int_ManPrepareInter( p );

    // construct proof for each clause
    // start the proof
    if ( p->fProofWrite )
    {
        p->pFile = fopen( "proof.cnf_", "w" );
        p->Counter = 0;
    }

    // write the root clauses
    Sto_ManForEachClauseRoot( p->pCnf, pClause )
        Int_ManProofWriteOne( p, pClause );

    // propagate root level assignments
    if ( Int_ManProcessRoots( p ) )
    {
        // if there is no conflict, consider learned clauses
        Sto_ManForEachClause( p->pCnf, pClause )
        {
            if ( pClause->fRoot )
                continue;
            if ( !Int_ManProofRecordOne( p, pClause ) )
            {
                RetValue = 0;
                break;
            }
        }
    }

    // stop the proof
    if ( p->fProofWrite )
    {
        fclose( p->pFile );
        p->pFile = NULL;
    }

    if ( fVerbose )
    {
    printf( "Vars = %d. Roots = %d. Learned = %d. Resol steps = %d.  Ave = %.2f.  Mem = %.2f MB\n",
        p->pCnf->nVars, p->pCnf->nRoots, p->pCnf->nClauses-p->pCnf->nRoots, p->Counter,
        1.0*(p->Counter-p->pCnf->nRoots)/(p->pCnf->nClauses-p->pCnf->nRoots),
        1.0*Sto_ManMemoryReport(p->pCnf)/(1<<20) );
p->timeTotal += Abc_Clock() - clkTotal;
    }

    *ppResult = Int_ManTruthRead( p, p->pCnf->pTail );
    return p->nVarsAB;
}


void Int_ManResize( Int_Man_t * p )
{
    // check if resizing is needed
    if ( p->nVarsAlloc < p->pCnf->nVars )
    {
        // find the new size
        if ( p->nVarsAlloc == 0 )
            p->nVarsAlloc = 1;
        while ( p->nVarsAlloc < p->pCnf->nVars )
            p->nVarsAlloc *= 2;
        // resize the arrays
        p->pTrail    = ABC_REALLOC(lit,         p->pTrail,    p->nVarsAlloc );
        p->pAssigns  = ABC_REALLOC(lit,         p->pAssigns,  p->nVarsAlloc );
        p->pSeens    = ABC_REALLOC(char,        p->pSeens,    p->nVarsAlloc );
        p->pVarTypes = ABC_REALLOC(int,         p->pVarTypes, p->nVarsAlloc );
        p->pReasons  = ABC_REALLOC(Sto_Cls_t *, p->pReasons,  p->nVarsAlloc );
        p->pWatches  = ABC_REALLOC(Sto_Cls_t *, p->pWatches,  p->nVarsAlloc*2 );
    }

    // clean the free space
    memset( p->pAssigns , 0xff, sizeof(lit) * p->pCnf->nVars );
    memset( p->pSeens   , 0,    sizeof(char) * p->pCnf->nVars );
    memset( p->pVarTypes, 0,    sizeof(int) * p->pCnf->nVars );
    memset( p->pReasons , 0,    sizeof(Sto_Cls_t *) * p->pCnf->nVars );
    memset( p->pWatches , 0,    sizeof(Sto_Cls_t *) * p->pCnf->nVars*2 );

    // compute the number of common variables
    p->nVarsAB = Int_ManGlobalVars( p );
    // compute the number of words in the truth table
    p->nWords = (p->nVarsAB <= 5 ? 1 : (1 << (p->nVarsAB - 5)));

    // check if resizing of clauses is needed
    if ( p->nClosAlloc < p->pCnf->nClauses )
    {
        // find the new size
        if ( p->nClosAlloc == 0 )
            p->nClosAlloc = 1;
        while ( p->nClosAlloc < p->pCnf->nClauses )
            p->nClosAlloc *= 2;
        // resize the arrays
        p->pProofNums = ABC_REALLOC(int, p->pProofNums,  p->nClosAlloc );
    }
    memset( p->pProofNums, 0, sizeof(int) * p->pCnf->nClauses );

    // check if resizing of truth tables is needed
    if ( p->nIntersAlloc < p->nWords * p->pCnf->nClauses )
    {
        p->nIntersAlloc = p->nWords * p->pCnf->nClauses;
        p->pInters = ABC_REALLOC(unsigned, p->pInters, p->nIntersAlloc );
    }
//    memset( p->pInters, 0, sizeof(unsigned) * p->nWords * p->pCnf->nClauses );
}


void Int_ManPrepareInter( Int_Man_t * p )
{
    // elementary truth tables
    unsigned uTruths[8][8] = {
        { 0xAAAAAAAA,0xAAAAAAAA,0xAAAAAAAA,0xAAAAAAAA,0xAAAAAAAA,0xAAAAAAAA,0xAAAAAAAA,0xAAAAAAAA },
        { 0xCCCCCCCC,0xCCCCCCCC,0xCCCCCCCC,0xCCCCCCCC,0xCCCCCCCC,0xCCCCCCCC,0xCCCCCCCC,0xCCCCCCCC },
        { 0xF0F0F0F0,0xF0F0F0F0,0xF0F0F0F0,0xF0F0F0F0,0xF0F0F0F0,0xF0F0F0F0,0xF0F0F0F0,0xF0F0F0F0 },
        { 0xFF00FF00,0xFF00FF00,0xFF00FF00,0xFF00FF00,0xFF00FF00,0xFF00FF00,0xFF00FF00,0xFF00FF00 },
        { 0xFFFF0000,0xFFFF0000,0xFFFF0000,0xFFFF0000,0xFFFF0000,0xFFFF0000,0xFFFF0000,0xFFFF0000 },
        { 0x00000000,0xFFFFFFFF,0x00000000,0xFFFFFFFF,0x00000000,0xFFFFFFFF,0x00000000,0xFFFFFFFF },
        { 0x00000000,0x00000000,0xFFFFFFFF,0xFFFFFFFF,0x00000000,0x00000000,0xFFFFFFFF,0xFFFFFFFF },
        { 0x00000000,0x00000000,0x00000000,0x00000000,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF }
    };
    Sto_Cls_t * pClause;
    int Var, VarAB, v;
    assert( p->nVarsAB <= 8 );

    // set interpolants for root clauses
    Sto_ManForEachClauseRoot( p->pCnf, pClause )
    {
        if ( !pClause->fA ) // clause of B
        {
            Int_ManTruthFill( Int_ManTruthRead(p, pClause), p->nWords );
//            Int_ManPrintInterOne( p, pClause );
            continue;
        }
        // clause of A
        Int_ManTruthClear( Int_ManTruthRead(p, pClause), p->nWords );
        for ( v = 0; v < (int)pClause->nLits; v++ )
        {
            Var = lit_var(pClause->pLits[v]);
            if ( p->pVarTypes[Var] < 0 ) // global var
            {
                VarAB = -p->pVarTypes[Var]-1;
                assert( VarAB >= 0 && VarAB < p->nVarsAB );
                if ( lit_sign(pClause->pLits[v]) ) // negative var
                    Int_ManTruthOrNot( Int_ManTruthRead(p, pClause), uTruths[VarAB], p->nWords );
                else
                    Int_ManTruthOr( Int_ManTruthRead(p, pClause), uTruths[VarAB], p->nWords );
            }
        }
//        Int_ManPrintInterOne( p, pClause );
    }
}


void Int_ManProofWriteOne( Int_Man_t * p, Sto_Cls_t * pClause )
{
    Int_ManProofSet( p, pClause, ++p->Counter );

    if ( p->fProofWrite )
    {
        int v;
        fprintf( p->pFile, "%d", Int_ManProofGet(p, pClause) );
        for ( v = 0; v < (int)pClause->nLits; v++ )
            fprintf( p->pFile, " %d", lit_print(pClause->pLits[v]) );
        fprintf( p->pFile, " 0 0\n" );
    }
}


int Int_ManGlobalVars( Int_Man_t * p )
{
    Sto_Cls_t * pClause;
    int Var, nVarsAB, v;

    // mark the variable encountered in the clauses of A
    Sto_ManForEachClauseRoot( p->pCnf, pClause )
    {
        if ( !pClause->fA )
            break;
        for ( v = 0; v < (int)pClause->nLits; v++ )
            p->pVarTypes[lit_var(pClause->pLits[v])] = 1;
    }

    if ( p->nGloVars )
    {
        for ( v = 0; v < p->nGloVars; v++ )
            p->pVarTypes[ p->pGloVars[v] ] = - v - 1;
        return p->nGloVars;
    }

    // check variables that appear in clauses of B
    nVarsAB = 0;
    Sto_ManForEachClauseRoot( p->pCnf, pClause )
    {
        if ( pClause->fA )
            continue;
        for ( v = 0; v < (int)pClause->nLits; v++ )
        {
            Var = lit_var(pClause->pLits[v]);
            if ( p->pVarTypes[Var] == 1 ) // var of A
            {
                // change it into a global variable
                nVarsAB++;
                p->pVarTypes[Var] = -1;
            }
        }
    }

    // order global variables
    nVarsAB = 0;
    for ( v = 0; v < p->pCnf->nVars; v++ )
        if ( p->pVarTypes[v] == -1 )
            p->pVarTypes[v] -= nVarsAB++;
//printf( "There are %d global variables.\n", nVarsAB );
    return nVarsAB;
}


int Int_ManProcessRoots( Int_Man_t * p )
{
    Sto_Cls_t * pClause;
    int Counter;

    // make sure the root clauses are preceeding the learnt clauses
    Counter = 0;
    Sto_ManForEachClause( p->pCnf, pClause )
    {
        assert( (int)pClause->fA    == (Counter < (int)p->pCnf->nClausesA) );
        assert( (int)pClause->fRoot == (Counter < (int)p->pCnf->nRoots)    );
        Counter++;
    }
    assert( p->pCnf->nClauses == Counter );

    // make sure the last clause if empty
    assert( p->pCnf->pTail->nLits == 0 );

    // go through the root unit clauses
    p->nTrailSize = 0;
    Sto_ManForEachClauseRoot( p->pCnf, pClause )
    {
        // create watcher lists for the root clauses
        if ( pClause->nLits > 1 )
        {
            Int_ManWatchClause( p, pClause, pClause->pLits[0] );
            Int_ManWatchClause( p, pClause, pClause->pLits[1] );
        }
        // empty clause and large clauses
        if ( pClause->nLits != 1 )
            continue;
        // unit clause
        assert( lit_check(pClause->pLits[0], p->pCnf->nVars) );
        if ( !Int_ManEnqueue( p, pClause->pLits[0], pClause ) )
        {
            // detected root level conflict
//            printf( "Error in Int_ManProcessRoots(): Detected a root-level conflict too early!\n" );
//            assert( 0 );
            // detected root level conflict
            Int_ManProofTraceOne( p, pClause, p->pCnf->pEmpty );
            if ( p->fVerbose )
                printf( "Found root level conflict!\n" );
            return 0;
        }
    }

    // propagate the root unit clauses
    pClause = Int_ManPropagate( p, 0 );
    if ( pClause )
    {
        // detected root level conflict
        Int_ManProofTraceOne( p, pClause, p->pCnf->pEmpty );
        if ( p->fVerbose )
            printf( "Found root level conflict!\n" );
        return 0;
    }

    // set the root level
    p->nRootSize = p->nTrailSize;
    return 1;
}


int Int_ManProofRecordOne( Int_Man_t * p, Sto_Cls_t * pClause )
{
    Sto_Cls_t * pConflict;
    int i;

    // empty clause never ends up there
    assert( pClause->nLits > 0 );
    if ( pClause->nLits == 0 )
        printf( "Error: Empty clause is attempted.\n" );

    // add assumptions to the trail
    assert( !pClause->fRoot );
    assert( p->nTrailSize == p->nRootSize );

    // if any of the clause literals are already assumed
    // it means that the clause is redundant and can be skipped
    for ( i = 0; i < (int)pClause->nLits; i++ )
        if ( p->pAssigns[lit_var(pClause->pLits[i])] == pClause->pLits[i] )
            return 1;

    for ( i = 0; i < (int)pClause->nLits; i++ )
        if ( !Int_ManEnqueue( p, lit_neg(pClause->pLits[i]), NULL ) )
        {
            assert( 0 ); // impossible
            return 0;
        }

    // propagate the assumptions
    pConflict = Int_ManPropagate( p, p->nRootSize );
    if ( pConflict == NULL )
    {
        assert( 0 ); // cannot prove
        return 0;
    }

    // skip the clause if it is weaker or the same as the conflict clause
    if ( pClause->nLits >= pConflict->nLits )
    {
        // check if every literal of conflict clause can be found in the given clause
        int j;
        for ( i = 0; i < (int)pConflict->nLits; i++ )
        {
            for ( j = 0; j < (int)pClause->nLits; j++ )
                if ( pConflict->pLits[i] == pClause->pLits[j] )
                    break;
            if ( j == (int)pClause->nLits ) // literal pConflict->pLits[i] is not found
                break;
        }
        if ( i == (int)pConflict->nLits ) // all lits are found
        {
            // undo to the root level
            Int_ManCancelUntil( p, p->nRootSize );
            return 1;
        }
    }

    // construct the proof
    Int_ManProofTraceOne( p, pConflict, pClause );

    // undo to the root level
    Int_ManCancelUntil( p, p->nRootSize );

    // add large clauses to the watched lists
    if ( pClause->nLits > 1 )
    {
        Int_ManWatchClause( p, pClause, pClause->pLits[0] );
        Int_ManWatchClause( p, pClause, pClause->pLits[1] );
        return 1;
    }
    assert( pClause->nLits == 1 );

    // if the clause proved is unit, add it and propagate
    if ( !Int_ManEnqueue( p, pClause->pLits[0], pClause ) )
    {
        assert( 0 ); // impossible
        return 0;
    }

    // propagate the assumption
    pConflict = Int_ManPropagate( p, p->nRootSize );
    if ( pConflict )
    {
        // construct the proof
        Int_ManProofTraceOne( p, pConflict, p->pCnf->pEmpty );
        if ( p->fVerbose )
            printf( "Found last conflict after adding unit clause number %d!\n", pClause->Id );
        return 0;
    }

    // update the root level
    p->nRootSize = p->nTrailSize;
    return 1;
}

static inline Sto_Cls_t * Int_ManPropagateOne( Int_Man_t * p, lit Lit )
{
    Sto_Cls_t ** ppPrev, * pCur, * pTemp;
    lit LitF = lit_neg(Lit);
    int i;
    // iterate through the literals
    ppPrev = p->pWatches + Lit;
    for ( pCur = p->pWatches[Lit]; pCur; pCur = *ppPrev )
    {
        // make sure the false literal is in the second literal of the clause
        if ( pCur->pLits[0] == LitF )
        {
            pCur->pLits[0] = pCur->pLits[1];
            pCur->pLits[1] = LitF;
            pTemp = pCur->pNext0;
            pCur->pNext0 = pCur->pNext1;
            pCur->pNext1 = pTemp;
        }
        assert( pCur->pLits[1] == LitF );

        // if the first literal is true, the clause is satisfied
        if ( pCur->pLits[0] == p->pAssigns[lit_var(pCur->pLits[0])] )
        {
            ppPrev = &pCur->pNext1;
            continue;
        }

        // look for a new literal to watch
        for ( i = 2; i < (int)pCur->nLits; i++ )
        {
            // skip the case when the literal is false
            if ( lit_neg(pCur->pLits[i]) == p->pAssigns[lit_var(pCur->pLits[i])] )
                continue;
            // the literal is either true or unassigned - watch it
            pCur->pLits[1] = pCur->pLits[i];
            pCur->pLits[i] = LitF;
            // remove this clause from the watch list of Lit
            *ppPrev = pCur->pNext1;
            // add this clause to the watch list of pCur->pLits[i] (now it is pCur->pLits[1])
            Int_ManWatchClause( p, pCur, pCur->pLits[1] );
            break;
        }
        if ( i < (int)pCur->nLits ) // found new watch
            continue;

        // clause is unit - enqueue new implication
        if ( Int_ManEnqueue(p, pCur->pLits[0], pCur) )
        {
            ppPrev = &pCur->pNext1;
            continue;
        }

        // conflict detected - return the conflict clause
        return pCur;
    }
    return NULL;
}

Sto_Cls_t * Int_ManPropagate( Int_Man_t * p, int Start )
{
    Sto_Cls_t * pClause;
    int i;
    abctime clk = Abc_Clock();
    for ( i = Start; i < p->nTrailSize; i++ )
    {
        pClause = Int_ManPropagateOne( p, p->pTrail[i] );
        if ( pClause )
        {
p->timeBcp += Abc_Clock() - clk;
            return pClause;
        }
    }
p->timeBcp += Abc_Clock() - clk;
    return NULL;
}


int Int_ManProofTraceOne( Int_Man_t * p, Sto_Cls_t * pConflict, Sto_Cls_t * pFinal )
{
    Sto_Cls_t * pReason;
    int i, v, Var, PrevId;
    int fPrint = 0;
    abctime clk = Abc_Clock();

    // collect resolvent literals
    if ( p->fProofVerif )
    {
        assert( (int)pConflict->nLits <= p->nResLitsAlloc );
        memcpy( p->pResLits, pConflict->pLits, sizeof(lit) * pConflict->nLits );
        p->nResLits = pConflict->nLits;
    }

    // mark all the variables in the conflict as seen
    for ( v = 0; v < (int)pConflict->nLits; v++ )
        p->pSeens[lit_var(pConflict->pLits[v])] = 1;

    // start the anticedents
//    pFinal->pAntis = Vec_PtrAlloc( 32 );
//    Vec_PtrPush( pFinal->pAntis, pConflict );

    if ( p->pCnf->nClausesA )
        Int_ManTruthCopy( Int_ManTruthRead(p, pFinal), Int_ManTruthRead(p, pConflict), p->nWords );

    // follow the trail backwards
    PrevId = Int_ManProofGet(p, pConflict);
    for ( i = p->nTrailSize - 1; i >= 0; i-- )
    {
        // skip literals that are not involved
        Var = lit_var(p->pTrail[i]);
        if ( !p->pSeens[Var] )
            continue;
        p->pSeens[Var] = 0;

        // skip literals of the resulting clause
        pReason = p->pReasons[Var];
        if ( pReason == NULL )
            continue;
        assert( p->pTrail[i] == pReason->pLits[0] );

        // add the variables to seen
        for ( v = 1; v < (int)pReason->nLits; v++ )
            p->pSeens[lit_var(pReason->pLits[v])] = 1;


        // record the reason clause
        assert( Int_ManProofGet(p, pReason) > 0 );
        p->Counter++;
        if ( p->fProofWrite )
            fprintf( p->pFile, "%d * %d %d 0\n", p->Counter, PrevId, Int_ManProofGet(p, pReason) );
        PrevId = p->Counter;

        if ( p->pCnf->nClausesA )
        {
            if ( p->pVarTypes[Var] == 1 ) // var of A
                Int_ManTruthOr( Int_ManTruthRead(p, pFinal), Int_ManTruthRead(p, pReason), p->nWords );
            else
                Int_ManTruthAnd( Int_ManTruthRead(p, pFinal), Int_ManTruthRead(p, pReason), p->nWords );
        }

        // resolve the temporary resolvent with the reason clause
        if ( p->fProofVerif )
        {
            int v1, v2;
            if ( fPrint )
                Int_ManPrintResolvent( p->pResLits, p->nResLits );
            // check that the var is present in the resolvent
            for ( v1 = 0; v1 < p->nResLits; v1++ )
                if ( lit_var(p->pResLits[v1]) == Var )
                    break;
            if ( v1 == p->nResLits )
                printf( "Recording clause %d: Cannot find variable %d in the temporary resolvent.\n", pFinal->Id, Var );
            if ( p->pResLits[v1] != lit_neg(pReason->pLits[0]) )
                printf( "Recording clause %d: The resolved variable %d is in the wrong polarity.\n", pFinal->Id, Var );
            // remove this variable from the resolvent
            assert( lit_var(p->pResLits[v1]) == Var );
            p->nResLits--;
            for ( ; v1 < p->nResLits; v1++ )
                p->pResLits[v1] = p->pResLits[v1+1];
            // add variables of the reason clause
            for ( v2 = 1; v2 < (int)pReason->nLits; v2++ )
            {
                for ( v1 = 0; v1 < p->nResLits; v1++ )
                    if ( lit_var(p->pResLits[v1]) == lit_var(pReason->pLits[v2]) )
                        break;
                // if it is a new variable, add it to the resolvent
                if ( v1 == p->nResLits )
                {
                    if ( p->nResLits == p->nResLitsAlloc )
                        printf( "Recording clause %d: Ran out of space for intermediate resolvent.\n", pFinal->Id );
                    p->pResLits[ p->nResLits++ ] = pReason->pLits[v2];
                    continue;
                }
                // if the variable is the same, the literal should be the same too
                if ( p->pResLits[v1] == pReason->pLits[v2] )
                    continue;
                // the literal is different
                printf( "Recording clause %d: Trying to resolve the clause with more than one opposite literal.\n", pFinal->Id );
            }
        }

//        Vec_PtrPush( pFinal->pAntis, pReason );
    }

    // unmark all seen variables
//    for ( i = p->nTrailSize - 1; i >= 0; i-- )
//        p->pSeens[lit_var(p->pTrail[i])] = 0;
    // check that the literals are unmarked
//    for ( i = p->nTrailSize - 1; i >= 0; i-- )
//        assert( p->pSeens[lit_var(p->pTrail[i])] == 0 );

    // use the resulting clause to check the correctness of resolution
    if ( p->fProofVerif )
    {
        int v1, v2;
        if ( fPrint )
            Int_ManPrintResolvent( p->pResLits, p->nResLits );
        for ( v1 = 0; v1 < p->nResLits; v1++ )
        {
            for ( v2 = 0; v2 < (int)pFinal->nLits; v2++ )
                if ( pFinal->pLits[v2] == p->pResLits[v1] )
                    break;
            if ( v2 < (int)pFinal->nLits )
                continue;
            break;
        }
        if ( v1 < p->nResLits )
        {
            printf( "Recording clause %d: The final resolvent is wrong.\n", pFinal->Id );
            Int_ManPrintClause( p, pConflict );
            Int_ManPrintResolvent( p->pResLits, p->nResLits );
            Int_ManPrintClause( p, pFinal );
        }

        // if there are literals in the clause that are not in the resolvent
        // it means that the derived resolvent is stronger than the clause
        // we can replace the clause with the resolvent by removing these literals
        if ( p->nResLits != (int)pFinal->nLits )
        {
            for ( v1 = 0; v1 < (int)pFinal->nLits; v1++ )
            {
                for ( v2 = 0; v2 < p->nResLits; v2++ )
                    if ( pFinal->pLits[v1] == p->pResLits[v2] )
                        break;
                if ( v2 < p->nResLits )
                    continue;
                // remove literal v1 from the final clause
                pFinal->nLits--;
                for ( v2 = v1; v2 < (int)pFinal->nLits; v2++ )
                    pFinal->pLits[v2] = pFinal->pLits[v2+1];
                v1--;
            }
            assert( p->nResLits == (int)pFinal->nLits );
        }
    }
p->timeTrace += Abc_Clock() - clk;

    // return the proof pointer
    if ( p->pCnf->nClausesA )
    {
//        Int_ManPrintInterOne( p, pFinal );
    }
    Int_ManProofSet( p, pFinal, p->Counter );
    // make sure the same proof ID is not asssigned to two consecutive clauses
    assert( p->pProofNums[pFinal->Id-1] != p->Counter );
    return p->Counter;
}


void Int_ManPrintResolvent( lit * pResLits, int nResLits )
{
    int i;
    printf( "Resolvent: {" );
    for ( i = 0; i < nResLits; i++ )
        printf( " %d", pResLits[i] );
    printf( " }\n" );
}


void Int_ManPrintClause( Int_Man_t * p, Sto_Cls_t * pClause )
{
    int i;
    printf( "Clause ID = %d. Proof = %d. {", pClause->Id, Int_ManProofGet(p, pClause) );
    for ( i = 0; i < (int)pClause->nLits; i++ )
        printf( " %d", pClause->pLits[i] );
    printf( " }\n" );
}
