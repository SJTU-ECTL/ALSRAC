#include "cktFullSimplify.h"

using namespace std;
using namespace abc;
using namespace cmdline;


parser Cmdline_Parser(int argc, char * argv[])
{
    parser option;
    option.add <string>    ("input",   'i', "Original Circuit file",    true);
    option.add <string>    ("output",  'o', "Approximate Circuit file", false);
    option.add <string>    ("genlib",  'g', "Map libarary file",        false, "data/genlib/mcnc.genlib");
    option.add <int>       ("nFrame",  'f', "Simulation Frame number",  false, 10240, range(1, INT_MAX));
    option.add <int>       ("level",   'l', "TFO level",                false, 3,     range(0, INT_MAX));
    option.add <int>       ("nLocalPI",'m', "Local PI number",          false, 10,    range(1, INT_MAX));
    option.parse_check(argc, argv);
    return option;
}


void Ckt_FullSimplifyTest(int argc, char * argv[])
{
    parser option = Cmdline_Parser(argc, argv);
    string input = option.get <string> ("input");
    string output = option.get <string> ("output");
    string genlib = option.get <string> ("genlib");
    int nFrame = option.get <int> ("nFrame");
    int level = option.get <int> ("level");
    int nLocalPI = option.get <int> ("nLocalPI");

    Abc_Start();

    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    string command = "read_genlib -v " + genlib;
    DASSERT(Cmd_CommandExecute(pAbc, command.c_str()) == 0);
    command = "read_blif " + input;
    DASSERT(Cmd_CommandExecute(pAbc, command.c_str()) == 0);
    Abc_Ntk_t * pOriNtk = Abc_FrameReadNtk(pAbc);
    Abc_NtkCleanup(pOriNtk, 0);
    shared_ptr <Ckt_Ntk_t> pCktNtk = make_shared <Ckt_Ntk_t> (pOriNtk);
    shared_ptr <Ckt_Ntk_t> pCktNtkOri = make_shared <Ckt_Ntk_t> (pOriNtk);
    pCktNtkOri->Init(102400);

    pCktNtk->Init(nFrame);
    pCktNtk->LogicSim(false);

    vector < shared_ptr <Ckt_Obj_t> > vRoots;
    vector < shared_ptr <Ckt_Obj_t> > vSupp;
    vector < shared_ptr <Ckt_Obj_t> > vNodes;
    vector < shared_ptr <Ckt_Obj_t> > vMffc;
    DASSERT(system("if [ ! -d tmp ]; then mkdir tmp; fi") != -1);
    DASSERT(system("rm tmp/*") != -1);
    for (int i = 0; i < pCktNtk->GetObjNum(); ++i) {
        shared_ptr <Ckt_Obj_t> pPivot = pCktNtk->GetObj(i);
        if (pPivot->IsPIO())
            continue;
        Ckt_ComputeRoot(pPivot, vRoots, level, nLocalPI);
        Ckt_ComputeSupport(vRoots, vSupp, nLocalPI);
        Ckt_CollectNodes(vRoots, vSupp, vNodes);
        string fileName = "./tmp/" + pPivot->GetName();
        Ckt_GenerateNtk(vRoots, vSupp, vNodes, fileName);
        if (!vNodes.size())
            continue;
        Ckt_SimplifyNtk(fileName);
        // Abc_Ntk_t * pTmpNtk = Abc_NtkDup(pCktNtk->GetAbcNtk());
        // int nLitsBef = Abc_NtkGetLitNum(pTmpNtk);
        // Ckt_ReplaceNtk(pTmpNtk, vRoots, vNodes, fileName + "_out.blif");
        // int nLitsAft = Abc_NtkGetLitNum(pTmpNtk);
        // shared_ptr <Ckt_Ntk_t> pCktNtkTmp = make_shared <Ckt_Ntk_t> (pTmpNtk);
        // pCktNtkTmp->Init(102400);
        // Abc_NtkDelete(pTmpNtk);

        // cout << pPivot << "," << nLitsBef << "," << nLitsAft << "," << pCktNtkTmp->MeasureError(pCktNtkOri) << endl;
    }

    Abc_Stop();
}


