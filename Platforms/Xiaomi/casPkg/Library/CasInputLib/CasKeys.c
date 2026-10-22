// SPDX-License-Identifier: BSD-2-Clause-Patent
// Debounced single-key events. Power confirms on release and never repeats.
#include <CasInput.h>

CAS_INPUT_U32 CasKeySample (CAS_INPUT_U32 Pon, CAS_INPUT_U32 Gpio) {
  return ((Pon & 1) ? CasRawPower : 0) | ((Pon & 2) ? CasRawDown : 0) | ((Gpio & 1) ? 0 : CasRawUp);
}

CAS_INPUT_U32 CasKeysUpdate (CAS_KEY_STATE *S, CAS_INPUT_U32 Raw, CAS_INPUT_U64 Now) {
  CAS_INPUT_U32 Old;
  if (S == 0) { return CasKeyFlush; }
  if (Raw > 7) { S->Initialized = 0; S->Armed = 0; return CasKeyFlush; }
  if (!S->Initialized || Now < S->LastNow) {
    S->Initialized = 1; S->Armed = 0; S->Raw = Raw; S->Stable = Raw;
    S->PowerLong = 0; S->Repeating = 0;
    S->LastNow = Now; S->RawSince = Now; S->PressSince = Now; S->RepeatSince = Now;
    return CasKeyNone;
  }
  S->LastNow = Now;
  if (Raw && (Raw & (Raw - 1))) {
    S->Armed = 0; S->Raw = Raw; S->Stable = Raw; S->RawSince = Now;
    return CasKeyFlush;
  }
  if (Raw != S->Raw) { S->Raw = Raw; S->RawSince = Now; return CasKeyNone; }
  if (Now - S->RawSince < CAS_KEY_DEBOUNCE_MS) { return CasKeyNone; }
  if (!S->Armed) {
    if (Raw == 0) { S->Armed = 1; S->Stable = 0; }
    return CasKeyNone;
  }
  if (Raw != S->Stable) {
    Old = S->Stable; S->Stable = Raw; S->PressSince = Now; S->RepeatSince = Now; S->Repeating = 0;
    if (Raw == CasRawPower) { S->PowerLong = 0; return CasKeyNone; }
    if (Raw == CasRawUp) { return CasKeyUp; }
    if (Raw == CasRawDown) { return CasKeyDown; }
    if (Old == CasRawPower && !S->PowerLong) { return CasKeyEnter; }
    return CasKeyNone;
  }
  if (Raw == CasRawPower && !S->PowerLong && Now - S->PressSince >= CAS_KEY_POWER_LONG_MS) {
    S->PowerLong = 1; return CasKeyEscape;
  }
  if ((Raw == CasRawUp || Raw == CasRawDown) &&
      Now - S->RepeatSince >= (S->Repeating ? CAS_KEY_REPEAT_MS : CAS_KEY_REPEAT_DELAY_MS)) {
    S->Repeating = 1; S->RepeatSince = Now;
    return Raw == CasRawUp ? CasKeyUp : CasKeyDown;
  }
  return CasKeyNone;
}
