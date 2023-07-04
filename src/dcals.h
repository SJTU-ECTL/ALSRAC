#ifndef DCALS_H
#define DCALS_H


#include <boost/progress.hpp>
#include <boost/random.hpp>
#include "simulatorPro.h"
#include "cktUtil.h"
#include "espressoApi.h"


class Dcals_Man_t
{
private:
    const int forbidRound = 1;

    Abc_Ntk_t * pOriNtk;
    Abc_Ntk_t * pAppNtk;
    Simulator_Pro_t * pOriSmlt;
    Simulator_Pro_t * pAppSmlt;
    unsigned seed;
    Metric_t metricType;
    int mapType;
    int roundId;
    int nFrame;
    int nEvalFrame;
    int nEstiFrame;
    double metric;
    double metricBound;
    double maxDelay;
    Mfs_Par_t * pPars;
    std::string outPath;
    std::deque <std::string> forbidList;

    Dcals_Man_t & operator = (const Dcals_Man_t &);
    Dcals_Man_t(const Dcals_Man_t &);

public:
    explicit Dcals_Man_t(Abc_Ntk_t * pNtk, int nFrame, double metricBound, Metric_t metricType, int mapType = 0, std::string outPath = "appntk/");
    ~Dcals_Man_t();
    Mfs_Par_t * InitMfsPars();
    void DCALS();
    void LocalAppChange();
    Hop_Obj_t * LocalAppChangeNode(Mfs_Man_t * p, Abc_Obj_t * pNode);
    Aig_Man_t * ConstructAppAig(Mfs_Man_t * p, Abc_Obj_t * pNode);
    void GenCand(IN bool genConst, INOUT std::vector <Lac_Cand_t> & cands);
    void GenCand(INOUT std::vector <Lac_Cand_t> & cands);
    Hop_Obj_t * BuildFunc(IN Abc_Obj_t * pPivot, IN Vec_Ptr_t * vFanins);
    Hop_Obj_t * BuildFuncEspresso(IN Abc_Obj_t * pPivot, IN Vec_Ptr_t * vFanins);
    void BatchErrorEst(IN std::vector <Lac_Cand_t> & cands, OUT Lac_Cand_t & bestCand);
};


Aig_Obj_t * Abc_NtkConstructAig_rec(Mfs_Man_t * p, Abc_Obj_t * pNode, Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig(Abc_Obj_t * pObjOld, Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig_rec(Hop_Obj_t * pObj, Aig_Man_t * pMan);
Vec_Ptr_t * Ckt_FindCut(Abc_Obj_t * pNode, int nMax);
Hop_Obj_t * Ckt_NtkMfsResubNode(Mfs_Man_t * p, Abc_Obj_t * pNode);
Hop_Obj_t * Ckt_NtkMfsSolveSatResub(Mfs_Man_t * p, Abc_Obj_t * pNode, int iFanin, int fOnlyRemove);
int Ckt_NtkMfsTryResubOnce(Mfs_Man_t * p, int * pCands, int nCands);
void Ckt_NtkMfsUpdateNetwork(Mfs_Man_t * p, Abc_Obj_t * pObj, Vec_Ptr_t * vMfsFanins, Hop_Obj_t * pFunc);
Abc_Obj_t * Ckt_UpdateNetwork(Abc_Obj_t * pObj, Vec_Ptr_t * vFanins, Hop_Obj_t * pFunc);
bool IsSimpPo(Abc_Obj_t * pObj);
Abc_Obj_t * GetFirstPoFanout(Abc_Obj_t * pObj);
Vec_Ptr_t * GetTFICone(Abc_Ntk_t * pNtk, Abc_Obj_t * pObj);
Vec_Ptr_t * ComputeDivisors(Abc_Obj_t * pNode, int nWinTfiLevs);
void ComputeDivisors_rec(Abc_Obj_t * pNode, Vec_Ptr_t * vDivs, int minLevel);
Hop_Obj_t * Ckt_NtkMfsInterplate(Mfs_Man_t * p, int * pCands, int nCands);
Kit_Graph_t * Ckt_TruthToGraph(unsigned * pTruth, int nVars, Vec_Int_t * vMemory);
int Ckt_TruthIsop(unsigned * puTruth, int nVars, Vec_Int_t * vMemory, int fTryBoth);
unsigned * Ckt_TruthIsop_rec(unsigned * puOn, unsigned * puOnDc, int nVars, Kit_Sop_t * pcRes, Vec_Int_t * vStore);
unsigned Ckt_TruthIsop5_rec( unsigned uOn, unsigned uOnDc, int nVars, Kit_Sop_t * pcRes, Vec_Int_t * vStore );
Vec_Ptr_t * Ckt_ComputeDivisors(Abc_Obj_t * pNode, int nLevDivMax, int nWinMax, int nFanoutsMax);


extern "C" {
void Abc_NtkDfs_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vNodes );
Hop_Obj_t * Kit_GraphToHop( Hop_Man_t * pMan, Kit_Graph_t * pGraph );
Vec_Ptr_t * Abc_MfsWinMarkTfi( Abc_Obj_t * pNode );
void Abc_MfsWinSweepLeafTfo_rec( Abc_Obj_t * pObj, int nLevelLimit );
}


#endif
