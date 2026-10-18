// bench_ram.cpp - RAM-scaling harness: process RSS vs number of MMR entries.
// The MMR is the main persistent-growth structure in a node (it grows with the
// chain), so bytes/entry is a device-relevant number. Also reports RSS before/after.
#include "org/minima/objects/mmr/m_m_r.hpp"
#include "org/minima/objects/mmr/m_m_r_data.hpp"
#include "org/minima/objects/mmr/m_m_r_entry.hpp"
#include "org/minima/objects/base/mini_data.hpp"
#include "org/minima/objects/base/mini_number.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
static long rssKb() {
    task_basic_info t; mach_msg_type_number_t c = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&t), &c) != KERN_SUCCESS) return -1;
    return static_cast<long>(t.resident_size / 1024);
}
#else
#include <fstream>
#include <unistd.h>
static long rssKb() {
    std::ifstream f("/proc/self/statm"); long size = 0, res = 0;
    if (!(f >> size >> res)) return -1;
    return res * (sysconf(_SC_PAGESIZE) / 1024);
}
#endif

using org::minima::objects::mmr::MMR;
using org::minima::objects::mmr::MMRData;
using org::minima::objects::base::MiniData;
using org::minima::objects::base::MiniNumber;

int main(int argc, char** argv) {
    const int mmrN = (argc > 1) ? std::atoi(argv[1]) : 100000;
    const long base = rssKb();
    std::printf("rss_baseline_kb\t%ld\tkB\n", base);

    MMR mmr;
    const int step = mmrN / 5 > 0 ? mmrN / 5 : 1;
    long prev = base;
    const auto tb0 = std::chrono::steady_clock::now();
    for (int i = 0; i < mmrN; ++i) {
        std::vector<std::uint8_t> h(32, 0);
        h[0] = static_cast<std::uint8_t>(i);
        h[1] = static_cast<std::uint8_t>(i >> 8);
        h[2] = static_cast<std::uint8_t>(i >> 16);
        h[3] = static_cast<std::uint8_t>(i >> 24);
        MMRData d(MiniData(h), MiniNumber::ZERO());
        mmr.addEntry(d);
        if ((i + 1) % step == 0) {
            const long r = rssKb();
            std::printf("mmr_%d_rss_kb\t%ld\tkB (delta %ld)\n", i + 1, r, r - prev);
            prev = r;
        }
    }
    const auto tb1 = std::chrono::steady_clock::now();
    std::printf("mmr_build_ms\t%.1f\tms\n",
                std::chrono::duration<double, std::milli>(tb1 - tb0).count());
    const long after = rssKb();
    std::printf("rss_after_mmr_kb\t%ld\tkB\n", after);
    std::printf("mmr_delta_kb\t%ld\tkB\n", after - base);
    std::printf("mmr_bytes_per_entry\t%.2f\tB\n", mmrN > 0 ? (double)(after - base) * 1024.0 / mmrN : 0.0);
    return 0;
}
