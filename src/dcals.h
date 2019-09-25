#ifndef DCALS_H
#define DCALS_H

#include <bits/stdc++.h>
#include "abcApi.h"
#include "simulator.h"
#include "cktNtk.h"


class Dcals_Man_t
{
private:
    abc::Abc_Ntk_t * pOriNtk;
    abc::Abc_Ntk_t * pAppNtk;
    int nFrame;
    int cutSize;
    double metric;
    double metricBound;
    abc::Mfs_Par_t * pPars;
public:
    explicit Dcals_Man_t(abc::Abc_Ntk_t * pNtk, int nFrame, int cutSize, double metricBound);
    ~Dcals_Man_t();
    abc::Mfs_Par_t * InitMfsPars();
    void DCALS();
    void LocalAppChange();
    int LocalAppChangeNode(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode);
    abc::Aig_Man_t * ConstructAppAig(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode);
};

abc::Aig_Obj_t * Abc_NtkConstructAig_rec(abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, abc::Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig(abc::Abc_Obj_t * pObjOld, abc::Aig_Man_t * pMan);
void Abc_MfsConvertHopToAig_rec(abc::Hop_Obj_t * pObj, abc::Aig_Man_t * pMan);
abc::Vec_Ptr_t * App_FindLocalInput(abc::Abc_Obj_t * pNode, int nMax);


#endif
