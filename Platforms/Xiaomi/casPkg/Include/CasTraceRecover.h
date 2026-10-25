// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef CAS_TRACE_RECOVER_H
#define CAS_TRACE_RECOVER_H
#include <CasEarlyText.h>
// Pure formatter, consumes two384-byte snapshots, writes only its output text.
CAS_U32 CasTraceDescribe(const unsigned char *A, const unsigned char *B,
                        CAS_U64 CurrentBoot, CAS_U64 ExpectedTag, CAS_EARLY_TEXT *Text);
void CasTraceRecoverEarly(void);
#endif