void Ckt_ComputeRoot(shared_ptr <Ckt_Obj_t> pCktObj, vector < shared_ptr <Ckt_Obj_t> > & vRoots, int nTfoLevel, int nMaxFO)
{
    vRoots.clear();
    DASSERT(!pCktObj->IsPO(), "Error: cannot traverse the fanout cone of PO");
    pCktObj->GetCktNtk()->SetUnvisited();
    Ckt_ComputeRoot_Rec(pCktObj, vRoots, nTfoLevel, nMaxFO);
}


void Ckt_ComputeRoot_Rec(shared_ptr <Ckt_Obj_t> pCktObj, vector < shared_ptr <Ckt_Obj_t> > & vRoots, int nTfoLevel, int nMaxFO)
{
    if (pCktObj->GetVisited())
        return;
    pCktObj->SetVisited();
    if (Ckt_CheckRoot(pCktObj, nTfoLevel, nMaxFO))
        vRoots.emplace_back(pCktObj);
    else
        for (int i = 0; i < pCktObj->GetFanoutNum(); ++i)
            Ckt_ComputeRoot_Rec(pCktObj->GetFanout(i), vRoots, nTfoLevel - 1, nMaxFO);
}


bool Ckt_CheckRoot(shared_ptr <Ckt_Obj_t> pCktObj, int nTfoLevel, int nMaxFO)
{
    if (nTfoLevel == 0)
        return true;
    if (pCktObj->GetFanoutNum() > nMaxFO)
        return true;
    for (int i = 0; i < pCktObj->GetFanoutNum(); ++i)
        if (pCktObj->GetFanout(i)->IsPO())
            return true;
    return false;
}


void Ckt_ComputeSupport(vector < shared_ptr <Ckt_Obj_t> > & vRoots, vector < shared_ptr <Ckt_Obj_t> > & vSupp, int nLocalPI)
{
    DASSERT(static_cast <int> (vRoots.size()) <= nLocalPI, "Error: the number of roots must be smaller than the number of local PI!");
    vSupp.clear();
    deque < shared_ptr <Ckt_Obj_t> > fringe;
    if (vRoots.size())
        vRoots[0]->GetCktNtk()->SetUnvisited();
    else
        return;
    for (auto & pCktObj : vRoots) {
        if (!pCktObj->GetVisited()) {
            pCktObj->SetVisited();
            fringe.emplace_back(pCktObj);
            // cout << "push " << pCktObj << endl;
        }
    }
    while (fringe.size() && static_cast <int>(fringe.size()) < nLocalPI) {
        shared_ptr <Ckt_Obj_t> pCktObj = fringe.front();
        // cout << "pop " << pCktObj << endl;
        if (pCktObj->GetFanoutNum() && pCktObj->GetFanout(0)->IsPO()) {
            vSupp.emplace_back(pCktObj);
            --nLocalPI;
            // cout << "update vSupp " << pCktObj << endl;
            fringe.pop_front();
            continue;
        }
        if (pCktObj->IsPI() || pCktObj->IsConst()) {
            vSupp.emplace_back(pCktObj);
            --nLocalPI;
            // cout << "update vSupp " << pCktObj << endl;
        }
        for (int i = 0; i < pCktObj->GetFaninNum(); ++i) {
            shared_ptr <Ckt_Obj_t> pFanin = pCktObj->GetFanin(i);
            if (!pFanin->GetVisited()) {
                pFanin->SetVisited();
                fringe.emplace_back(pFanin);
                // cout << "push " << pFanin << endl;
            }
        }
        fringe.pop_front();
    }
    for (auto & pCktObj: fringe) {
        vSupp.emplace_back(pCktObj);
        // cout << "update vSupp " << pCktObj << endl;
    }
}


void Ckt_CollectNodes(vector < shared_ptr <Ckt_Obj_t> > & vRoots, vector < shared_ptr <Ckt_Obj_t> > & vSupp, vector < shared_ptr <Ckt_Obj_t> > & vNodes)
{
    vNodes.clear();
    set < shared_ptr <Ckt_Obj_t> > vSuppSet(vSupp.begin(), vSupp.end());
    DASSERT(vSupp.size() == vSuppSet.size(), "Warning : there are identical elements in the local inputs!");
    if (vRoots.size())
        vRoots[0]->GetCktNtk()->SetUnvisited();
    else
        return;
    for (auto & pCktObj : vRoots) {
        DASSERT(!pCktObj->IsPO(), "Warning : collect nodes from PO!");
        Ckt_CollectNodes_Rec(pCktObj, vSuppSet, vNodes);
    }
}


