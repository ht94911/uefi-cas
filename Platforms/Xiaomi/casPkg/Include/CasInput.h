// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef CAS_INPUT_H
#define CAS_INPUT_H
#include <CasPlatform.h>

#define CAS_SPMI_CORE 0x0c440000
#define CAS_SPMI_CORE_SIZE 0x1100
#define CAS_SPMI_OBSERVER 0x0e600000
#define CAS_SPMI_OBSERVER_SIZE 0x100000
#define CAS_SPMI_CONFIG 0x0c40a000
#define CAS_SPMI_CONFIG_SIZE 0x26000
#define CAS_SPMI_MAP_OFFSET 0x900
#define CAS_SPMI_OWNER_OFFSET 0x700
#define CAS_SPMI_APID_COUNT 512
#define CAS_SPMI_EE 0
#define CAS_SPMI_TIMEOUT_US 1000
#define CAS_PON_RT_STS 0x0810
#define CAS_GPIO6_RT_STS 0xc510
#define CAS_KEY_DEBOUNCE_MS 20
#define CAS_KEY_REPEAT_DELAY_MS 500
#define CAS_KEY_REPEAT_MS 120
#define CAS_KEY_POWER_LONG_MS 700
#define CAS_KEY_POLL_100NS 50000
#define CAS_INPUT_DIAG_ADDRESS (CAS_LOG_BASE + 0x800)
#define CAS_INPUT_DIAG_MAGIC 0x59454b43

typedef unsigned int CAS_INPUT_U32;
typedef unsigned long long CAS_INPUT_U64;

enum { CasSpmiOk, CasSpmiVersion, CasSpmiMapping, CasSpmiAddress,
       CasSpmiDenied, CasSpmiFailure, CasSpmiTimeout, CasSpmiBadBus };
enum { CasKeyNone, CasKeyUp, CasKeyDown, CasKeyEnter, CasKeyEscape, CasKeyFlush };
enum { CasRawUp = 1, CasRawDown = 2, CasRawPower = 4 };

typedef struct {
  CAS_INPUT_U32 (*Read32)(void *Context, CAS_INPUT_U64 Address);
  void (*Write32)(void *Context, CAS_INPUT_U64 Address, CAS_INPUT_U32 Value);
  void (*DelayUs)(void *Context, CAS_INPUT_U32 Us);
  void *Context;
} CAS_SPMI_BUS;

typedef struct {
  CAS_INPUT_U32 Version, Ready, PonApid, GpioApid, LastStatus, LastAddress;
} CAS_SPMI_STATE;

typedef struct {
  CAS_INPUT_U32 Initialized, Armed, Raw, Stable, PowerLong, Repeating;
  CAS_INPUT_U64 LastNow, RawSince, PressSince, RepeatSince;
} CAS_KEY_STATE;

typedef struct {
  CAS_INPUT_U32 Magic, Version, SpmiVersion, InitStatus, PonApid, GpioApid;
  CAS_INPUT_U32 LastStatus, LastRaw, ReadErrors, Dropped, Events;
  CAS_INPUT_U32 ResetConfig[12];
} CAS_INPUT_DIAG;

typedef char CAS_INPUT_DIAG_FITS[(sizeof (CAS_INPUT_DIAG) <= 0x800) ? 1 : -1];

CAS_INPUT_U32 CasSpmiInitialize (const CAS_SPMI_BUS *Bus, CAS_SPMI_STATE *State);
CAS_INPUT_U32 CasSpmiReadByte (const CAS_SPMI_BUS *Bus, CAS_SPMI_STATE *State,
                             CAS_INPUT_U32 Address, CAS_INPUT_U32 *Value);
CAS_INPUT_U32 CasKeySample (CAS_INPUT_U32 Pon, CAS_INPUT_U32 Gpio);
CAS_INPUT_U32 CasKeysUpdate (CAS_KEY_STATE *State, CAS_INPUT_U32 Raw, CAS_INPUT_U64 NowMs);
#endif
