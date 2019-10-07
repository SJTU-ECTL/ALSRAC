#include "simulator.h"
using namespace std;
using namespace boost;


Simulator_t::Simulator_t(Abc_Ntk_t * pNtk, int nFrame)
{
    DASSERT(Abc_NtkIsAigLogic(pNtk), "network is not in aig");
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    this->pNtk = pNtk;
    this->nFrame = nFrame;
    this->nBlock = (nFrame % 64) ? ((nFrame >> 6) + 1) : (nFrame >> 6);
    this->nLastBlock = (nFrame % 64)? (nFrame % 64): 64;
    Abc_NtkForEachObj(pNtk, pObj, i) {
        // DASSERT(pObj->pTemp == nullptr && pObj->pCopy == nullptr);
        pObj->pTemp = static_cast <void *>(new tVec);
        static_cast <tVec *>(pObj->pTemp)->resize(nBlock);
    }
}


Simulator_t::~Simulator_t()
{
}


void Simulator_t::Input(Distribution dist, unsigned seed)
{
    DASSERT(dist == Distribution::uniform);
    random::uniform_int_distribution <uint64_t> unf;
    random::mt19937 engine(seed);
    variate_generator <random::mt19937, random::uniform_int_distribution <uint64_t> > randomPI(engine, unf);

    // primary inputs
    Abc_Obj_t * pObj = nullptr;
    int k = 0;
    Abc_NtkForEachPi(pNtk, pObj, k) {
        for (int i = 0; i < nBlock; ++i)
            (*static_cast <tVec *>(pObj->pTemp))[i] = randomPI();
    }

    // constant nodes
    DASSERT(Abc_NtkIsAigLogic(pNtk), "network is not in aig");
    Abc_NtkForEachNode(pNtk, pObj, k) {
        Hop_Obj_t * pHopObj = static_cast <Hop_Obj_t *> (pObj->pData);
        if (Hop_ObjFanin0(pHopObj) == nullptr && Hop_ObjFanin1(pHopObj) == nullptr && pHopObj->Type != AIG_PI) {
            if (Hop_ObjIsConst1(pHopObj)) {
                for (int i = 0; i < nBlock; ++i)
                    (*static_cast <tVec *>(pObj->pTemp))[i] = static_cast <uint64_t> (ULLONG_MAX);
            }
            else {
                for (int i = 0; i < nBlock; ++i)
                    (*static_cast <tVec *>(pObj->pTemp))[i] = 0;
            }
        }
    }
}


void Simulator_t::Simulate()
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Vec_Ptr_t * vNodes = Abc_NtkDfs(pNtk, 0);
    Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pObj, i)
        UpdateAigNode(pObj);
    Vec_PtrFree(vNodes);
}