void Ckt_CollectNodes_Rec(shared_ptr <Ckt_Obj_t> pCktObj, set < shared_ptr <Ckt_Obj_t> > & vSuppSet, vector < shared_ptr <Ckt_Obj_t> > & vNodes)
{
    if (pCktObj->GetVisited())
        return;
    if (vSuppSet.count(pCktObj))
        return;
    pCktObj->SetVisited();
    for (int i = 0; i < pCktObj->GetFaninNum(); ++i)
        Ckt_CollectNodes_Rec(pCktObj->GetFanin(i), vSuppSet, vNodes);

    vNodes.emplace_back(pCktObj);
}


void Ckt_GenerateNtk(vector < shared_ptr <Ckt_Obj_t> > & vRoots, vector < shared_ptr <Ckt_Obj_t> > & vSupp, vector < shared_ptr <Ckt_Obj_t> > & vNodes, string fileName)
{
    DASSERT(vSupp.size());
    DASSERT(vRoots.size());

    // core network
    Abc_Ntk_t * pWinNtk = Abc_NtkAlloc(ABC_NTK_LOGIC, ABC_FUNC_SOP, 1);

    for (auto & pCktObj : vSupp) {
        // cout << "adding pis " << pCktObj << endl;
        Abc_Obj_t * pOldObj = pCktObj->GetAbcObj();
        Abc_Obj_t * pNewObj = Abc_NtkCreatePi(pWinNtk);
        Abc_ObjAssignName(pNewObj, Abc_ObjName(pOldObj), nullptr);
        pOldObj->pCopy = pNewObj;
    }

    for (auto & pCktObj : vNodes) {
        // cout << "copying nodes " << pCktObj << endl;
        Abc_Obj_t * pOldObj = pCktObj->GetAbcObj();
        Abc_Obj_t * pNewObj = Abc_NtkDupObj(pWinNtk, pOldObj, 0);
        // cout << Abc_ObjName(pOldObj) << "\t" << Abc_ObjType(pOldObj) << endl;
        Abc_ObjAssignName(pNewObj, Abc_ObjName(pOldObj), nullptr);
        DASSERT(pNewObj == Abc_ObjCopy(pOldObj), "Error: the copy of the original object is not exactly the current object!");
        Abc_Obj_t * pFanin = nullptr;
        int i = 0;
        Abc_ObjForEachFanin(pOldObj, pFanin, i) {
            DASSERT(pOldObj != nullptr && pFanin != nullptr);
            Abc_ObjAddFanin(Abc_ObjCopy(pOldObj), Abc_ObjCopy(pFanin));
        }
    }
    DASSERT(static_cast <int> (vSupp.size()) == Abc_NtkPiNum(pWinNtk));

    for (auto & pCktObj : vRoots) {
        // cout << "adding pos " << pCktObj << endl;
        Abc_Obj_t * pOldObj = pCktObj->GetAbcObj();
        Abc_Obj_t * pNewObj = Abc_NtkCreatePo(pWinNtk);
        Abc_ObjAssignName(pNewObj, Abc_ObjName(pOldObj), nullptr);
        Abc_ObjAddFanin(pNewObj, Abc_ObjCopy(pOldObj));
    }

    // compute care set
    int bitWidth = static_cast <int> (vSupp.size());
    vector <bool> careSet(1 << bitWidth);
    for (int i = 0; i < vSupp[0]->GetSimNum(); ++i) {
        for (int j = 0; j < 64; ++j) {
            uint32_t value = 0;
            for (auto & pCktObj : vSupp) {
                // cout << pCktObj->GetSimVal(i, j);
                value = (value << 1) + pCktObj->GetSimVal(i, j);
            }
            // cout << endl;
            careSet[value] = true;
        }
    }
    // exdc network
    int dcNum = 0;
    for (int i = 0; i < static_cast <int>(careSet.size()); ++i)
        if (!careSet[i])
            ++dcNum;
    // cout << dcNum << "\t" << careSet.size() << "\t" << dcNum / static_cast <double>(careSet.size()) << endl;
    if (dcNum) {
        Abc_Ntk_t * pExdc = Abc_NtkAlloc(ABC_NTK_LOGIC, ABC_FUNC_SOP, 1);
        pWinNtk->pExdc = pExdc;

        char * pSop = Abc_SopStart((Mem_Flex_t *)pExdc->pManFunc, dcNum, bitWidth);
        char * pCube = pSop;
        for (int i = 0; i < static_cast <int>(careSet.size()); ++i) {
            if (!careSet[i]) {
                DASSERT(pCube != nullptr);
                for (int k = bitWidth - 1; k >= 0; --k) {
                    // cout << Ckt_GetBit(i, k);
                    *(pCube++) = Ckt_GetBit(i, k)? '1': '0';
                }
                pCube += 3;
            }
        }

        for (auto & pCktObj : vSupp) {
            // cout << "adding exdc pis " << pCktObj << endl;
            Abc_Obj_t * pOldObj = pCktObj->GetAbcObj();
            Abc_Obj_t * pNewObj = Abc_NtkCreatePi(pExdc);
            Abc_ObjAssignName(pNewObj, Abc_ObjName(pOldObj), nullptr);
            pOldObj->pCopy = pNewObj;
        }

        for (auto & pCktObj : vRoots) {
            // cout << "adding exdc pos" << pCktObj << endl;
            Abc_Obj_t * pOldObj = pCktObj->GetAbcObj();
            Abc_Obj_t * pNewObj = Abc_NtkCreateNode(pExdc);
            Abc_ObjAssignName(pNewObj, Abc_ObjName(pOldObj), nullptr);
            pOldObj->pCopy = pNewObj;

            Abc_Obj_t * pPo = Abc_NtkCreatePo(pExdc);
            Abc_ObjAssignName(pPo, Abc_ObjName(pOldObj), nullptr);
            Abc_ObjAddFanin(pPo, pNewObj);
        }

        for (auto & pLocalOut : vRoots)
            for (auto & pLocalIn: vSupp)
                Abc_ObjAddFanin(Abc_ObjCopy(pLocalOut->GetAbcObj()), Abc_ObjCopy(pLocalIn->GetAbcObj()));

        for (auto & pLocalOut : vRoots)
            Abc_ObjCopy(pLocalOut->GetAbcObj())->pData = pSop;
    }

    string fileNameComp = fileName + ".blif";
    Io_Write(pWinNtk, const_cast <char *>(fileNameComp.c_str()), IO_FILE_BLIF);
    Abc_NtkDelete(pWinNtk);

    // // care network
    // int cNum = 0;
    // for (int i = 0; i < static_cast <int>(careSet.size()); ++i)
    //     if (careSet[i])
    //         ++cNum;
    // if (cNum != (1 << bitWidth))
    // {
    //     Abc_Ntk_t * pCare = Abc_NtkAlloc(ABC_NTK_LOGIC, ABC_FUNC_SOP, 1);

    //     char * pSop = Abc_SopStart((Mem_Flex_t *)pCare->pManFunc, cNum, bitWidth);
    //     char * pCube = pSop;
    //     for (int i = 0; i < static_cast <int>(careSet.size()); ++i) {
    //         if (careSet[i]) {
    //             DASSERT(pCube != nullptr);
    //             for (int k = bitWidth - 1; k >= 0; --k) {
    //                 *(pCube++) = Ckt_GetBit(i, k)? '1': '0';
    //             }
    //             pCube += 3;
    //         }
    //     }

    //     for (auto & pCktObj : vSupp) {
    //         Abc_Obj_t * pOldObj = pCktObj->GetAbcObj();
    //         Abc_Obj_t * pNewObj = Abc_NtkCreatePi(pCare);
    //         Abc_ObjAssignName(pNewObj, Abc_ObjName(pOldObj), nullptr);
    //         pOldObj->pCopy = pNewObj;
    //     }

    //     for (auto & pCktObj : vRoots) {
    //         Abc_Obj_t * pOldObj = pCktObj->GetAbcObj();
    //         Abc_Obj_t * pNewObj = Abc_NtkCreateNode(pCare);
    //         Abc_ObjAssignName(pNewObj, Abc_ObjName(pOldObj), nullptr);
    //         pOldObj->pCopy = pNewObj;

    //         Abc_Obj_t * pPo = Abc_NtkCreatePo(pCare);
    //         Abc_ObjAssignName(pPo, Abc_ObjName(pOldObj), nullptr);
    //         Abc_ObjAddFanin(pPo, pNewObj);
    //     }

    //     for (auto & pLocalOut : vRoots)
    //         for (auto & pLocalIn: vSupp)
    //             Abc_ObjAddFanin(Abc_ObjCopy(pLocalOut->GetAbcObj()), Abc_ObjCopy(pLocalIn->GetAbcObj()));

    //     for (auto & pLocalOut : vRoots)
    //         Abc_ObjCopy(pLocalOut->GetAbcObj())->pData = pSop;

    //     string fileNameCare = fileName + "_care.blif";
    //     Io_Write(pCare, const_cast <char *>(fileNameCare.c_str()), IO_FILE_BLIF);
    //     Abc_NtkDelete(pCare);
    // }
}


