#include "dcals.h"


using namespace std;


Dcals_Man_t::Dcals_Man_t(Abc_Ntk_t * pNtk, int nFrame, int cutSize, double metricBound)
{
    this->pOriNtk = pNtk;
    this->pAppNtk = Abc_NtkDup(this->pOriNtk);
    this->nFrame = nFrame;
    this->cutSize = cutSize;
    this->metric = 0;
    this->metricBound = metricBound;
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
    while (metric < metricBound)
        LocalAppChange();
}


void Dcals_Man_t::LocalAppChange()
{
    DASSERT(Abc_NtkIsLogic(pAppNtk));
    DASSERT(Abc_NtkToAig(pAppNtk));
    DASSERT(Abc_NtkHasAig(pAppNtk));
    DASSERT(pAppNtk->pExcare == nullptr);

    // start the manager
    Mfs_Man_t * pMfsMan = Mfs_ManAlloc(pPars);
    DASSERT(pMfsMan->pCare == nullptr);

    // generate candidates
    double bestEr = 1.0;
    int bestId = -1;
    Abc_Obj_t * pObjApp = nullptr;
    int i = 0;
    Abc_NtkForEachNode(pAppNtk, pObjApp, i) {
        // duplicate network
        Abc_Ntk_t * pCandNtk = Abc_NtkDup(pAppNtk);
        pMfsMan->pNtk = pCandNtk;
        Abc_Obj_t * pObjCand = Abc_NtkObj(pCandNtk, i);
        DASSERT(!strcmp(Abc_ObjName(pObjCand), Abc_ObjName(pObjApp)));
        // compute level
        Abc_NtkLevel(pCandNtk);
        Abc_NtkStartReverseLevels(pCandNtk, pPars->nGrowthLevel);
        // evaluate a candidate
        int isUpdated = LocalAppChangeNode(pMfsMan, pObjCand);
        if (isUpdated) {
            double er = MeasureER(pOriNtk, pCandNtk, 102400, 100);
            if (er < bestEr) {
                bestEr = er;
                bestId = i;
                // cout << "candidate node " << Abc_ObjName(pObjApp) << " best error " << bestEr << endl;
            }
        }
        // recycle memory
        Abc_NtkStopReverseLevels(pCandNtk);
        Abc_NtkDelete(pCandNtk);
    }

    // apply local approximate change
    if (bestId != -1) {
        cout << "best node " << Abc_ObjName(Abc_NtkObj(pAppNtk, bestId)) << " best error " << bestEr << endl;
        metric = bestEr;
        pMfsMan->pNtk = pAppNtk;
        Abc_NtkLevel(pAppNtk);
        Abc_NtkStartReverseLevels(pAppNtk, pPars->nGrowthLevel);
        LocalAppChangeNode(pMfsMan, Abc_NtkObj(pAppNtk, bestId));
        Abc_NtkStopReverseLevels(pAppNtk);
    }

    // sweep
    Abc_NtkSweep(pAppNtk, 0);

    // recycle memory
    Mfs_ManStop(pMfsMan);

    // make sure the consistence of id
    Abc_Ntk_t * pAppNtkNew = Abc_NtkDup(pAppNtk);
    Abc_NtkDelete(pAppNtk);
    pAppNtk = pAppNtkNew;
}


int Dcals_Man_t::LocalAppChangeNode(Mfs_Man_t * p, Abc_Obj_t * pNode)
{
    // prepare data structure for this node
    Mfs_ManClean(p);
    // compute window roots, window support, and window nodes
    p->vRoots = Abc_MfsComputeRoots(pNode, p->pPars->nWinTfoLevs, p->pPars->nFanoutsMax);
    p->vSupp  = Abc_NtkNodeSupport(p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots));
    p->vNodes = Abc_NtkDfsNodes(p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots));
    if (p->pPars->nWinMax && Vec_PtrSize(p->vNodes) > p->pPars->nWinMax)
        return 0;
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
        return 0;
    // solve the SAT problem
    return Abc_NtkMfsResubNode(p, pNode);
}


Aig_Man_t * Dcals_Man_t::ConstructAppAig(Mfs_Man_t * p, Abc_Obj_t * pNode)
{
    Aig_Obj_t * pObjAig, * pDC, * pRoot;
    // find local pi
    Vec_Ptr_t * vLocalPI = App_FindLocalInput(pNode, cutSize);
    // perform logic simulation
    Simulator_t smlt(pNode->pNtk, nFrame);
    smlt.Input();
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
    smlt.Stop();

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
