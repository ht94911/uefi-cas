// SPDX-License-Identifier: BSD-2-Clause-Patent
#include "DiagCore.h"

int CasDiagRangeValid(CD_RANGE R, CD_U64 Alignment) {
  return Alignment && !(Alignment & (Alignment-1)) && R.Size &&
    !(R.Base % Alignment) && !(R.Size % Alignment) && R.Base <= (~0ULL)-R.Size;
}
static int overlap(CD_RANGE A, CD_RANGE B) { return A.Base < B.Base+B.Size && B.Base < A.Base+A.Size; }
static int contains(CD_RANGE A, CD_RANGE B) { return A.Base <= B.Base && B.Base+B.Size <= A.Base+A.Size; }
static int ranges(const CD_RANGE *R, CD_U32 N, int Disjoint) {
  CD_U32 I, J;
  if (N > CD_MAX_RANGES || (N && !R)) return 0;
  for (I=0; I<N; ++I) {
    if (!CasDiagRangeValid(R[I],1)) return 0;
    if (Disjoint) for (J=0; J<I; ++J) if (overlap(R[I],R[J])) return 0;
  }
  return 1;
}
static int covered(const CD_RANGE *R, CD_U32 N, CD_RANGE C) {
  CD_U32 I;
  for (I=0; I<N; ++I) if (contains(R[I],C)) return 1;
  return 0;
}
int CasDiagEligible(CD_RANGE C, const CD_RANGE *Banks, CD_U32 BankCount,
  const CD_RANGE *Smem, CD_U32 SmemCount, const CD_RANGE *Free, CD_U32 FreeCount,
  const CD_RANGE *Reserved, CD_U32 ReservedCount, CD_U32 Complete) {
  CD_U32 I;
  if (!CasDiagRangeValid(C,CD_ALIGNMENT) || C.Base<0x100000000ULL || C.Size!=CD_REQUEST ||
      C.Base+C.Size>0x1000000000000ULL) return CdBadRange;
  if (Complete!=CD_FACTS_COMPLETE) return CdIncomplete;
  if (!ranges(Banks,BankCount,1) || !ranges(Smem,SmemCount,1) ||
      !ranges(Free,FreeCount,1) || !ranges(Reserved,ReservedCount,0)) return CdBadRange;
  if (!covered(Banks,BankCount,C)) return CdNoFdtBank;
  if (!covered(Smem,SmemCount,C)) return CdNoSmemBank;
  if (!covered(Free,FreeCount,C)) return CdNoUefiOwnership;
  for (I=0; I<ReservedCount; ++I) if (overlap(C,Reserved[I])) return CdProtected;
  return CdAllowed;
}
CD_U64 CasDiagPattern(CD_U64 Address, CD_U32 Pass) {
  CD_U64 Value=(Address*0x9e3779b185ebca87ULL)^0xc35a96f0271b4de8ULL;
  return Pass ? ~Value : Value;
}
static CD_U32 read32(const CD_U8 *P) {
  return (CD_U32)P[0]|((CD_U32)P[1]<<8)|((CD_U32)P[2]<<16)|((CD_U32)P[3]<<24);
}
static CD_U64 read64(const CD_U8 *P) { return read32(P)|((CD_U64)read32(P+4)<<32); }
static void put32(CD_U8 *P, CD_U32 V) { CD_U32 I; for(I=0;I<4;++I) P[I]=(CD_U8)(V>>(I*8)); }
static void put64(CD_U8 *P, CD_U64 V) { put32(P,(CD_U32)V);put32(P+4,(CD_U32)(V>>32)); }
static const CD_U8 Magic[8]={'C','A','S','L','O','G','0','1'};
static int ascii(CD_U8 V) { return (V>=32 && V<=126) || V==9 || V==10 || V==13; }
static CD_U32 crc(const CD_U8 *P, CD_U32 N) {
  CD_U32 I,J,C=~0U;
  for(I=0;I<N;++I) {
    C^=(I>=44 && I<48) ? 0U : P[I];
    for(J=0;J<8;++J) C=(C>>1)^(0xedb88320U & (0U-(C&1U)));
  }
  return ~C;
}
static CD_U32 buildlen(const char *Build) {
  CD_U32 I;
  if(!Build) return 0;
  for(I=0;I<32;++I) { if(!Build[I]) return I; if(!ascii((CD_U8)Build[I]) || (CD_U8)Build[I]<32) return 0; }
  return 0;
}
CD_U32 CasDiagLogEncode(CD_U8 *Out, CD_U32 Capacity, const CD_U8 *Payload,
  CD_U32 N, CD_U64 Sequence, CD_U64 BootStamp, CD_U32 Flags, const char *Build) {
  CD_U32 I,B=buildlen(Build),Total;
  if(!Out || !B || !Sequence || !BootStamp || (Flags&~7U) || (N&&!Payload) ||
      Capacity<CD_LOG_HEADER || Capacity>CD_LOG_CAPACITY || N>Capacity-CD_LOG_HEADER) return 0;
  for(I=0;I<N;++I) if(!ascii(Payload[I])) return 0;
  Total=CD_LOG_HEADER+N;
  for(I=0;I<CD_LOG_HEADER;++I) Out[I]=0;
  for(I=0;I<8;++I) Out[I]=Magic[I];
  put32(Out+8,1);put32(Out+12,CD_LOG_HEADER);put32(Out+16,Total);put32(Out+20,N);
  put64(Out+24,Sequence);put64(Out+32,BootStamp);put32(Out+40,Flags);
  for(I=0;I<B;++I) Out[48+I]=(CD_U8)Build[I];
  for(I=0;I<N;++I) Out[CD_LOG_HEADER+I]=Payload[I];
  put32(Out+44,crc(Out,Total));
  return Total;
}
int CasDiagLogValidate(const CD_U8 *In, CD_U32 Available, const char *Build, CD_LOG_INFO *Info) {
  CD_U32 I,N,Total,B,Expected=buildlen(Build);
  if(!In || !Info || Available<8) return CdLogTruncated;
  for(I=0;I<8;++I) if(In[I]!=Magic[I]) return CdLogMissing;
  if(Available<CD_LOG_HEADER) return CdLogTruncated;
  Total=read32(In+16);N=read32(In+20);
  if(read32(In+8)!=1 || read32(In+12)!=CD_LOG_HEADER || Total<CD_LOG_HEADER ||
      Total>CD_LOG_CAPACITY || N!=Total-CD_LOG_HEADER || !read64(In+24) ||
      !read64(In+32) || (read32(In+40)&~7U)) return CdLogHeader;
  if(Total>Available) return CdLogTruncated;
  for(I=80;I<96;++I) if(In[I]) return CdLogHeader;
  B=0;
  while(B<32 && In[48+B]) { if(In[48+B]<32 || In[48+B]>126) return CdLogHeader; ++B; }
  if(!B || B==32) return CdLogHeader;
  for(I=B;I<32;++I) if(In[48+I]) return CdLogHeader;
  if(read32(In+44)!=crc(In,Total)) return CdLogChecksum;
  for(I=0;I<N;++I) if(!ascii(In[CD_LOG_HEADER+I])) return CdLogHeader;
  Info->Length=N;Info->Flags=read32(In+40);Info->Sequence=read64(In+24);Info->BootStamp=read64(In+32);
  for(I=0;I<32;++I) Info->Build[I]=In[48+I];
  if(Build) {
    if(!Expected || Expected!=B) return CdLogForeign;
    for(I=0;I<B;++I) if((CD_U8)Build[I]!=In[48+I]) return CdLogForeign;
  }
  return CdLogValid;
}