void Ckt_SimplifyNtk(string fileName)
{
    FILE * fp = fopen((fileName + ".rug").c_str(), "w");
    DASSERT(fp != nullptr);
    fprintf(fp, "read_blif %s.blif;\n", fileName.c_str());
    fprintf(fp, "full_simplify;\n");
    fprintf(fp, "write_blif %s_out.blif;\n", fileName.c_str());
    fclose(fp);
    DASSERT(system(("sis -x -f " + fileName + ".rug").c_str()) != -1);

    // FILE * fp = fopen((fileName + ".rug").c_str(), "w");
    // DASSERT(fp != nullptr);
    // fprintf(fp, "read_blif %s.blif;\n", fileName.c_str());
    // // string fileNameC = fileName + "_care.blif";
    // // ifstream f(fileNameC.c_str());
    // // if (f.good())
    // //     fprintf(fp, "care_set %s_care.blif;\n", fileName.c_str());
    // fprintf(fp, "mfs;\n");
    // fprintf(fp, "write_blif %s_out.blif;\n", fileName.c_str());
    // fclose(fp);
    // DASSERT(system(("abc -x -f " + fileName + ".rug").c_str()) != -1);

    cout << fileName << "," << Ckt_ReadNewNtk(fileName + ".blif") << "," << Ckt_ReadNewNtk(fileName + "_out.blif") << endl;
}


