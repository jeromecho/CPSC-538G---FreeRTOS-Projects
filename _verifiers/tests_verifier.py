import unittest
from verify import verify_gedf_trace
from enum import IntEnum


class Event(IntEnum):
    RELEASE = 0
    SWITCH_IN = 1
    SWITCH_OUT = 2
    OTHER = 4


"""
TODO: Extend verifier to work with aperiodic tasks
"""


class Task(IntEnum):
    PERIODIC_0 = 0
    PERIODIC_1 = 1
    PERIODIC_2 = 2
    PERIODIC_3 = 3


class Macro:
    def __getattr__(self, name):
        # Define the supported prefixes
        mapping = {"TICK_": 5, "DEADLINE_": 9}

        for prefix, length in mapping.items():
            if name.startswith(prefix):
                try:
                    # Slice from the end of the prefix to get the integer
                    return int(name[length:])
                except ValueError:
                    break  # Found prefix but the rest isn't an int

        raise AttributeError(
            f"'{type(self).__name__}' object has no attribute '{name}'"
        )


MACRO = Macro()


class TestGEDFVerifier(unittest.TestCase):
    def setUp(self):
        # Default execution map: Task 0 takes 2 ticks, Task 1 takes 2 ticks
        self.exec_map = {"0": 2, "1": 2}
        self.exec_map_multicore = {}

    def test_valid_execution(self):
        """Test a perfect execution: Task 0 releases at 0 and finishes at 2."""
        trace = """
        0,0,100,0,0,1,0,0,0,0,0,0,0,10,0
        0,1,110,0,1,1,0,0,0,0,0,0,0,10,0
        2,4,120,0,2,1,0,0,0,0,0,0,0,10,0
        """
        violations = verify_gedf_trace(trace, self.exec_map, num_cores=1)
        self.assertEqual(len(violations), 0, "Valid trace should have 0 violations.")

    def test_under_execution_violation(self):
        """Test if it catches a task finishing earlier than its required execution time."""
        # Task 0 needs 2 ticks, but finishes after 1 tick
        trace = """
        0,0,100,0,0,1,0,0,0,0,0,0,0,10,0
        0,1,110,0,1,1,0,0,0,0,0,0,0,10,0
        1,4,120,0,2,1,0,0,0,0,0,0,0,10,0
        """
        violations = verify_gedf_trace(trace, self.exec_map, num_cores=1)
        self.assertTrue(any("Execution Error" in v for v in violations))

    def test_work_conserving_violation(self):
        """Test if it catches an idle core when a task is in the ready queue."""
        # Task 0 is released at 0, but no 'Run' event occurs. Core is idle at tick 0.
        trace = """
        0,0,100,0,0,1,0,0,0,0,0,0,0,10,0
        1,4,120,0,1,1,0,0,0,0,0,0,0,10,0
        """
        violations = verify_gedf_trace(trace, self.exec_map, num_cores=1)
        self.assertTrue(any("Work Conserving Violation" in v for v in violations))

    def test_priority_violation(self):
        """Test if a later deadline task runs while an earlier deadline task waits."""
        # Task 0 (D=10) and Task 1 (D=5) both release at 0.
        # If Task 0 runs on Core 0, it's a priority violation.
        trace = f"""
        {MACRO.TICK_0},{Event.RELEASE},100,0,0,1,{Task.PERIODIC_0},0,0,0,0,0,0,10,0
        {MACRO.TICK_0},{Event.RELEASE},105,0,1,1,{Task.PERIODIC_1},0,0,0,0,0,0,5,1
        {MACRO.TICK_0},{Event.SWITCH_IN},110,0,2,1,{Task.PERIODIC_0},0,0,0,0,0,0,10,0
        """
        # num_cores=1 forces a choice between the two tasks
        violations = verify_gedf_trace(trace, self.exec_map, num_cores=1)
        self.assertTrue(any("Priority Violation" in v for v in violations))

    def test_priority_violation_multicore(self):
        """Test if a later deadline task runs while an earlier deadline task waits."""
        # Task 0 (D=10) and Task 1 (D=5) both release at 0.
        # If Task 0 runs on Core 0, it's a priority violation.
        trace = f"""
        {MACRO.TICK_0},{Event.RELEASE},100,0,0,1,{Task.PERIODIC_0},0,0,0,0,0,0,{MACRO.DEADLINE_7},0
        {MACRO.TICK_0},{Event.RELEASE},105,0,1,1,{Task.PERIODIC_1},0,0,0,0,0,0,{MACRO.DEADLINE_6},1
        {MACRO.TICK_0},{Event.RELEASE},105,0,1,1,{Task.PERIODIC_2},0,0,0,0,0,0,{MACRO.DEADLINE_5},1
        {MACRO.TICK_0},{Event.SWITCH_IN},110,0,2,1,{Task.PERIODIC_1},0,0,0,0,0,0,{MACRO.DEADLINE_6},0
        {MACRO.TICK_0},{Event.SWITCH_IN},110,0,2,1,{Task.PERIODIC_0},0,0,0,0,0,0,{MACRO.DEADLINE_7},0
        """
        # num_cores=1 forces a choice between the two tasks
        violations = verify_gedf_trace(trace, self.exec_map_multicore, num_cores=2)
        self.assertTrue(any("Priority Violation" in v for v in violations))


if __name__ == "__main__":
    unittest.main(argv=["first-arg-is-ignored"], exit=False)
