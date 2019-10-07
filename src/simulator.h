#ifndef SIMULATOR_H
#define SIMULATOR_H


#include <boost/random.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include "abcApi.h"
#include "cktBit.h"
#include "debugAssert.h"


typedef std::vector <uint64_t> tVec;


enum class Distribution
{
    uniform, gaussian, exponential, bimodal
};


class Simulator_t
{
private:
    Abc_Ntk_t * pNtk;
    int nFrame;
    int nBlock;
    int nLastBlock;

    Simulator_t & operator = (const Simulator_t &);
    Simulator_t(const Simulator_t &);

public:
    explicit Simulator_t(Abc_Ntk_t * pNtk, int nFrame = 64);
    ~Simulator_t();
    void Input(Distribution dist = Distribution::uniform, unsigned seed = 314);
    void Input(std::string fileName);
    void Simulate();
    void UpdateAigNode(Abc_Obj_t * pObj);
    boost::multiprecision::int256_t GetInput(int lsb, int msb, int frameId = 0) const;
    boost::multiprecision::int256_t GetOutput(int lsb, int msb, int frameId = 0) const;
    void PrintInputStream(int frameId = 0) const;
    void PrintOutputStream(int frameId = 0) const;
    void Stop();

    inline int GetFrameNum() const {return nFrame;}
    inline int GetBlockNum() const {return nBlock;}
    inline int GetLastBlockLen() const {return nLastBlock;}
    inline bool GetValue(Abc_Obj_t * pObj, int blockId, int bitId) const {return Ckt_GetBit((*static_cast <tVec *>(pObj->pTemp))[blockId], bitId);}
};


double MeasureAEMR(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame = 102400, unsigned seed = 314, bool isCheck = true);
double MeasureER(Abc_Ntk_t * pNtk1, Abc_Ntk_t * pNtk2, int nFrame = 102400, unsigned seed = 314, bool isCheck = true);


#endif
