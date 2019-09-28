#ifndef DCALS_H
#define DCALS_H


#include "simulator.h"


class Dcals_Man_t
{
private:
    Abc_Ntk_t * pOriNtk;
    Abc_Ntk_t * pAppNtk;
    int nFrame;
    int cutSize;
    double metric;
    double metricBound;
    Mfs_Par_t * pPars;
public:
    explicit Dcals_Man_t(Abc_Ntk_t * pNtk, int nFrame, int cutSize, double metricBound);
    ~Dcals_Man_t();
    Mfs_Par_t * InitMfsPars();
    void DCALS();
    void LocalAppChange();
    int LocalAppChangeNode(Mfs_Man_t * p, Abc_Obj_t * pNode);
    Aig_Man_t * ConstructAppAig(Mfs_Man_t * p, Abc_Obj_t * pNode);
};

Aig_Obj_t * Abc_NtkConstructAig_rec(Mfs_Man_t * p, Abc_Obj_t * pNode, Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig(Abc_Obj_t * pObjOld, Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig_rec(Hop_Obj_t * pObj, Aig_Man_t * pMan);
Vec_Ptr_t * App_FindLocalInput(Abc_Obj_t * pNode, int nMax);


#endif
