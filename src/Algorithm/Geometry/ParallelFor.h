#pragma once

#include <algorithm>
#include <thread>
#include <vector>

// [0, count) 를 덩어리로 나눠 여러 스레드에서 func(chunk, begin, end) 를 부른다.
// 덩어리별 결과를 따로 모으려면 GetParallelChunkCount 로 칸 수를 먼저 정한다
inline int GetParallelChunkCount(int count, int minChunk)
{
    const int iHardware = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    return std::max(1, std::min(iHardware, count / std::max(1, minChunk)));
}

template <typename Func>
void ParallelFor(int count, int minChunk, Func func)
{
    const int iChunks = GetParallelChunkCount(count, minChunk);
    const int iStep = (count + iChunks - 1) / std::max(1, iChunks);
    if (iChunks <= 1)
    {
        func(0, 0, count);
        return;
    }

    std::vector<std::thread> vecThreads;
    vecThreads.reserve(static_cast<size_t>(iChunks - 1));
    for (int iChunk = 1; iChunk < iChunks; ++iChunk)
    {
        const int iBegin = iChunk * iStep;
        const int iEnd = std::min(count, iBegin + iStep);
        if (iBegin < iEnd)
        {
            vecThreads.emplace_back(func, iChunk, iBegin, iEnd);
        }
    }
    func(0, 0, std::min(count, iStep));
    for (std::thread& thWorker : vecThreads)
    {
        thWorker.join();
    }
}
