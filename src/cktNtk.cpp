#include "cktNtk.h"


using namespace std;
using namespace abc;


Ckt_Obj_t::Ckt_Obj_t(Abc_Obj_t * p_abc_obj)
    : pAbcObj(p_abc_obj), type(RenewObjType()), isVisited(false)
{
}


Ckt_Obj_t::~Ckt_Obj_t(void)
{
}


void Ckt_Obj_t::AddFanin(shared_ptr <Ckt_Obj_t> pCktFanin)
{
    pCktFanins.emplace_back(pCktFanin);
    pCktFanin->pCktFanouts.emplace_back(shared_from_this());
}


Ckt_ObjType_t Ckt_Obj_t::RenewObjType(void)
{
    if (Abc_ObjIsPi(pAbcObj))
        return Ckt_ObjType_t::PI;
    if (Abc_ObjIsPo(pAbcObj))
        return Ckt_ObjType_t::PO;
    DEBUG_ASSERT(Abc_ObjIsNode(pAbcObj), module_a{}, "object is not node");
    if ( Abc_NodeIsConst0(pAbcObj) )
        return Ckt_ObjType_t::CONST0;
    else if ( Abc_NodeIsConst1(pAbcObj) )
        return Ckt_ObjType_t::CONST1;
    else
        return Ckt_ObjType_t::INTER;
}


Ckt_MapType_t Ckt_Obj_t::RenewMapType(void)
{
    DEBUG_ASSERT(GetCktNtk()->GetFuncType() == Ckt_Func_t::MAP, module_a{}, "object is not mapped");
    DEBUG_ASSERT(type == Ckt_ObjType_t::INTER, module_a{}, "object is not a node");
    if ( NodeIsBuf() )
        pMapType = make_shared <Ckt_MapType_t> (Ckt_MapType_t::BUF);
    else if ( NodeIsInv() )
        pMapType = make_shared <Ckt_MapType_t> (Ckt_MapType_t::INV);
    else if ( NodeIsXor() )
        pMapType = make_shared <Ckt_MapType_t> (Ckt_MapType_t::XOR);
    else if ( NodeIsXnor() )
        pMapType = make_shared <Ckt_MapType_t> (Ckt_MapType_t::XNOR);
    else if ( NodeIsAnd() ) {
        DEBUG_ASSERT(Abc_ObjFaninNum(pAbcObj) == 2, module_a{}, "# fanin is not correct");
        pMapType = make_shared <Ckt_MapType_t> (Ckt_MapType_t::AND2);
    }
    else if ( NodeIsOr() ) {
        DEBUG_ASSERT(Abc_ObjFaninNum(pAbcObj) == 2, module_a{}, "# fanin is not correct");
        pMapType = make_shared <Ckt_MapType_t> (Ckt_MapType_t::OR2);
    }
    else if ( NodeIsNand() ) {
        DEBUG_ASSERT(Abc_ObjFaninNum(pAbcObj) <= 4, module_a{}, "# fanin is not correct");
        pMapType = make_shared <Ckt_MapType_t> (static_cast <Ckt_MapType_t> ( static_cast <int> (Ckt_MapType_t::NAND2) + Abc_ObjFaninNum(pAbcObj) - 2 ));
    }
    else if ( NodeIsNor() ) {
        DEBUG_ASSERT(Abc_ObjFaninNum(pAbcObj) <= 4, module_a{}, "# fanin is not correct");
        pMapType = make_shared <Ckt_MapType_t> (static_cast <Ckt_MapType_t> ( static_cast <int> (Ckt_MapType_t::NOR2) + Abc_ObjFaninNum(pAbcObj) - 2 ));
    }
    else if ( NodeIsAOI21() )
        pMapType = make_shared <Ckt_MapType_t> (Ckt_MapType_t::AOI21);
    else if ( NodeIsAOI22() )
        pMapType = make_shared <Ckt_MapType_t> (Ckt_MapType_t::AOI22);
    else if ( NodeIsOAI21() )
        pMapType = make_shared <Ckt_MapType_t> (Ckt_MapType_t::OAI21);
    else if ( NodeIsOAI22() )
        pMapType = make_shared <Ckt_MapType_t> (Ckt_MapType_t::OAI22);
    else {
        pMapType = make_shared <Ckt_MapType_t> (Ckt_MapType_t::UNKNOWN);
        DEBUG_ASSERT(0, module_a{}, "unknown gate type");
    }
    return *pMapType;
}


