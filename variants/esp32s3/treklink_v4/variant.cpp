/*
 * TrekLink start-up for a LilyGo variant (specs/architecture design §3.1).
 * Compiled only by its TrekLink env, through the env's build_src_filter.
 */
#include "TrekLink.h"

// Stock weak hook, called once modules, input, screen and radio are up (src/main.cpp:991).
void lateInitVariant()
{
    treklink::start();
}
