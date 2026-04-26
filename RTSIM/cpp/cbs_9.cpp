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
        TextTrace ttrace("trace_cbs9.txt");

        // create the scheduler and the kernel
        EDFScheduler sched;
        RTKernel kern(&sched);

        // Periodic Tasks:
        /*
        PeriodicTask p1(6, 6, 0, "P1");
        p1.insertCode("fixed(2);");
        p1.setAbort(false);
        kern.addTask(p1);

        PeriodicTask p2(3, 3, 0, "P2");
        p2.insertCode("fixed(1);");
        p2.setAbort(false);
        kern.addTask(p2);
        */

        // CBS:
        // `false` for "soft" CBS that eagerly replenishes its budget
        // in exchange for delaying its deadline
        CBServer serv1(1, 8, 8, false, "server1", "FIFOSched");
        CBServer serv2(4, 8, 8, false, "server2", "FIFOSched");
        // "" for parameters passed to task
        kern.addTask(serv1, "");
        kern.addTask(serv2, "");

        // Aperiodic Task:
        // set very large relative deadline (task deadline doesn't matter if CBS server is
        // managing task's deadline)
        Task a1_1(nullptr, 100, 0, "CBS1_a3_1");
        a1_1.insertCode("fixed(3);");
        a1_1.setAbort(false);
        a1_1.activate();
        serv1.addTask(a1_1);

        Task a2_1(nullptr, 100, 0, "CBS1_a4_1");
        a2_1.insertCode("fixed(4);");
        a2_1.setAbort(false);
        a2_1.activate(20);
        serv1.addTask(a2_1);

        Task a3_1(nullptr, 100, 0, "CBS1_a1_1");
        a3_1.insertCode("fixed(1);");
        a3_1.setAbort(false);
        a3_1.activate(40);
        serv1.addTask(a3_1);

        Task a1_2(nullptr, 100, 0, "CBS2_a3_1");
        a1_2.insertCode("fixed(3);");
        a1_2.setAbort(false);
        a1_2.activate();
        serv2.addTask(a1_2);

        Task a2_2(nullptr, 100, 0, "CBS2_a4_1");
        a2_2.insertCode("fixed(4);");
        a2_2.setAbort(false);
        a2_2.activate(20);
        serv2.addTask(a2_2);

        Task a3_2(nullptr, 100, 0, "CBS2_a2_1");
        a3_2.insertCode("fixed(2);");
        a3_2.setAbort(false);
        a3_2.activate(40);
        serv2.addTask(a3_2);

        // ttrace.attachToTask(p1);
        // ttrace.attachToTask(p2);

        ttrace.attachToTask(a1_1);
        ttrace.attachToTask(a2_1);
        ttrace.attachToTask(a3_1);
        ttrace.attachToTask(a1_2);
        ttrace.attachToTask(a2_2);
        ttrace.attachToTask(a3_2);

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
