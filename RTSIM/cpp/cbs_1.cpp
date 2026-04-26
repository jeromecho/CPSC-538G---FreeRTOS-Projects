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
        TextTrace ttrace("trace_cbs1.txt");

        // create the scheduler and the kernel
        EDFScheduler sched;
        RTKernel kern(&sched);

        // Periodic Tasks:
        PeriodicTask p1(7, 7, 0, "P1");
        p1.insertCode("fixed(4);");
        kern.addTask(p1);

        // CBS:
        // `false` for "soft" CBS that eagerly replenishes its budget
        // in exchange for delaying its deadline
        CBServer serv(3, 8, 8, false, "server1", "FIFOSched");
        // "" for parameters passed to task
        kern.addTask(serv, "");

        // Aperiodic Task:
        // set very large relative deadline (task deadline doesn't matter if CBS server is
        // managing task's deadline)
        Task a1(nullptr, 100, 0, "CBS1_a1");
        a1.insertCode("fixed(4);");
        a1.setAbort(false);
        serv.addTask(a1);

        Task a2(nullptr, 100, 0, "CBS1_a2");
        a2.insertCode("fixed(3);");
        a2.setAbort(false);
        serv.addTask(a2);

        // Analogous Logic to vTestRunner1
        a1.activate(3);
        a2.activate(13);

        ttrace.attachToTask(p1);
        ttrace.attachToTask(a1);
        ttrace.attachToTask(a2);

        // run the simulation for 21 units of time
        SIMUL.dbg.enable(_TASK_DBG_LEV);
        SIMUL.dbg.enable(_KERNEL_DBG_LEV);
        SIMUL.dbg.enable(_SERVER_DBG_LEV);
        // SIMUL.dbg.enable(_FIFO_SCHED_DBG_LEV);
        SIMUL.run(21);
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