CD_PROBE_RESULT CasDiagRunProbe(CD_RANGE R, int Gate, const CD_PROBE_IO *Io, void *Context) {
  CD_PROBE_RESULT Result={CdProbeDenied,0,0};CD_U64 Actual=0;CD_U32 Pass;
  if(Gate!=CdAllowed || !CasDiagRangeValid(R,CD_ALIGNMENT) || R.Base<0x100000000ULL || R.Size!=CD_REQUEST ||
      !Io || !Io->Allocate || !Io->Write || !Io->Cache || !Io->Read || !Io->Clear || !Io->Free)return Result;
  if(Io->Allocate(Context,R,&Actual)) {Result.Result=CdProbeAllocation;return Result;}
  if(Actual!=R.Base) {
    Result.Result=CdProbeAddress;R.Base=Actual;
    Result.ReleaseFailed=Io->Free(Context,R)!=0;return Result;
  }
  Result.Result=CdProbePass;
  for(Pass=0;Pass<2;++Pass) {
    if(Io->Write(Context,R,Pass)) {Result.Result=CdProbeWrite;break;}
    if(Io->Cache(Context,R)) {Result.Result=CdProbeCache;break;}
    if(Io->Read(Context,R,Pass)) {Result.Result=CdProbeRead;break;}
  }
  // All ordinary error returns after ownership take the same bounded cleanup.
  Result.CleanupFailed=Io->Clear(Context,R)!=0;
  if(Io->Cache(Context,R))Result.CleanupFailed=1;
  Result.ReleaseFailed=Io->Free(Context,R)!=0;
  if(Result.Result==CdProbePass) {
    if(Result.CleanupFailed)Result.Result=CdProbeClear;
    else if(Result.ReleaseFailed)Result.Result=CdProbeRelease;
  }
  return Result;
}
