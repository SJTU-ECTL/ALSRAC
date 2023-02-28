#ifndef SIMULATOR_PRO_H
#define SIMULATOR_PRO_H


#include <boost/random.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include "abcApi.h"
#include "cktBit.h"
#include "cktUtil.h"
#include "debugAssert.h"


enum class Metric_t{ER, NMED, MRED};
enum class Map_t{SCL, LUT};


typedef std::vector <uint64_t> tVec;


class Lac_Cand_t
{
private:
    double error;
    Abc_Obj_t * pObj;
    Hop_Obj_t * pFunc;
    Vec_Ptr_t * vFanins;
    tVec estimation;

    Lac_Cand_t & operator = (const Lac_Cand_t &);
    Lac_Cand_t(Lac_Cand_t &&);

public:
    explicit Lac_Cand_t();
    explicit Lac_Cand_t(Abc_Obj_t * pObj, Hop_Obj_t * pFunc, Vec_Ptr_t * vFanins);
    Lac_Cand_t(const Lac_Cand_t & lac);
    ~Lac_Cand_t();
    void Print() const;
    bool ExistResub(double metricBound) const;
    void UpdateBest(double errorNew, Abc_Obj_t * pObj, Hop_Obj_t * pFuncNew, Vec_Ptr_t * vFaninsNew);
    void UpdateBest(tVec & estNew, Abc_Obj_t * pObjNew, Hop_Obj_t * pFuncNew, Vec_Ptr_t * vFaninsNew);

    inline double GetError() const {return error;}
    inline void SetError(double errorNew) {error = errorNew;}
    inline Abc_Obj_t * GetObj() const {return pObj;}
    inline Hop_Obj_t * GetFunc() const {return pFunc;}
    inline Vec_Ptr_t * GetFanins() const {return vFanins;}
};


class Simulator_Pro_t
{
private:
    Abc_Ntk_t * pNtk;
    int nFrame;
    int nBlock;
    int nLastBlock;
    int maxId;
    std::vector <int> topoIds;
    std::vector <tVec> values;
    std::vector <tVec> tmpValues;
    std::vector <tVec> sinks;
    std::vector < std::list <Abc_Obj_t *> > djCuts;
    std::vector < std::vector <Abc_Obj_t *> > cutNtks;
    std::vector < std::vector <tVec> > bdCuts;

    Simulator_Pro_t & operator = (const Simulator_Pro_t &);
    Simulator_Pro_t(const Simulator_Pro_t &);

public:
    explicit Simulator_Pro_t(Abc_Ntk_t * pNtk, int nFrame = 64);
    ~Simulator_Pro_t();
    void Input(unsigned seed = 314);
    void Input(std::string fileName);
    void Simulate();
    void SimulateCutNtks();
    void SimulateResub(Abc_Obj_t * pOldObj, void * pResubFunc, Vec_Ptr_t * vResubFanins);
    void UpdateAigNode(Abc_Obj_t * pObj);
    void UpdateSopNode(Abc_Obj_t * pObj);
    void UpdateAigObjCutNtk(Abc_Obj_t * pObj);
    void UpdateSopObjCutNtk(Abc_Obj_t * pObj);
    void UpdateAigNodeResub(Abc_Obj_t * pObj, Hop_Obj_t * pResubFunc = nullptr, Vec_Ptr_t * vResubFanins = nullptr);
    void UpdateAigNodeResub(IN Abc_Obj_t * pObj, IN Hop_Obj_t * pResubFunc, IN Vec_Ptr_t * vResubFanins, OUT tVec & outValue);
    void UpdateSopNodeResub(Abc_Obj_t * pObj, char * pResubFunc = nullptr, Vec_Ptr_t * vResubFanins = nullptr);
    void UpdateSopNodeResub(Abc_Obj_t * pObj, char * pResubFunc, Vec_Ptr_t * vResubFanins, tVec & outValues);
    boost::multiprecision::int256_t GetInput(int lsb, int msb, int frameId = 0) const;
    boost::multiprecision::int256_t GetOutput(int lsb, int msb, int frameId = 0, bool isTmpValue = false) const;
    void PrintInputStream(int frameId = 0) const;
    void PrintOutputStream(int frameId = 0, bool isReverse = false) const;
    void BuildCutNtks();
    void BuildAppCutNtks();
    void FindDisjointCut(Abc_Obj_t * pObj, std::list <Abc_Obj_t *> & djCut);
    void ExpandCut(Abc_Obj_t * pObj, std::list <Abc_Obj_t *> & djCut);
    Abc_Obj_t * ExpandWhich(std::list <Abc_Obj_t *> & djCut);
    void UpdateBoolDiff(IN Abc_Obj_t * pPo, IN Vec_Ptr_t * vNodes, INOUT std::vector <tVec> & bds);
    void UpdateBoolDiff(IN Vec_Ptr_t * vNodes, INOUT std::vector <tVec> & bds);

