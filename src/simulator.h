#ifndef SIMULATOR_H
#define SIMULATOR_H


#include <bits/stdc++.h>
#include <boost/random.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include "abcApi.h"
#include "debugAssert.h"
#include "cktBit.h"


enum class Distribution
{
    uniform, gaussian, exponential, bimodal
};


class Simulator_t
{
private:
    abc::Abc_Ntk_t * pNtk;
    int nFrame;
    int nBlock;
    int nLastBlock;

    Simulator_t & operator = (const Simulator_t &);
    Simulator_t(const Simulator_t &);

public:
    explicit Simulator_t(abc::Abc_Ntk_t * pNtk, int nFrame = 64);
    ~Simulator_t();
    void Input(Distribution dist = Distribution::uniform, unsigned seed = 314);
    void Simulate();
    void UpdateAigNode(abc::Abc_Obj_t * pObj);
    boost::multiprecision::int256_t GetInput(int lsb, int msb, int frameId = 0) const;
    boost::multiprecision::int256_t GetOutput(int lsb, int msb, int frameId = 0) const;
    void Stop();

    inline int GetFrameNum() const {return nFrame;}
    inline int GetBlockNum() const {return nBlock;}
    inline int GetLastBlockLen() const {return nLastBlock;}
};


double MeasureAEM(abc::Abc_Ntk_t * pNtk1, abc::Abc_Ntk_t * pNtk2, int nFrame = 102400);
double MeasureER(abc::Abc_Ntk_t * pNtk1, abc::Abc_Ntk_t * pNtk2, int nFrame = 102400, unsigned seed = 314);


#endif
