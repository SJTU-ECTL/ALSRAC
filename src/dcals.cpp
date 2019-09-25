#include "dcals.h"


using namespace std;
using namespace abc;


Dcals_Man_t::Dcals_Man_t(abc::Abc_Ntk_t * pNtk, int nFrame, int cutSize, double maxAEM)
{
    this->pNtk = pNtk;
    this->nFrame = nFrame;
    this->cutSize = cutSize;
    this->maxAEM = maxAEM;
    this->pPars = InitMfsPars();
    DASSERT(nFrame > 0);
    DASSERT(pNtk != nullptr);
    DASSERT(Abc_NtkIsLogic(pNtk));
}


Dcals_Man_t::~Dcals_Man_t()
{
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


void Dcals_Man_t::LocalAppChange()
{
    DASSERT(Abc_NtkToAig(pNtk));
    DASSERT(Abc_NtkHasAig(pNtk));
    DASSERT(pNtk->pExcare == nullptr);

    // start the manager
    Mfs_Man_t * pMfsMan = Mfs_ManAlloc(pPars);
    DASSERT(pMfsMan->pCare == nullptr);

    // generate candidates
    for (int i = 0; i < Abc_NtkObjNum(pNtk); ++i) {
        // duplicate network
        Abc_Ntk_t * pNewNtk = Abc_NtkDup(pNtk);
        DASSERT(Abc_NtkObjNum(pNtk) == Abc_NtkObjNum(pNewNtk));
        pMfsMan->pNtk = pNewNtk;
        Abc_Obj_t * pObj = Abc_NtkObj(pNewNtk, i);
        if (pObj == nullptr || !Abc_ObjIsNode(pObj))
            continue;
        // compute level
        Abc_NtkLevel(pNewNtk);
        Abc_NtkStartReverseLevels(pNewNtk, pPars->nGrowthLevel);
        // evaluate a candidate
        LocalAppChangeNode(pMfsMan, pObj);
        // recycle memory
        Abc_NtkStopReverseLevels(pNewNtk);
        Abc_NtkDelete(pNewNtk);
    }

    // sweep
    Abc_NtkSweep(pNtk, 0);

    // recycle memory
    Mfs_ManStop(pMfsMan);
}


void Dcals_Man_t::LocalAppChangeNode(Mfs_Man_t * p, Abc_Obj_t * pNode)
{
    // prepare data structure for this node
    Mfs_ManClean(p);
    // compute window roots, window support, and window nodes
    p->vRoots = Abc_MfsComputeRoots(pNode, p->pPars->nWinTfoLevs, p->pPars->nFanoutsMax);
    p->vSupp  = Abc_NtkNodeSupport(p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots));
    p->vNodes = Abc_NtkDfsNodes(p->pNtk, (Abc_Obj_t **)Vec_PtrArray(p->vRoots), Vec_PtrSize(p->vRoots));
    if (p->pPars->nWinMax && Vec_PtrSize(p->vNodes) > p->pPars->nWinMax)
        return;
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
        return;
    // solve the SAT problem
    Abc_NtkMfsResubNode(p, pNode);
}


Aig_Man_t * Dcals_Man_t::ConstructAppAig(Mfs_Man_t * p, Abc_Obj_t * pNode)
{
    // perform logic simulation
    // Simulator_t smlt(p->pNtk, nFrame);
    // smlt.Input();
    // smlt.Simulate();
    // find local pi
    // Vec_Ptr_t * vLocalPI = App_FindLocalInput(pNode, nLocalPI);

    Aig_Man_t * pMan;
    Abc_Obj_t * pFanin, * pObj;
    Aig_Obj_t * pObjAig, * pDC, * pRoot;
    int i, k;
    // start the new manager
    pMan = Aig_ManStart(1000);
    // construct the root node's AIG cone
    pObjAig = Abc_NtkConstructAig_rec(p, pNode, pMan);
    Aig_ObjCreateCo(pMan, pObjAig);

    // add approximate care set
    // set <string> patterns;
    // int nCluster = min(frameNumber, 64);
    // for (int i = 0; i < pCktNtk->GetSimNum(); ++i) {
    //     for (int j = 0; j < nCluster; ++j) {
    //         string pattern = "";
    //         Vec_PtrForEachEntry(Abc_Obj_t *, vLocalPI, pObj, k) {
    //             shared_ptr <Ckt_Obj_t> pCktObj = pCktNtk->GetCktObj(string(Abc_ObjName(pObj)));
    //             DEBUG_ASSERT(pCktObj->GetName() == string(Abc_ObjName(pObj)), module_a{}, "object does not match");
    //             pattern +=  pCktObj->GetSimVal(i, j)? '1': '0';
    //         }
    //         DASSERT(static_cast <int>(pattern.length()) == Vec_PtrSize(vLocalPI));
    //         patterns.insert(pattern);
    //     }
    // }
    // pRoot = Aig_ManConst0(pMan);
    // for (auto pattern: patterns) {
    //     pDC = Aig_ManConst1(pMan);
    //     Vec_PtrForEachEntry(Abc_Obj_t *, vLocalPI, pObj, k) {
    //         if (pattern[k] == '1')
    //             pDC = Aig_And(pMan, pDC, (Aig_Obj_t *)pObj->pCopy);
    //         else
    //             pDC = Aig_And(pMan, pDC, Aig_Not((Aig_Obj_t *)pObj->pCopy));
    //     }
    //     pRoot = Aig_Or(pMan, pRoot, pDC);
    // }
    // Aig_ObjCreateCo(pMan, pRoot);

    // Vec_PtrFree(vLocalPI);

    // construct the node
    pObjAig = (Aig_Obj_t *)pNode->pCopy;
    Aig_ObjCreateCo(pMan, pObjAig);
    // construct the divisors
    Vec_PtrForEachEntry( Abc_Obj_t *, p->vDivs, pFanin, i )
    {
        pObjAig = (Aig_Obj_t *)pFanin->pCopy;
        Aig_ObjCreateCo( pMan, pObjAig );
    }
    Aig_ManCleanup( pMan );

    // recycle memory
    // smlt.Stop();
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
