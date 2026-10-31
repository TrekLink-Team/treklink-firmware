/*
 * TrekLink overlay entry point (specs/architecture design §3.1).
 *
 * Each TrekLink variant.cpp defines the stock weak hook lateInitVariant() and calls treklink::start().
 * Stock setup() runs it after modules, input, the screen and the radio are up (src/main.cpp:991).
 */
#pragma once

namespace treklink
{

/// Logs the TrekLink identity, applies first-boot defaults and creates the TrekLink modules the
/// variant's capabilities enable. Called once, from lateInitVariant().
void start();

/// Factory defaults written into config by stock variantDefaultConfig() (VariantDefaults.cpp).
void applyConfigDefaults();

} // namespace treklink