int Ckt_CollectMffc(vector < shared_ptr <Ckt_Obj_t> > & vNodes, vector < shared_ptr <Ckt_Obj_t> > & vMffc)
{
    DASSERT(vNodes.size());
    vMffc.clear();
    vNodes[0]->GetCktNtk()->SetUnvisited();
    for (auto & pCktObj : vNodes)
        Ckt_CollectMffc_Rec(pCktObj, vMffc, true);
    int nLits = 0;
    for (auto pCktObj : vMffc)
        nLits += Abc_SopGetLitNum((char *)(pCktObj->GetAbcObj()->pData));
    return nLits;
}


void Ckt_CollectMffc_Rec(shared_ptr <Ckt_Obj_t> pCktObj, vector < shared_ptr <Ckt_Obj_t> > & vMffc, bool isTop)
{
    if (pCktObj->GetVisited())
        return;
    pCktObj->SetVisited();
    if (!isTop && (pCktObj->IsPI() || pCktObj->GetFanoutNum()))
        return;
    for (int i = 0; i < pCktObj->GetFaninNum(); ++i)
        Ckt_CollectMffc_Rec(pCktObj->GetFanin(i), vMffc, false);
    vMffc.emplace_back(pCktObj);
}


int Ckt_ReadNewNtk(string fileName)
{
    Abc_Ntk_t * pNtk = Io_Read(const_cast <char *>(fileName.c_str()), IO_FILE_BLIF, 1, 0);
    int nLits = Abc_NtkGetLitNum(pNtk);
    Abc_NtkDelete(pNtk);
    return nLits;
}


