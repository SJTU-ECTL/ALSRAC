#ifndef CKT_NTK_H
#define CKT_NTK_H


#include <bits/stdc++.h>
#include <boost/random.hpp>
#include "abcApi.h"
#include "cktBit.h"
#include "debug_assert.hpp"


enum class Ckt_Func_t
{
    SOP, MAP, AIG
};


enum class Ckt_ObjType_t
{
    PI, PO, CONST0, CONST1, INTER
};


enum class Ckt_MapType_t
{
    BUF,  INV,   XOR,    XNOR,   AND2,
    OR2,  NAND2, NAND3,  NAND4,  NOR2,
    NOR3, NOR4,  AOI21,  AOI22,  OAI21,
    OAI22,UNKNOWN
};


class Ckt_Ntk_t;


class Ckt_Obj_t
    : public std::enable_shared_from_this <Ckt_Obj_t>
{
private:
    // common
    abc::Abc_Obj_t *                                        pAbcObj;                                                                    // the ABC object
    std::weak_ptr <Ckt_Ntk_t>                               pCktNtk;
    Ckt_ObjType_t                                           type;
    int                                                     nSim;
    bool                                                    isVisited;
    std::vector < std::shared_ptr <Ckt_Obj_t> >             pCktFanins;
    std::vector < std::weak_ptr <Ckt_Obj_t> >               pCktFanouts;
    std::vector <uint64_t>                                  simValue;

    // mapped network
    std::shared_ptr <Ckt_MapType_t>                         pMapType;

    // sop network
    std::shared_ptr <bool>                                  isCompl;

    Ckt_Obj_t &                                             operator =  (const Ckt_Obj_t &);                                            // forbid assignment constructor
    Ckt_Obj_t                                                           (const Ckt_Obj_t & other);                                      // forbid copy constructor

public:
    explicit                                                Ckt_Obj_t   (abc::Abc_Obj_t * p_abc_obj);
                                                            ~Ckt_Obj_t  (void);

    void                                                    AddFanin    (std::shared_ptr <Ckt_Obj_t> pCktFanin);
    Ckt_ObjType_t                                           RenewObjType(void);
    Ckt_MapType_t                                           RenewMapType(void);
    bool                                                    RenewIsCompl(void);
    void                                                    GetSubCktRec(abc::Hop_Obj_t * pObj, std::vector <abc::Hop_Obj_t *> & vNodes, std::set <int> & sTerNodes);
    void                                                    RenewSimValS(void);
    void                                                    RenewSimValM(void);
    bool                                                    NodeIsBuf   (void);
    bool                                                    NodeIsInv   (void);
    bool                                                    NodeIsAnd   (void);
    bool                                                    NodeIsOr    (void);
    bool                                                    NodeIsNand  (void);
    bool                                                    NodeIsNor   (void);
    bool                                                    NodeIsXor   (void);
    bool                                                    NodeIsXnor  (void);
    bool                                                    NodeIsAOI21 (void);
    bool                                                    NodeIsAOI22 (void);
    bool                                                    NodeIsOAI21 (void);
    bool                                                    NodeIsOAI22 (void);

    inline void                                             InitSim     (int n_sim)                                                     {nSim = n_sim; simValue.resize(nSim);}
    inline std::shared_ptr <Ckt_Ntk_t>                      GetCktNtk   (void) const                                                    {return pCktNtk.lock();}
    inline abc::Abc_Obj_t *                                 GetAbcObj   (void) const                                                    {return pAbcObj;}
    inline std::string                                      GetName     (void) const                                                    {return std::string(Abc_ObjName(pAbcObj));}
    inline Ckt_ObjType_t                                    GetType     (void) const                                                    {return type;}
    inline std::shared_ptr <Ckt_MapType_t>                  GetMapType  (void) const                                                    {return pMapType;}
    inline int                                              GetSimNum   (void) const                                                    {return nSim;}
    inline uint64_t                                         GetSimVal   (int i) const                                                   {return simValue[i];}
    inline bool                                             GetSimVal   (int i, int j) const                                            {return Ckt_GetBit(simValue[i], j);}
    inline bool                                             GetVisited  (void) const                                                    {return isVisited;}
    inline std::shared_ptr <Ckt_Obj_t>                      GetFanin    (int i = 0) const                                               {return pCktFanins[i];}
    inline int                                              GetFaninNum (void) const                                                    {return static_cast <int> (pCktFanins.size());}
    inline std::shared_ptr <Ckt_Obj_t>                      GetFanout   (int i = 0) const                                               {return pCktFanouts[i].lock();}
    inline int                                              GetFanoutNum(void) const                                                    {return static_cast <int> (pCktFanouts.size());}
    inline bool                                             IsPI        (void) const                                                    {return type == Ckt_ObjType_t::PI;}
    inline bool                                             IsPO        (void) const                                                    {return type == Ckt_ObjType_t::PO;}
    inline bool                                             IsConst     (void) const                                                    {return type == Ckt_ObjType_t::CONST0 || type == Ckt_ObjType_t::CONST1;}
    inline bool                                             IsConst0    (void) const                                                    {return type == Ckt_ObjType_t::CONST0;}
    inline bool                                             IsConst1    (void) const                                                    {return type == Ckt_ObjType_t::CONST1;}
    inline bool                                             IsNode      (void) const                                                    {return type == Ckt_ObjType_t::CONST0 || type == Ckt_ObjType_t::CONST1 || type == Ckt_ObjType_t::INTER;}
    inline bool                                             IsInter     (void) const                                                    {return type == Ckt_ObjType_t::INTER;}
    inline void                                             SetSimVal   (int i, uint64_t value)                                         {simValue[i] = value;}
    inline void                                             SetCktNtk   (std::shared_ptr <Ckt_Ntk_t> p_ckt_ntk)                         {pCktNtk = p_ckt_ntk;}
    inline void                                             SetUnvisited(void)                                                          {isVisited = false;}
    inline void                                             SetVisited  (void)                                                          {isVisited = true;}
    inline void                                             ResizeSimVal(int len)                                                       {simValue.resize(len);}
    inline void                                             FlipSimVal  (int i)                                                         {simValue[i] = ~simValue[i];}
};


