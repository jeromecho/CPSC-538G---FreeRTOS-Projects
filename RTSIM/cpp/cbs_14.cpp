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
        TextTrace ttrace("trace_cbs14.txt");

        // create the scheduler and the kernel
        EDFScheduler sched;
        RTKernel kern(&sched);

        CBServer serv1(8, 8, 8, false, "serv1", "FIFOSched");
        PeriodicTask p1(7, 7, 0, "P1");
        p1.insertCode("fixed(4);");
        p1.setAbort(false);
        p1.killOnMiss(false);
        kern.addTask(p1);
        ttrace.attachToTask(p1);

        for (int i = 0; i < 5; i++)
        {
            string name = "CBS1_a" + std::to_string(i);
            Task *a = new Task(nullptr, 100, 0, name);
            a->insertCode("fixed(4);");
            serv1.addTask(*a);
            a->activate(i);
            ttrace.attachToTask(*a);
        }
        kern.addTask(serv1, "");
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
