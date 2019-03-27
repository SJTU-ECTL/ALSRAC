#ifndef CKT_PI_DC_H
#define CKT_PI_DC_H


#include <bits/stdc++.h>
#include <boost/random.hpp>
#include "abcApi.h"
#include "debug_assert.hpp"


class Ckt_Set_t
{
private:
    Ckt_Set_t &                                     operator =                (const Ckt_Set_t &);
    Ckt_Set_t                                                                 (const Ckt_Set_t & other);

public:
    abc::Abc_Ntk_t *                                pAbcNtk;
    int                                             patLen;
    bool                                            isCareSet;
    std::unordered_map <abc::Abc_Obj_t *, int>      abc2PatId;
    std::vector < std::string >                     patterns;

    explicit                                        Ckt_Set_t                 (abc::Abc_Ntk_t * p_abc_ntk, bool is_care_set);
                                                    ~Ckt_Set_t                (void);
    void                                            AddPattern                (const std::string & pattern);
    void                                            AddPatternR               (void);
    void                                            AddPatternR2              (int nDC);
};

std::ostream &                                      operator <<               (std::ostream & os, const Ckt_Set_t & cktSet);
int                                                 Ckt_MfsTest               (abc::Abc_Ntk_t * pNtk, Ckt_Set_t & cktSet);
int                                                 Ckt_MfsResub              (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, Ckt_Set_t & cktSet);
void                                                Ckt_SetMfsPars            (abc::Mfs_Par_t * pPars);
abc::Aig_Man_t *                                    Ckt_ConstructAppAig       (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, Ckt_Set_t & cktSet);
abc::Aig_Obj_t *                                    Ckt_ConstructAppAig_rec   (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, abc::Aig_Man_t * pMan, Ckt_Set_t & cktSet);
abc::Aig_Obj_t *                                    Ckt_ConstructAppAig2_rec  (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, abc::Aig_Man_t * pMan, Ckt_Set_t & cktSet);
void                                                Abc_MfsConvertHopToAig    (abc::Abc_Obj_t * pObjOld, abc::Aig_Man_t * pMan);
void                                                Abc_MfsConvertHopToAig2   (abc::Abc_Obj_t * pObjOld, abc::Aig_Man_t * pMan);
void                                                Abc_MfsConvertHopToAig_rec(abc::Hop_Obj_t * pObj, abc::Aig_Man_t * pMan);


#endif