void Ckt_Obj_t::RenewSopFunc(void)
{
    if (IsPI() || IsPO() || IsConst())
        return;
    char * pCube, * pSop = (char *)pAbcObj->pData;
    int Value, v;
    DEBUG_ASSERT(pSop && !Abc_SopIsExorType(pSop), module_a{});
    int nVars = Abc_SopGetVarNum(pSop);
    sopFunc.clear();
    Abc_SopForEachCube(pSop, nVars, pCube) {
        string s = "";
        // for ( v = 0; (pCube[v] != ' ') && (Value = pCube[v]); v++ ) {
        Abc_CubeForEachVar(pCube, Value, v) {
            if (Value == '0' || Value == '1' || Value == '-')
                s += static_cast<char>(Value);
            else
                DEBUG_ASSERT(0, module_a{}, "unknown sop symbol");
        }
        DEBUG_ASSERT(pCube[v] == ' ', module_a{}, "sop form invalid");
        DEBUG_ASSERT(pCube[v + 1] == '1', module_a{}, "only support minterm form sop");
        sopFunc.emplace_back(s);
    }
}


void Ckt_Obj_t::RenewSimValS(void)
{
    switch (type) {
        case Ckt_ObjType_t::PI:
        break;
        case Ckt_ObjType_t::CONST0:
            for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                simValue[i] = 0;
        break;
        case Ckt_ObjType_t::CONST1:
            for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                simValue[i] = static_cast <uint64_t> (ULLONG_MAX);
        break;
        case Ckt_ObjType_t::PO:
            for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                simValue[i] = pCktFanins[0]->simValue[i];
        break;
        case Ckt_ObjType_t::INTER:
            for (auto pCube = sopFunc.begin(); pCube != sopFunc.end(); ++pCube) {
                vector <uint64_t> product(simValue.size(), static_cast <uint64_t> (ULLONG_MAX));
                for (int j = 0; j < static_cast <int> (pCube->length()); ++j) {
                    if ((*pCube)[j] == '0') {
                        for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                            product[i] &= ~(pCktFanins[j]->simValue[i]);
                    }
                    else if ((*pCube)[j] == '1') {
                        for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                            product[i] &= pCktFanins[j]->simValue[i];
                    }
                }
                if (pCube == sopFunc.begin())
                    simValue.assign(product.begin(), product.end());
                else {
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] |= product[i];
                }
            }
        break;
        default:
        DEBUG_ASSERT(0, module_a{}, "unknown object type");
    }
}


void Ckt_Obj_t::RenewSimValM(void)
{
    switch (type) {
        case Ckt_ObjType_t::PI:
        break;
        case Ckt_ObjType_t::PO:
            for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                simValue[i] = pCktFanins[0]->simValue[i];
        break;
        case Ckt_ObjType_t::CONST0:
            for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                simValue[i] = 0;
        break;
        case Ckt_ObjType_t::CONST1:
            for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                simValue[i] = static_cast <uint64_t> (ULLONG_MAX);
        break;
        case Ckt_ObjType_t::INTER:
            switch (*pMapType) {
                case Ckt_MapType_t::BUF:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = pCktFanins[0]->simValue[i];
                break;
                case Ckt_MapType_t::INV:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~pCktFanins[0]->simValue[i];
                break;
                case Ckt_MapType_t::XOR:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = pCktFanins[0]->simValue[i] ^ pCktFanins[1]->simValue[i];
                break;
                case Ckt_MapType_t::XNOR:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~(pCktFanins[0]->simValue[i] ^ pCktFanins[1]->simValue[i]);
                break;
                case Ckt_MapType_t::AND2:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = pCktFanins[0]->simValue[i] & pCktFanins[1]->simValue[i];
                break;
                case Ckt_MapType_t::OR2:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = pCktFanins[0]->simValue[i] | pCktFanins[1]->simValue[i];
                break;
                case Ckt_MapType_t::NAND2:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~(pCktFanins[0]->simValue[i] & pCktFanins[1]->simValue[i]);
                break;
                case Ckt_MapType_t::NAND3:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~(pCktFanins[0]->simValue[i] & pCktFanins[1]->simValue[i] & pCktFanins[2]->simValue[i]);
                break;
                case Ckt_MapType_t::NAND4:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~(pCktFanins[0]->simValue[i] & pCktFanins[1]->simValue[i] & pCktFanins[2]->simValue[i] & pCktFanins[3]->simValue[i]);
                break;
                case Ckt_MapType_t::NOR2:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~(pCktFanins[0]->simValue[i] | pCktFanins[1]->simValue[i]);
                break;
                case Ckt_MapType_t::NOR3:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~(pCktFanins[0]->simValue[i] | pCktFanins[1]->simValue[i] | pCktFanins[2]->simValue[i]);
                break;
                case Ckt_MapType_t::NOR4:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~(pCktFanins[0]->simValue[i] | pCktFanins[1]->simValue[i] | pCktFanins[2]->simValue[i] | pCktFanins[3]->simValue[i]);
                break;
                case Ckt_MapType_t::AOI21:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~((pCktFanins[0]->simValue[i] & pCktFanins[1]->simValue[i]) | pCktFanins[2]->simValue[i]);
                break;
                case Ckt_MapType_t::AOI22:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~((pCktFanins[0]->simValue[i] & pCktFanins[1]->simValue[i]) | (pCktFanins[2]->simValue[i] & pCktFanins[3]->simValue[i]));
                break;
                case Ckt_MapType_t::OAI21:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~((pCktFanins[0]->simValue[i] | pCktFanins[1]->simValue[i]) & pCktFanins[2]->simValue[i]);
                break;
                case Ckt_MapType_t::OAI22:
                    for (int i = 0; i < static_cast <int> (simValue.size()); ++i)
                        simValue[i] = ~((pCktFanins[0]->simValue[i] | pCktFanins[1]->simValue[i]) & (pCktFanins[2]->simValue[i] | pCktFanins[3]->simValue[i]));
                break;
                default:
                    DEBUG_ASSERT(0, module_a{}, "unknown gate type");
            }
        break;
        default:
            DEBUG_ASSERT(0, module_a{}, "unknown object type");
    }
}