class Ckt_Ntk_t
    : public std::enable_shared_from_this <Ckt_Ntk_t>
{
private:
    abc::Abc_Ntk_t *                                        pAbcNtk;                                                                    // the ABC network
    bool                                                    isDupAbcNtk;
    int                                                     nSim;
    Ckt_Func_t                                              funcType;
    std::vector < std::shared_ptr <Ckt_Obj_t> >             pCktObjs;                                                                   // CKT objects
    std::vector < std::shared_ptr <Ckt_Obj_t> >             pCktPis;
    std::vector < std::shared_ptr <Ckt_Obj_t> >             pCktPos;
    std::unordered_map < int, std::shared_ptr <Ckt_Obj_t> > abcId2Ckt;


    Ckt_Ntk_t &                                             operator =  (const Ckt_Ntk_t &);                                            // forbid assignment constructor
                                                            Ckt_Ntk_t   (const Ckt_Ntk_t & other);                                      // forbid copy constructor

public:
    explicit                                                Ckt_Ntk_t   (abc::Abc_Ntk_t * p_abc_ntk, bool is_dup_abc_ntk = true);
                                                            ~Ckt_Ntk_t  (void);

    void                                                    Init        (int frame_number = 10240);
    void                                                    AddObj      (std::shared_ptr <Ckt_Obj_t> pCktObj);
    void                                                    PrintObjs   (void) const;
    void                                                    GenInputDist(unsigned seed = 314);
    void                                                    SetUnvisited(void);
    void                                                    SortObjects (std::vector < std::shared_ptr <Ckt_Obj_t> > & pTopoObjs);
    void                                                    DFS         (std::shared_ptr <Ckt_Obj_t> pCktObj, std::vector < std::shared_ptr <Ckt_Obj_t> > & pTopoObjs);
    void                                                    FeedForward (void);
    void                                                    FeedForward (std::vector < std::shared_ptr <Ckt_Obj_t> > & pTopoObjs);
    void                                                    LogicSim    (bool isVerbose = true);
    void                                                    CheckSim    (void);
    float                                                   MeasureError(std::shared_ptr <Ckt_Ntk_t> pRefNtk, int seed = 314);


    inline std::string                                      GetName     (void) const                                                    {return std::string(pAbcNtk->pName);}
    inline abc::Abc_Ntk_t *                                 GetAbcNtk   (void) const                                                    {return pAbcNtk;}
    inline Ckt_Func_t                                       GetFuncType (void) const                                                    {return funcType;}
    inline int                                              GetSimNum   (void) const                                                    {return nSim;}
    inline int                                              GetObjNum   (void) const                                                    {return static_cast <int> (pCktObjs.size());}
    inline int                                              GetPiNum    (void) const                                                    {return static_cast <int> (pCktPis.size());}
    inline int                                              GetPoNum    (void) const                                                    {return static_cast <int> (pCktPos.size());}
    inline std::shared_ptr <Ckt_Obj_t>                      GetPi       (int i) const                                                   {return pCktPis[i];}
    inline std::shared_ptr <Ckt_Obj_t>                      GetPo       (int i) const                                                   {return pCktPos[i];}
};


std::ostream &                                              operator << (std::ostream & os, const std::shared_ptr <Ckt_Obj_t> pCktObj);
std::ostream &                                              operator << (std::ostream & os, const Ckt_MapType_t mapType);
std::ostream &                                              operator << (std::ostream & os, const Ckt_ObjType_t objType);


#endif
