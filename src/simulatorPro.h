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

    Simulator_Pro_t & operator = (const Simulator_Pro_t &);
    Simulator_Pro_t(const Simulator_Pro_t &);

public:
    explicit Simulator_Pro_t(Abc_Ntk_t * pNtk, int nFrame = 64);
    ~Simulator_Pro_t();
    void Input(unsigned seed = 314);
    void Input(std::string fileName);
    void Simulate();
    void UpdateAigNode(Abc_Obj_t * pObj);
    boost::multiprecision::int256_t GetInput(int lsb, int msb, int frameId = 0) const;
    boost::multiprecision::int256_t GetOutput(int lsb, int msb, int frameId = 0) const;
    void PrintInputStream(int frameId = 0) const;
    void PrintOutputStream(int frameId = 0) const;

    inline Abc_Ntk_t * GetNetwork() const {return pNtk;}
    inline int GetFrameNum() const {return nFrame;}
    inline int GetBlockNum() const {return nBlock;}
    inline int GetLastBlockLen() const {return nLastBlock;}
    inline std::vector <tVec> * GetPValues() {return &values;}
    inline bool GetValue(Abc_Obj_t * pObj, int blockId, int bitId) const {return Ckt_GetBit(values[pObj->Id][blockId], bitId);}
};


double MeasureAEMR(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame = 102400, unsigned seed = 314, bool isCheck = true);
double MeasureER(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame = 102400, unsigned seed = 314, bool isCheck = true);
double GetER(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2, bool isCheck = true);
bool IOChecker(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2);
bool SmltChecker(Simulator_Pro_t * pSmlt1, Simulator_Pro_t * pSmlt2);


#endif
