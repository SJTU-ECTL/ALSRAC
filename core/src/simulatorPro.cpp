#include "simulatorPro.h"


using namespace std;
using namespace boost;


Lac_Cand_t::Lac_Cand_t()
{
    this->pObj = nullptr;
    this->error = 1.0;
    this->pFunc = nullptr;
    this->vFanins = nullptr;
    estimation.clear();
}


Lac_Cand_t::Lac_Cand_t(Abc_Obj_t * pObj, Hop_Obj_t * pFunc, Vec_Ptr_t * vFanins)
{
    DASSERT(Abc_NtkIsAigLogic(pObj->pNtk));
    this->error = 1.0;
    this->pObj = pObj;
    this->pFunc = pFunc;
    this->vFanins = Vec_PtrDup(vFanins);
}


Lac_Cand_t::Lac_Cand_t(const Lac_Cand_t & lac)
{
    DASSERT(Abc_NtkIsAigLogic(lac.pObj->pNtk));
    this->error = lac.error;
    this->pObj = lac.pObj;
    this->pFunc = lac.pFunc;
    this->vFanins = Vec_PtrDup(lac.vFanins);
}


Lac_Cand_t::~Lac_Cand_t()
{
    if (vFanins != nullptr)
        Vec_PtrFree(vFanins);
}


void Lac_Cand_t::Print() const
{
    DASSERT(ExistResub(1.0));
    DASSERT(Abc_NtkIsAigLogic(pObj->pNtk));
    Ckt_PrintNodeFunc(pObj);
    Ckt_PrintHopFunc(pFunc, vFanins);
    // cout << "new error " << error << endl;
}


bool Lac_Cand_t::ExistResub(double metricBound) const
{
    DASSERT(metricBound >= 0.0 && metricBound <= 1.0);
    if (error <= metricBound && pObj != nullptr && pFunc != nullptr)
        return true;
    return false;
}


void Lac_Cand_t::UpdateBest(double errorNew, Abc_Obj_t * pObjNew, Hop_Obj_t * pFuncNew, Vec_Ptr_t * vFaninsNew)
{
    if (errorNew <= error) {
        error = errorNew;
        pObj = pObjNew;
        pFunc = pFuncNew;
        if (vFanins != nullptr) {
            Vec_PtrFree(vFanins);
            vFanins = nullptr;
        }
        vFanins = Vec_PtrDup(vFaninsNew);
    }
}


void Lac_Cand_t::UpdateBest(tVec & estNew, Abc_Obj_t * pObjNew, Hop_Obj_t * pFuncNew, Vec_Ptr_t * vFaninsNew)
{
    bool isUpdate = true;
    if (!estimation.empty()) {
        DASSERT(estNew.size() == estimation.size());
        int sz = static_cast <int> (estNew.size());
        int i = 0;
        for (i = sz - 1; i >= 0; --i) {
            if (estNew[i] > estimation[i]) {
                isUpdate = false;
                break;
            }
        }
        if (i == -1)
            isUpdate = false;
    }
    if (isUpdate) {
        error = 0.0;
        pObj = pObjNew;
        pFunc = pFuncNew;
        if (vFanins != nullptr) {
            Vec_PtrFree(vFanins);
            vFanins = nullptr;
        }
        vFanins = Vec_PtrDup(vFaninsNew);
        estimation.assign(estNew.begin(), estNew.end());
    }
}


Simulator_Pro_t::Simulator_Pro_t(Abc_Ntk_t * pNtk, int nFrame)
{
    DASSERT(Abc_NtkIsAigLogic(pNtk) || Abc_NtkIsSopLogic(pNtk), "network must be in aig or sop logic");
    this->pNtk = pNtk;
    this->nFrame = nFrame;
    this->nBlock = (nFrame % 64) ? ((nFrame >> 6) + 1) : (nFrame >> 6);
    this->nLastBlock = (nFrame % 64)? (nFrame % 64): 64;
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    maxId = -1;
    Abc_NtkForEachObj(pNtk, pObj, i)
        maxId = max(maxId, pObj->Id);
    this->values.resize(maxId + 1);
    this->tmpValues.resize(maxId + 1);
    Abc_NtkForEachObj(pNtk, pObj, i) {
        this->values[pObj->Id].resize(nBlock);
        this->tmpValues[pObj->Id].resize(nBlock);
    }
}


Simulator_Pro_t::~Simulator_Pro_t()
{
}


