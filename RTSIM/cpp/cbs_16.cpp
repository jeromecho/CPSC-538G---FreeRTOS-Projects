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
        TextTrace ttrace("trace_cbs16.txt");

        // create the scheduler and the kernel
        EDFScheduler sched;
        RTKernel kern(&sched);
        CBServer serv1(3, 7, 7, false, "serv1", "FIFOSched");
        PeriodicTask p1(7, 7, 0, "P1");
        p1.insertCode("fixed(4);");
        p1.setAbort(true);

        Task a1(nullptr, 100, 0, "CBS1_a1");
        a1.insertCode("fixed(4);");
        serv1.addTask(a1);
        Task a2(nullptr, 100, 0, "CBS1_a2");
        a2.insertCode("fixed(3);");
        serv1.addTask(a2);
        Task a3(nullptr, 100, 0, "CBS1_a3");
        a3.insertCode("fixed(4);");
        serv1.addTask(a3);
        Task a4(nullptr, 100, 0, "CBS1_a4");
        a4.insertCode("fixed(3);");
        serv1.addTask(a4);
        Task a5(nullptr, 100, 0, "CBS1_a5");
        a5.insertCode("fixed(4);");
        serv1.addTask(a5);
        Task a6(nullptr, 100, 0, "CBS1_a6");
        a6.insertCode("fixed(3);");
        serv1.addTask(a6);

        kern.addTask(p1);
        kern.addTask(serv1, "");

        a1.activate(0);
        a2.activate(0);
        a3.activate(0);
        a4.activate(0);
        a5.activate(0);
        a6.activate(0);

        ttrace.attachToTask(p1);
        ttrace.attachToTask(a1);
        ttrace.attachToTask(a2);
        ttrace.attachToTask(a3);
        ttrace.attachToTask(a4);
        ttrace.attachToTask(a5);
        ttrace.attachToTask(a6);
        // run the simulation for 21 units of time
        SIMUL.dbg.enable(_TASK_DBG_LEV);
        SIMUL.dbg.enable(_KERNEL_DBG_LEV);
        SIMUL.dbg.enable(_SERVER_DBG_LEV);
        // SIMUL.dbg.enable(_FIFO_SCHED_DBG_LEV);
        SIMUL.run(50);
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