bool Ckt_Obj_t::NodeIsBuf(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    return Abc_SopIsBuf(pSop);
}


bool Ckt_Obj_t::NodeIsInv(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    return Abc_SopIsInv(pSop);
}


bool Ckt_Obj_t::NodeIsAnd(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    if ( !Abc_SopIsComplement(pSop) ) {    //111 1
        char * pCur;
        if ( Abc_SopGetCubeNum(pSop) != 1 )
            return 0;
        for ( pCur = pSop; *pCur != ' '; pCur++ )
            if ( *pCur != '1' )
                return 0;
    }
    else {    //0-- 0\n-0- 0\n--0 0\n
        char * pCube, * pCur;
        int nVars, nLits;
        nVars = Abc_SopGetVarNum( pSop );
        if ( nVars != Abc_SopGetCubeNum(pSop) )
            return 0;
        Abc_SopForEachCube( pSop, nVars, pCube )
        {
            nLits = 0;
            for ( pCur = pCube; *pCur != ' '; pCur++ ) {
                if ( *pCur ==  '0' )
                    nLits ++;
                else if ( *pCur == '-' )
                    continue;
                else
                    return 0;
            }
            if ( nLits != 1 )
                return 0;
        }
    }
    return 1;
}


bool Ckt_Obj_t::NodeIsOr(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    if ( Abc_SopIsComplement(pSop) ) {    //000 0
        char * pCur;
        if ( Abc_SopGetCubeNum(pSop) != 1 )
            return 0;
        for ( pCur = pSop; *pCur != ' '; pCur++ )
            if ( *pCur != '0' )
                return 0;
    }
    else {    //1-- 1\n-1- 1\n--1 1\n
        char * pCube, * pCur;
        int nVars, nLits;
        nVars = Abc_SopGetVarNum( pSop );
        if ( nVars != Abc_SopGetCubeNum(pSop) )
            return 0;
        Abc_SopForEachCube( pSop, nVars, pCube )
        {
            nLits = 0;
            for ( pCur = pCube; *pCur != ' '; pCur++ ) {
                if ( *pCur ==  '1' )
                    nLits ++;
                else if ( *pCur == '-' )
                    continue;
                else
                    return 0;
            }
            if ( nLits != 1 )
                return 0;
        }
    }
    return 1;
}


