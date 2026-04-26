from verify import verify_gedf_trace
from verify_data import (
    TRACE_3,
    EXECUTION_MAP_3,
    TRACE_4,
    EXECUTION_MAP_4,
    TRACE_5,
    EXECUTION_MAP_5,
)

TRACES = [TRACE_3, TRACE_4, TRACE_5]

EXECUTION_MAPS = [EXECUTION_MAP_3, EXECUTION_MAP_4, EXECUTION_MAP_5]

for i in range(len(TRACES)):
    ofst = 3
    trace = TRACES[i]
    map = EXECUTION_MAPS[i]
    results = verify_gedf_trace(trace, map)
    if not results:
        print(
            f"Test {i + ofst}: Verification Successful: Trace matches Global EDF behavior."
        )
    else:
        print(f"Test {i + ofst}: Verification Failed: Found {len(results)} violations.")
        for v in results[:10]:  # Print first 10
            print(v)
