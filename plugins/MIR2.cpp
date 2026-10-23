// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"

InterfaceTable* ft = nullptr;

PluginLoad(MIR2) {
    ft = inTable;
    mir2plugin::registerSpectralUnits();
    mir2plugin::registerPitchUnits();
    mir2plugin::registerOnsetUnits();
    mir2plugin::registerTemporalUnits();
    mir2plugin::registerStereoUnits();
    mir2plugin::registerFeatureDynamicsUnits();
    mir2plugin::registerAdvancedSpectralUnits();
    mir2plugin::registerAuditoryUnits();
    mir2plugin::registerPitchAdvancedUnits();
    mir2plugin::registerWaveletUnits();
    mir2plugin::registerConstantQUnits();


}
