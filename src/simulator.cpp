#include "simulator.h"
using namespace abc;
using namespace std;
using namespace boost;


typedef vector <uint64_t> tVec;


Simulator::Simulator(Abc_Ntk_t * pNtk, int nFrame)
{
    DASSERT(Abc_NtkIsAigLogic(pNtk), "network is not in aig");
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    this->pNtk = pNtk;
    this->nFrame = nFrame;
    this->nBlock = (nFrame % 64) ? ((nFrame >> 6) + 1) : (nFrame >> 6);
    Abc_NtkForEachObj(pNtk, pObj, i) {
        DASSERT(pObj->pTemp == nullptr && pObj->pCopy == nullptr);
        pObj->pTemp = static_cast <void *>(new tVec);
        static_cast <tVec *>(pObj->pTemp)->resize(nBlock);
    }
}


Simulator::~Simulator()
{
}


void Simulator::Input(Distribution dist, unsigned seed)
{
    DASSERT(dist == Distribution::uniform);
    boost::uniform_int <> unf(0, 1);
    boost::random::mt19937 engine(seed);
    boost::variate_generator <boost::random::mt19937, boost::uniform_int <> > randomPI(engine, unf);

    // primary inputs
    Abc_Obj_t * pObj = nullptr;
    int k = 0;
    Abc_NtkForEachPi(pNtk, pObj, k) {
        for (int i = 0; i < nBlock; ++i) {
            for (uint64_t j = 0; j < 64; ++j) {
                if (randomPI())
                    Ckt_SetBit((*static_cast <tVec *>(pObj->pTemp))[i], j);
                else
                    Ckt_ResetBit((*static_cast <tVec *>(pObj->pTemp))[i], j);
            }
        }
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


void Simulator::Simulate()
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Vec_Ptr_t * vNodes = Abc_NtkDfs(pNtk, 0);
    Vec_PtrForEachEntry(Abc_Obj_t *, vNodes, pObj, i)
        UpdateAigNode(pObj);
    Vec_PtrFree(vNodes);
}


void Simulator::UpdateAigNode(Abc_Obj_t * pObj)
{
    Hop_Obj_t * pHopObj = nullptr;
    int maxHopId = -1;
    int i = 0;

    // get hop order
    Hop_Man_t * pMan = static_cast <Hop_Man_t *> (pNtk->pManFunc);
    Hop_Obj_t * pRoot = static_cast <Hop_Obj_t *> (pObj->pData);
    Hop_Obj_t * pRootR = Hop_Regular(pRoot);
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
    Hop_ManForEachPi(pMan, pHopObj, i)
        hop2Val[pHopObj->Id] = static_cast <tVec *> (Abc_ObjFanin(pObj, i)->pTemp);

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
    DASSERT(pRootR->Type != AIG_CONST1);
    if (Hop_IsComplement(pRoot)) {
        for (int j = 0; j < nBlock; ++j)
            (*static_cast <tVec *>(pObj->pTemp))[j] ^= static_cast <uint64_t> (ULLONG_MAX);
    }

    // recycle memory
    Vec_PtrFree(vHopNodes);
}


vector <multiprecision::int256_t> Simulator::Output()
{
    vector <multiprecision::int256_t> out(nFrame);
    int nLastFrame = (nFrame % 64)? (nFrame % 64): 64;
    for (int i = 0; i < nBlock; ++i) {
        for (int j = 0; j < ((i == nBlock - 1)? (nLastFrame): 64); ++j) {
            int id = (i << 6) + j;
            out[id] = 0;
            for (int k = Abc_NtkPoNum(pNtk) - 1; k >= 0; --k) {
                Abc_Obj_t * pObj = Abc_ObjFanin0(Abc_NtkPo(pNtk, k));
                if (Ckt_GetBit((*static_cast <tVec *>(pObj->pTemp))[i], j))
                    out[id] += 1;
                out[id] <<= 1;
            }
        }
    }
    return out;
}


void Simulator::Stop()
{
    Abc_Obj_t * pObj = nullptr;
    int i = 0;
    Abc_NtkForEachObj(pNtk, pObj, i) {
        tVec().swap(*(static_cast < tVec * >(pObj->pTemp)));
        delete static_cast < tVec * >(pObj->pTemp);
        pObj->pTemp = nullptr;
    }
}


double MeasureAEM(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame)
{
    Simulator smlt1(pNtk1, nFrame);
    smlt1.Input(Distribution::uniform, 314);
    smlt1.Simulate();
    vector <multiprecision::int256_t> out1 = smlt1.Output();
    smlt1.Stop();

    Simulator smlt2(pNtk1, nFrame);
    smlt2.Input(Distribution::uniform, 314);
    smlt2.Simulate();
    vector <multiprecision::int256_t> out2 = smlt2.Output();
    smlt2.Stop();

    const double factor = 1 / static_cast <double> (nFrame);
    multiprecision::cpp_dec_float_50 aem(0);
    for (int i = 0; i < nFrame; ++i) {
        aem +=
            (static_cast <multiprecision::cpp_dec_float_50> (abs(out1[i] - out2[i])) *
            static_cast <multiprecision::cpp_dec_float_50> (factor));
    }
    return static_cast <double>(aem);
}