bool Ckt_Obj_t::NodeIsNand(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    if ( Abc_SopIsComplement(pSop) ) {    //111 0
        char * pCur;
        if ( Abc_SopGetCubeNum(pSop) != 1 )
            return 0;
        for ( pCur = pSop; *pCur != ' '; pCur++ )
            if ( *pCur != '1' )
                return 0;
    }
    else {    //0-- 1\n-0- 1\n--0 1\n
        char * pCube, * pCur;
        int nVars, nLits;
        nVars = Abc_SopGetVarNum( pSop );
        if ( nVars != Abc_SopGetCubeNum(pSop) )
            return 0;
        Abc_SopForEachCube( pSop, nVars, pCube )
        {
            nLits = 0;
            for ( pCur = pCube; *pCur != ' '; pCur++ ) {
                if ( *pCur ==  '0' )
                    nLits ++;
                else if ( *pCur == '-' )
                    continue;
                else
                    return 0;
            }
            if ( nLits != 1 )
                return 0;
        }
    }
    return 1;
}


bool Ckt_Obj_t::NodeIsNor(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    if ( !Abc_SopIsComplement(pSop) ) {    //000 1
        char * pCur;
        if ( Abc_SopGetCubeNum(pSop) != 1 )
            return 0;
        for ( pCur = pSop; *pCur != ' '; pCur++ )
            if ( *pCur != '0' )
                return 0;
    }
    else {    //1-- 0\n-1- 0\n--1 0\n
        char * pCube, * pCur;
        int nVars, nLits;
        nVars = Abc_SopGetVarNum( pSop );
        if ( nVars != Abc_SopGetCubeNum(pSop) )
            return 0;
        Abc_SopForEachCube( pSop, nVars, pCube )
        {
            nLits = 0;
            for ( pCur = pCube; *pCur != ' '; pCur++ ) {
                if ( *pCur ==  '1' )
                    nLits ++;
                else if ( *pCur == '-' )
                    continue;
                else
                    return 0;
            }
            if ( nLits != 1 )
                return 0;
        }
    }
    return 1;
}


bool Ckt_Obj_t::NodeIsXor(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    if ( !Abc_SopIsComplement(pSop) ) {    //01 1\n10 1\n
        char * pCube, * pCur;
        int nVars, nLits;
        nVars = Abc_SopGetVarNum( pSop );
        if ( nVars != 2 )
            return 0;
        Abc_SopForEachCube( pSop, nVars, pCube )
        {
            nLits = 0;
            for ( pCur = pCube; *pCur != ' '; pCur++ ) {
                if ( *pCur ==  '1' )
                    nLits ++;
                else if ( *pCur == '-' )
                    return 0;
            }
            if ( nLits != 1 )
                return 0;
        }
    }
    else
        return 0;
    return 1;
}


bool Ckt_Obj_t::NodeIsXnor(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    if ( strcmp( pSop, "00 1\n11 1\n" ) == 0 )
        return 1;
    else if ( strcmp( pSop, "11 1\n00 1\n" ) == 0 )
        return 1;
    else
        return 0;
}


bool Ckt_Obj_t::NodeIsAOI21(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    if ( strcmp( pSop, "-00 1\n0-0 1\n" ) == 0 )
        return 1;
    else
        return 0;
}


bool Ckt_Obj_t::NodeIsAOI22(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    if ( strcmp( pSop, "--11 0\n11-- 0\n" ) == 0 )
        return 1;
    else
        return 0;
}


bool Ckt_Obj_t::NodeIsOAI21(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    if ( strcmp( pSop, "--0 1\n00- 1\n" ) == 0 )
        return 1;
    else
        return 0;
}


bool Ckt_Obj_t::NodeIsOAI22(void)
{
    char * pSop  = Mio_GateReadSop( (Mio_Gate_t *)pAbcObj->pData );
    if ( strcmp( pSop, "--00 1\n00-- 1\n" ) == 0 )
        return 1;
    else
        return 0;
}


Ckt_Ntk_t::Ckt_Ntk_t(Abc_Ntk_t * p_abc_ntk)
{
    pAbcNtk = Abc_NtkDup(p_abc_ntk);
}


Ckt_Ntk_t::~Ckt_Ntk_t(void)
{
    if (pAbcNtk != nullptr)
        Abc_NtkDelete(pAbcNtk);
}


