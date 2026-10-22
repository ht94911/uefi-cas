// SPDX-License-Identifier: BSD-2-Clause-Patent
// Independent limited v5 observer reader. Never issues a PMIC write command.
#include <CasInput.h>

CAS_INPUT_U32 CasSpmiInitialize (const CAS_SPMI_BUS *B, CAS_SPMI_STATE *S) {
  CAS_INPUT_U32 I, Entry, Ppid, Owner, *Target;
  if (B == 0 || S == 0 || B->Read32 == 0 || B->Write32 == 0 || B->DelayUs == 0) { return CasSpmiBadBus; }
  S->Ready = 0; S->LastStatus = 0; S->LastAddress = 0;
  S->PonApid = CAS_SPMI_APID_COUNT; S->GpioApid = CAS_SPMI_APID_COUNT;
  S->Version = B->Read32 (B->Context, CAS_SPMI_CORE);
  if ((S->Version >> 28) != 5) { return CasSpmiVersion; }
  for (I = 0; I < CAS_SPMI_APID_COUNT; ++I) {
    Entry = B->Read32 (B->Context, CAS_SPMI_CORE + CAS_SPMI_MAP_OFFSET + I * 4);
    if (!Entry) { continue; }
    Ppid = (Entry >> 8) & 0xfff;
    if (Ppid == (CAS_PON_RT_STS >> 8)) { Target = &S->PonApid; }
    else if (Ppid == (CAS_GPIO6_RT_STS >> 8)) { Target = &S->GpioApid; }
    else { continue; }
    Owner = B->Read32 (B->Context, CAS_SPMI_CONFIG + CAS_SPMI_OWNER_OFFSET + I * 4) & 7;
    // Observer reads do not require write ownership. Prefer this EE's mapping;
    // if none exists use the first matching observer mapping, as v5 allows.
    if (*Target == CAS_SPMI_APID_COUNT || Owner == CAS_SPMI_EE) { *Target = I; }
  }
  if (S->PonApid == CAS_SPMI_APID_COUNT || S->GpioApid == CAS_SPMI_APID_COUNT) { return CasSpmiMapping; }
  S->Ready = 1;
  return CasSpmiOk;
}

CAS_INPUT_U32 CasSpmiReadByte (const CAS_SPMI_BUS *B, CAS_SPMI_STATE *S, CAS_INPUT_U32 Address, CAS_INPUT_U32 *Value) {
  CAS_INPUT_U32 Apid, Offset, Attempt, Status;
  if (B == 0 || S == 0 || Value == 0 || B->Read32 == 0 || B->Write32 == 0 || B->DelayUs == 0) { return CasSpmiBadBus; }
  *Value = 0;
  if (!S->Ready || (S->Version >> 28) != 5) { return CasSpmiVersion; }
  // SID0 only. Reset-configuration bytes are allowed for diagnostics, not writes.
  if (Address == CAS_PON_RT_STS || (Address >= 0x840 && Address <= 0x84b)) { Apid = S->PonApid; }
  else if (Address == CAS_GPIO6_RT_STS) { Apid = S->GpioApid; }
  else { return CasSpmiAddress; }
  if (Apid >= CAS_SPMI_APID_COUNT) { return CasSpmiMapping; }
  Offset = 0x10000 * CAS_SPMI_EE + 0x80 * Apid;
  if (Offset > CAS_SPMI_OBSERVER_SIZE - 0x20) { return CasSpmiMapping; }
  S->LastAddress = Address; S->LastStatus = 0;
  B->Write32 (B->Context, CAS_SPMI_OBSERVER + Offset, (1u << 27) | ((Address & 255) << 4));
  for (Attempt = 0; Attempt < CAS_SPMI_TIMEOUT_US; ++Attempt) {
    Status = B->Read32 (B->Context, CAS_SPMI_OBSERVER + Offset + 8);
    S->LastStatus = Status;
    if (Status & 1) {
      if (Status & 4) { return CasSpmiDenied; }
      if (Status & 10) { return CasSpmiFailure; }
      *Value = B->Read32 (B->Context, CAS_SPMI_OBSERVER + Offset + 0x18) & 255;
      return CasSpmiOk;
    }
    B->DelayUs (B->Context, 1);
  }
  return CasSpmiTimeout;
}
