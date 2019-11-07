#include "dcals.h"


using namespace std;
using namespace boost;


Dcals_Man_t::Dcals_Man_t(Abc_Ntk_t * pNtk, int nFrame, int cutSize, double metricBound, int metricType, int mapType)
{
    DASSERT(nFrame > 0);
    DASSERT(pNtk != nullptr);
    if (Abc_NtkHasMapping(pNtk))
        this->maxDelay = Ckt_GetDelay(pNtk);
    else
        this->maxDelay = DBL_MAX;
    DASSERT(Abc_NtkIsLogic(pNtk));
    DASSERT(Abc_NtkToAig(pNtk));
    DASSERT(Abc_NtkHasAig(pNtk));
    this->pOriNtk = pNtk;
    this->pAppNtk = Abc_NtkDup(this->pOriNtk);
    this->pOriSmlt = nullptr;
    this->pAppSmlt = nullptr;
    this->metricType = metricType;
    this->mapType = mapType;
    this->nFrame = nFrame;
    this->cutSize = cutSize;
    this->metric = 0;
    this->metricBound = metricBound;
    this->pPars = InitMfsPars();
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
    pPars->nWinMax      =   300;
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
        LocalAppChange();
        cout << "time = " << clock() - st << endl << endl;
    }
}


void Dcals_Man_t::LocalAppChange()
{
    DASSERT(Abc_NtkIsLogic(pAppNtk));
    DASSERT(Abc_NtkToAig(pAppNtk));
    DASSERT(Abc_NtkHasAig(pAppNtk));
    DASSERT(pAppNtk->pExcare == nullptr);

    // new seed for logic simulation
    random_device rd;
    seed = static_cast <unsigned>(rd());
    cout << "nframe = " << nFrame << endl;
    cout << "seed = " << seed << endl;

    // patience parameters
    static int pat1, pat2;
    cout << "patience = " << pat1 << "," << pat2 << endl;

    // init simulator
    pOriSmlt = new Simulator_Pro_t(this->pOriNtk, maxNFrame);
    pOriSmlt->Input(100);
    pOriSmlt->Simulate();
    pAppSmlt = new Simulator_Pro_t(this->pAppNtk, maxNFrame);
    pAppSmlt->Input(100);
    pAppSmlt->Simulate();

    // generate candidates
    vector <Lac_Cand_t> cands;
    Lac_Cand_t bestCand;
    cands.reserve(1024);
    if (!metricType)
        GenCand(false, cands);
    else
        GenCand(true, cands);
    BatchErrorEst(cands, bestCand);

    // apply local approximate change
    bool isApply = false;
    if (bestCand.ExistResub(1.0)) {
        pat1 = 0;
        if (bestCand.GetError() <= metricBound) {
            isApply = true;
            pat2 = 0;
        }
        else {
            ++pat2;
            if (pat2 == 5) {
                pat2 = 0;
                isApply = true;
            }
        }
    }
    else {
        ++pat1;
        if (pat1 == 5) {
            nFrame *= 0.9;
            pat1 = 0;
        }
    }
    if (isApply) {
        metric = bestCand.GetError();
        bestCand.Print();
        Ckt_UpdateNetwork(bestCand.GetObj(), bestCand.GetFanins(), bestCand.GetFunc());
        // check metric
        double curEr = (!metricType)?
            MeasureER(pOriNtk, pAppNtk, maxNFrame, 100, true):
            MeasureAEMR(pOriNtk, pAppNtk, maxNFrame, 100, true);
        cout << "RAEM = " << MeasureRAEM(pOriNtk, pAppNtk, 102400, 100, true) << endl;
        DASSERT(curEr == metric);
    }

    // recycle memory
    delete pOriSmlt;
    delete pAppSmlt;

    // disturb the network
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    Abc_FrameReplaceCurrentNetwork(pAbc, Abc_NtkDup(pAppNtk));
    string Command = string("strash; balance; rewrite; refactor; balance; rewrite; rewrite -z; balance; refactor -z; rewrite -z; balance");
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
    Command = string("logic; sweep;");
    DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
    // string Command = string("sweep;");
    // DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
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


void Dcals_Man_t::ConstResub()
{
    DASSERT(Abc_NtkIsLogic(pAppNtk));
    DASSERT(Abc_NtkToAig(pAppNtk));
    DASSERT(Abc_NtkHasAig(pAppNtk));

    double bestEr = 1.0;
    int bestId = -1;
    bool isConst0 = false;
    Abc_Obj_t * pObjApp = nullptr;
    int i = 0;
    cout << "nframe = " << nFrame << endl;

    // init simulator
    pOriSmlt = new Simulator_Pro_t(this->pOriNtk, maxNFrame);
    pOriSmlt->Input(100);
    pOriSmlt->Simulate();
    pAppSmlt = new Simulator_Pro_t(this->pAppNtk, maxNFrame);
    pAppSmlt->Input(100);
    pAppSmlt->Simulate();

    // generate candidates
    boost::progress_display pd(Abc_NtkNodeNum(pAppNtk));
    Hop_Man_t * pHopMan = static_cast <Hop_Man_t *> (pAppNtk->pManFunc);
    Vec_Ptr_t * vFanins = Vec_PtrAlloc(10);
    Abc_NtkForEachNode(pAppNtk, pObjApp, i) {
        // process bar
        ++pd;
        // skip constant nodes
        if (Abc_NodeIsConst(pObjApp))
            continue;
        // evaluate candidate zero
        Hop_Obj_t * pFunc = Hop_ManConst0(pHopMan);
        double er = 0;
        if (!metricType)
            er = MeasureResubER(pOriSmlt, pAppSmlt, pObjApp, pFunc, vFanins, false);
        else
            er = MeasureResubAEMR(pOriSmlt, pAppSmlt, pObjApp, pFunc, vFanins, false);
        if (er < bestEr) {
            bestEr = er;
            bestId = i;
            isConst0 = true;
        }
        // evaluate candidate one
        pFunc = Hop_ManConst1(pHopMan);
        if (!metricType)
            er = MeasureResubER(pOriSmlt, pAppSmlt, pObjApp, pFunc, vFanins, false);
        else
            er = MeasureResubAEMR(pOriSmlt, pAppSmlt, pObjApp, pFunc, vFanins, false);
        if (er < bestEr) {
            bestEr = er;
            bestId = i;
            isConst0 = false;
        }
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

    // recycle memory
    Vec_PtrFree(vFanins);
    delete pOriSmlt;
    delete pAppSmlt;

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
    // find local pi
    Vec_Ptr_t * vCut = Ckt_FindCut(pNode, cutSize);
    // uniform distribution
    random::uniform_int_distribution <int> unf(0, maxNFrame - 1);
    random::mt19937 engine(seed);
    variate_generator <random::mt19937, random::uniform_int_distribution <int> > genId(engine, unf);
    // generate approximate care set
    set <string> patterns;
    for (int i = 0; i < nFrame; ++i) {
        int frameId = genId();
        int blockId = frameId >> 6;
        int bitId = frameId % 64;
        string pattern = "";
        Abc_Obj_t * pObj = nullptr;
        int k = 0;
        Vec_PtrForEachEntry(Abc_Obj_t *, vCut, pObj, k)
            pattern += pAppSmlt->GetValue(pObj, blockId, bitId) ? '1': '0';
        patterns.insert(pattern);
    }
    // start the new manager
    Aig_Man_t * pMan = Aig_ManStart( 1000 );
    // construct the root node's AIG cone
    Aig_Obj_t * pObjAig = Abc_NtkConstructAig_rec( p, pNode, pMan );
    Aig_ObjCreateCo( pMan, pObjAig );
    // add approximate care set
    DASSERT(!patterns.empty());
    Aig_Obj_t * pRoot = Aig_ManConst0(pMan);
    for (auto pattern: patterns) {
        Aig_Obj_t * pDC = Aig_ManConst1(pMan);
        int k = 0;
        Abc_Obj_t * pObj = nullptr;
        Vec_PtrForEachEntry(Abc_Obj_t *, vCut, pObj, k) {
            if (pattern[k] == '1')
                pDC = Aig_And(pMan, pDC, (Aig_Obj_t *)pObj->pCopy);
            else
                pDC = Aig_And(pMan, pDC, Aig_Not((Aig_Obj_t *)pObj->pCopy));
        }
        pRoot = Aig_Or(pMan, pRoot, pDC);
    }
    Aig_ObjCreateCo(pMan, pRoot);
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
    // clean up
    Vec_PtrFree(vCut);
    Aig_ManCleanup(pMan);
    return pMan;
}


void Dcals_Man_t::GenCand(IN bool genConst,INOUT vector <Lac_Cand_t> & cands)
{
    DASSERT(cands.empty());

    // compute level
    Abc_NtkLevel(pAppNtk);
    Abc_NtkStartReverseLevels(pAppNtk, pPars->nGrowthLevel);

    // start mfs manager
    Mfs_Man_t * pMfsMan = Mfs_ManAlloc(pPars);
    DASSERT(pMfsMan->pCare == nullptr);
    pMfsMan->pNtk = pAppNtk;

    // collect candidates
    int i = 0;
    Abc_Obj_t * pObj = nullptr;
    Hop_Obj_t * pFunc = nullptr;
    Hop_Man_t * pHopMan = static_cast <Hop_Man_t *> (pAppNtk->pManFunc);
    Abc_NtkForEachNode(pAppNtk, pObj, i) {
        // skip constant nodes
        if (Abc_NodeIsConst(pObj))
            continue;
        // interpolation
        pFunc = LocalAppChangeNode(pMfsMan, pObj);
        if (pFunc != nullptr)
            cands.emplace_back(pObj, pFunc, pMfsMan->vMfsFanins);
        if (genConst) {
            // const 0
            Vec_PtrClear(pMfsMan->vMfsFanins);
            pFunc = Hop_ManConst0(pHopMan);
            cands.emplace_back(pObj, pFunc, pMfsMan->vMfsFanins);
            // const 1
            pFunc = Hop_ManConst1(pHopMan);
            cands.emplace_back(pObj, pFunc, pMfsMan->vMfsFanins);
        }
    }

    // clean up
    Abc_NtkStopReverseLevels(pAppNtk);
    Mfs_ManStop(pMfsMan);
}


void Dcals_Man_t::BatchErrorEst(IN vector <Lac_Cand_t> & cands, OUT Lac_Cand_t & bestCand)
{
    DASSERT(pOriSmlt->GetNetwork() != pAppSmlt->GetNetwork());
    DASSERT(pOriSmlt->GetNetwork() == pOriNtk);
    DASSERT(pAppSmlt->GetNetwork() == pAppNtk);
    DASSERT(Abc_NtkIsAigLogic(pOriNtk));
    DASSERT(Abc_NtkIsAigLogic(pAppNtk));
    DASSERT(SmltChecker(pOriSmlt, pAppSmlt));
    // get disjoint cuts and the corresponding networks
    pAppSmlt->BuildCutNtks();
    // simulate networks of disjoint cuts
    pAppSmlt->SimulateCutNtks();
    // topological sort
    Vec_Ptr_t * vNodes = Abc_NtkDfs(pAppNtk, 0);
    // calculate the values after resubstitution
    vector <tVec> newValues(cands.size());
    int nBlock = pAppSmlt->GetBlockNum();
    int nFrame = pAppSmlt->GetFrameNum();
    for (uint32_t i = 0; i < cands.size(); ++i) {
        newValues[i].resize(nBlock);
        pAppSmlt->UpdateAigNodeResub(cands[i].GetObj(), cands[i].GetFunc(), cands[i].GetFanins(), newValues[i]);
    }
    // calculate increased error
    int nPo = Abc_NtkPoNum(pAppNtk);
    vector < vector <tVec> > bds(nPo);
    for (auto & bdPo: bds) {
        bdPo.resize(pAppSmlt->GetMaxId() + 1);
        for (auto & bdObjPo: bdPo)
            bdObjPo.resize(nBlock);
    }
    if (!metricType) {
        int baseErr = GetER(pOriSmlt, pAppSmlt, false, false);
        vector <tVec> isERInc(pAppSmlt->GetMaxId() + 1);
        vector <tVec> isERDec(pAppSmlt->GetMaxId() + 1);
        int i = 0;
        Abc_Obj_t * pObj = nullptr;
        Abc_NtkForEachObj(pAppNtk, pObj, i) {
            isERInc[pObj->Id].resize(nBlock);
            isERDec[pObj->Id].resize(nBlock);
            for (int j = 0; j < nBlock; ++j) {
                isERInc[pObj->Id][j] = 0;
                isERDec[pObj->Id][j] = static_cast <uint64_t> (ULLONG_MAX);
            }
        }
        tVec isOnePoRight(nBlock, 0);
        tVec isAllPosRight(nBlock, static_cast <uint64_t> (ULLONG_MAX));
        Abc_Obj_t * pAppPo = nullptr;
        Abc_Obj_t * pOriPo = nullptr;
        Abc_NtkForEachPo(pAppNtk, pAppPo, i) {
            pOriPo = Abc_NtkPo(pOriNtk, i);
            // update the correctness of primary outputs
            for (int j = 0; j < nBlock; ++j) {
                isOnePoRight[j] = ~(pOriSmlt->GetValues(pOriPo, j) ^ pAppSmlt->GetValues(pAppPo, j));
                isAllPosRight[j] &= isOnePoRight[j];
            }
            // update the influence on each primary output
            pAppSmlt->UpdateBoolDiff(pAppPo, vNodes, bds[i]);
            // update the flag of increasement/decreasement
            int j = 0;
            Vec_PtrForEachEntryReverse(Abc_Obj_t *, vNodes, pObj, j) {
                for (int k = 0; k < nBlock; ++k) {
                    isERInc[pObj->Id][k] |= bds[i][pObj->Id][k];
                    isERDec[pObj->Id][k] &= (isOnePoRight[k] ^ bds[i][pObj->Id][k]);
                }
            }
        }
        // updated the influece of each candidates
        for (uint32_t ii = 0; ii < cands.size(); ++ii) {
            Lac_Cand_t & cand = cands[ii];
            Abc_Obj_t * pCand = cand.GetObj();
            tVec & isChanged = newValues[ii];
            for (int i = 0; i < nBlock; ++i)
                isChanged[i] ^= pAppSmlt->GetValues(pCand, i);
            int er = baseErr;
            int movLen = 64 - pAppSmlt->GetLastBlockLen();
            for (int i = 0; i < nBlock; ++i) {
                uint64_t temp = isAllPosRight[i] & isERInc[pCand->Id][i] & isChanged[i];
                if (i == nBlock - 1) {
                    temp >>= movLen;
                    temp <<= movLen;
                }
                er += Ckt_CountOneNum(temp);
                temp = ~isAllPosRight[i] & isERDec[pCand->Id][i] & isChanged[i];
                if (i == nBlock - 1) {
                    temp >>= movLen;
                    temp <<= movLen;
                }
                er -= Ckt_CountOneNum(temp);
            }
            bestCand.UpdateBest(er / static_cast <double> (pAppSmlt->GetFrameNum()), cand.GetObj(), cand.GetFunc(), cand.GetFanins());
        }
    }
    else {
        vector < vector <int8_t> > offsets;
        GetOffset(pOriSmlt, pAppSmlt, false, offsets);
        int i = 0;
        Abc_Obj_t * pAppPo = nullptr;
        Abc_NtkForEachPo(pAppNtk, pAppPo, i)
            // update the influence on each primary output
            pAppSmlt->UpdateBoolDiff(pAppPo, vNodes, bds[i]);
        // updated the influece of each candidates
        for (uint32_t ii = 0; ii < cands.size(); ++ii) {
            Lac_Cand_t & cand = cands[ii];
            Abc_Obj_t * pCand = cand.GetObj();
            tVec & isChanged = newValues[ii];
            for (int i = 0; i < nBlock; ++i)
                isChanged[i] ^= pAppSmlt->GetValues(pCand, i);
            vector < vector <int8_t> > offsetsTmp(offsets);
            for (int i = 0; i < nPo; ++i)
                offsetsTmp[i].assign(offsets[i].begin(), offsets[i].end());
            int frameId = 0;
            for (int blockId = 0; blockId < nBlock; ++blockId) {
                for (int bitId = 0; bitId < 64; ++bitId) {
                    if (frameId >= nFrame)
                        break;
                    if (Ckt_GetBit(isChanged[blockId], bitId)) {
                        for (int i = 0; i < nPo; ++i) {
                            Abc_Obj_t * pAppPo = Abc_NtkPo(pAppNtk, i);
                            if (Ckt_GetBit(bds[i][pCand->Id][blockId], bitId)) {
                                if (pAppSmlt->GetValue(pAppPo, blockId, bitId))
                                    --offsetsTmp[i][frameId];
                                else
                                    ++offsetsTmp[i][frameId];
                            }
                        }
                    }
                    ++frameId;
                }
            }
            double er = GetAEMRFromOffset(offsetsTmp);
            bestCand.UpdateBest(er, cand.GetObj(), cand.GetFunc(), cand.GetFanins());
        }
    }
    // clean up
    Vec_PtrFree(vNodes);
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


void Ckt_UpdateNetwork(Abc_Obj_t * pObj, Vec_Ptr_t * vFanins, Hop_Obj_t * pFunc)
{
    Abc_Obj_t * pObjNew, * pFanin;
    int k;
    // create the new node
    pObjNew = Abc_NtkCreateNode( pObj->pNtk );
    pObjNew->pData = pFunc;
    Vec_PtrForEachEntry( Abc_Obj_t *, vFanins, pFanin, k )
        Abc_ObjAddFanin( pObjNew, pFanin );
    // replace the old node by the new node
    Abc_ObjReplace( pObj, pObjNew );
}
