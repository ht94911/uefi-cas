// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Library/ConfigurationMapLib.h>
// No stock-XBL consumer is selected; keep only descriptive values.
STATIC EFI_CONFIGURATION_ENTRY_DESCRIPTOR mConfiguration[] = {
  {"NumCpus", 8}, {"NumActiveCores", 1}, {"EnableMultiThreading", 0},
  {"EnableUfsIOC", 0}, {"EnableShell", 0}, {"EnableLogFsSyncInRetail", 0}
};
VOID GetConfigurationMap (EFI_CONFIGURATION_ENTRY_DESCRIPTOR **Map, UINT8 *Count) {
  *Map = mConfiguration;
  *Count = ARRAY_SIZE (mConfiguration);
}
