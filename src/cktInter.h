#ifndef CKT_INTER_H
#define CKT_INTER_H


#include <bits/stdc++.h>
#include <boost/random.hpp>
#include "abcApi.h"
#include "debug_assert.hpp"


class Ckt_Assignment_t
{
private:
    Ckt_Assignment_t &                              operator =                (const Ckt_Assignment_t &);
    Ckt_Assignment_t                                                          (const Ckt_Assignment_t & other);

public:
    abc::Abc_Ntk_t *                                pAbcNtk;
    int                                             patLen;
    std::unordered_map <abc::Abc_Obj_t *, int>      abc2AssId;
    std::vector < std::vector < bool > >            patterns;

    explicit                                        Ckt_Assignment_t          (abc::Abc_Ntk_t * p_abc_ntk);
                                                    ~Ckt_Assignment_t         (void);
    void                                            AddPattern                (const std::vector <bool> & pattern);
    void                                            AddPatternR               (void);
};

std::ostream &                                      operator <<               (std::ostream & os, const Ckt_Assignment_t & assignments);
std::ostream &                                      operator <<               (std::ostream & os, const std::vector <bool> & pattern);
int                                                 Ckt_MfsTest               (abc::Abc_Ntk_t * pNtk, Ckt_Assignment_t & assignments, bool isCareSet);
int                                                 Ckt_MfsResub              (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, Ckt_Assignment_t & assignments, bool isCareSet);
void                                                Ckt_SetMfsPars            (abc::Mfs_Par_t * pPars);
abc::Aig_Man_t *                                    Ckt_ConstructAppAig       (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, Ckt_Assignment_t & assignments, bool isCareSet);
abc::Aig_Obj_t *                                    Ckt_ConstructAppAig_rec   (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, abc::Aig_Man_t * pMan, Ckt_Assignment_t & assignments);
abc::Aig_Obj_t *                                    Ckt_ConstructAppAig2_rec  (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, abc::Aig_Man_t * pMan, Ckt_Assignment_t & assignments);
void                                                Abc_MfsConvertHopToAig    (abc::Abc_Obj_t * pObjOld, abc::Aig_Man_t * pMan);
void                                                Abc_MfsConvertHopToAig2   (abc::Abc_Obj_t * pObjOld, abc::Aig_Man_t * pMan);
void                                                Abc_MfsConvertHopToAig_rec(abc::Hop_Obj_t * pObj, abc::Aig_Man_t * pMan);


#endif
