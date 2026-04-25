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
        TextTrace ttrace("trace_cbs11.txt");

        // create the scheduler and the kernel
        EDFScheduler sched;
        RTKernel kern(&sched);

        PeriodicTask p1(6, 6, 0, "P1");
        p1.insertCode("fixed(2);");
        p1.setAbort(false);

        // CBS:
        // `false` for "soft" CBS that eagerly replenishes its budget
        // in exchange for delaying its deadline
        CBServer serv1(1, 8, 8, false, "server1", "FIFOSched");
        CBServer serv2(4, 8, 8, false, "server2", "FIFOSched");

        // Aperiodic Task:
        // set very large relative deadline (task deadline doesn't matter if CBS server is
        // managing task's deadline)
        Task a1_1(nullptr, 100, 0, "CBS1_a1");
        a1_1.insertCode("fixed(3);");
        a1_1.activate(0);
        a1_1.setAbort(false);
        serv1.addTask(a1_1);

        Task a1_2(nullptr, 100, 0, "CBS2_a1");
        a1_2.insertCode("fixed(3);");
        a1_2.activate(0);
        a1_2.setAbort(false);
        serv2.addTask(a1_2);

        Task a2_1(nullptr, 100, 0, "CBS1_a2");
        a2_1.insertCode("fixed(4);");
        a2_1.activate(20);
        a2_1.setAbort(false);
        serv1.addTask(a2_1);

        Task a2_2(nullptr, 100, 0, "CBS2_a2");
        a2_2.insertCode("fixed(4);");
        a2_2.activate(20);
        a2_2.setAbort(false);
        serv2.addTask(a2_2);

        Task a3_1(nullptr, 100, 0, "CBS1_a3");
        a3_1.insertCode("fixed(1);");
        a3_1.activate(40);
        a3_1.setAbort(false);
        serv1.addTask(a3_1);

        Task a3_2(nullptr, 100, 0, "CBS2_a3");
        a3_2.insertCode("fixed(2);");
        a3_2.activate(40);
        a3_2.setAbort(false);
        serv2.addTask(a3_2);

        ttrace.attachToTask(p1);
        ttrace.attachToTask(a1_1);
        ttrace.attachToTask(a1_2);
        ttrace.attachToTask(a2_1);
        ttrace.attachToTask(a2_2);
        ttrace.attachToTask(a3_1);
        ttrace.attachToTask(a3_2);

        kern.addTask(p1, "");
        kern.addTask(serv1, "");
        kern.addTask(serv2, "");

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
