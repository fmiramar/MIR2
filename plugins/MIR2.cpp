// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"

InterfaceTable* ft = nullptr;

PluginLoad(MIR2) {
    ft = inTable;
    mir2plugin::registerSpectralUnits();
    mir2plugin::registerPitchUnits();
}
