// Host replay of src/modules/FallEngine.h on a tk.py imu samples CSV; prints every pre-alarm and recovery.
// Build: g++ -std=c++11 -O2 -I src/modules test/fall_engine/replay.cpp -o /tmp/fall_replay
// Run:   /tmp/fall_replay <samples.csv>
#include "FallEngine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s samples.csv\n", argv[0]);
        return 2;
    }
    FILE *f = std::fopen(argv[1], "r");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    treklink::FallEngine e;
    char line[512];
    long t0 = -1;
    int alarms = 0;
    static const char *R[] = {"none", "freefall", "impact", "gait", "window_end", "shaking", "still_tilted", "down_no_recovery"};
    while (std::fgets(line, sizeof line, f)) {
        // label,seq,t_ms,ax,ay,az,gx,gy,gz,accel_g,gyro_rads,state
        char *tok[16];
        int n = 0;
        for (char *p = std::strtok(line, ",\r\n"); p && n < 16; p = std::strtok(nullptr, ",\r\n"))
            tok[n++] = p;
        if (n != 12 || !std::strchr("0123456789", tok[2][0]))
            continue;
        char *end;
        long t = std::strtol(tok[2], &end, 10);
        if (*end)
            continue;
        float v[8];
        bool ok = true;
        for (int i = 0; i < 8 && ok; i++) {
            v[i] = std::strtof(tok[3 + i], &end);
            ok = *end == 0;
        }
        if (!ok || v[6] > 40)
            continue;
        if (t0 < 0)
            t0 = t;
        treklink::FallEngine::Event ev = e.update((uint32_t)t, v[0], v[1], v[2], v[3], v[4], v[5]);
        if (ev == treklink::FallEngine::PREALARM) {
            alarms++;
            std::printf("%8.1f prealarm %s\n", (t - t0) / 1000.0, R[e.reason]);
        }
    }
    std::printf("alarms %d\n", alarms);
    return 0;
}
