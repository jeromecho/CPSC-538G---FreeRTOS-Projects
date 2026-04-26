#include <kernel.hpp>
#include <edfsched.hpp>
#include <fifosched.hpp>
// #include <jtrace.hpp>
#include <texttrace.hpp>
#include <rttask.hpp>
#include <cbserver.hpp>
#include <supercbs.hpp>

using namespace MetaSim;
using namespace RTSim;

int main()
{
    try
    {
        // CBS Test 1
        TextTrace ttrace("trace_cbs12.txt");

        // create the scheduler and the kernel
        EDFScheduler sched;
        RTKernel kern(&sched);
        CBServer serv1(1, 8, 8, false, "serv1", "FIFOSched");
        CBServer serv2(2, 8, 8, false, "serv2", "FIFOSched");
        CBServer serv3(3, 8, 8, false, "serv3", "FIFOSched");

        PeriodicTask p1(6, 6, 0, "P1"); // Period 6, Deadline 6, Phase 0
        p1.insertCode("fixed(2);");
        p1.setAbort(true);

        Task a1_1(nullptr, 100, 0, "CBS1_a1");
        a1_1.insertCode("fixed(3);");
        serv1.addTask(a1_1);
        Task a1_2(nullptr, 100, 0, "CBS2_a1");
        a1_2.insertCode("fixed(3);");
        serv2.addTask(a1_2);
        Task a1_3(nullptr, 100, 0, "CBS3_a1");
        a1_3.insertCode("fixed(3);");
        serv3.addTask(a1_3);

        Task a2_1(nullptr, 100, 0, "CBS1_a2");
        a2_1.insertCode("fixed(4);");
        serv1.addTask(a2_1);
        Task a2_2(nullptr, 100, 0, "CBS2_a2");
        a2_2.insertCode("fixed(4);");
        serv2.addTask(a2_2);
        Task a2_3(nullptr, 100, 0, "CBS3_a2");
        a2_3.insertCode("fixed(4);");
        serv3.addTask(a2_3);

        Task a3_1(nullptr, 100, 0, "CBS1_a3");
        a3_1.insertCode("fixed(2);");
        serv1.addTask(a3_1);
        Task a3_2(nullptr, 100, 0, "CBS2_a3");
        a3_2.insertCode("fixed(2);");
        serv2.addTask(a3_2);
        Task a3_3(nullptr, 100, 0, "CBS3_a3");
        a3_3.insertCode("fixed(2);");
        serv3.addTask(a3_3);

        kern.addTask(p1);
        kern.addTask(serv1, "");
        kern.addTask(serv2, "");
        kern.addTask(serv3, "");

        a1_1.activate(0);
        a1_2.activate(0);
        a1_3.activate(0);
        a2_1.activate(20);
        a2_2.activate(20);
        a2_3.activate(20);
        a3_1.activate(40);
        a3_2.activate(50);
        a3_3.activate(60);

        ttrace.attachToTask(p1);
        ttrace.attachToTask(a1_1);
        ttrace.attachToTask(a1_2);
        ttrace.attachToTask(a1_3);
        ttrace.attachToTask(a2_1);
        ttrace.attachToTask(a2_2);
        ttrace.attachToTask(a2_3);
        ttrace.attachToTask(a3_1);
        ttrace.attachToTask(a3_2);
        ttrace.attachToTask(a3_3);

        // run the simulation for 21 units of time
        SIMUL.dbg.enable(_TASK_DBG_LEV);
        SIMUL.dbg.enable(_KERNEL_DBG_LEV);
        SIMUL.dbg.enable(_SERVER_DBG_LEV);
        // SIMUL.dbg.enable(_FIFO_SCHED_DBG_LEV);
        SIMUL.run(100);
    }
    catch (BaseExc &e)
    {
        cout << e.what() << endl;
    }
    catch (parse_util::ParseExc &e2)
    {
        cout << e2.what() << endl;
    }
}
