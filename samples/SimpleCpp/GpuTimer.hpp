#ifndef GpuTimer_Hpp
#define GpuTimer_Hpp

#include <vector>
#include <queue>
#include <stdint.h>

typedef uint32_t u32;

/// GpuTimer must *not* be used as a temporary variable. Create it once on startup and
/// then call start() and elapsed() every frame around OpenGL calls you want to time (the same every time).
/// Use several GpuTimers if you want to time different sequence of OpenGL calls. This is because
/// of the latency involved: elapsed() returns the oldest available measurement, which might
/// be several frames behind.
class GpuTimer
{
public:
    /// The next OpenGL call will be the first one timed.
    void start();

    /// Returns the *oldest available* measurement. The duration returned might be several frames behind.
    double elapsed();

private:
    struct QueryPair
    {
        u32 startQuery, endQuery;
    };

    std::queue<QueryPair> inflightPairs; // Pairs of queries we're waiting on.
    std::queue<QueryPair> availablePairs; // Freed pairs.
};

/// The purpose of SampleStats is to simplify common operations on profiling data (e.g. code segment execution times).
template <typename T, int SIZE=50>
class SampleStats
{
public:
    /// Allocates sample of size SIZE. Stats (average, min, max) are invalid until sample has been fully filled up (after
    /// that adding new data will remove the oldest from the sample).
    SampleStats(): data(SIZE, T(0)), currentIndex(0), recomputeNeeded(true) {}

    /// Adds value to the sample data, removing the oldest value in the data if necessary (when sample size is over SIZE).
    void add(const T& value)
    {
        if (currentIndex == SIZE)
            currentIndex = 0;
        data[currentIndex] = value;
        currentIndex++;
        recomputeNeeded = true;
    }

    T average()
    {
        if (recomputeNeeded)
            recomputeStats();
        return cachedAvg;
    }

    T min()
    {
        if (recomputeNeeded)
            recomputeStats();
        return cachedMin;
    }

    T max()
    {
        if (recomputeNeeded)
            recomputeStats();
        return cachedMax;
    }

private:
    void recomputeStats()
    {
        T sum = T(0);
        cachedMin = data[0];
        cachedMax = data[0];
        for (int i = 0; i < SIZE; ++i) {
            const T& v = data[i];
            sum += v;
            if (v < cachedMin) cachedMin = v;
            if (cachedMax < v) cachedMax = v;
        }
        cachedAvg = sum / SIZE;
        recomputeNeeded = false;
    }

    std::vector<T> data;
    int currentIndex;
    T cachedAvg, cachedMin, cachedMax;
    bool recomputeNeeded;
};

#endif // GpuTimer_Hpp