void Simulator_t::UpdateAigNode(Abc_Obj_t * pObj)
{
    Hop_Obj_t * pHopObj = nullptr;
    Abc_Obj_t * pFanin = nullptr;
    int maxHopId = -1;
    int i = 0;

    Hop_Man_t * pMan = static_cast <Hop_Man_t *> (pNtk->pManFunc);
    Hop_Obj_t * pRoot = static_cast <Hop_Obj_t *> (pObj->pData);
    Hop_Obj_t * pRootR = Hop_Regular(pRoot);

    // skip constant node
    if (pRootR->Type == AIG_CONST1)
        return;

    // get topological order of subnetwork in aig
    Vec_Ptr_t * vHopNodes = Hop_ManDfsNode(pMan, pRootR);

    // init
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    Hop_ManForEachPi(pMan, pHopObj, i)
        maxHopId = max(maxHopId, pHopObj->Id);
    vector < tVec > values(maxHopId + 1);
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i)
        values[pHopObj->Id].resize(nBlock);
    unordered_map <int, tVec *> hop2Val;
    Abc_ObjForEachFanin(pObj, pFanin, i)
        hop2Val[Hop_ManPi(pMan, i)->Id] = static_cast <tVec *>(pFanin->pTemp);

    // special case for inverter or buffer
    if (pRootR->Type == AIG_PI) {
        pFanin = Abc_ObjFanin0(pObj);
        static_cast <tVec *>(pObj->pTemp)->assign(
            static_cast <tVec *>(pFanin->pTemp)->begin(),
            static_cast <tVec *>(pFanin->pTemp)->end()
        );
    }

    // simulate
    Vec_PtrForEachEntry(Hop_Obj_t *, vHopNodes, pHopObj, i) {
        DASSERT(Hop_ObjIsAnd(pHopObj));
        Hop_Obj_t * pHopFanin0 = Hop_ObjFanin0(pHopObj);
        Hop_Obj_t * pHopFanin1 = Hop_ObjFanin1(pHopObj);
        DASSERT(!Hop_ObjIsConst1(pHopFanin0));
        DASSERT(!Hop_ObjIsConst1(pHopFanin1));
        tVec & val0 = Hop_ObjIsPi(pHopFanin0) ? *hop2Val[pHopFanin0->Id] : values[pHopFanin0->Id];
        tVec & val1 = Hop_ObjIsPi(pHopFanin1) ? *hop2Val[pHopFanin1->Id] : values[pHopFanin1->Id];
        tVec & out = (pHopObj == pRootR) ? *static_cast <tVec *>(pObj->pTemp) : values[pHopObj->Id];
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

    // record
    if (Hop_IsComplement(pRoot)) {
        for (int j = 0; j < nBlock; ++j)
            (*static_cast <tVec *>(pObj->pTemp))[j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }
    // cout << Abc_ObjName(pObj) << "," << (*static_cast <tVec *>(pObj->pTemp))[0] << endl;

    // recycle memory
    Vec_PtrFree(vHopNodes);
}


multiprecision::int256_t Simulator_t::GetInput(int lsb, int msb, int frameId) const
{
    DASSERT(lsb >= 0 && msb < Abc_NtkPiNum(pNtk));
    DASSERT(frameId <= nFrame);
    DASSERT(lsb <= msb && msb - lsb <= 256);
    multiprecision::int256_t ret(0);
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    for (int k = msb; k >= lsb; --k) {
        Abc_Obj_t * pObj = Abc_NtkPi(pNtk, k);
        if (Ckt_GetBit((*static_cast <tVec *>(pObj->pTemp))[blockId], bitId))
            ++ret;
        ret <<= 1;
    }
    return ret;
}


multiprecision::int256_t Simulator_t::GetOutput(int lsb, int msb, int frameId) const
{
    DASSERT(lsb >= 0 && msb < Abc_NtkPoNum(pNtk));
    DASSERT(frameId <= nFrame);
    DASSERT(lsb <= msb && msb - lsb <= 256);
    multiprecision::int256_t ret(0);
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    for (int k = msb; k >= lsb; --k) {
        Abc_Obj_t * pObj = Abc_ObjFanin0(Abc_NtkPo(pNtk, k));
        if (Ckt_GetBit((*static_cast <tVec *>(pObj->pTemp))[blockId], bitId))
            ++ret;
        ret <<= 1;
    }
    return ret;
}


void Simulator_t::PrintInputStream(int frameId) const
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    Abc_NtkForEachPi(pNtk, pObj, i)
        cout << Ckt_GetBit((*static_cast <tVec *>(pObj->pTemp))[blockId], bitId);
    cout << endl;
}


void Simulator_t::PrintOutputStream(int frameId) const
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    int blockId = frameId >> 6;
    int bitId = frameId % 64;
    Abc_NtkForEachPo(pNtk, pObj, i)
        cout << Ckt_GetBit((*static_cast <tVec *>(Abc_ObjFanin0(pObj)->pTemp))[blockId], bitId);
    cout << endl;
}


