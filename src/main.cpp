#include "headers.h"
#include "cmdline.h"
#include "dcals.h"
#include "simulatorPro.h"


using namespace std;
using namespace cmdline;


parser Cmdline_Parser(int argc, char * argv[])
{
    parser option;
    option.add <string> ("input", 'i', "Original Circuit file", true);
    option.add <string> ("approx", 'x', "Approximate Circuit file", false, "");
    option.add <string> ("genlib", 'l', "Standard Cell Library", false, "data/genlib/mcnc.genlib");
    option.add <string> ("metricType", 'm', "Error metric type, er, aemr, raem", false, "er");
    option.add <int> ("select", 's', "Mode Selection, 0 = dcals, 1 = measure", false, 0, range(0, 1));
    option.add <int> ("mapType", 't', "Mapping Type, 0 = mcnc, 1 = lut", false, 0, range(0, 1));
    option.add <int> ("nFrame", 'f', "Initial Simulation Round", false, 64, range(1, INT_MAX));
    option.add <int> ("nCut", 'c', "Initial Cut Size", false, 30, range(1, INT_MAX));
    option.add <double> ("errorBound", 'b', "Error constraint upper bound", false, 0.002, range(0.0, 1.0));
    option.parse_check(argc, argv);
    return option;
}


int main(int argc, char * argv[])
{
    parser option = Cmdline_Parser(argc, argv);
    string input = option.get <string> ("input");
    string approx = option.get <string> ("approx");
    string genlib = option.get <string> ("genlib");
    string metricType = option.get <string> ("metricType");
    int select = option.get <int> ("select");
    int mapType = option.get <int> ("mapType");
    int nFrame = option.get <int> ("nFrame");
    int nCut = option.get <int> ("nCut");
    double errorBound = option.get <double> ("errorBound");

    Abc_Start();
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    ostringstream command("");
    command << "read_genlib " << genlib;
    DASSERT(!Cmd_CommandExecute(pAbc, command.str().c_str()));

    // dcals
    if (select == 0) {
        command.str("");
        command << "read_blif " << input;
        DASSERT(!Cmd_CommandExecute(pAbc, command.str().c_str()));
        Abc_Ntk_t * pNtk = Abc_NtkDup(Abc_FrameReadNtk(pAbc));
        uint32_t pos0 = input.find(".blif");
        DASSERT(pos0 != input.npos);
        uint32_t pos1 = input.rfind("/");
        if (pos1 == input.npos)
            pos1 = -1;
        Ckt_NtkRename(pNtk, input.substr(pos1 + 1, pos0 - pos1 - 1).c_str());

        if (metricType == "er") {
            Dcals_Man_t alsEng(pNtk, nFrame, nCut, errorBound, Metric_t::ER, mapType);
            alsEng.DCALS();
        }
        else if (metricType == "aemr") {
            Dcals_Man_t alsEng(pNtk, nFrame, nCut, errorBound, Metric_t::AEMR, mapType);
            alsEng.DCALS();
        }
        else if (metricType == "raem") {
            Dcals_Man_t alsEng(pNtk, nFrame, nCut, errorBound, Metric_t::RAEM, mapType);
            alsEng.DCALS();
        }

        Abc_NtkDelete(pNtk);
    }

    // measure
    if (select == 1) {
        command.str("");
        command << "read_blif " << input << ";aig;";
        DASSERT(!Cmd_CommandExecute(pAbc, command.str().c_str()));
        Abc_Ntk_t * pNtk1 = Abc_NtkDup(Abc_FrameReadNtk(pAbc));
        command << "read_blif " << approx;
        DASSERT(!Cmd_CommandExecute(pAbc, command.str().c_str()));
        Abc_Ntk_t * pNtk2 = Abc_NtkDup(Abc_FrameReadNtk(pAbc));
        if (Abc_NtkHasMapping(pNtk2)) {
            cout << "area = " << Ckt_GetArea(pNtk2) << endl;
            cout << "delay = " << Ckt_GetDelay(pNtk2) << endl;
        }
        else {
            cout << "size = " << Abc_NtkNodeNum(pNtk2) << endl;
            cout << "depth = " << Abc_NtkLevel(pNtk2) << endl;
        }
        DASSERT(Abc_NtkToAig(pNtk2));
        random_device rd;
        unsigned seed = static_cast <unsigned>(rd());
        seed = 100;
        cout << "error rate = " << MeasureER(pNtk1, pNtk2, nFrame, seed) << endl;
        cout << "average error magnitude rate = " << MeasureAEMR(pNtk1, pNtk2, nFrame, seed) << endl;
        Abc_NtkDelete(pNtk1);
        Abc_NtkDelete(pNtk2);
    }

    // recycle memory
    Abc_Stop();

    return 0;
}
