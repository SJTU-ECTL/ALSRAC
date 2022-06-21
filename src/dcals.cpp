#include "dcals.h"


using namespace std;
using namespace boost;


Dcals_Man_t::Dcals_Man_t(Abc_Ntk_t * pNtk, int nFrame, int cutSize, double metricBound, Metric_t metricType, int mapType, string outPath)
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
    this->seed = 0;
    this->metricType = metricType;
    this->mapType = mapType;
    this->roundId = 0;
    this->nFrame = nFrame;
    this->cutSize = cutSize;
    this->nEvalFrame = 102400;
    if (metricType == Metric_t::RAEM)
        this->nEstiFrame = 1024;
    else
        this->nEstiFrame = 102400;
    this->metric = 0;
    this->metricBound = metricBound;
    this->pPars = InitMfsPars();
    this->outPath = outPath;
    this->forbidList.clear();
    for (int i = 0; i < forbidRound; ++i)
        this->forbidList.push_back("");
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

    // constant propagation
    Abc_NtkSweep(pAppNtk, 0);

    // main loop
    clock_t st = clock();
    while (metric < metricBound) {
        cout << "--------------- round " << roundId << " ---------------" << endl;
        LocalAppChange();
        cout << "time = " << clock() - st << endl << endl;
        ++roundId;
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
    pOriSmlt = new Simulator_Pro_t(this->pOriNtk, nEstiFrame);
    pOriSmlt->Input(seed);
    pOriSmlt->Simulate();
    pAppSmlt = new Simulator_Pro_t(this->pAppNtk, nEstiFrame);
    pAppSmlt->Input(seed);
    pAppSmlt->Simulate();

    // generate candidates
    clock_t st = clock();
    vector <Lac_Cand_t> cands;
    Lac_Cand_t bestCand;
    cands.reserve(8192);
    GenCand(cands);
    cout << "cand number = " << cands.size() << endl;
    cout << "cand time = " << clock() - st << endl;
    BatchErrorEst(cands, bestCand);

    // apply local approximate change
    bool isApply = false;
    double realEr = 0;
    if (bestCand.ExistResub(1.0)) {
        // update and check error
        if (metricType == Metric_t::ER)
            realEr = MeasureER(pOriNtk, pAppNtk, nEvalFrame, seed, true);
        else if (metricType == Metric_t::AEMR)
            realEr = MeasureAEMR(pOriNtk, pAppNtk, nEvalFrame, seed, true);
        else if (metricType == Metric_t::RAEM)
            realEr = MeasureRAEM(pOriNtk, pAppNtk, nEvalFrame, seed, true);
        else
            DASSERT(0);
        cout << "estimated error = " << bestCand.GetError() << endl;
        // DASSERT(realEr <= bestCand.GetError());

        // update patience
        pat1 = 0;
        if (realEr <= metricBound) {
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
        cout << "Apply candidate:" << endl;
        bestCand.Print();
        if (!forbidList.empty())
            forbidList.pop_front();
        if (Hop_DagSize(bestCand.GetFunc()) == Hop_DagSize( static_cast <Hop_Obj_t *> (bestCand.GetObj()->pData) ))
            forbidList.push_back(string(Abc_ObjName(bestCand.GetObj())));
        Abc_Obj_t * pObjNew = Ckt_UpdateNetwork(bestCand.GetObj(), bestCand.GetFanins(), bestCand.GetFunc());
        forbidList.push_back(string(Abc_ObjName(pObjNew)));
        metric = realEr;
    }
    cout << "current error = " << metric << endl;

    // recycle memory
    delete pOriSmlt;
    delete pAppSmlt;

    if (metricType == Metric_t::RAEM) {
        // evaluate the current approximate circuit
        Abc_NtkSweep(pAppNtk, 0);
        int size = Abc_NtkNodeNum(pAppNtk);
        int depth = Abc_NtkLevel(pAppNtk);
        cout << "size = " << size << endl;
        cout << "depth = " << depth << endl;
        ostringstream fileName("");
        fileName << outPath << pAppNtk->pName << "_" << metric << "_" << size << "_" << depth << ".blif";
        Ckt_WriteBlif(pAppNtk, fileName.str());
    }
    else {
        // disturb the network
        if (roundId % 10 == 0) {
            Abc_NtkSweep(pAppNtk, 0);
            Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
            Abc_FrameReplaceCurrentNetwork(pAbc, Abc_NtkDup(pAppNtk));
            string Command = string("strash; balance; rewrite; refactor; balance; rewrite; rewrite -z; balance; refactor -z; rewrite -z; balance; logic;");
            DASSERT(!Cmd_CommandExecute(pAbc, Command.c_str()));
            Abc_NtkDelete(pAppNtk);
            pAppNtk = Abc_NtkDup(Abc_FrameReadNtk(pAbc));
        }
        // evaluate the current approximate circuit
        if (roundId % 10 == 0 || metric > 0.01 * metricBound) {
            ostringstream fileName("");
            fileName << outPath << pAppNtk->pName << "_" << metric;
            if (!mapType)
                Ckt_EvalASIC(pAppNtk, fileName.str(), maxDelay, true);
            else {
                Ckt_EvalFPGA(pAppNtk, fileName.str(), "strash; if -K 6 -a;");
                // Ckt_EvalFPGA(pAppNtk, fileName.str(), "strash; if -K 6;");
            }
        }
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
    random::uniform_int_distribution <int> unf(0, nEstiFrame - 1);
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


void Dcals_Man_t::GenCand(IN bool genConst, INOUT vector <Lac_Cand_t> & cands)
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


void Dcals_Man_t::GenCand(INOUT vector <Lac_Cand_t> & cands)
{
    cands.clear();
    Abc_NtkLevel(pAppNtk);
    Abc_NtkStartReverseLevels(pAppNtk, pPars->nGrowthLevel);
    Abc_Obj_t * pPivot = nullptr;
    int ii = 0;
    const int nCandLimit = 1;
    if (metricType == Metric_t::ER || metricType == Metric_t::AEMR) {
        Abc_NtkForEachNode(pAppNtk, pPivot, ii) {
            // skip nodes with less than one inputs
            if (Abc_ObjFaninNum(pPivot) < 1)
                continue;
            // avoid resubstitution loop
            auto pos = find(forbidList.begin(), forbidList.end(), string(Abc_ObjName(pPivot)));
            bool skipResub = (pos != forbidList.end())? true: false;
            // bool skipResub = false;
            // compute divisors
            Vec_Ptr_t * vDivs = Ckt_ComputeDivisors(pPivot, Abc_ObjRequiredLevel(pPivot) - 1, pPars->nWinMax, pPars->nFanoutsMax);

            // enumerate resubstitution
            int nFanins = Abc_ObjFaninNum(pPivot);
            int cnt = 0;
            for (int i = 0; i < nFanins; ++i) {
                // init temp divisors
                Vec_Ptr_t * vFanins = Vec_PtrAlloc(10);
                for (int j = 0; j < nFanins; ++j) {
                    if (i != j)
                        Vec_PtrPush(vFanins, Abc_ObjFanin(pPivot, j));
                }
                // try removing the i-th fanin
                {
                    Hop_Obj_t * pFunc = BuildFuncEspresso(pPivot, vFanins);
                    if (pFunc != nullptr)
                        cands.emplace_back(pPivot, pFunc, vFanins);
                }
                // try replacing the i-th fanin with another divisor
                if (!skipResub) {
                    Vec_PtrPush(vFanins, nullptr);
                    Abc_Obj_t * pDiv = nullptr;
                    int j = 0;
                    Vec_PtrForEachEntry(Abc_Obj_t *, vDivs, pDiv, j) {
                        if (pDiv == Abc_ObjFanin(pPivot, i))
                            continue;
                        Vec_PtrWriteEntry(vFanins, nFanins - 1, pDiv);
                        Hop_Obj_t * pFunc = BuildFuncEspresso(pPivot, vFanins);
                        if (pFunc != nullptr) {
                            cands.emplace_back(pPivot, pFunc, vFanins);
                            ++cnt;
                            if (cnt > nCandLimit)
                                break;
                        }
                    }
                }
                // clean up
                Vec_PtrFree(vFanins);
            }
            // clean up
            Vec_PtrFree(vDivs);
        }
    }
    if (metricType == Metric_t::AEMR || metricType == Metric_t::RAEM) {
        Abc_NtkForEachNode(pAppNtk, pPivot, ii) {
            // skip nodes with less than one inputs
            if (Abc_ObjFaninNum(pPivot) < 1)
                continue;
            Vec_Ptr_t * vFanins = Vec_PtrAlloc(1);
            Vec_PtrClear(vFanins);
            Hop_Obj_t * pFunc = Hop_ManConst0((Hop_Man_t *)(pAppNtk->pManFunc));
            cands.emplace_back(pPivot, pFunc, vFanins);
            pFunc = Hop_ManConst1((Hop_Man_t *)(pAppNtk->pManFunc));
            cands.emplace_back(pPivot, pFunc, vFanins);
            Vec_PtrFree(vFanins);
        }
    }
    Abc_NtkStopReverseLevels(pAppNtk);
}


Hop_Obj_t * Dcals_Man_t::BuildFunc(IN Abc_Obj_t * pPivot, IN Vec_Ptr_t * vFanins)
{
    DASSERT(pAppNtk == pPivot->pNtk);
    Abc_Obj_t * pFanin = nullptr;
    int k = 0;
    Vec_PtrForEachEntry(Abc_Obj_t *, vFanins, pFanin, k)
        DASSERT(pAppNtk == pFanin->pNtk);

    // check the existence of resubstitution and build truth table
    int nVars = Vec_PtrSize(vFanins);
    DASSERT(nVars <= 5);
    DASSERT(nFrame <= pAppSmlt->GetFrameNum());
    int nTruth = Kit_TruthWordNum(nVars);
    unsigned * puTruth = new unsigned[nTruth];
    memset(puTruth, 0, nTruth * sizeof(unsigned));
    int nBlock = (nFrame & 64) ? ((nFrame >> 6) + 1) : (nFrame >> 6);
    int nLastBlock = (nFrame & 64)? (nFrame & 64): 64;
    set <uint64_t> patterns;
    for (int i = 0; i < nBlock; ++i) {
        int nBits = (i == nBlock - 1)? nLastBlock: 64;
        for (int j = 0; j < nBits; ++j) {
            int pattern = 0;
            Vec_PtrForEachEntryReverse(Abc_Obj_t *, vFanins, pFanin, k) {
                pattern <<= 1;
                if (pAppSmlt->GetValue(pFanin, i, j))
                    ++pattern;
            }
            bool val = pAppSmlt->GetValue(pPivot, i, j);
            if (!patterns.count(pattern)) {
                patterns.emplace(pattern);
                Kit_TruthSetBit(puTruth, pattern);
            }
            else {
                if (val != static_cast <bool> (Kit_TruthHasBit(puTruth, pattern))) {
                    delete puTruth;
                    return nullptr;
                }
            }
        }
    }
    for (int i = 0; i < (32 >> nVars) - 1; ++i)
        puTruth[0] |= (puTruth[0] << (1 << nVars));

    // compute ISOP
    Vec_Int_t * vMem = Vec_IntAlloc(0);
    Kit_Graph_t * pGraph = Ckt_TruthToGraph(puTruth, nVars, vMem);
    Hop_Obj_t * pFunc = Kit_GraphToHop((Hop_Man_t *)pAppNtk->pManFunc, pGraph);
    Kit_GraphFree(pGraph);
    Vec_IntFree(vMem);

    // Ckt_PrintNodeFunc(pPivot);
    // cout << "truthtable ";
    // for (int i = 0; i < nTruth; ++i)
    //     cout << puTruth[i] << " ";
    // cout << endl;
    // Ckt_PrintHopFunc(pFunc, vFanins);
    // cout << endl;

    // clean up
    delete puTruth;

    if ( Hop_DagSize(pFunc) <= Hop_DagSize( static_cast <Hop_Obj_t *> (pPivot->pData) ) )
        return pFunc;
    else
        return nullptr;
}


Hop_Obj_t * Dcals_Man_t::BuildFuncEspresso(IN Abc_Obj_t * pPivot, IN Vec_Ptr_t * vFanins)
{
    DASSERT(pAppNtk == pPivot->pNtk);
    Abc_Obj_t * pFanin = nullptr;
    int k = 0;
    Vec_PtrForEachEntry(Abc_Obj_t *, vFanins, pFanin, k)
        DASSERT(pAppNtk == pFanin->pNtk);

    // check the existence of resubstitution and build truth table
    typedef unordered_map <string, bool> table_t;
    table_t truthTable;
    DASSERT(nFrame <= pAppSmlt->GetFrameNum());
    for (int i = 0; i < nFrame; ++i) {
        int blockId = i >> 6;
        int bitId = i % 64;
        string minterm("");
        Vec_PtrForEachEntry(Abc_Obj_t *, vFanins, pFanin, k)
            minterm += pAppSmlt->GetValue(pFanin, blockId, bitId) ? '1' : '0';
        bool val = pAppSmlt->GetValue(pPivot, blockId, bitId);
        table_t::const_iterator got = truthTable.find(minterm);
        if (got == truthTable.end())
            truthTable[minterm] = val;
        else {
            if (got->second != val) {
                return nullptr;
            }
        }
    }

    // construct function with espresso
    int nVars = Vec_PtrSize(vFanins);
    pPLA PLA = new_PLA();
    PLA->pla_type = FR_type;

    DASSERT(cube.fullset == nullptr);
    cube.num_binary_vars = nVars;
    cube.num_vars = cube.num_binary_vars + 1;
    cube.part_size = ALLOC(int, cube.num_vars);

    DASSERT(cube.fullset == nullptr);
    DASSERT(cube.part_size != nullptr);
    cube.part_size[cube.num_vars-1] = 1;
    cube_setup();
    PLA_labels(PLA);

    if (PLA->F == nullptr) {
        PLA->F = new_cover(10);
        PLA->D = new_cover(10);
        PLA->R = new_cover(10);
    }

    pcube cf = cube.temp[0];
    for (table_t::const_iterator iter = truthTable.begin(); iter != truthTable.end(); ++iter) {
        set_clear(cf, cube.size);
        const string & minterm = iter->first;
        for (int i = 0; i < nVars; ++i) {
            if (minterm[i] == '0')
                set_insert(cf, 2*i);
            else if (minterm[i] == '1')
                set_insert(cf, 2*i+1);
            else
                DASSERT(0);
        }
        set_insert(cf,2*nVars);
        if (iter->second)
            PLA->F = sf_addset(PLA->F, cf);
        else
            PLA->R = sf_addset(PLA->R, cf);
    }

    pcover X;
    free_cover(PLA->D);
    X = d1merge(sf_join(PLA->F, PLA->R), cube.num_vars - 1);
    PLA->D = complement(cube1list(X));
    free_cover(X);

    PLA->F = espresso(PLA->F, PLA->D, PLA->R);

    // convert to hop
    Hop_Man_t * pHopMan = static_cast <Hop_Man_t *> (pAppNtk->pManFunc);
    Hop_Obj_t * pFunc = Hop_ManConst0(pHopMan);
    register pcube last, c;
    foreach_set(PLA->F, last, c) {
        Hop_Obj_t * pAnd = Hop_ManConst1(pHopMan);
        for (int var = 0; var < cube.num_binary_vars; var++) {
            if ("?01-" [GETINPUT(c, var)] == '1')
                pAnd = Hop_And( pHopMan, pAnd , Hop_IthVar(pHopMan, var) );
            else if ("?01-" [GETINPUT(c, var)] == '0')
                pAnd = Hop_And( pHopMan, pAnd , Hop_Not(Hop_IthVar(pHopMan, var)) );
        }
        DASSERT(cube.num_binary_vars == cube.num_vars - 1);
        DASSERT(cube.output != -1);
        int llast = cube.last_part[cube.output];
        DASSERT(cube.first_part[cube.output] == llast);
        DASSERT("01" [is_in_set(c, llast) != 0] == '1');
        pFunc = Hop_Or( pHopMan, pFunc, pAnd );
    }
    // Ckt_PrintNodeFunc(pPivot);
    // Ckt_PrintHopFunc(pFunc, vFanins);
    // cout << endl;

    // clean up
    free_PLA(PLA);
    FREE(cube.part_size);
    setdown_cube();
    sf_cleanup();
    sm_cleanup();

    // filter functions
    if ( Hop_DagSize(pFunc) <= Hop_DagSize( static_cast <Hop_Obj_t *> (pPivot->pData) ) )
        return pFunc;
    else
        return nullptr;
}


void Dcals_Man_t::BatchErrorEst(IN vector <Lac_Cand_t> & cands, OUT Lac_Cand_t & bestCand)
{
    // check
    DASSERT(pOriSmlt->GetNetwork() != pAppSmlt->GetNetwork());
    DASSERT(pOriSmlt->GetNetwork() == pOriNtk);
    DASSERT(pAppSmlt->GetNetwork() == pAppNtk);
    DASSERT(Abc_NtkIsAigLogic(pOriNtk));
    DASSERT(Abc_NtkIsAigLogic(pAppNtk));
    DASSERT(SmltChecker(pOriSmlt, pAppSmlt));
    // get disjoint cuts and the corresponding networks
    // clock_t st = clock();
    // pAppSmlt->BuildCutNtks();
    pAppSmlt->BuildAppCutNtks();
    // cout << "build cut time = " << clock() - st << endl;
    // simulate networks of disjoint cuts
    // st = clock();
    pAppSmlt->SimulateCutNtks();
    // cout << "simulate cut time = " << clock() - st << endl;
    // calculate the values after resubstitution
    // st = clock();
    vector <tVec> newValues(cands.size());
    int nBlock = pAppSmlt->GetBlockNum();
    for (uint32_t i = 0; i < cands.size(); ++i) {
        newValues[i].resize(nBlock);
        pAppSmlt->UpdateAigNodeResub(cands[i].GetObj(), cands[i].GetFunc(), cands[i].GetFanins(), newValues[i]);
    }
    // cout << "compute new value time = " << clock() - st << endl;
    // topological sort
    Vec_Ptr_t * vNodes = Abc_NtkDfs(pAppNtk, 0);
    // for different metric types, estimate error upper bound
    int nPo = Abc_NtkPoNum(pAppNtk);
    if (metricType == Metric_t::ER) {
        // init boolean difference
        vector <tVec> bd(pAppSmlt->GetMaxId() + 1, tVec(nBlock, 0));
        // compute current error
        int baseErr = GetER(pOriSmlt, pAppSmlt, false, false);
        // compute po erroneous information
        tVec isAllPosRight(nBlock, static_cast <uint64_t> (ULLONG_MAX));
        Abc_Obj_t * pAppPo = nullptr;
        Abc_Obj_t * pOriPo = nullptr;
        int i = 0;
        Abc_NtkForEachPo(pAppNtk, pAppPo, i) {
            pOriPo = Abc_NtkPo(pOriNtk, i);
            for (int j = 0; j < nBlock; ++j)
                isAllPosRight[j] &= (~(pOriSmlt->GetValues(pOriPo, j) ^ pAppSmlt->GetValues(pAppPo, j)));
        }
        // compute boolean difference
        // st = clock();
        pAppSmlt->UpdateBoolDiff(vNodes, bd);
        // cout << "boolean difference time = " << clock() - st << endl;
        // updated the influece of each candidates
        // st = clock();
        for (uint32_t ii = 0; ii < cands.size(); ++ii) {
            Lac_Cand_t & cand = cands[ii];
            Abc_Obj_t * pCand = cand.GetObj();
            tVec & isChanged = newValues[ii];
            for (int i = 0; i < nBlock; ++i)
                isChanged[i] ^= pAppSmlt->GetValues(pCand, i);
            int er = baseErr;
            int movLen = 64 - pAppSmlt->GetLastBlockLen();
            for (int i = 0; i < nBlock; ++i) {
                // uint64_t temp = isAllPosRight[i] & isERInc[pCand->Id][i] & isChanged[i];
                uint64_t temp = isAllPosRight[i] & bd[pCand->Id][i] & isChanged[i];
                if (i == nBlock - 1) {
                    temp >>= movLen;
                    temp <<= movLen;
                }
                er += Ckt_CountOneNum(temp);
            }
            bestCand.UpdateBest(er / static_cast <double> (pAppSmlt->GetFrameNum()), cand.GetObj(), cand.GetFunc(), cand.GetFanins());
        }
        // cout << "evaluate candidate time = " << clock() - st << endl;
    }
    else if (metricType == Metric_t::AEMR || metricType == Metric_t::RAEM) {
        vector < vector <tVec> > bds(nPo);
        for (auto & bdPo: bds) {
            bdPo.resize(pAppSmlt->GetMaxId() + 1);
            for (auto & bdObjPo: bdPo)
                bdObjPo.resize(nBlock);
        }
        int nFrame = pAppSmlt->GetFrameNum();
        vector < vector <int8_t> > offsets;
        GetOffset(pOriSmlt, pAppSmlt, false, offsets);
        int i = 0;
        Abc_Obj_t * pAppPo = nullptr;
        Abc_NtkForEachPo(pAppNtk, pAppPo, i)
            // update the influence on each primary output
            pAppSmlt->UpdateBoolDiff(pAppPo, vNodes, bds[i]);
        // updated the influece of each candidates
        // boost::progress_display pd(cands.size());
        for (uint32_t ii = 0; ii < cands.size(); ++ii) {
            // ++pd;
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
    else
        DASSERT(0);
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
        pFunc = Ckt_NtkMfsInterplate( p, pCands, nCands );
        if ( pFunc == nullptr )
            return nullptr;
        // Ckt_PrintHopFunc(pFunc, p->vMfsFanins);
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
            pFunc = Ckt_NtkMfsInterplate( p, pCands, nCands+1 );
            if ( pFunc == nullptr )
                return nullptr;
            // update the divisors
            Vec_PtrPush( p->vMfsFanins, Vec_PtrEntry(p->vDivs, iVar) );
            // Ckt_PrintHopFunc(pFunc, p->vMfsFanins);
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


Abc_Obj_t * Ckt_UpdateNetwork(Abc_Obj_t * pObj, Vec_Ptr_t * vFanins, Hop_Obj_t * pFunc)
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
    return pObjNew;
}


bool IsSimpPo(Abc_Obj_t * pObj)
{
    DASSERT(Abc_ObjIsPo(pObj));
    Abc_Obj_t * pDriver = Abc_ObjFanin0(pObj);
    Abc_Obj_t * pFanin = nullptr;
    int i = 0;
    Abc_ObjForEachFanin(pDriver, pFanin, i) {
        if (!Abc_ObjIsPi(pFanin))
            return false;
    }
    return true;
}


Abc_Obj_t * GetFirstPoFanout(Abc_Obj_t * pObj)
{
    DASSERT(Abc_ObjIsNode(pObj));
    Abc_Obj_t * pFanout = nullptr;
    int i = 0;
    Abc_ObjForEachFanout(pObj, pFanout, i) {
        if (Abc_ObjIsPo(pFanout))
            return pFanout;
    }
    return nullptr;
}


Vec_Ptr_t * GetTFICone(Abc_Ntk_t * pNtk, Abc_Obj_t * pObj)
{
    DASSERT(pObj->pNtk == pNtk);
    // set the traversal ID
    Abc_NtkIncrementTravId( pNtk );
    // start the array of nodes
    Vec_Ptr_t * vNodes = Vec_PtrAlloc( 100 );
    // collect
    if ( Abc_NtkIsStrash(pNtk) && Abc_AigNodeIsConst(pObj) )
        DASSERT(0);
    if ( Abc_ObjIsCo(pObj) )
    {
        Abc_NodeSetTravIdCurrent(pObj);
        Abc_NtkDfs_rec( Abc_ObjFanin0Ntk(Abc_ObjFanin0(pObj)), vNodes );
    }
    else if ( Abc_ObjIsNode(pObj) || Abc_ObjIsCi(pObj) )
        Abc_NtkDfs_rec( pObj, vNodes );
    return vNodes;
}


Vec_Ptr_t * ComputeDivisors(Abc_Obj_t * pNode, int nWinTfiLevs)
{
    Vec_Ptr_t * vDivs = Vec_PtrAlloc(100);
    Abc_NtkIncrementTravId(pNode->pNtk);
    int i = 0;
    Abc_Obj_t * pFanin = nullptr;
    Abc_ObjForEachFanin(pNode, pFanin, i)
        ComputeDivisors_rec(pFanin, vDivs, pNode->Level - nWinTfiLevs);
    return vDivs;
}


void ComputeDivisors_rec(Abc_Obj_t * pNode, Vec_Ptr_t * vDivs, int minLevel)
{
    if (Abc_NodeIsTravIdCurrent(pNode) || Abc_ObjLevel(pNode) < minLevel)
        return;
    Abc_NodeSetTravIdCurrent(pNode);
    Abc_Obj_t * pFanin = nullptr;
    int i = 0;
    Abc_ObjForEachFanin(pNode, pFanin, i)
        ComputeDivisors_rec(pFanin, vDivs, minLevel);
    Vec_PtrPush(vDivs, pNode);
}


Hop_Obj_t * Ckt_NtkMfsInterplate( Mfs_Man_t * p, int * pCands, int nCands )
{
    int fDumpFile = 0;
    char FileName[32];
    sat_solver * pSat;
    Sto_Man_t * pCnf = NULL;
    unsigned * puTruth;
    Kit_Graph_t * pGraph;
    Hop_Obj_t * pFunc;
    int nFanins, status;
    int c, i, * pGloVars;

    // derive the SAT solver for interpolation
    pSat = Abc_MfsCreateSolverResub( p, pCands, nCands, 0 );

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
    nFanins = Int_ManInterpolate( p->pMan, pCnf, 0, &puTruth );
    Sto_ManFree( pCnf );
    assert( nFanins == nCands );

    // transform interpolant into AIG
    pGraph = Kit_TruthToGraph( puTruth, nFanins, p->vMem );
    pFunc = Kit_GraphToHop( (Hop_Man_t *)p->pNtk->pManFunc, pGraph );
    Kit_GraphFree( pGraph );
    // if (pFunc != nullptr) {
    //     cout << "nFanins " << nFanins << " ";
    //     cout << "truthtable ";
    //     int nWordsAll = Kit_TruthWordNum(nFanins);
    //     for (int i = 0; i < nWordsAll; ++i)
    //         cout << puTruth[i] << " ";
    //     cout << endl;
    // }
    return pFunc;
}


Kit_Graph_t * Ckt_TruthToGraph( unsigned * pTruth, int nVars, Vec_Int_t * vMemory )
{
    Kit_Graph_t * pGraph;
    int RetValue;
    // derive SOP
    RetValue = Ckt_TruthIsop( pTruth, nVars, vMemory, 1 ); // tried 1 and found not useful in "renode"
    if ( RetValue == -1 )
        return NULL;
    if ( Vec_IntSize(vMemory) > (1<<16) )
        return NULL;
//    printf( "Isop size = %d.\n", Vec_IntSize(vMemory) );
    assert( RetValue == 0 || RetValue == 1 );
    // derive factored form
    pGraph = Kit_SopFactor( vMemory, RetValue, nVars, vMemory );
    return pGraph;
}


int Ckt_TruthIsop( unsigned * puTruth, int nVars, Vec_Int_t * vMemory, int fTryBoth )
{
    Kit_Sop_t cRes, * pcRes = &cRes;
    Kit_Sop_t cRes2, * pcRes2 = &cRes2;
    unsigned * pResult;
    int RetValue = 0;
    assert( nVars >= 0 && nVars <= 16 );
    // if nVars < 5, make sure it does not depend on those vars
//    for ( i = nVars; i < 5; i++ )
//        assert( !Kit_TruthVarInSupport(puTruth, 5, i) );
    // prepare memory manager
    Vec_IntClear( vMemory );

#define KIT_ISOP_MEM_LIMIT  (1<<20)

    Vec_IntGrow( vMemory, KIT_ISOP_MEM_LIMIT );
    // compute ISOP for the direct polarity
    pResult = Ckt_TruthIsop_rec( puTruth, puTruth, nVars, pcRes, vMemory );
    if ( pcRes->nCubes == -1 )
    {
        vMemory->nSize = -1;
        return -1;
    }
    assert( Kit_TruthIsEqual( puTruth, pResult, nVars ) );
    if ( pcRes->nCubes == 0 || (pcRes->nCubes == 1 && pcRes->pCubes[0] == 0) )
    {
        vMemory->pArray[0] = 0;
        Vec_IntShrink( vMemory, pcRes->nCubes );
        return 0;
    }
    if ( fTryBoth )
    {
        // compute ISOP for the complemented polarity
        Kit_TruthNot( puTruth, puTruth, nVars );
        pResult = Ckt_TruthIsop_rec( puTruth, puTruth, nVars, pcRes2, vMemory );
        if ( pcRes2->nCubes >= 0 )
        {
            assert( Kit_TruthIsEqual( puTruth, pResult, nVars ) );
            if ( pcRes->nCubes > pcRes2->nCubes || (pcRes->nCubes == pcRes2->nCubes && pcRes->nLits > pcRes2->nLits) )
            {
                RetValue = 1;
                pcRes = pcRes2;
            }
        }
        Kit_TruthNot( puTruth, puTruth, nVars );
    }
//    printf( "%d ", vMemory->nSize );
    // move the cover representation to the beginning of the memory buffer
    memmove( vMemory->pArray, pcRes->pCubes, pcRes->nCubes * sizeof(unsigned) );
    Vec_IntShrink( vMemory, pcRes->nCubes );
    return RetValue;
}


unsigned * Ckt_TruthIsop_rec( unsigned * puOn, unsigned * puOnDc, int nVars, Kit_Sop_t * pcRes, Vec_Int_t * vStore )
{
    Kit_Sop_t cRes0, cRes1, cRes2;
    Kit_Sop_t * pcRes0 = &cRes0, * pcRes1 = &cRes1, * pcRes2 = &cRes2;
    unsigned * puRes0, * puRes1, * puRes2;
    unsigned * puOn0, * puOn1, * puOnDc0, * puOnDc1, * pTemp, * pTemp0, * pTemp1;
    int i, k, Var, nWords, nWordsAll;
//    assert( Kit_TruthIsImply( puOn, puOnDc, nVars ) );
    // allocate room for the resulting truth table
    nWordsAll = Kit_TruthWordNum( nVars );
    pTemp = Vec_IntFetch( vStore, nWordsAll );
    if ( pTemp == NULL )
    {
        pcRes->nCubes = -1;
        return NULL;
    }
    // check for constants
    if ( Kit_TruthIsConst0( puOn, nVars ) )
    {
        pcRes->nLits  = 0;
        pcRes->nCubes = 0;
        pcRes->pCubes = NULL;
        Kit_TruthClear( pTemp, nVars );
        return pTemp;
    }
    if ( Kit_TruthIsConst1( puOnDc, nVars ) )
    {
        pcRes->nLits  = 0;
        pcRes->nCubes = 1;
        pcRes->pCubes = Vec_IntFetch( vStore, 1 );
        if ( pcRes->pCubes == NULL )
        {
            pcRes->nCubes = -1;
            return NULL;
        }
        pcRes->pCubes[0] = 0;
        Kit_TruthFill( pTemp, nVars );
        return pTemp;
    }
    assert( nVars > 0 );
    // find the topmost var
    for ( Var = nVars-1; Var >= 0; Var-- )
        if ( Kit_TruthVarInSupport( puOn, nVars, Var ) ||
             Kit_TruthVarInSupport( puOnDc, nVars, Var ) )
             break;
    assert( Var >= 0 );
    // consider a simple case when one-word computation can be used
    if ( Var < 5 )
    {
        unsigned uRes = Ckt_TruthIsop5_rec( puOn[0], puOnDc[0], Var+1, pcRes, vStore );
        for ( i = 0; i < nWordsAll; i++ )
            pTemp[i] = uRes;
        return pTemp;
    }
    assert( Var >= 5 );
    nWords = Kit_TruthWordNum( Var );
    // cofactor
    puOn0   = puOn;    puOn1   = puOn + nWords;
    puOnDc0 = puOnDc;  puOnDc1 = puOnDc + nWords;
    pTemp0  = pTemp;   pTemp1  = pTemp + nWords;
    // solve for cofactors
    Kit_TruthSharp( pTemp0, puOn0, puOnDc1, Var );
    puRes0 = Ckt_TruthIsop_rec( pTemp0, puOnDc0, Var, pcRes0, vStore );
    if ( pcRes0->nCubes == -1 )
    {
        pcRes->nCubes = -1;
        return NULL;
    }
    Kit_TruthSharp( pTemp1, puOn1, puOnDc0, Var );
    puRes1 = Ckt_TruthIsop_rec( pTemp1, puOnDc1, Var, pcRes1, vStore );
    if ( pcRes1->nCubes == -1 )
    {
        pcRes->nCubes = -1;
        return NULL;
    }
    Kit_TruthSharp( pTemp0, puOn0, puRes0, Var );
    Kit_TruthSharp( pTemp1, puOn1, puRes1, Var );
    Kit_TruthOr( pTemp0, pTemp0, pTemp1, Var );
    Kit_TruthAnd( pTemp1, puOnDc0, puOnDc1, Var );
    puRes2 = Ckt_TruthIsop_rec( pTemp0, pTemp1, Var, pcRes2, vStore );
    if ( pcRes2->nCubes == -1 )
    {
        pcRes->nCubes = -1;
        return NULL;
    }
    // create the resulting cover
    pcRes->nLits  = pcRes0->nLits  + pcRes1->nLits  + pcRes2->nLits + pcRes0->nCubes + pcRes1->nCubes;
    pcRes->nCubes = pcRes0->nCubes + pcRes1->nCubes + pcRes2->nCubes;
    pcRes->pCubes = Vec_IntFetch( vStore, pcRes->nCubes );
    if ( pcRes->pCubes == NULL )
    {
        pcRes->nCubes = -1;
        return NULL;
    }
    k = 0;
    for ( i = 0; i < pcRes0->nCubes; i++ )
        pcRes->pCubes[k++] = pcRes0->pCubes[i] | (1 << ((Var<<1)+0));
    for ( i = 0; i < pcRes1->nCubes; i++ )
        pcRes->pCubes[k++] = pcRes1->pCubes[i] | (1 << ((Var<<1)+1));
    for ( i = 0; i < pcRes2->nCubes; i++ )
        pcRes->pCubes[k++] = pcRes2->pCubes[i];
    assert( k == pcRes->nCubes );
    // create the resulting truth table
    Kit_TruthOr( pTemp0, puRes0, puRes2, Var );
    Kit_TruthOr( pTemp1, puRes1, puRes2, Var );
    // copy the table if needed
    nWords <<= 1;
    for ( i = 1; i < nWordsAll/nWords; i++ )
        for ( k = 0; k < nWords; k++ )
            pTemp[i*nWords + k] = pTemp[k];
    // verify in the end
//    assert( Kit_TruthIsImply( puOn, pTemp, nVars ) );
//    assert( Kit_TruthIsImply( pTemp, puOnDc, nVars ) );
    return pTemp;
}


unsigned Ckt_TruthIsop5_rec( unsigned uOn, unsigned uOnDc, int nVars, Kit_Sop_t * pcRes, Vec_Int_t * vStore )
{
    unsigned uMasks[5] = { 0xAAAAAAAA, 0xCCCCCCCC, 0xF0F0F0F0, 0xFF00FF00, 0xFFFF0000 };
    Kit_Sop_t cRes0, cRes1, cRes2;
    Kit_Sop_t * pcRes0 = &cRes0, * pcRes1 = &cRes1, * pcRes2 = &cRes2;
    unsigned uOn0, uOn1, uOnDc0, uOnDc1, uRes0, uRes1, uRes2;
    int i, k, Var;
    assert( nVars <= 5 );
    assert( (uOn & ~uOnDc) == 0 );
    if ( uOn == 0 )
    {
        pcRes->nLits  = 0;
        pcRes->nCubes = 0;
        pcRes->pCubes = NULL;
        return 0;
    }
    if ( uOnDc == 0xFFFFFFFF )
    {
        pcRes->nLits  = 0;
        pcRes->nCubes = 1;
        pcRes->pCubes = Vec_IntFetch( vStore, 1 );
        if ( pcRes->pCubes == NULL )
        {
            pcRes->nCubes = -1;
            return 0;
        }
        pcRes->pCubes[0] = 0;
        return 0xFFFFFFFF;
    }
    assert( nVars > 0 );
    // find the topmost var
    for ( Var = nVars-1; Var >= 0; Var-- )
        if ( Kit_TruthVarInSupport( &uOn, 5, Var ) ||
             Kit_TruthVarInSupport( &uOnDc, 5, Var ) )
             break;
    assert( Var >= 0 );
    // cofactor
    uOn0   = uOn1   = uOn;
    uOnDc0 = uOnDc1 = uOnDc;
    Kit_TruthCofactor0( &uOn0, Var + 1, Var );
    Kit_TruthCofactor1( &uOn1, Var + 1, Var );
    Kit_TruthCofactor0( &uOnDc0, Var + 1, Var );
    Kit_TruthCofactor1( &uOnDc1, Var + 1, Var );
    // solve for cofactors
    uRes0 = Ckt_TruthIsop5_rec( uOn0 & ~uOnDc1, uOnDc0, Var, pcRes0, vStore );
    if ( pcRes0->nCubes == -1 )
    {
        pcRes->nCubes = -1;
        return 0;
    }
    uRes1 = Ckt_TruthIsop5_rec( uOn1 & ~uOnDc0, uOnDc1, Var, pcRes1, vStore );
    if ( pcRes1->nCubes == -1 )
    {
        pcRes->nCubes = -1;
        return 0;
    }
    uRes2 = Ckt_TruthIsop5_rec( (uOn0 & ~uRes0) | (uOn1 & ~uRes1), uOnDc0 & uOnDc1, Var, pcRes2, vStore );
    if ( pcRes2->nCubes == -1 )
    {
        pcRes->nCubes = -1;
        return 0;
    }
    // create the resulting cover
    pcRes->nLits  = pcRes0->nLits  + pcRes1->nLits  + pcRes2->nLits + pcRes0->nCubes + pcRes1->nCubes;
    pcRes->nCubes = pcRes0->nCubes + pcRes1->nCubes + pcRes2->nCubes;
    pcRes->pCubes = Vec_IntFetch( vStore, pcRes->nCubes );
    if ( pcRes->pCubes == NULL )
    {
        pcRes->nCubes = -1;
        return 0;
    }
    k = 0;
    for ( i = 0; i < pcRes0->nCubes; i++ )
        pcRes->pCubes[k++] = pcRes0->pCubes[i] | (1 << ((Var<<1)+0));
    for ( i = 0; i < pcRes1->nCubes; i++ )
        pcRes->pCubes[k++] = pcRes1->pCubes[i] | (1 << ((Var<<1)+1));
    for ( i = 0; i < pcRes2->nCubes; i++ )
        pcRes->pCubes[k++] = pcRes2->pCubes[i];
    assert( k == pcRes->nCubes );
    // derive the final truth table
    uRes2 |= (uRes0 & ~uMasks[Var]) | (uRes1 & uMasks[Var]);
//    assert( (uOn & ~uRes2) == 0 );
//    assert( (uRes2 & ~uOnDc) == 0 );
    return uRes2;
}


Vec_Ptr_t * Ckt_ComputeDivisors(Abc_Obj_t * pNode, int nLevDivMax, int nWinMax, int nFanoutsMax)
{
    Vec_Ptr_t * vCone, * vDivs;
    Abc_Obj_t * pObj, * pFanout, * pFanin;
    int k, f, m;
    int nDivsPlus = 0, nTrueSupp;

    // mark the TFI with the current trav ID
    Abc_NtkIncrementTravId( pNode->pNtk );
    vCone = Abc_MfsWinMarkTfi( pNode );

    // count the number of PIs
    nTrueSupp = 0;
    Vec_PtrForEachEntry( Abc_Obj_t *, vCone, pObj, k )
        nTrueSupp += Abc_ObjIsCi(pObj);
//    printf( "%d(%d) ", Vec_PtrSize(p->vSupp), m );

    // mark with the current trav ID those nodes that should not be divisors:
    // (1) the node and its TFO
    // (2) the MFFC of the node
    // (3) the node's fanins (these are treated as a special case)
    Abc_NtkIncrementTravId( pNode->pNtk );
    Abc_MfsWinSweepLeafTfo_rec( pNode, nLevDivMax );
//    Abc_MfsWinVisitMffc( pNode );
    Abc_ObjForEachFanin( pNode, pObj, k )
        Abc_NodeSetTravIdCurrent( pObj );

    // at this point the nodes are marked with two trav IDs:
    // nodes to be collected as divisors are marked with previous trav ID
    // nodes to be avoided as divisors are marked with current trav ID

    // start collecting the divisors
    vDivs = Vec_PtrAlloc( nWinMax );
    Vec_PtrForEachEntry( Abc_Obj_t *, vCone, pObj, k )
    {
        if ( !Abc_NodeIsTravIdPrevious(pObj) )
            continue;
        if ( (int)pObj->Level > nLevDivMax )
            continue;
        Vec_PtrPush( vDivs, pObj );
        if ( Vec_PtrSize(vDivs) >= nWinMax )
            break;
    }
    Vec_PtrFree( vCone );

    // explore the fanouts of already collected divisors
    if ( Vec_PtrSize(vDivs) < nWinMax )
    Vec_PtrForEachEntry( Abc_Obj_t *, vDivs, pObj, k )
    {
        // consider fanouts of this node
        Abc_ObjForEachFanout( pObj, pFanout, f )
        {
            // stop if there are too many fanouts
            if ( nFanoutsMax && f > nFanoutsMax )
                break;
            // skip nodes that are already added
            if ( Abc_NodeIsTravIdPrevious(pFanout) )
                continue;
            // skip nodes in the TFO or in the MFFC of node
            if ( Abc_NodeIsTravIdCurrent(pFanout) )
                continue;
            // skip COs
            if ( !Abc_ObjIsNode(pFanout) )
                continue;
            // skip nodes with large level
            if ( (int)pFanout->Level > nLevDivMax )
                continue;
            // skip nodes whose fanins are not divisors  -- here we skip more than we need to skip!!! (revise later)  August 7, 2009
            Abc_ObjForEachFanin( pFanout, pFanin, m )
                if ( !Abc_NodeIsTravIdPrevious(pFanin) )
                    break;
            if ( m < Abc_ObjFaninNum(pFanout) )
                continue;
            // make sure this divisor in not among the nodes
//            Vec_PtrForEachEntry( Abc_Obj_t *, p->vNodes, pFanin, m )
//                assert( pFanout != pFanin );
            // add the node to the divisors
            Vec_PtrPush( vDivs, pFanout );
            // Vec_PtrPushUnique( p->vNodes, pFanout );
            Abc_NodeSetTravIdPrevious( pFanout );
            nDivsPlus++;
            if ( Vec_PtrSize(vDivs) >= nWinMax )
                break;
        }
        if ( Vec_PtrSize(vDivs) >= nWinMax )
            break;
    }

    // sort the divisors by level in the increasing order
    // Vec_PtrSort( vDivs, (int (*)(void))Abc_NodeCompareLevelsIncrease );
    Vec_PtrSort( vDivs, (int (*)(const void *, const void *))Abc_NodeCompareLevelsIncrease );

    // add the fanins of the node
    Abc_ObjForEachFanin( pNode, pFanin, k )
        Vec_PtrPush( vDivs, pFanin );

    return vDivs;
}
