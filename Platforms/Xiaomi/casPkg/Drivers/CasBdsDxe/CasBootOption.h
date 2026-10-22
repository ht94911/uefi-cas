// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef CAS_BOOT_OPTION_H
#define CAS_BOOT_OPTION_H
#include <Uefi.h>
BOOLEAN CasParseUniXnuOption (CONST UINT8 *Data, UINTN Size, CONST EFI_GUID *App,
                             UINTN *PathOffset, UINTN *PathSize);
EFI_STATUS CasBootUniXnu (EFI_HANDLE Parent);
#endif