void Simulator_Pro_t::Input(unsigned seed)
{
    // uniform distribution
    random::uniform_int_distribution <uint64_t> unf;
    random::mt19937 engine(seed);
    variate_generator <random::mt19937, random::uniform_int_distribution <uint64_t> > randomPI(engine, unf);

    // primary inputs
    Abc_Obj_t * pObj = nullptr;
    int k = 0;
    Abc_NtkForEachPi(pNtk, pObj, k) {
        for (int i = 0; i < nBlock; ++i) {
            values[pObj->Id][i] = randomPI();
            tmpValues[pObj->Id][i] = values[pObj->Id][i];
        }
    }

    // constant nodes
    if (Abc_NtkIsAigLogic(pNtk)) {
        Abc_NtkForEachNode(pNtk, pObj, k) {
            Hop_Obj_t * pHopObj = static_cast <Hop_Obj_t *> (pObj->pData);
            Hop_Obj_t * pHopObjR = Hop_Regular(pHopObj);
            if (Hop_ObjIsConst1(pHopObjR)) {
                DASSERT(Hop_ObjFanin0(pHopObjR) == nullptr);
                DASSERT(Hop_ObjFanin1(pHopObjR) == nullptr);
                if (!Hop_IsComplement(pHopObj)) {
                    for (int i = 0; i < nBlock; ++i) {
                        values[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                        tmpValues[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                    }
                }
                else {
                    for (int i = 0; i < nBlock; ++i) {
                        values[pObj->Id][i] = 0;
                        tmpValues[pObj->Id][i] = 0;
                    }
                }
            }
        }
    }
    else if (Abc_NtkIsSopLogic(pNtk)) {
        Abc_NtkForEachNode(pNtk, pObj, k) {
            if (Abc_NodeIsConst1(pObj)) {
                for (int i = 0; i < nBlock; ++i) {
                    values[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                    tmpValues[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                }
            }
            else if (Abc_NodeIsConst0(pObj)) {
                for (int i = 0; i < nBlock; ++i) {
                    values[pObj->Id][i] = 0;
                    tmpValues[pObj->Id][i] = 0;
                }
            }
        }
    }
    else {
        DASSERT(0, "network must be in aig or sop logic");
    }
}


void Simulator_Pro_t::Input(string fileName)
{
    // primary inputs
    FILE * fp = fopen(fileName.c_str(), "r");
    char buf[1000];
    int cnt = 0;
    while (fgets(buf, sizeof(buf), fp) != nullptr) {
        DASSERT(static_cast <int>(strlen(buf)) == Abc_NtkPiNum(pNtk) + 3);
        int blockId = cnt >> 6;
        int bitId = cnt % 64;
        for (int i = 0; i < Abc_NtkPiNum(pNtk); ++i) {
            Abc_Obj_t * pObj = Abc_NtkPi(pNtk, i);
            if (buf[i + 2] == '1')
                Ckt_SetBit( values[pObj->Id][blockId], bitId );
            else
                Ckt_ResetBit( values[pObj->Id][blockId], bitId );
        }
        ++cnt;
        DASSERT(cnt <= nFrame);
    }
    DASSERT(cnt == nFrame);
    fclose(fp);

    // constant nodes
    DASSERT(Abc_NtkIsAigLogic(pNtk), "network is not in aig");
    Abc_Obj_t * pObj = nullptr;
    int k = 0;
    if (Abc_NtkIsAigLogic(pNtk)) {
        Abc_NtkForEachNode(pNtk, pObj, k) {
            Hop_Obj_t * pHopObj = static_cast <Hop_Obj_t *> (pObj->pData);
            Hop_Obj_t * pHopObjR = Hop_Regular(pHopObj);
            if (Hop_ObjIsConst1(pHopObjR)) {
                DASSERT(Hop_ObjFanin0(pHopObjR) == nullptr);
                DASSERT(Hop_ObjFanin1(pHopObjR) == nullptr);
                if (!Hop_IsComplement(pHopObj)) {
                    for (int i = 0; i < nBlock; ++i) {
                        values[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                        tmpValues[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                    }
                }
                else {
                    for (int i = 0; i < nBlock; ++i) {
                        values[pObj->Id][i] = 0;
                        tmpValues[pObj->Id][i] = 0;
                    }
                }
            }
        }
    }
    else if (Abc_NtkIsSopLogic(pNtk)) {
        Abc_NtkForEachNode(pNtk, pObj, k) {
            if (Abc_NodeIsConst1(pObj)) {
                for (int i = 0; i < nBlock; ++i) {
                    values[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                    tmpValues[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                }
            }
            else if (Abc_NodeIsConst0(pObj)) {
                for (int i = 0; i < nBlock; ++i) {
                    values[pObj->Id][i] = 0;
                    tmpValues[pObj->Id][i] = 0;
                }
            }
        }
    }
    else {
        DASSERT(0, "network must be in aig or sop logic");
    }
}


void Simulator_Pro_t::Simulate()
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Vec_Ptr_t * vNodes = Abc_NtkDfs(pNtk, 0);
    // internal nodes
    if (Abc_NtkIsAigLogic(pNtk)) {
        Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pObj, i)
            UpdateAigNode(pObj);
    }
    else if (Abc_NtkIsSopLogic(pNtk)) {
        Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pObj, i)
            UpdateSopNode(pObj);
    }
    else {
        DASSERT(0);
    }
    Vec_PtrFree(vNodes);
    // primary outputs
    Abc_NtkForEachPo(pNtk, pObj, i) {
        Abc_Obj_t * pFanin = Abc_ObjFanin0(pObj);
        values[pObj->Id].assign(values[pFanin->Id].begin(), values[pFanin->Id].end());
    }
}


void Simulator_Pro_t::SimulateCutNtks()
{
    DASSERT(Abc_NtkIsAigLogic(pNtk));

    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    bdCuts.resize(maxId + 1);
    Abc_NtkForEachNode(pNtk, pObj, i) {
        if (!Abc_NodeIsConst(pObj)) {
            // flip the node
            for (int j = 0; j < nBlock; ++j)
                tmpValues[pObj->Id][j] = ~values[pObj->Id][j];
            // simulate
            Abc_NtkIncrementTravId(pNtk);
            Abc_NodeSetTravIdCurrent(pObj);
            for (auto & pInner: cutNtks[pObj->Id])
                Abc_NodeSetTravIdCurrent(pInner);
            for (auto & pInner: cutNtks[pObj->Id])
                UpdateAigObjCutNtk(pInner);
            // get boolean difference from the node to its disjoint cuts
            int cutSize = static_cast <int >(djCuts[pObj->Id].size());
            bdCuts[pObj->Id].resize(cutSize);
            int j = 0;
            for (auto & pCutNode: djCuts[pObj->Id]) {
                bdCuts[pObj->Id][j].resize(nBlock);
                for (int k = 0; k < nBlock; ++k)
                    bdCuts[pObj->Id][j][k] = values[pCutNode->Id][k] ^ tmpValues[pCutNode->Id][k];
                ++j;
            }
        }
    }
}


void Simulator_Pro_t::SimulateResub(Abc_Obj_t * pOldObj, void * pResubFunc, Vec_Ptr_t * vResubFanins)
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Vec_Ptr_t * vNodes = Ckt_NtkDfsResub(pNtk, pOldObj, vResubFanins);
    // internal nodes
    if (Abc_NtkIsAigLogic(pNtk)) {
        Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pObj, i) {
            if (pObj != pOldObj)
                UpdateAigNodeResub(pObj, nullptr, nullptr);
            else
                UpdateAigNodeResub(pObj, static_cast <Hop_Obj_t *> (pResubFunc), vResubFanins);
        }
    }
    else if (Abc_NtkIsSopLogic(pNtk)) {
        Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pObj, i) {
            if (pObj != pOldObj)
                UpdateSopNodeResub(pObj, nullptr, nullptr);
            else
                UpdateSopNodeResub(pObj, static_cast <char *> (pResubFunc), vResubFanins);
        }
    }
    else {
        DASSERT(0);
    }
    // primary outputs
    Abc_NtkForEachPo(pNtk, pObj, i) {
        Abc_Obj_t * pFanin = Abc_ObjFanin0(pObj);
        tmpValues[pObj->Id].assign(tmpValues[pFanin->Id].begin(), tmpValues[pFanin->Id].end());
    }
    Vec_PtrFree(vNodes);
}


void Simulator_Pro_t::UpdateAigNode(Abc_Obj_t * pObj)
{
    Hop_Man_t * pMan = static_cast <Hop_Man_t *> (pNtk->pManFunc);
    Hop_Obj_t * pRoot = static_cast <Hop_Obj_t *> (pObj->pData);
    Hop_Obj_t * pRootR = Hop_Regular(pRoot);

    // skip constant node
    if (Hop_ObjIsConst1(pRootR))
        return;

    // get topological order of subnetwork in aig
    Vec_Ptr_t * vHopNodes = Hop_ManDfsNode(pMan, pRootR);

    // init internal hop nodes
    int maxHopId = -1;
    int i = 0;
    Hop_Obj_t * pHopObj = nullptr;
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    Hop_ManForEachPi(pMan, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    vector < tVec > interValues(maxHopId + 1);
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        interValues[pHopObj->Id].resize(nBlock);
    unordered_map <int, tVec *> hop2Val;
    Abc_Obj_t * pFanin = nullptr;
    Abc_ObjForEachFanin(pObj, pFanin, i)
        hop2Val[Hop_ManPi(pMan, i)->Id] = &values[pFanin->Id];

    // special case for inverter or buffer
    if (pRootR->Type == AIG_PI) {
        pFanin = Abc_ObjFanin0(pObj);
        values[pObj->Id].assign(values[pFanin->Id].begin(), values[pFanin->Id].end());
    }

    // simulate
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i) {
        DASSERT(Hop_ObjIsAnd(pHopObj));
        Hop_Obj_t * pHopFanin0 = Hop_ObjFanin0(pHopObj);
        Hop_Obj_t * pHopFanin1 = Hop_ObjFanin1(pHopObj);
        DASSERT(!Hop_ObjIsConst1(pHopFanin0));
        DASSERT(!Hop_ObjIsConst1(pHopFanin1));
        tVec & val0 = Hop_ObjIsPi(pHopFanin0) ? *hop2Val[pHopFanin0->Id] : interValues[pHopFanin0->Id];
        tVec & val1 = Hop_ObjIsPi(pHopFanin1) ? *hop2Val[pHopFanin1->Id] : interValues[pHopFanin1->Id];
        tVec & out = (pHopObj == pRootR) ? values[pObj->Id] : interValues[pHopObj->Id];
        bool isFanin0C = Hop_ObjFaninC0(pHopObj);
        bool isFanin1C = Hop_ObjFaninC1(pHopObj);
        if (!isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & val1[j];
        }
        else if (!isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & (~val1[j]);
        }
        else if (isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = (~val0[j]) & val1[j];
        }
        else if (isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = ~(val0[j] | val1[j]);
        }
    }

    // complement
    if (Hop_IsComplement(pRoot)) {
        for (int j = 0; j < nBlock; ++j)
            values[pObj->Id][j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }

    // recycle memory
    Vec_PtrFree(vHopNodes);
}


void Simulator_Pro_t::UpdateSopNode(Abc_Obj_t * pObj)
{
    DASSERT(Abc_ObjIsNode(pObj));
    // skip constant node
    if (Abc_NodeIsConst(pObj))
        return;
    // update sop
    char * pSop = static_cast <char *> (pObj->pData);
    int nVars = Abc_SopGetVarNum(pSop);
    vector <uint64_t> product(nBlock);
    for (char * pCube = pSop; *pCube; pCube += nVars + 3) {
        bool isFirst = true;
        for (int i = 0; pCube[i] != ' '; i++) {
            Abc_Obj_t * pFanin = Abc_ObjFanin(pObj, i);
            switch (pCube[i]) {
                case '-':
                    continue;
                    break;
                case '0':
                    if (isFirst) {
                        isFirst = false;
                        for (int k = 0; k < nBlock; ++k)
                            product[k] = ~values[pFanin->Id][k];
                    }
                    else {
                        for (int k = 0; k < nBlock; ++k)
                            product[k] &= ~values[pFanin->Id][k];
                    }
                    break;
                case '1':
                    if (isFirst) {
                        isFirst = false;
                        for (int k = 0; k < nBlock; ++k)
                            product[k] = values[pFanin->Id][k];
                    }
                    else {
                        for (int k = 0; k < nBlock; ++k)
                            product[k] &= values[pFanin->Id][k];
                    }
                    break;
                default:
                    DASSERT(0);
            }
        }
        if (isFirst) {
            isFirst = false;
            for (int k = 0; k < nBlock; ++k)
                product[k] = static_cast <uint64_t> (ULLONG_MAX);
        }
        DASSERT(!isFirst);
        if (pCube == pSop) {
            values[pObj->Id].assign(product.begin(), product.end());
        }
        else {
            for (int k = 0; k < nBlock; ++k)
                values[pObj->Id][k] |= product[k];
        }
    }

    // complement
    if (Abc_SopIsComplement(pSop)) {
        for (int j = 0; j < nBlock; ++j)
            values[pObj->Id][j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }
}


void Simulator_Pro_t::UpdateAigObjCutNtk(Abc_Obj_t * pObj)
{
    DASSERT(!Abc_ObjIsPi(pObj));
    DASSERT(!Abc_NodeIsConst(pObj));
    if (Abc_ObjIsPo(pObj)) {
        Abc_Obj_t * pDriver = Abc_ObjFanin0(pObj);
        if (Abc_NodeIsTravIdCurrent(pDriver))
            tmpValues[pObj->Id].assign(tmpValues[pDriver->Id].begin(), tmpValues[pDriver->Id].end());
        else
            tmpValues[pObj->Id].assign(values[pDriver->Id].begin(), values[pDriver->Id].end());
        return;
    }

    Hop_Man_t * pMan = static_cast <Hop_Man_t *> (pNtk->pManFunc);
    Hop_Obj_t * pRoot = static_cast <Hop_Obj_t *> (pObj->pData);
    Hop_Obj_t * pRootR = Hop_Regular(pRoot);

    // skip constant node
    if (Hop_ObjIsConst1(pRootR))
        return;

    // get topological order of subnetwork in aig
    Vec_Ptr_t * vHopNodes = Hop_ManDfsNode(pMan, pRootR);

    // init internal hop nodes
    int maxHopId = -1;
    int i = 0;
    Hop_Obj_t * pHopObj = nullptr;
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    Hop_ManForEachPi(pMan, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    vector < tVec > interValues(maxHopId + 1);
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        interValues[pHopObj->Id].resize(nBlock);
    unordered_map <int, tVec *> hop2Val;
    Abc_Obj_t * pFanin = nullptr;
    Abc_ObjForEachFanin(pObj, pFanin, i) {
        if (Abc_NodeIsTravIdCurrent(pFanin))
            hop2Val[Hop_ManPi(pMan, i)->Id] = &tmpValues[pFanin->Id];
        else
            hop2Val[Hop_ManPi(pMan, i)->Id] = &values[pFanin->Id];
    }

    // special case for inverter or buffer
    if (pRootR->Type == AIG_PI) {
        pFanin = Abc_ObjFanin0(pObj);
        if (Abc_NodeIsTravIdCurrent(pFanin))
            tmpValues[pObj->Id].assign(tmpValues[pFanin->Id].begin(), tmpValues[pFanin->Id].end());
        else
            tmpValues[pObj->Id].assign(values[pFanin->Id].begin(), values[pFanin->Id].end());
    }

    // simulate
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i) {
        DASSERT(Hop_ObjIsAnd(pHopObj));
        Hop_Obj_t * pHopFanin0 = Hop_ObjFanin0(pHopObj);
        Hop_Obj_t * pHopFanin1 = Hop_ObjFanin1(pHopObj);
        DASSERT(!Hop_ObjIsConst1(pHopFanin0));
        DASSERT(!Hop_ObjIsConst1(pHopFanin1));
        tVec & val0 = Hop_ObjIsPi(pHopFanin0) ? *hop2Val[pHopFanin0->Id] : interValues[pHopFanin0->Id];
        tVec & val1 = Hop_ObjIsPi(pHopFanin1) ? *hop2Val[pHopFanin1->Id] : interValues[pHopFanin1->Id];
        tVec & out = (pHopObj == pRootR) ? tmpValues[pObj->Id] : interValues[pHopObj->Id];
        bool isFanin0C = Hop_ObjFaninC0(pHopObj);
        bool isFanin1C = Hop_ObjFaninC1(pHopObj);
        if (!isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & val1[j];
        }
        else if (!isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & (~val1[j]);
        }
        else if (isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = (~val0[j]) & val1[j];
        }
        else if (isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = ~(val0[j] | val1[j]);
        }
    }

    // complement
    if (Hop_IsComplement(pRoot)) {
        for (int j = 0; j < nBlock; ++j)
            tmpValues[pObj->Id][j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }

    // recycle memory
    Vec_PtrFree(vHopNodes);
}


void Simulator_Pro_t::UpdateSopObjCutNtk(Abc_Obj_t * pObj)
{
    DASSERT(!Abc_ObjIsPi(pObj));
    DASSERT(!Abc_NodeIsConst(pObj));
    if (Abc_ObjIsPo(pObj)) {
        Abc_Obj_t * pDriver = Abc_ObjFanin0(pObj);
        if (Abc_NodeIsTravIdCurrent(pDriver))
            tmpValues[pObj->Id].assign(tmpValues[pDriver->Id].begin(), tmpValues[pDriver->Id].end());
        else
            tmpValues[pObj->Id].assign(values[pDriver->Id].begin(), values[pDriver->Id].end());
        return;
    }
    // update sop
    char * pSop = static_cast <char *> (pObj->pData);
    int nVars = Abc_SopGetVarNum(pSop);
    vector <uint64_t> product(nBlock);
    for (char * pCube = pSop; *pCube; pCube += nVars + 3) {
        bool isFirst = true;
        for (int i = 0; pCube[i] != ' '; i++) {
            Abc_Obj_t * pFanin = Abc_ObjFanin(pObj, i);
            tVec & valueFi = Abc_NodeIsTravIdCurrent(pFanin)? tmpValues[pFanin->Id]: values[pFanin->Id];
            switch (pCube[i]) {
                case '-':
                    continue;
                    break;
                case '0':
                    if (isFirst) {
                        isFirst = false;
                        for (int k = 0; k < nBlock; ++k)
                            product[k] = ~valueFi[k];
                    }
                    else {
                        for (int k = 0; k < nBlock; ++k)
                            product[k] &= ~valueFi[k];
                    }
                    break;
                case '1':
                    if (isFirst) {
                        isFirst = false;
                        for (int k = 0; k < nBlock; ++k)
                            product[k] = valueFi[k];
                    }
                    else {
                        for (int k = 0; k < nBlock; ++k)
                            product[k] &= valueFi[k];
                    }
                    break;
                default:
                    DASSERT(0);
            }
        }
        if (isFirst) {
            isFirst = false;
            for (int k = 0; k < nBlock; ++k)
                product[k] = static_cast <uint64_t> (ULLONG_MAX);
        }
        DASSERT(!isFirst);
        if (pCube == pSop) {
            tmpValues[pObj->Id].assign(product.begin(), product.end());
        }
        else {
            for (int k = 0; k < nBlock; ++k)
                tmpValues[pObj->Id][k] |= product[k];
        }
    }

    // complement
    if (Abc_SopIsComplement(pSop)) {
        for (int j = 0; j < nBlock; ++j)
            tmpValues[pObj->Id][j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }
}


void Simulator_Pro_t::UpdateAigNodeResub(Abc_Obj_t * pObj, Hop_Obj_t * pResubFunc, Vec_Ptr_t * vResubFanins)
{
    Hop_Man_t * pMan = static_cast <Hop_Man_t *> (pNtk->pManFunc);
    Hop_Obj_t * pRoot = (pResubFunc == nullptr) ? static_cast <Hop_Obj_t *> (pObj->pData): pResubFunc;
    Hop_Obj_t * pRootR = Hop_Regular(pRoot);

    // update constant node
    if (pResubFunc == nullptr) {
        if (pRootR->Type == AIG_CONST1)
            return;
    }
    else {
        if (Hop_ObjIsConst1(pRootR)) {
            DASSERT(Hop_ObjFanin0(pRootR) == nullptr);
            DASSERT(Hop_ObjFanin1(pRootR) == nullptr);
            if (!Hop_IsComplement(pRoot)) {
                for (int i = 0; i < nBlock; ++i)
                    tmpValues[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
                return;
            }
            else {
                for (int i = 0; i < nBlock; ++i)
                    tmpValues[pObj->Id][i] = 0;
                return;
            }
        }
    }

    // get topological order of subnetwork in aig
    Vec_Ptr_t * vHopNodes = Hop_ManDfsNode(pMan, pRootR);

    // init internal hop nodes
    int maxHopId = -1;
    int i = 0;
    Hop_Obj_t * pHopObj = nullptr;
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    Hop_ManForEachPi(pMan, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    vector < tVec > interValues(maxHopId + 1);
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        interValues[pHopObj->Id].resize(nBlock);
    unordered_map <int, tVec *> hop2Val;
    Abc_Obj_t * pFanin = nullptr;
    if (pResubFunc != nullptr) {
        Vec_PtrForEachEntry(Abc_Obj_t *, vResubFanins, pFanin, i) {
            hop2Val[Hop_ManPi(pMan, i)->Id] = &tmpValues[pFanin->Id];
        }
    }
    else {
        Abc_ObjForEachFanin(pObj, pFanin, i)
            hop2Val[Hop_ManPi(pMan, i)->Id] = &tmpValues[pFanin->Id];
    }

    // special case for inverter or buffer
    if (pRootR->Type == AIG_PI) {
        DASSERT(Vec_PtrSize(vHopNodes) == 0);
        if (pResubFunc == nullptr)
            pFanin = Abc_ObjFanin0(pObj);
        else {
            pFanin = static_cast <Abc_Obj_t *> (Vec_PtrEntry(vResubFanins, 0));
        }
        tmpValues[pObj->Id].assign(tmpValues[pFanin->Id].begin(), tmpValues[pFanin->Id].end());
    }

    // simulate
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i) {
        DASSERT(Hop_ObjIsAnd(pHopObj));
        Hop_Obj_t * pHopFanin0 = Hop_ObjFanin0(pHopObj);
        Hop_Obj_t * pHopFanin1 = Hop_ObjFanin1(pHopObj);
        DASSERT(!Hop_ObjIsConst1(pHopFanin0));
        DASSERT(!Hop_ObjIsConst1(pHopFanin1));
        tVec & val0 = Hop_ObjIsPi(pHopFanin0) ? *hop2Val[pHopFanin0->Id] : interValues[pHopFanin0->Id];
        tVec & val1 = Hop_ObjIsPi(pHopFanin1) ? *hop2Val[pHopFanin1->Id] : interValues[pHopFanin1->Id];
        tVec & out = (pHopObj == pRootR) ? tmpValues[pObj->Id] : interValues[pHopObj->Id];
        bool isFanin0C = Hop_ObjFaninC0(pHopObj);
        bool isFanin1C = Hop_ObjFaninC1(pHopObj);
        if (!isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & val1[j];
        }
        else if (!isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & (~val1[j]);
        }
        else if (isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = (~val0[j]) & val1[j];
        }
        else if (isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = ~(val0[j] | val1[j]);
        }
    }

    // complement
    if (Hop_IsComplement(pRoot)) {
        for (int j = 0; j < nBlock; ++j)
            tmpValues[pObj->Id][j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }

    // recycle memory
    Vec_PtrFree(vHopNodes);
}


void Simulator_Pro_t::UpdateAigNodeResub(IN Abc_Obj_t * pObj, IN Hop_Obj_t * pResubFunc, IN Vec_Ptr_t * vResubFanins, OUT tVec & outValue)
{
    DASSERT(pObj != nullptr);
    DASSERT(!Abc_NodeIsConst(pObj));
    DASSERT(pObj->pNtk == pNtk);
    DASSERT(pResubFunc != nullptr);
    DASSERT(static_cast <int> (outValue.size()) == nBlock);

    Hop_Man_t * pMan = static_cast <Hop_Man_t *> (pNtk->pManFunc);
    Hop_Obj_t * pRoot = pResubFunc;
    Hop_Obj_t * pRootR = Hop_Regular(pRoot);

    // update constant node
    if (Hop_ObjIsConst1(pRootR)) {
        DASSERT(Hop_ObjFanin0(pRootR) == nullptr);
        DASSERT(Hop_ObjFanin1(pRootR) == nullptr);
        if (!Hop_IsComplement(pRoot)) {
            for (int i = 0; i < nBlock; ++i)
                outValue[i] = static_cast <uint64_t> (ULLONG_MAX);
            return;
        }
        else {
            for (int i = 0; i < nBlock; ++i)
                outValue[i] = 0;
            return;
        }
    }

    // get topological order of subnetwork in aig
    Vec_Ptr_t * vHopNodes = Hop_ManDfsNode(pMan, pRootR);

    // init internal hop nodes
    int maxHopId = -1;
    int i = 0;
    Hop_Obj_t * pHopObj = nullptr;
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    Hop_ManForEachPi(pMan, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    vector < tVec > interValues(maxHopId + 1);
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        interValues[pHopObj->Id].resize(nBlock);
    unordered_map <int, tVec *> hop2Val;
    Abc_Obj_t * pFanin = nullptr;
    Vec_PtrForEachEntry(Abc_Obj_t *, vResubFanins, pFanin, i) {
        hop2Val[Hop_ManPi(pMan, i)->Id] = &values[pFanin->Id];
    }

    // special case for inverter or buffer
    if (pRootR->Type == AIG_PI) {
        DASSERT(Vec_PtrSize(vHopNodes) == 0);
        if (pResubFunc == nullptr)
            pFanin = Abc_ObjFanin0(pObj);
        else {
            pFanin = static_cast <Abc_Obj_t *> (Vec_PtrEntry(vResubFanins, 0));
        }
        outValue.assign(values[pFanin->Id].begin(), values[pFanin->Id].end());
    }

    // simulate
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i) {
        DASSERT(Hop_ObjIsAnd(pHopObj));
        Hop_Obj_t * pHopFanin0 = Hop_ObjFanin0(pHopObj);
        Hop_Obj_t * pHopFanin1 = Hop_ObjFanin1(pHopObj);
        DASSERT(!Hop_ObjIsConst1(pHopFanin0));
        DASSERT(!Hop_ObjIsConst1(pHopFanin1));
        tVec & val0 = Hop_ObjIsPi(pHopFanin0) ? *hop2Val[pHopFanin0->Id] : interValues[pHopFanin0->Id];
        tVec & val1 = Hop_ObjIsPi(pHopFanin1) ? *hop2Val[pHopFanin1->Id] : interValues[pHopFanin1->Id];
        tVec & out = (pHopObj == pRootR) ? outValue : interValues[pHopObj->Id];
        bool isFanin0C = Hop_ObjFaninC0(pHopObj);
        bool isFanin1C = Hop_ObjFaninC1(pHopObj);
        if (!isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & val1[j];
        }
        else if (!isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = val0[j] & (~val1[j]);
        }
        else if (isFanin0C && !isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = (~val0[j]) & val1[j];
        }
        else if (isFanin0C && isFanin1C) {
            for (int j = 0; j < nBlock; ++j)
                out[j] = ~(val0[j] | val1[j]);
        }
    }

    // complement
    if (Hop_IsComplement(pRoot)) {
        for (int j = 0; j < nBlock; ++j)
            outValue[j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }

    // recycle memory
    Vec_PtrFree(vHopNodes);
}


void Simulator_Pro_t::UpdateSopNodeResub(Abc_Obj_t * pObj, char * pResubFunc, Vec_Ptr_t * vResubFanins)
{
    DASSERT(Abc_ObjIsNode(pObj));
    DASSERT(pObj->pNtk == pNtk);
    // update constant node
    if (pResubFunc == nullptr) {
        if (Abc_NodeIsConst(pObj))
            return;
    }
    else {
        if (Abc_SopIsConst1(pResubFunc)) {
            for (int i = 0; i < nBlock; ++i)
                tmpValues[pObj->Id][i] = static_cast <uint64_t> (ULLONG_MAX);
            return;
        }
        else if (Abc_SopIsConst0(pResubFunc)) {
            for (int i = 0; i < nBlock; ++i)
                tmpValues[pObj->Id][i] = 0;
            return;
        }
    }

    // simulate
    char * pSop = (pResubFunc != nullptr)? pResubFunc: static_cast <char *> (pObj->pData);
    int nVars = Abc_SopGetVarNum(pSop);
    vector <uint64_t> product(nBlock);
    if (pResubFunc != nullptr)
        DASSERT(Vec_PtrSize(vResubFanins) == nVars);
    else
        DASSERT(Abc_ObjFaninNum(pObj) == nVars);
    for (char * pCube = pSop; *pCube; pCube += nVars + 3) {
        bool isFirst = true;
        for (int i = 0; pCube[i] != ' '; i++) {
            Abc_Obj_t * pFanin = (pResubFunc != nullptr)? static_cast <Abc_Obj_t *> (Vec_PtrEntry(vResubFanins, i)): Abc_ObjFanin(pObj, i);
            switch (pCube[i]) {
                case '-':
                    continue;
                    break;
                case '0':
                    if (isFirst) {
                        isFirst = false;
                        for (int k = 0; k < nBlock; ++k)
                            product[k] = ~tmpValues[pFanin->Id][k];
                    }
                    else {
                        for (int k = 0; k < nBlock; ++k)
                            product[k] &= ~tmpValues[pFanin->Id][k];
                    }
                    break;
                case '1':
                    if (isFirst) {
                        isFirst = false;
                        for (int k = 0; k < nBlock; ++k)
                            product[k] = tmpValues[pFanin->Id][k];
                    }
                    else {
                        for (int k = 0; k < nBlock; ++k)
                            product[k] &= tmpValues[pFanin->Id][k];
                    }
                    break;
                default:
                    DASSERT(0);
            }
        }
        if (isFirst) {
            isFirst = false;
            for (int k = 0; k < nBlock; ++k)
                product[k] = static_cast <uint64_t> (ULLONG_MAX);
        }
        DASSERT(!isFirst);
        if (pCube == pSop) {
            tmpValues[pObj->Id].assign(product.begin(), product.end());
        }
        else {
            for (int k = 0; k < nBlock; ++k)
                tmpValues[pObj->Id][k] |= product[k];
        }
    }

    // complement
    if (Abc_SopIsComplement(pSop)) {
        for (int j = 0; j < nBlock; ++j)
            tmpValues[pObj->Id][j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }
}


void Simulator_Pro_t::UpdateSopNodeResub(Abc_Obj_t * pObj, char * pResubFunc, Vec_Ptr_t * vResubFanins, tVec & outValues)
{
    DASSERT(Abc_ObjIsNode(pObj));
    DASSERT(pObj->pNtk == pNtk);
    DASSERT(pResubFunc != nullptr);
    // update constant node
    if (Abc_SopIsConst1(pResubFunc)) {
        for (int i = 0; i < nBlock; ++i)
            outValues[i] = static_cast <uint64_t> (ULLONG_MAX);
        return;
    }
    else if (Abc_SopIsConst0(pResubFunc)) {
        for (int i = 0; i < nBlock; ++i)
            outValues[i] = 0;
        return;
    }

    // simulate
    char * pSop = (pResubFunc != nullptr)? pResubFunc: static_cast <char *> (pObj->pData);
    int nVars = Abc_SopGetVarNum(pSop);
    vector <uint64_t> product(nBlock);
    if (pResubFunc != nullptr)
        DASSERT(Vec_PtrSize(vResubFanins) == nVars);
    else
        DASSERT(Abc_ObjFaninNum(pObj) == nVars);
    for (char * pCube = pSop; *pCube; pCube += nVars + 3) {
        bool isFirst = true;
        for (int i = 0; pCube[i] != ' '; i++) {
            Abc_Obj_t * pFanin = (pResubFunc != nullptr)? static_cast <Abc_Obj_t *> (Vec_PtrEntry(vResubFanins, i)): Abc_ObjFanin(pObj, i);
            switch (pCube[i]) {
                case '-':
                    continue;
                    break;
                case '0':
                    if (isFirst) {
                        isFirst = false;
                        for (int k = 0; k < nBlock; ++k)
                            product[k] = ~values[pFanin->Id][k];
                    }
                    else {
                        for (int k = 0; k < nBlock; ++k)
                            product[k] &= ~values[pFanin->Id][k];
                    }
                    break;
                case '1':
                    if (isFirst) {
                        isFirst = false;
                        for (int k = 0; k < nBlock; ++k)
                            product[k] = values[pFanin->Id][k];
                    }
                    else {
                        for (int k = 0; k < nBlock; ++k)
                            product[k] &= values[pFanin->Id][k];
                    }
                    break;
                default:
                    DASSERT(0);
            }
        }
        if (isFirst) {
            isFirst = false;
            for (int k = 0; k < nBlock; ++k)
                product[k] = static_cast <uint64_t> (ULLONG_MAX);
        }
        DASSERT(!isFirst);
        if (pCube == pSop) {
            outValues.assign(product.begin(), product.end());
        }
        else {
            for (int k = 0; k < nBlock; ++k)
                outValues[k] |= product[k];
        }
    }

    // complement
    if (Abc_SopIsComplement(pSop)) {
        for (int j = 0; j < nBlock; ++j)
            outValues[j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }
}


multiprecision::int256_t Simulator_Pro_t::GetInput(int lsb, int msb, int frameId) const
{
    DASSERT(lsb >= 0 && msb < Abc_NtkPiNum(pNtk));
    DASSERT(frameId <= nFrame);
    DASSERT(lsb <= msb && msb - lsb <= 256);
    multiprecision::int256_t ret(0);
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    for (int k = msb; k >= lsb; --k) {
        Abc_Obj_t * pObj = Abc_NtkPi(pNtk, k);
        if (GetValue(pObj, blockId, bitId))
            ++ret;
        ret <<= 1;
    }
    return ret;
}


multiprecision::int256_t Simulator_Pro_t::GetOutput(int lsb, int msb, int frameId, bool isTmpValue) const
{
    DASSERT(lsb >= 0 && msb < Abc_NtkPoNum(pNtk));
    DASSERT(frameId <= nFrame);
    DASSERT(lsb <= msb && msb - lsb <= 256);
    multiprecision::int256_t ret(0);
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    for (int k = msb; k >= lsb; --k) {
        ret <<= 1;
        Abc_Obj_t * pObj = Abc_NtkPo(pNtk, k);
        if (!isTmpValue) {
            if (GetValue(pObj, blockId, bitId))
                ++ret;
        }
        else {
            if (GetTmpValue(pObj, blockId, bitId))
                ++ret;
        }
    }
    return ret;
}


void Simulator_Pro_t::PrintInputStream(int frameId) const
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    Abc_NtkForEachPi(pNtk, pObj, i)
        cout << GetValue(pObj, blockId, bitId);
    cout << endl;
}


void Simulator_Pro_t::PrintOutputStream(int frameId, bool isReverse) const
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    if (!isReverse) {
        Abc_NtkForEachPo(pNtk, pObj, i)
            cout << GetValue(pObj, blockId, bitId);
    }
    else {
        for (int i = Abc_NtkPoNum(pNtk) - 1; i >= 0; --i)
            cout << GetValue(Abc_NtkPo(pNtk, i), blockId, bitId);
    }
    cout << endl;
}


void Simulator_Pro_t::BuildCutNtks()
{
    DASSERT(cutNtks.empty());
    // init
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    cutNtks.resize(maxId + 1);
    djCuts.resize(maxId + 1);
    int sinkLen = (Abc_NtkPoNum(pNtk) >> 6) + 1;
    sinks.resize(maxId + 1);
    Abc_NtkForEachObj(pNtk, pObj, i)
        sinks[pObj->Id].resize(sinkLen, 0);
    topoIds.resize(maxId + 1);
    Vec_Ptr_t * vNodes = Abc_NtkDfs(pNtk, 0);
    Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pObj, i)
        topoIds[pObj->Id] = i;
    int topoId = -1;
    Abc_NtkForEachPi(pNtk, pObj, i)
        topoIds[pObj->Id] = topoId--;
    topoId = Vec_PtrSize(vNodes);
    Abc_NtkForEachPo(pNtk, pObj, i)
        topoIds[pObj->Id] = topoId++;

    // update sink POs
    Abc_NtkForEachPo(pNtk, pObj, i)
        Ckt_SetBit(sinks[pObj->Id][i >> 6], i);
    Vec_PtrForEachEntryReverse(Abc_Obj_t *, vNodes, pObj, i) {
        Abc_Obj_t * pFanout = nullptr;
        int j = 0;
        Abc_ObjForEachFanout(pObj, pFanout, j) {
            for (int k = 0; k < sinkLen; ++k)
                sinks[pObj->Id][k] |= sinks[pFanout->Id][k];
        }
    }

    // collect cut networks
    Abc_NtkForEachNode(pNtk, pObj, i) {
        if (Abc_NodeIsConst(pObj))
            continue;
        Abc_NtkIncrementTravId(pNtk);
        FindDisjointCut(pObj, djCuts[pObj->Id]);
        Abc_Obj_t * pNode = nullptr;
        int j = 0;
        Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pNode, j) {
            if (Abc_NodeIsTravIdCurrent(pNode))
                cutNtks[pObj->Id].emplace_back(pNode);
        }
        Abc_NtkForEachPo(pNtk, pNode, j) {
            if (Abc_NodeIsTravIdCurrent(pNode))
                cutNtks[pObj->Id].emplace_back(pNode);
        }
    }

    // clean up
    Vec_PtrFree(vNodes);
}


void Simulator_Pro_t::BuildAppCutNtks()
{
    DASSERT(cutNtks.empty());
    DASSERT(djCuts.empty());
    cutNtks.resize(maxId + 1);
    djCuts.resize(maxId + 1);
    // collect cut networks
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Abc_NtkForEachNode(pNtk, pObj, i) {
        if (Abc_NodeIsConst(pObj))
            continue;
        Abc_Obj_t * pFanout = nullptr;
        int j = 0;
        Abc_ObjForEachFanout(pObj, pFanout, j) {
            djCuts[pObj->Id].emplace_back(pFanout);
            cutNtks[pObj->Id].emplace_back(pFanout);
        }
    }
}


void Simulator_Pro_t::FindDisjointCut(Abc_Obj_t * pObj, list <Abc_Obj_t *> & djCut)
{
    djCut.clear();
    ExpandCut(pObj, djCut);
    Abc_Obj_t * pObjExpd = nullptr;
    while ((pObjExpd = ExpandWhich(djCut)) != nullptr)
        ExpandCut(pObjExpd, djCut);
}


void Simulator_Pro_t::ExpandCut(Abc_Obj_t * pObj, list <Abc_Obj_t *> & djCut)
{
    Abc_Obj_t * pFanout = nullptr;
    int i = 0;
    Abc_ObjForEachFanout(pObj, pFanout, i) {
        if (!Abc_NodeIsTravIdCurrent(pFanout)) {
            if ( Abc_ObjFanoutNum(pFanout) || Abc_ObjIsPo(pFanout) ) {
                Abc_NodeSetTravIdCurrent(pFanout);
                djCut.emplace_back(pFanout);
            }
        }
    }
}


Abc_Obj_t * Simulator_Pro_t::ExpandWhich(list <Abc_Obj_t *> & djCut)
{
    for (auto ppAbcObj1 = djCut.begin(); ppAbcObj1 != djCut.end(); ++ppAbcObj1) {
        auto ppAbcObj2 = ppAbcObj1;
        ++ppAbcObj2;
        for (; ppAbcObj2 != djCut.end(); ++ppAbcObj2) {
            uint32_t sinkLen = sinks[(*ppAbcObj1)->Id].size();
            DASSERT( sinkLen == sinks[(*ppAbcObj2)->Id].size() );
            DASSERT( topoIds[(*ppAbcObj1)->Id] != topoIds[(*ppAbcObj2)->Id] );
            for (int i = 0; i < static_cast <int> (sinkLen); ++i) {
                if (sinks[(*ppAbcObj1)->Id][i] & sinks[(*ppAbcObj2)->Id][i]) {
                    Abc_Obj_t * pRet = nullptr;
                    if ( topoIds[(*ppAbcObj1)->Id] < topoIds[(*ppAbcObj2)->Id] ) {
                        pRet = *ppAbcObj1;
                        djCut.erase(ppAbcObj1);
                    }
                    else {
                        pRet = *ppAbcObj2;
                        djCut.erase(ppAbcObj2);
                    }
                    return pRet;
                }
            }
        }
    }
    return nullptr;
}


void Simulator_Pro_t::UpdateBoolDiff(IN Abc_Obj_t * pPo, IN Vec_Ptr_t * vNodes, INOUT vector <tVec> & bds)
{
    DASSERT(pPo->pNtk == pNtk);
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Abc_NtkForEachPo(pNtk, pObj, i) {
        if (pObj == pPo) {
            for (int j = 0; j < nBlock; ++j)
                bds[pObj->Id][j] = static_cast <uint64_t> (ULLONG_MAX);
        }
        else {
            for (int j = 0; j < nBlock; ++j)
                bds[pObj->Id][j] = 0;
        }
    }
    Vec_PtrForEachEntryReverse(Abc_Obj_t *, vNodes, pObj, i) {
        if (Abc_NodeIsConst(pObj))
            continue;
        for (int j = 0; j < nBlock; ++j)
            bds[pObj->Id][j] = 0;
        int k = 0;
        for (auto & pCut: djCuts[pObj->Id]) {
            for (int j = 0; j < nBlock; ++j)
                bds[pObj->Id][j] |= bdCuts[pObj->Id][k][j] & bds[pCut->Id][j];
            ++k;
        }
    }
}


void Simulator_Pro_t::UpdateBoolDiff(IN Vec_Ptr_t * vNodes, INOUT vector <tVec> & bd)
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Abc_NtkForEachPo(pNtk, pObj, i) {
        for (int j = 0; j < nBlock; ++j)
            bd[pObj->Id][j] = static_cast <uint64_t> (ULLONG_MAX);
    }
    DASSERT(nBlock > 0);
    Vec_PtrForEachEntryReverse(Abc_Obj_t *, vNodes, pObj, i) {
        if (Abc_NodeIsConst(pObj))
            continue;
        int k = 0;
        for (auto & pCut: djCuts[pObj->Id]) {
            for (int j = 0; j < nBlock; ++j)
                bd[pObj->Id][j] |= bdCuts[pObj->Id][k][j] & bd[pCut->Id][j];
            ++k;
        }
    }
}


double MeasureMSE(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame, unsigned seed, bool isCheck)
{
    // check PI/PO
    if (isCheck)
        DASSERT(IOChecker(pNtk1, pNtk2));

    // simulation
    Simulator_Pro_t smlt1(pNtk1, nFrame);
    smlt1.Input(seed);
    smlt1.Simulate();
    Simulator_Pro_t smlt2(pNtk2, nFrame);
    smlt2.Input(seed);
    smlt2.Simulate();

    // compute
    return GetMSE(&smlt1, &smlt2, false, false);
}


double GetMSE(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, bool isCheck, bool isResub)
{
    if (isCheck)
        DASSERT(SmltChecker(pSmlt1, pSmlt2));
    typedef multiprecision::cpp_dec_float_100 bigFlt;
    typedef multiprecision::int256_t bigInt;
    bigInt sum(0);
    int nPo = Abc_NtkPoNum(pSmlt1->GetNetwork());
    int nFrame = pSmlt1->GetFrameNum();
    if (isResub) {
        assert(0);
    }
    else {
        for (int k = 0; k < nFrame; ++k) {
            auto tmp = (pSmlt1->GetOutput(0, nPo - 1, k, 0) - pSmlt2->GetOutput(0, nPo - 1, k, 0));
            sum += (tmp * tmp);
        }
    }
    return static_cast <double> (static_cast <bigFlt>(sum) / static_cast <bigFlt>(nFrame));
}


double MeasureNMED(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame, unsigned seed, bool isCheck)
{
    // check PI/PO
    if (isCheck)
        DASSERT(IOChecker(pNtk1, pNtk2));

    // simulation
    Simulator_Pro_t smlt1(pNtk1, nFrame);
    smlt1.Input(seed);
    smlt1.Simulate();
    Simulator_Pro_t smlt2(pNtk2, nFrame);
    smlt2.Input(seed);
    smlt2.Simulate();

    // compute
    return GetNMED(&smlt1, &smlt2, false, false);
}


double MeasureMRED(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame, unsigned seed, bool isCheck)
{
    // check PI/PO
    if (isCheck)
        DASSERT(IOChecker(pNtk1, pNtk2));

    // simulation
    Simulator_Pro_t smlt1(pNtk1, nFrame);
    smlt1.Input(seed);
    smlt1.Simulate();
    Simulator_Pro_t smlt2(pNtk2, nFrame);
    smlt2.Input(seed);
    smlt2.Simulate();

    // compute
    typedef multiprecision::cpp_dec_float_100 bigFlt;
    bigFlt sum(0);
    int nPo = Abc_NtkPoNum(pNtk1);
    for (int k = 0; k < nFrame; ++k) {
        bigFlt acc = static_cast <bigFlt>(smlt1.GetOutput(0, nPo - 1, k, 0));
        if (acc != 0.0)
            sum += abs(1 - static_cast <bigFlt>(smlt2.GetOutput(0, nPo - 1, k, 0)) / acc);
        else
            sum += abs(1 - static_cast <bigFlt>(smlt2.GetOutput(0, nPo - 1, k, 0)));
    }
    return static_cast <double> (sum / static_cast <double>(nFrame));
}


double MeasureResubNMED(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, Abc_Obj_t * pOldObj, void * pResubFunc, Vec_Ptr_t * vResubFanins, bool isCheck)
{
    if (isCheck)
        DASSERT(SmltChecker(pSmlt1, pSmlt2));
    pSmlt2->SimulateResub(pOldObj, pResubFunc, vResubFanins);
    return GetNMED(pSmlt1, pSmlt2, false, true);
}


double GetNMED(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, bool isCheck, bool isResub)
{
    if (isCheck)
        DASSERT(SmltChecker(pSmlt1, pSmlt2));
    typedef multiprecision::cpp_dec_float_100 bigFlt;
    typedef multiprecision::int256_t bigInt;
    bigInt sum(0);
    int nPo = Abc_NtkPoNum(pSmlt1->GetNetwork());
    int nFrame = pSmlt1->GetFrameNum();
    if (isResub) {
        for (int k = 0; k < nFrame; ++k)
            sum += (abs(pSmlt1->GetOutput(0, nPo - 1, k, 0) - pSmlt2->GetOutput(0, nPo - 1, k, 1)));
    }
    else {
        for (int k = 0; k < nFrame; ++k)
            sum += (abs(pSmlt1->GetOutput(0, nPo - 1, k, 0) - pSmlt2->GetOutput(0, nPo - 1, k, 0)));
    }
    bigInt frac = (static_cast <bigInt> (nFrame)) << nPo;;
    return static_cast <double> (static_cast <bigFlt>(sum) / static_cast <bigFlt>(frac));
}


void GetOffset(IN Simulator_Pro_t * pOriSmlt, IN Simulator_Pro_t * pAppSmlt, IN bool isCheck, INOUT vector < vector <int8_t> > & offsets)
{
    if (isCheck)
        DASSERT(SmltChecker(pOriSmlt, pAppSmlt));
    int nBlock = pOriSmlt->GetBlockNum();
    int nFrame = pOriSmlt->GetFrameNum();
    Abc_Ntk_t * pOriNtk = pOriSmlt->GetNetwork();
    Abc_Ntk_t * pAppNtk = pAppSmlt->GetNetwork();
    int nPo = Abc_NtkPoNum(pAppNtk);
    DASSERT(offsets.empty());
    offsets.resize(nPo);
    for (int i = 0; i < nPo; ++i) {
        int frameId = 0;
        Abc_Obj_t * pOriPo = Abc_NtkPo(pOriNtk, i);
        Abc_Obj_t * pAppPo = Abc_NtkPo(pAppNtk, i);
        offsets[i].resize(nFrame);
        for (int blockId = 0; blockId < nBlock; ++blockId) {
            uint64_t oriValues = pOriSmlt->GetValues(pOriPo, blockId);
            uint64_t appValues = pAppSmlt->GetValues(pAppPo, blockId);
            for (int bitId = 0; bitId < 64; ++bitId) {
                if (frameId >= nFrame)
                    break;
                bool oriBit = static_cast <bool> (oriValues & static_cast <uint64_t> (1));
                bool appBit = static_cast <bool> (appValues & static_cast <uint64_t> (1));
                oriValues >>= 1;
                appValues >>= 1;
                if (!oriBit && appBit)
                    offsets[i][frameId] = 1;
                else if (oriBit && !appBit)
                    offsets[i][frameId] = -1;
                else
                    offsets[i][frameId] = 0;
                ++frameId;
            }
        }
    }
}


double GetNMEDFromOffset(IN vector < vector <int8_t> > & offsets)
{
    int nPo = offsets.size();
    DASSERT(nPo);
    int nFrame = offsets[0].size();
    typedef multiprecision::int256_t bigInt;
    typedef multiprecision::cpp_dec_float_100 bigFlt;
    bigInt sum(0);
    for (int j = 0; j < nFrame; ++j) {
        bigInt weight(1);
        bigInt em(0);
        for (int i = 0; i < nPo; ++i) {
            if (offsets[i][j] == 1)
                em += weight;
            else if (offsets[i][j] == -1)
                em -= weight;
            else if (offsets[i][j] != 0)
                DASSERT(0);
            weight <<= 1;
        }
        sum += abs(em);
    }
    bigInt frac = (static_cast <bigInt> (nFrame)) << nPo;
    return static_cast <double> (static_cast <bigFlt>(sum) / static_cast <bigFlt>(frac));
}


double MeasureER(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame, unsigned seed, bool isCheck)
{
    if (isCheck)
        DASSERT(IOChecker(pNtk1, pNtk2));

    // simulation
    Simulator_Pro_t smlt1(pNtk1, nFrame);
    smlt1.Input(seed);
    smlt1.Simulate();
    Simulator_Pro_t smlt2(pNtk2, nFrame);
    smlt2.Input(seed);
    smlt2.Simulate();

    // compute
    return GetER(&smlt1, &smlt2, false, false) / static_cast <double> (nFrame);
}


double MeasureResubER(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, Abc_Obj_t * pOldObj, void * pResubFunc, Vec_Ptr_t * vResubFanins, bool isCheck)
{
    if (isCheck)
        DASSERT(SmltChecker(pSmlt1, pSmlt2));
    pSmlt2->SimulateResub(pOldObj, pResubFunc, vResubFanins);
    return GetER(pSmlt1, pSmlt2, false, true) / static_cast <double> (pSmlt1->GetFrameNum());
}


int GetER(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, bool isCheck, bool isResub)
{
    if (isCheck)
        DASSERT(SmltChecker(pSmlt1, pSmlt2));

    int ret = 0;
    Abc_Ntk_t * pNtk1 = pSmlt1->GetNetwork();
    Abc_Ntk_t * pNtk2 = pSmlt2->GetNetwork();
    int nPo = Abc_NtkPoNum(pNtk1);
    int nBlock = pSmlt1->GetBlockNum();
    int nLastBlock = pSmlt1->GetLastBlockLen();
    vector <tVec> * pValues1 = pSmlt1->GetPValues();
    vector <tVec> * pValues2 = isResub? pSmlt2->GetPTmpValues(): pSmlt2->GetPValues();
    for (int k = 0; k < nBlock; ++k) {
        uint64_t temp = 0;
        for (int i = 0; i < nPo; ++i)
            temp |= (*pValues1)[Abc_NtkPo(pNtk1, i)->Id][k] ^
                    (*pValues2)[Abc_NtkPo(pNtk2, i)->Id][k];
        if (k == nBlock - 1) {
            temp >>= (64 - nLastBlock);
            temp <<= (64 - nLastBlock);
        }
        ret += Ckt_CountOneNum(temp);
    }
    return ret;
}


bool IOChecker(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2)
{
    int nPo = Abc_NtkPoNum(pNtk1);
    if (nPo != Abc_NtkPoNum(pNtk2))
        return false;
    for (int i = 0; i < nPo; ++i)
        if (strcmp(Abc_ObjName(Abc_NtkPo(pNtk1, i)), Abc_ObjName(Abc_NtkPo(pNtk2, i))) != 0)
            return false;
    int nPi = Abc_NtkPiNum(pNtk1);
    if (nPi != Abc_NtkPiNum(pNtk2))
        return false;
    for (int i = 0; i < nPi; ++i)
        if (strcmp(Abc_ObjName(Abc_NtkPi(pNtk1, i)), Abc_ObjName(Abc_NtkPi(pNtk2, i))) != 0)
            return false;
    return true;
}


bool SmltChecker(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2)
{
    if (!IOChecker(pSmlt1->GetNetwork(), pSmlt2->GetNetwork()))
        return false;
    if (pSmlt1->GetFrameNum() != pSmlt2->GetFrameNum())
        return false;
    return true;
}


Vec_Ptr_t * Ckt_NtkDfsResub(Abc_Ntk_t * pNtk, Abc_Obj_t * pObjOld, Vec_Ptr_t * vFanins)
{
    Vec_Ptr_t * vNodes;
    Abc_Obj_t * pObj;
    int i;
    // set the traversal ID
    Abc_NtkIncrementTravId( pNtk );
    // start the array of nodes
    vNodes = Vec_PtrAlloc( 100 );
    Abc_NtkForEachCo( pNtk, pObj, i )
    {
        Abc_NodeSetTravIdCurrent( pObj );
        Ckt_NtkDfsResub_rec(Abc_ObjFanin0(pObj), vNodes, pObjOld, vFanins);
    }
    return vNodes;
}


void Ckt_NtkDfsResub_rec(Abc_Obj_t * pNode, Vec_Ptr_t * vNodes, Abc_Obj_t * pObjOld, Vec_Ptr_t * vFanins)
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
    if ( Abc_ObjIsCi(pNode) || (Abc_NtkIsStrash(pNode->pNtk) && Abc_AigNodeIsConst(pNode)) )
        return;
    assert( Abc_ObjIsNode( pNode ) || Abc_ObjIsBox( pNode ) );
    // visit the transitive fanin of the node
    if (pNode != pObjOld) {
        Abc_ObjForEachFanin( pNode, pFanin, i )
            Ckt_NtkDfsResub_rec(pFanin, vNodes, pObjOld, vFanins);
    }
    else {
        Vec_PtrForEachEntry(Abc_Obj_t *, vFanins, pFanin, i)
            Ckt_NtkDfsResub_rec(pFanin, vNodes, pObjOld, vFanins);
    }
    // add the node after the fanins have been added
    Vec_PtrPush( vNodes, pNode );
}
