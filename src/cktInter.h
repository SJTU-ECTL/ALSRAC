#ifndef CKT_INTER_H
#define CKT_INTER_H


#include <bits/stdc++.h>
#include <boost/random.hpp>
#include "abcApi.h"
#include "debug_assert.hpp"


class Ckt_DC_t
{
private:
    Ckt_DC_t &                                      operator =                (const Ckt_DC_t &);
    Ckt_DC_t                                                                  (const Ckt_DC_t & other);

public:
    abc::Abc_Ntk_t *                                pAbcNtk;
    int                                             patLen;
    std::unordered_map <int, int>                   abc2DC;
    std::vector < std::vector < bool > >            patterns;

    explicit                                        Ckt_DC_t                  (abc::Abc_Ntk_t * p_abc_ntk);
                                                    ~Ckt_DC_t                 (void);
    void                                            AddPattern                (const std::vector <bool> & pattern);
    void                                            AddPatternR               ();
};

std::ostream &                                      operator <<               (std::ostream & os, const Ckt_DC_t & dc);
std::ostream &                                      operator <<               (std::ostream & os, const std::vector <bool> & pattern);
int                                                 Ckt_MfsTest               (abc::Abc_Ntk_t * pNtk, Ckt_DC_t & dc);
int                                                 Ckt_MfsResub              (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, Ckt_DC_t & dc);
void                                                Ckt_SetMfsPars            (abc::Mfs_Par_t * pPars);
abc::Aig_Man_t *                                    Ckt_ConstructAppAig       (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, Ckt_DC_t & dc);
abc::Aig_Obj_t *                                    Ckt_ConstructAppAig_rec   (abc::Mfs_Man_t * p, abc::Abc_Obj_t * pNode, abc::Aig_Man_t * pMan, Ckt_DC_t & dc);
void                                                Abc_MfsConvertHopToAig    (abc::Abc_Obj_t * pObjOld, abc::Aig_Man_t * pMan);
void                                                Abc_MfsConvertHopToAig_rec(abc::Hop_Obj_t * pObj, abc::Aig_Man_t * pMan);


#endif
