// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef CAS_TRACE_RECOVER_H
#define CAS_TRACE_RECOVER_H
#include <CasEarlyText.h>
#define CAS_TRACE_RECOVERY_BYTES 1280
#ifndef CAS_TRACE_RECOVERY_TAG
#define CAS_TRACE_RECOVERY_TAG 0x5862dbb7383d971aULL
#endif
typedef char CAS_TRACE_RECOVERY_FITS[(CAS_TRACE_RECOVERY_BYTES <= CAS_HANDOFF_LOG_SIZE) ? 1 : -1];
typedef char CAS_TRACE_RECOVERY_STACK_FITS[(2 * CAS_TRACE_RECOVERY_BYTES + sizeof(CAS_EARLY_TEXT) <= 0x2000) ? 1 : -1];
// Pure formatter, consumes two1280-byte snapshots, writes only its output text.
CAS_U32 CasTraceDescribe(const unsigned char *A, const unsigned char *B,
                        CAS_U64 CurrentBoot, CAS_U64 ExpectedTag, CAS_EARLY_TEXT *Text);
void CasTraceRecoverEarly(void);
#endif
