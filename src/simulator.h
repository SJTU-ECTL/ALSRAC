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


class Simulator
{
private:
    abc::Abc_Ntk_t * pNtk;
    int nFrame;
    int nBlock;

    Simulator & operator = (const Simulator &);
    Simulator(const Simulator &);

public:
    explicit Simulator(abc::Abc_Ntk_t * pNtk, int nFrame = 64);
    ~Simulator();
    void Input(Distribution dist = Distribution::uniform, unsigned seed = 314);
    void Simulate();
    void UpdateAigNode(abc::Abc_Obj_t * pObj);
    std::vector <boost::multiprecision::int256_t> Output();
    void Stop();
};


double MeasureAEM(abc::Abc_Ntk_t * pNtk1, abc::Abc_Ntk_t * pNtk2, int nFrame = 102400);
