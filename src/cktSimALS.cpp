#include "cktSimALS.h"


using namespace std;
using namespace abc;


void ALS_Sim(string file, string approx, int nFrame)
{
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    string command = "read_blif " + file;
    DEBUG_ASSERT( Cmd_CommandExecute(pAbc, command.c_str()) == 0, module_a{}, "read_blif failed");

    // logic simulation
    shared_ptr <Ckt_Ntk_t> pCktNtk = make_shared <Ckt_Ntk_t> (Abc_FrameReadNtk(pAbc));
    pCktNtk->Init(nFrame);
    pCktNtk->GenInputDist(314);
    pCktNtk->FeedForward();

    // start a ABC network
    Abc_Ntk_t * pAbcNtk = Abc_NtkAlloc(ABC_NTK_LOGIC, ABC_FUNC_SOP, 1);

    // copy PIs/POs
    Abc_Obj_t * pAbcObj, * pAbcPi, * pAbcPo, * pAbcProd;
    int i;
    int j;
    Abc_NtkForEachPi(pCktNtk->GetAbcNtk(), pAbcObj, i)
        Abc_NtkDupObj(pAbcNtk, pAbcObj, 1);
    Abc_NtkForEachPo(pCktNtk->GetAbcNtk(), pAbcObj, i)
        Abc_NtkDupObj(pAbcNtk, pAbcObj, 1);
    Abc_NtkForEachPi(pAbcNtk, pAbcObj, i)
        cout << Abc_ObjName(pAbcObj) << "\t";
    cout << endl;
    Abc_NtkForEachPo(pAbcNtk, pAbcObj, i)
        cout << Abc_ObjName(pAbcObj) << "\t";
    cout << endl;

    // set functions
    DEBUG_ASSERT(Abc_NtkPoNum(pAbcNtk) == pCktNtk->GetPoNum(), module_a{}, "#POs are unequal");
    DEBUG_ASSERT(Abc_NtkPoNum(pAbcNtk), module_a{}, "#POs = 0");
    Abc_NtkForEachPo(pAbcNtk, pAbcPo, i) {
        shared_ptr <Ckt_Obj_t> pCktPo = pCktNtk->GetPo(i);
        DEBUG_ASSERT(pCktPo->GetName() == string(Abc_ObjName(pAbcPo)), module_a{}, "POs are unequal");
        cout << "------------------------------------------------------" << endl;
        cout << "PO : " << pCktPo->GetName() << endl;
        cout << "PI : ";
        Abc_NtkForEachPi(pAbcNtk, pAbcPi, j) {
            shared_ptr <Ckt_Obj_t> pCktPi = pCktNtk->GetPi(j);
            DEBUG_ASSERT(pCktPi->GetName() == string(Abc_ObjName(pAbcPi)), module_a{}, "PIs are unequal");
            cout << pCktPi->GetName() << "\t";
        }
        cout << endl;
        for (int k = 0; k < pCktNtk->GetSimNum(); ++k) {
            for (int l = 0; l < 64; ++l) {
                if (pCktNtk->GetPi(0)->GetSimVal(k, l))
                    pAbcProd = Abc_NtkPi(pAbcNtk, 0);
                else
                    pAbcProd = Abc_ObjNot(Abc_NtkPi(pAbcNtk, 0));
                for ( j = 1; (j < Abc_NtkPiNum(pAbcNtk)) && (((pAbcPi) = Abc_NtkPi(pAbcNtk, j)), 1); j++ ) {
                    // cout << pCktPi->GetSimVal(k, l);
                    if (pCktNtk->GetPi(j)->GetSimVal(k, l)) {
                    }
                }
                // cout << endl;
            }
        }
        break;
    }

    // delete the ABC network
    Abc_NtkDelete(pAbcNtk);
}
