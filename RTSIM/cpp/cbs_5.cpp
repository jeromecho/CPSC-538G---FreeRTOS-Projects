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
        TextTrace ttrace("trace_cbs5.txt");

        // create the scheduler and the kernel
        EDFScheduler sched;
        RTKernel kern(&sched);

        // Periodic Tasks:
        PeriodicTask p1(4, 4, 0, "P1");
        p1.insertCode("fixed(2);");
        p1.setAbort(false);
        kern.addTask(p1);

        PeriodicTask p2(8, 8, 0, "P2");
        p2.insertCode("fixed(3);");
        p2.setAbort(false);
        kern.addTask(p2);

        // CBS:
        // `false` for "soft" CBS that eagerly replenishes its budget
        // in exchange for delaying its deadline
        CBServer serv(1, 10, 10, false, "server1", "FIFOSched");
        // "" for parameters passed to task
        kern.addTask(serv, "");

        // Aperiodic Task:
        // set very large relative deadline (task deadline doesn't matter if CBS server is
        // managing task's deadline)
        Task a1(nullptr, 100, 0, "CBS1_a3_1");
        a1.insertCode("fixed(3);");
        a1.setAbort(false);
        a1.activate();
        serv.addTask(a1);

        Task a2(nullptr, 100, 0, "CBS1_a4_1");
        a2.insertCode("fixed(4);");
        a2.setAbort(false);
        a2.activate();
        serv.addTask(a2);

        Task a3(nullptr, 100, 0, "CBS1_a3_2");
        a3.insertCode("fixed(3);");
        a3.setAbort(false);
        a3.activate();
        serv.addTask(a3);

        Task a4(nullptr, 100, 0, "CBS1_a4_2");
        a4.insertCode("fixed(4);");
        a4.setAbort(false);
        a4.activate();
        serv.addTask(a4);

        ttrace.attachToTask(p1);
        ttrace.attachToTask(p2);
        ttrace.attachToTask(a1);
        ttrace.attachToTask(a2);
        ttrace.attachToTask(a3);
        ttrace.attachToTask(a4);

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