void Ckt_ReplaceNtk(Abc_Ntk_t * pHostNtk, vector < shared_ptr <Ckt_Obj_t> > & vRoots, vector < shared_ptr <Ckt_Obj_t> > & vNodes, string fileName)
{
    // read new network
    Abc_Ntk_t * pNewNtk = Io_Read(const_cast <char *>(fileName.c_str()), IO_FILE_BLIF, 1, 0);

    // generate set of vRoots
    set < shared_ptr <Ckt_Obj_t> > vRootSet(vRoots.begin(), vRoots.end());

    // // delete old nodes
    // for (auto & pCktObj: vNodes) {
    //     if (vRootSet.count(pCktObj))
    //         continue;
    //     Abc_Obj_t * pOldObj = pCktObj->GetAbcObj();
    //     Abc_Obj_t * pHostObj = Abc_NtkFindNode(pHostNtk, Abc_ObjName(pOldObj));
    //     DASSERT(pHostObj != nullptr);
    //     // cout << "delete " << Abc_ObjName(pHostObj) << "," << Abc_ObjType(pHostObj) << endl;
    //     Abc_NtkDeleteObj(pHostObj);
    // }

    // add new nodes
    Vec_Ptr_t * vTopoSeq = Abc_NtkDfs(pNewNtk, 0);
    Abc_Obj_t * pObj;
    int i;
    Vec_PtrForEachEntry(Abc_Obj_t *, vTopoSeq, pObj, i) {
        Abc_Obj_t * pHostNewObj = Abc_NtkDupObj(pHostNtk, pObj, 1);
        // cout << "create node " << Abc_ObjName(pHostNewObj) << endl;
        Abc_Obj_t * pFanin;
        int j;
        Abc_ObjForEachFanin(pObj, pFanin, j) {
            Abc_Obj_t * pHostFanin = Abc_NtkFindCi(pHostNtk, Abc_ObjName(pFanin));
            if (pHostFanin == nullptr)
                pHostFanin = Abc_NtkFindNode(pHostNtk, Abc_ObjName(pFanin));
            DASSERT(pHostFanin != nullptr);
            // cout << "add fanin " << Abc_ObjName(pHostFanin) << endl;
            Abc_ObjAddFanin(pHostNewObj, pHostFanin);
        }
    }

    // patch local POs
    Abc_NtkForEachPo(pNewNtk, pObj, i) {
        Abc_Obj_t * pDriver = Abc_ObjFanin0(pObj);
        // cout << "patching " << Abc_ObjName(pDriver) << endl;
        Abc_Obj_t * pHostOldLPO = Abc_NtkFindNode(pHostNtk, Abc_ObjName(pDriver));
        DASSERT(pHostOldLPO != nullptr);
        Abc_Obj_t * pHostNewLPO = Abc_NtkDupObj(pHostNtk, pDriver, 0);

        Abc_Obj_t * pFanin;
        int j;
        Abc_ObjForEachFanin(pDriver, pFanin, j) {
            Abc_Obj_t * pHostFanin = Abc_NtkFindCi(pHostNtk, Abc_ObjName(pFanin));
            if (pHostFanin == nullptr)
                pHostFanin = Abc_NtkFindNode(pHostNtk, Abc_ObjName(pFanin));
            DASSERT(pHostFanin != nullptr);
            // cout << "add fanin " << Abc_ObjName(pHostFanin) << endl;
            Abc_ObjAddFanin(pHostNewLPO, pHostFanin);
        }


        Abc_ObjReplace(pHostOldLPO, pHostNewLPO);
        Abc_ObjAssignName(pHostNewLPO, Abc_ObjName(pDriver), nullptr);
    }

    // cleanup dangling nodes
    Abc_NtkCleanup(pHostNtk, 0);

    // memory cleanup
    Vec_PtrFree(vTopoSeq);
    Abc_NtkDelete(pNewNtk);
}


void Abc_NodeMffcConeSupp_rec( Abc_Obj_t * pNode, Vec_Ptr_t * vCone, Vec_Ptr_t * vSupp, int fTopmost )
{
    Abc_Obj_t * pFanin;
    int i;
    // skip visited nodes
    if ( Abc_NodeIsTravIdCurrent(pNode) )
        return;
    Abc_NodeSetTravIdCurrent(pNode);
    // add to the new support nodes
    if ( !fTopmost && (Abc_ObjIsCi(pNode) || pNode->vFanouts.nSize > 0) )
    {
        if ( vSupp ) Vec_PtrPush( vSupp, pNode );
        return;
    }
    // recur on the children
    Abc_ObjForEachFanin( pNode, pFanin, i )
        Abc_NodeMffcConeSupp_rec( pFanin, vCone, vSupp, 0 );
    // collect the internal node
    if ( vCone ) Vec_PtrPush( vCone, pNode );
//    printf( "%d ", pNode->Id );
}