void Simulator_t::Stop()
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Abc_NtkForEachObj(pNtk, pObj, i) {
        tVec().swap(*(static_cast < tVec * >(pObj->pTemp)));
        delete static_cast < tVec * >(pObj->pTemp);
        pObj->pTemp = nullptr;
    }
}


double MeasureAEM(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame, unsigned seed, bool isCheck)
{
    // check PI/PO
    int nPo = Abc_NtkPoNum(pNtk1);
    if (isCheck) {
        DASSERT(nPo == Abc_NtkPoNum(pNtk2));
        for (int i = 0; i < nPo; ++i)
            DASSERT(!strcmp(Abc_ObjName(Abc_NtkPo(pNtk1, i)), Abc_ObjName(Abc_NtkPo(pNtk2, i))));
        int nPi = Abc_NtkPiNum(pNtk1);
        DASSERT(nPi == Abc_NtkPiNum(pNtk2));
        for (int i = 0; i < nPi; ++i)
            DASSERT(!strcmp(Abc_ObjName(Abc_NtkPi(pNtk1, i)), Abc_ObjName(Abc_NtkPi(pNtk2, i))));
    }

    // simulation
    Simulator_t smlt1(pNtk1, nFrame);
    smlt1.Input(Distribution::uniform, seed);
    smlt1.Simulate();
    Simulator_t smlt2(pNtk2, nFrame);
    smlt2.Input(Distribution::uniform, seed);
    smlt2.Simulate();

    // compute
    typedef double hpfloat;
    hpfloat aem(0);
    const hpfloat factor = 1 / static_cast <double> (nFrame);
    for (int k = 0; k < nFrame; ++k) {
        aem += (static_cast <hpfloat> (abs(smlt1.GetOutput(0, nPo - 1, k) - smlt2.GetOutput(0, nPo - 1, k))) * factor);
    }

    smlt1.Stop();
    smlt2.Stop();
    return static_cast<double> (aem);
}


double MeasureER(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame, unsigned seed, bool isCheck)
{
    // check PI/PO
    int nPo = Abc_NtkPoNum(pNtk1);
    if (isCheck) {
        DASSERT(nPo == Abc_NtkPoNum(pNtk2));
        for (int i = 0; i < nPo; ++i)
            DASSERT(!strcmp(Abc_ObjName(Abc_NtkPo(pNtk1, i)), Abc_ObjName(Abc_NtkPo(pNtk2, i))));
        int nPi = Abc_NtkPiNum(pNtk1);
        DASSERT(nPi == Abc_NtkPiNum(pNtk2));
        for (int i = 0; i < nPi; ++i)
            DASSERT(!strcmp(Abc_ObjName(Abc_NtkPi(pNtk1, i)), Abc_ObjName(Abc_NtkPi(pNtk2, i))));
    }

    // simulation
    Simulator_t smlt1(pNtk1, nFrame);
    smlt1.Input(Distribution::uniform, seed);
    smlt1.Simulate();
    Simulator_t smlt2(pNtk2, nFrame);
    smlt2.Input(Distribution::uniform, seed);
    smlt2.Simulate();

    // compute
    int ret = 0;
    for (int k = 0; k < smlt1.GetBlockNum(); ++k) {
        uint64_t temp = 0;
        for (int i = 0; i < nPo; ++i)
            temp |= (*static_cast <tVec *>(Abc_ObjFanin0(Abc_NtkPo(pNtk1, i))->pTemp))[k] ^
                    (*static_cast <tVec *>(Abc_ObjFanin0(Abc_NtkPo(pNtk2, i))->pTemp))[k];
        if (k == smlt1.GetBlockNum() - 1) {
            temp >>= (64 - smlt1.GetLastBlockLen());
            temp <<= (64 - smlt1.GetLastBlockLen());
        }
        ret += Ckt_CountOneNum(temp);
    }

    smlt1.Stop();
    smlt2.Stop();
    return ret / static_cast<double> (nFrame);
}