void Ckt_Ntk_t::Init(int frame_number)
{
    // funcType
    if (Abc_NtkIsSopLogic(pAbcNtk))
        funcType = Ckt_Func_t::SOP;
    else if (Abc_NtkIsMappedLogic(pAbcNtk))
        funcType = Ckt_Func_t::MAP;
    else
        DEBUG_ASSERT(0, module_a{}, "unknown function type");

    // pCktObjs
    Abc_Obj_t * pAbcObj;
    int i;
    typedef unordered_map < Abc_Obj_t *, shared_ptr <Ckt_Obj_t> > umap;
    umap m;
    Abc_NtkForEachObj(pAbcNtk, pAbcObj, i) {
        shared_ptr <Ckt_Obj_t> pCktObj = make_shared <Ckt_Obj_t> (pAbcObj);
        AddObj(pCktObj);
        m.insert(umap::value_type(pAbcObj, pCktObj));
    }

    // pCktPis / pCktPos
    for (auto & pCktObj : pCktObjs) {
        if (Abc_ObjIsPi(pCktObj->GetAbcObj()))
            pCktPis.emplace_back(pCktObj);
        else if (Abc_ObjIsPo(pCktObj->GetAbcObj()))
            pCktPos.emplace_back(pCktObj);
    }

    // pCktFanins / pCktFanouts
    for (auto & pCktObj : pCktObjs) {
        Abc_ObjForEachFanin(pCktObj->GetAbcObj(), pAbcObj, i) {
            umap::const_iterator ppCktObj = m.find(pAbcObj);
            DEBUG_ASSERT(ppCktObj != m.end(), module_a{}, "object not found");
            pCktObj->AddFanin(ppCktObj->second);
        }
    }

    // pMapType
    if (funcType == Ckt_Func_t::MAP) {
        for (auto & pCktObj : pCktObjs) {
            if (pCktObj->IsNode())
                pCktObj->RenewMapType();
        }
    }

    // sopFunc
    if (funcType == Ckt_Func_t::SOP) {
        for (auto & pCktObj : pCktObjs) {
            if (pCktObj->IsNode())
                pCktObj->RenewSopFunc();
        }
    }

    // simValue
    nSim = frame_number >> 6;
    for (auto & pCktObj : pCktObjs)
        pCktObj->InitSim(nSim);
}


void Ckt_Ntk_t::AddObj(shared_ptr <Ckt_Obj_t> pCktObj)
{
    pCktObjs.emplace_back(pCktObj);
    pCktObj->SetCktNtk(shared_from_this());
}


void Ckt_Ntk_t::PrintObjs(void) const
{
    for (auto & pCktObj : pCktObjs)
        cout << pCktObj << endl;
}


void Ckt_Ntk_t::GenInputDist(unsigned seed)
{
    boost::uniform_int <> distribution(0, 1);
    boost::random::mt19937 engine(seed);
    boost::variate_generator <boost::random::mt19937, boost::uniform_int <> > randomPI(engine, distribution);
    for (auto & pCktPi : pCktPis) {
        DEBUG_ASSERT(pCktPi->GetSimNum() == GetSimNum(), module_a{}, "simulation number not equal");
        for (int i = 0; i < nSim; ++i) {
            uint64_t value = 0;
            for (int j = 0; j < 64; ++j) {
                if (randomPI())
                    Ckt_SetBit(value, static_cast <uint64_t> (j));
            }
            pCktPi->SetSimVal(i, value);
        }
    }
}


void Ckt_Ntk_t::SetUnvisited(void)
{
    for (auto & pCktObj : pCktObjs)
        pCktObj->SetUnvisited();
}


void Ckt_Ntk_t::SortObjects(vector < shared_ptr <Ckt_Obj_t> > & pTopoObjs)
{
    pTopoObjs.clear();
    // reset traversal flag
    SetUnvisited();
    // start a vector of nodes
    pTopoObjs.reserve(GetObjNum());
    // topological sort
    for (auto & pCktPo : pCktPos)
        DFS(pCktPo, pTopoObjs);
}


void Ckt_Ntk_t::DFS(shared_ptr <Ckt_Obj_t> pCktObj, vector < shared_ptr <Ckt_Obj_t> > & pTopoObjs)
{
    // if this node is already visited, skip
    if (pCktObj->GetVisited())
        return;
    // mark the node as visited
    pCktObj->SetVisited();
    // visit the transitive fanin of the node
    for (int i = 0; i < pCktObj->GetFaninNum(); ++i)
        DFS(pCktObj->GetFanin(i), pTopoObjs);
    // add the node after the fanins have been added
    pTopoObjs.emplace_back(pCktObj);
    // pCktObj->SetTopoId(static_cast <int> (pTopoObjs.size()));
}


