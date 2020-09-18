Timer.h: 

A simple templated profiling instrument to time multi-thread code execution.

A Timer counts the number of times it's scope is entered and its cumulative execution duration.
These values are collected into a Record struct, namely into its templated data members of types
I and std::chrono::duration<R>, respecitvely, with respective template parameters defaulted to
sizer_t and double. Time durations are measured with a generic clock of template type Clock,
which defaults to std::chrono::system_clock. A measurement starts at Timer's construction and 
stops and is recorded as Timer goes out of scope, unless the stop() function has already been 
called. A granularity template parameter allows to toggle switch on and off Timers for 
progressively lighter blockss of code. All Records are stored in the same object, a static
Register consisting of a generic Map with generic Key,Value pair defaulting to std::string and
Recorc respectively. Timers' name tags are appended to a static string so that embedded scopes
with the desired level of granularity can be traced to measure their footprint on the enclosing
function performance.

To deal with the multi-thread case, we arrange for different threads to write conncurrently to
different Registers. Threads are mapped to a unique Register by a generic AtomicMapper object.
In our default implementation, Registers are laid out in a static array, whose indexes have 
associated std::atomic_flags functioning like gates, i.e., indicating whether the Register is
available or has already been assigned. When a Timer is first constructed by a thread, it sends
a request for a register index to the AtomicMapper, which maps the thread to a free gate and
returns the corresponding array index. The register index is static, so it needs initialization
only once. In our implementation the mapping is based on a generic Hash function, defaulted to
std::hash<Key>, converting a generic Key, defaulted to std::thread_id, to an array index. If the
index returned by the hash function corresponds to a gate that is not free (collision) the index
is incremented till a free gate is found. Based on simple tests, performance remains negligibly
affected by collisions up to a load factors < 70%. Hence, we use about 40% more Registers/Gates
than threads.

In addition to basic timing, Timer can measure simple statistics such as the RMS and the MAX 
execution time. An option to measure the overhead associated with the setup of Timer itself was
also attempted but eventually removed as the unaccounted costs of the constructor/destructor
functions amounting to 50-100% of the total cause gross underestimates.

To use compile with: -DUSE_TIMER[=TIMER_GRANULARITY] -DTIMER_STATS -DMULTI_THREAD

