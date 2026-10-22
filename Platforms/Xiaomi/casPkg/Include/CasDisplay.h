// SPDX-License-Identifier: BSD-2-Clause-Patent
// cas DT / SDE 6.0 register facts; algorithm written independently.
#ifndef CAS_DISPLAY_H
#define CAS_DISPLAY_H
#include <CasPlatform.h>

#define CAS_MDP_BASE 0x0ae00000
#define CAS_MDP_SIZE 0x00085000
#define CAS_PANEL_WIDTH 1080
#define CAS_PANEL_HEIGHT 2340
#define CAS_DISPLAY_ADDRESS (CAS_LOG_BASE + 0x100)
#define CAS_DISPLAY_MAGIC 0x50534443
#define CAS_DISPLAY_VERSION 1
#define CAS_PIPE_COUNT 8
#define CAS_CTL_COUNT 6
#define CAS_MIXER_COUNT 6

typedef unsigned int CAS_U32;
typedef unsigned long long CAS_U64;

typedef struct {
  CAS_U32 Size, Xy, OutSize, OutXy, Address, Stride;
  CAS_U32 Format, Unpack, OpMode, Multirect, Secure;
} CAS_PIPE_SNAPSHOT;

typedef struct {
  CAS_U32 Top, IntfActive, Start, Flush;
  CAS_U32 Layers[CAS_MIXER_COUNT][4];
} CAS_CTL_SNAPSHOT;

typedef struct {
  CAS_U32 Revision;
  CAS_PIPE_SNAPSHOT Pipes[CAS_PIPE_COUNT];
  CAS_CTL_SNAPSHOT Ctls[CAS_CTL_COUNT];
  CAS_U32 IntfAutoRefresh[2];
  CAS_U32 IntfTearEnable[2];
  CAS_U32 PpAutoRefresh[6];
} CAS_DISPLAY_SNAPSHOT;

typedef struct {
  CAS_U64 Base, Bytes;
  CAS_U32 Width, Height, Stride;
  CAS_U32 RedMask, GreenMask, BlueMask, ReservedMask;
  CAS_U32 RedShift, GreenShift, BlueShift;
  CAS_U32 PipeMask, CtlMask, ManualCtlMask, IntfMask;
} CAS_FRAMEBUFFER;

typedef struct {
  CAS_U32 Magic, Version, Size, Error;
  CAS_U32 Ready, Stages, RefreshCount, BusyCount;
  CAS_U32 RefreshErrors, Stopped, LastStage, Reserved;
  CAS_FRAMEBUFFER Framebuffer;
  CAS_DISPLAY_SNAPSHOT Snapshot;
} CAS_DISPLAY_STATE;

typedef char CAS_DISPLAY_STATE_FITS[(sizeof (CAS_DISPLAY_STATE) <= 0xf00) ? 1 : -1];

enum {
  CasDisplayOk, CasDisplayRevision, CasDisplayRoute, CasDisplayVirtualRect,
  CasDisplayGeometry, CasDisplayFormat, CasDisplaySecure,
  CasDisplayBounds, CasDisplayInconsistent, CasDisplayCoverage,
  CasDisplayRefreshMode
};

// Pure decoder: no MMIO or framebuffer accesses. Also compiled in host tests.
CAS_U32 CasDecodeDisplay (const CAS_DISPLAY_SNAPSHOT *Snapshot, CAS_FRAMEBUFFER *Result);
// Operations match UEFI GOP 0..3. Buffer pixels are little-endian B,G,R,reserved.
CAS_U32 CasDisplayBlt (const CAS_FRAMEBUFFER *F, CAS_U32 *Buffer, CAS_U32 Operation,
  CAS_U64 SourceX, CAS_U64 SourceY, CAS_U64 DestinationX, CAS_U64 DestinationY,
  CAS_U64 Width, CAS_U64 Height, CAS_U64 Delta);
CAS_U32 CasDisplayInitialize (void);
CAS_DISPLAY_STATE *CasDisplayState (void);
void CasDisplayRefresh (void);
void CasDisplayMark (CAS_U32 Stage);
void CasDisplayFault (CAS_U64 Error);
void CasDisplayStop (void);

#endif