void Ckt_Ntk_t::FeedForward(void)
{
    // get topological order
    vector < shared_ptr <Ckt_Obj_t> > pTopoObjs;
    SortObjects(pTopoObjs);
    // update
    if (funcType == Ckt_Func_t::SOP) {
        for (auto & pCktObj : pTopoObjs)
            pCktObj->RenewSimValS();
    }
    else if (funcType == Ckt_Func_t::MAP) {
        for (auto & pCktObj : pTopoObjs)
            pCktObj->RenewSimValM();
    }
}


void Ckt_Ntk_t::FeedForward(vector < shared_ptr <Ckt_Obj_t> > & pTopoObjs)
{
    if (funcType == Ckt_Func_t::SOP) {
        for (auto & pCktObj : pTopoObjs)
            pCktObj->RenewSimValS();
    }
    else if (funcType == Ckt_Func_t::MAP) {
        for (auto & pCktObj : pTopoObjs)
            pCktObj->RenewSimValM();
    }
}


void Ckt_Ntk_t::TestSimSpeed(void)
{
    GenInputDist();
    vector < shared_ptr <Ckt_Obj_t> > pTopoObjs;
    SortObjects(pTopoObjs);
    clock_t st = clock();
    FeedForward(pTopoObjs);
    clock_t ed = clock();
    cout << "circuit = " << GetName() << endl;
    cout << "frame number = " << nSim * 64 << endl;
    cout << "time = " << ed - st << " us" << endl;
}


void Ckt_Ntk_t::CheckSim(void)
{
    // only for c6288
    DEBUG_ASSERT(string(pAbcNtk->pName) == "c6288", module_a{}, "circuit is not c6288");
    DEBUG_ASSERT(pCktPis.size() == 32 && pCktPos.size() == 32, module_a{}, "# i/o is not correct");
    GenInputDist();
    FeedForward();
    for (int i = 0; i < nSim; ++i) {
        for (int j = 0; j < 64; ++j) {
            uint64_t f1 = 0;
            for (int k = 15; k >= 0; --k) {
                if (Ckt_GetBit(pCktPis[k]->GetSimVal(i), static_cast <uint64_t> (j)))
                    Ckt_SetBit(f1, static_cast <uint64_t> (k));
            }
            uint64_t f2 = 0;
            for (int k = 31; k >= 16; --k) {
                if (Ckt_GetBit(pCktPis[k]->GetSimVal(i), static_cast <uint64_t> (j)))
                    Ckt_SetBit(f2, static_cast <uint64_t> (k - 16));
            }
            uint64_t res = 0;
            for (int k = 29; k >= 0; --k) {
                if (Ckt_GetBit(pCktPos[k]->GetSimVal(i), static_cast <uint64_t> (j)))
                    Ckt_SetBit(res, static_cast <uint64_t> (k));
            }
            if (Ckt_GetBit(pCktPos[30]->GetSimVal(i), static_cast <uint64_t> (j)))
                Ckt_SetBit(res, static_cast <uint64_t> (31));
            if (Ckt_GetBit(pCktPos[31]->GetSimVal(i), static_cast <uint64_t> (j)))
                Ckt_SetBit(res, static_cast <uint64_t> (30));
            cout << f1 << "*" << f2 << "=" << res << endl;
            DEBUG_ASSERT(f1 * f2 == res, module_a{}, "the logic simulator is somewhere wrong");
        }
    }
}


float Ckt_Ntk_t::MeasureError(shared_ptr <Ckt_Ntk_t> pRefNtk)
{
    // make sure POs are same
    DEBUG_ASSERT(pCktPos.size() == pRefNtk->pCktPos.size(), module_a{}, "# pos are not equal");
    int poNum = static_cast <int> (pCktPos.size());
    for (int i = 0; i < poNum; ++i)
        DEBUG_ASSERT(pCktPos[i]->GetName() == pRefNtk->pCktPos[i]->GetName(), module_a{}, "pos are different");
    // logic simulation
    DEBUG_ASSERT(nSim == pRefNtk->GetSimNum(), module_a{}, "# pos are not equal");
    GenInputDist(314);
    FeedForward();
    pRefNtk->GenInputDist(314);
    pRefNtk->FeedForward();
    // compare simulation value of POs
    int ret = 0;
    for (int k = 0; k < nSim; ++k) {
        uint64_t temp = 0;
        for (int i = 0; i < poNum; ++i)
            temp |= pCktPos[i]->GetSimVal(k) ^ pRefNtk->pCktPos[i]->GetSimVal(k);
        ret += Ckt_CountOneNum(temp);
    }
    return ret / 64.0 / static_cast<float> (nSim);
}
