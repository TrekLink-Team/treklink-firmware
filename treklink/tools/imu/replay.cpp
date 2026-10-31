// Replay `tk.py imu` captures through the firmware's fall detector core (specs/fall-detection design §7).
//
//   g++ -std=c++11 -O2 -o replay treklink/tools/imu/replay.cpp
//   ./replay [--bias gx,gy,gz] [--still-gyro rads] [--impact-g g] [--ff-g g] [--ff-ms ms] file-samples.csv...
//
// Prints every detector event with its time, so a threshold change can be checked against recorded
// falls and rests before it is flashed. --bias is the per-axis gyro mean in rad/s, as summary.py
// prints it for a rest capture; without it the raw angular rate is used.

#include "../../src/fall/FallDetector.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace treklink::fall;

static const char *eventName(Event e)
{
    switch (e) {
    case Event::CHECK_STARTED:
        return "CHECK_STARTED";
    case Event::USER_OK:
        return "USER_OK";
    case Event::RAISE_PREALARM:
        return "RAISE_PREALARM";
    default:
        return "NONE";
    }
}

static int replay(const char *path, DetectorConfig cfg, const float bias[3])
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return 1;
    }
    char line[512];
    int col[12];
    for (int &c : col)
        c = -1;
    const char *want[] = {"t_ms", "gx", "gy", "gz", "accel_g"};
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 1;
    }
    {
        int i = 0;
        for (char *tok = strtok(line, ",\r\n"); tok; tok = strtok(nullptr, ",\r\n"), i++)
            for (int k = 0; k < 5; k++)
                if (!strcmp(tok, want[k]))
                    col[k] = i;
    }
    for (int k = 0; k < 5; k++)
        if (col[k] < 0) {
            fprintf(stderr, "%s: no column %s\n", path, want[k]);
            fclose(f);
            return 1;
        }

    FallDetector d(cfg);
    int n = 0, events = 0;
    printf("%s\n", path);
    while (fgets(line, sizeof(line), f)) {
        std::vector<std::string> v;
        for (char *tok = strtok(line, ",\r\n"); tok; tok = strtok(nullptr, ",\r\n"))
            v.push_back(tok);
        if ((int)v.size() <= col[4])
            continue;
        uint32_t t = (uint32_t)strtoul(v[col[0]].c_str(), nullptr, 10);
        float gx = strtof(v[col[1]].c_str(), nullptr) - bias[0];
        float gy = strtof(v[col[2]].c_str(), nullptr) - bias[1];
        float gz = strtof(v[col[3]].c_str(), nullptr) - bias[2];
        float a = strtof(v[col[4]].c_str(), nullptr);
        float w = std::sqrt(gx * gx + gy * gy + gz * gz);
        Event e = d.step(t, a, w);
        n++;
        if (e != Event::NONE) {
            events++;
            printf("  %8u ms  %-15s", t, eventName(e));
            if (e == Event::CHECK_STARTED)
                printf(" cause %s, free fall %u ms", d.cause() == Cause::IMPACT ? "impact" : "free fall", d.lastFreeFallMs());
            printf("\n");
        }
    }
    fclose(f);
    printf("  %d samples, %d events\n", n, events);
    return 0;
}

int main(int argc, char **argv)
{
    DetectorConfig cfg;
    float bias[3] = {0, 0, 0};
    int rc = 0, files = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--bias") && i + 1 < argc) {
            if (sscanf(argv[++i], "%f,%f,%f", &bias[0], &bias[1], &bias[2]) != 3) {
                fprintf(stderr, "--bias wants gx,gy,gz\n");
                return 2;
            }
        } else if (!strcmp(argv[i], "--still-gyro") && i + 1 < argc) {
            cfg.stillGyroRads = strtof(argv[++i], nullptr);
        } else if (!strcmp(argv[i], "--impact-g") && i + 1 < argc) {
            cfg.impactG = strtof(argv[++i], nullptr);
        } else if (!strcmp(argv[i], "--ff-g") && i + 1 < argc) {
            cfg.freeFallG = strtof(argv[++i], nullptr);
        } else if (!strcmp(argv[i], "--ff-ms") && i + 1 < argc) {
            cfg.freeFallMinMs = (uint32_t)strtoul(argv[++i], nullptr, 10);
        } else {
            rc |= replay(argv[i], cfg, bias);
            files++;
        }
    }
    if (!files) {
        fprintf(stderr, "usage: replay [--bias gx,gy,gz] [--still-gyro rads] [--impact-g g] [--ff-g g] [--ff-ms ms] "
                        "file-samples.csv...\n");
        return 2;
    }
    return rc;
}
