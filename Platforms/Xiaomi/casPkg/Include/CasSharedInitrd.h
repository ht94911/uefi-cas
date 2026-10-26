// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifdef CAS_UPSTREAM_PROFILE
STATIC CONST EFI_GUID mUniXNUEntryArgHobGuid = {
  0xDAEE82E6, 0x8090, 0x4E72, { 0xBF, 0xB9, 0x78, 0xC1, 0x3C, 0xD4, 0xE3, 0x59 }
};
STATIC BOOLEAN CasSetExistingInitrd (VOID *Fdt, INT32 Node, CONST CHAR8 *Name, UINT64 Address) {
  INT32 Length;
  CONST VOID *Old = FdtGetProp (Fdt, Node, Name, &Length);
  UINT32 Value32;
  UINT64 Value64;
  if (Old == NULL) { return FALSE; }
  if (Length == 4 && Address <= MAX_UINT32) {
    Value32 = CpuToFdt32 ((UINT32)Address);
    return FdtSetProp (Fdt, Node, Name, &Value32, sizeof (Value32)) == 0;
  }
  if (Length == 8) {
    Value64 = CpuToFdt64 (Address);
    return FdtSetProp (Fdt, Node, Name, &Value64, sizeof (Value64)) == 0;
  }
  return FALSE;
}
STATIC BOOLEAN CasPrepareSharedInitrd (VOID *Fdt, CONST CAS_BOOT_INFO *Info) {
  INT32 Chosen;
  UINT8 CONST *Fat = (VOID *)(UINTN)CAS_PAYLOAD_BASE;
  UINT32 OriginalSize = FdtTotalSize (Fdt);
  if (Info->InitrdCopy != CAS_PAYLOAD_BASE || Info->InitrdSize < 512 ||
      Info->InitrdSize > CAS_PAYLOAD_SIZE || Info->InitrdSize % 512 != 0 ||
      Fat[510] != 0x55 || Fat[511] != 0xaa) { return FALSE; }
  Chosen = FdtPathOffset (Fdt, "/chosen");
  return Chosen >= 0 &&
    CasSetExistingInitrd (Fdt, Chosen, "linux,initrd-start", Info->InitrdCopy) &&
    CasSetExistingInitrd (Fdt, Chosen, "linux,initrd-end", Info->InitrdCopy + Info->InitrdSize) &&
    FdtTotalSize (Fdt) == OriginalSize;
}
#endif
