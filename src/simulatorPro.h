#ifndef SIMULATOR_PRO_H
#define SIMULATOR_PRO_H


#include <boost/random.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include "abcApi.h"
#include "cktBit.h"
#include "debugAssert.h"


typedef std::vector <uint64_t> tVec;


class Simulator_Pro_t
{
private:
    Abc_Ntk_t * pNtk;
    int nFrame;
    int nBlock;
    int nLastBlock;
    std::vector <tVec> values;
    std::vector <tVec> tmpValues;

    Simulator_Pro_t & operator = (const Simulator_Pro_t &);
    Simulator_Pro_t(const Simulator_Pro_t &);

public:
    explicit Simulator_Pro_t(Abc_Ntk_t * pNtk, int nFrame = 64);
    ~Simulator_Pro_t();
    void Input(unsigned seed = 314);
    void Input(std::string fileName);
    void Simulate();
    void SimulateResub(Abc_Obj_t * pOldObj, Hop_Obj_t * pResubFunc, Vec_Ptr_t * vResubFanins);
    void UpdateAigNode(Abc_Obj_t * pObj);
    void UpdateAigNodeResub(Abc_Obj_t * pObj, Hop_Obj_t * pResubFunc = nullptr, Vec_Ptr_t * vResubFanins = nullptr);
    boost::multiprecision::int256_t GetInput(int lsb, int msb, int frameId = 0) const;
    boost::multiprecision::int256_t GetOutput(int lsb, int msb, int frameId = 0, bool isTmpValue = false) const;
    void PrintInputStream(int frameId = 0) const;
    void PrintOutputStream(int frameId = 0) const;

    inline Abc_Ntk_t * GetNetwork() const {return pNtk;}
    inline int GetFrameNum() const {return nFrame;}
    inline int GetBlockNum() const {return nBlock;}
    inline int GetLastBlockLen() const {return nLastBlock;}
    inline std::vector <tVec> * GetPValues() {return &values;}
    inline std::vector <tVec> * GetPTmpValues() {return &tmpValues;}
    inline bool GetValue(Abc_Obj_t * pObj, int blockId, int bitId) const {return Ckt_GetBit(values[pObj->Id][blockId], bitId);}
    inline bool GetTmpValue(Abc_Obj_t * pObj, int blockId, int bitId) const {return Ckt_GetBit(tmpValues[pObj->Id][blockId], bitId);}
};


double MeasureAEMR(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame = 102400, unsigned seed = 314, bool isCheck = true);
double MeasureResubAEMR(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, Abc_Obj_t * pOldObj, Hop_Obj_t * pResubFunc, Vec_Ptr_t * vResubFanins, bool isCheck = true);
double GetAEMR(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, bool isCheck = true, bool isResub = false);
double MeasureER(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame = 102400, unsigned seed = 314, bool isCheck = true);
double MeasureResubER(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, Abc_Obj_t * pOldObj, Hop_Obj_t * pResubFunc, Vec_Ptr_t * vResubFanins, bool isCheck = true);
double GetER(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, bool isCheck = true, bool isResub = false);
bool IOChecker(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2);
bool SmltChecker(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2);
Vec_Ptr_t * Ckt_NtkDfsResub(Abc_Ntk_t * pNtk, Abc_Obj_t * pObjOld, Vec_Ptr_t * vFanins);
void Ckt_NtkDfsResub_rec(Abc_Obj_t * pNode, Vec_Ptr_t * vNodes, Abc_Obj_t * pObjOld, Vec_Ptr_t * vFanins);


#endif
