#include "cktUtil.h"


using namespace std;


Abc_Ntk_t * MapToASIC(Abc_Ntk_t * pNtk)
{
    double DelayTarget = -1;
    double AreaMulti = 0;
    double DelayMulti = 0;
    float LogFan = 0;
    float Slew = 0;
    float Gain = 250;
    int nGatesMin = 0;
    int fAreaOnly = 0;
    int fRecovery = 1;
    int fSweep = 0;
    int fSwitching = 0;
    int fSkipFanout = 0;
    int fUseProfile = 0;
    int fVerbose = 0;
    // Abc_Ntk_t * pNtkRes = Abc_NtkMap(pNtk, DelayTarget, AreaMulti, DelayMulti, LogFan, Slew, Gain, nGatesMin, fRecovery, fSwitching, fSkipFanout, fUseProfile, fVerbose);
    // DASSERT(pNtkRes != nullptr);
    // return pNtkRes;
}