    inline Abc_Ntk_t * GetNetwork() const {return pNtk;}
    inline int GetFrameNum() const {return nFrame;}
    inline int GetBlockNum() const {return nBlock;}
    inline int GetLastBlockLen() const {return nLastBlock;}
    inline int GetMaxId() const {return maxId;}
    inline std::vector <tVec> * GetPValues() {return &values;}
    inline std::vector <tVec> * GetPTmpValues() {return &tmpValues;}
    inline uint64_t GetValues(Abc_Obj_t * pObj, int blockId) const {DASSERT(pObj->pNtk == pNtk); return values[pObj->Id][blockId];}
    inline bool GetValue(Abc_Obj_t * pObj, int blockId, int bitId) const {DASSERT(pObj->pNtk == pNtk); return Ckt_GetBit(values[pObj->Id][blockId], bitId);}
    inline bool GetTmpValue(Abc_Obj_t * pObj, int blockId, int bitId) const {return Ckt_GetBit(tmpValues[pObj->Id][blockId], bitId);}
    inline std::vector < std::list <Abc_Obj_t *> > * GetDjCuts() {return &djCuts;}
    inline uint64_t GetBdCut(Abc_Obj_t * pObj, int cutId, int blockId) const {return bdCuts[pObj->Id][cutId][blockId];}
};


double MeasureMSE(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame = 102400, unsigned seed = 314, bool isCheck = true);
double GetMSE(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, bool isCheck = true, bool isResub = false);
double MeasureNMED(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame = 102400, unsigned seed = 314, bool isCheck = true);
double MeasureMRED(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame, unsigned seed, bool isCheck = true);
double MeasureResubNMED(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, Abc_Obj_t * pOldObj, void * pResubFunc, Vec_Ptr_t * vResubFanins, bool isCheck = true);
double GetNMED(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, bool isCheck = true, bool isResub = false);
void GetOffset(IN Simulator_Pro_t * pSmlt1, IN Simulator_Pro_t * pSmlt2, IN bool isCheck, INOUT std::vector < std::vector <int8_t> > & offsets);
double GetNMEDFromOffset(IN std::vector < std::vector <int8_t> > & offsets);
double MeasureER(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame = 102400, unsigned seed = 314, bool isCheck = true);
double MeasureResubER(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, Abc_Obj_t * pOldObj, void * pResubFunc, Vec_Ptr_t * vResubFanins, bool isCheck = true);
int GetER(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, bool isCheck = true, bool isResub = false);
bool IOChecker(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2);
bool SmltChecker(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2);
Vec_Ptr_t * Ckt_NtkDfsResub(Abc_Ntk_t * pNtk, Abc_Obj_t * pObjOld, Vec_Ptr_t * vFanins);
void Ckt_NtkDfsResub_rec(Abc_Obj_t * pNode, Vec_Ptr_t * vNodes, Abc_Obj_t * pObjOld, Vec_Ptr_t * vFanins);

#endif
