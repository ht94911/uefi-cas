// SPDX-License-Identifier: BSD-2-Clause-Patent
// Recovery-only shim. No UEFI/EL/MMU/allocator/storage/kernel operations.
#include <CasTraceRecover.h>

static CAS_U64 Q(const unsigned char *P, CAS_U32 At) {
  CAS_U64 V=0; CAS_U32 I;
  for(I=0;I<8;++I)V|=(CAS_U64)P[At+I]<<(I*8);
  return V;
}
static CAS_U32 Crc(const unsigned char *P, CAS_U32 N) {
  CAS_U32 V=~0U,I,J;
  for(I=0;I<N;++I){V^=P[I];for(J=0;J<8;++J)V=(V>>1)^((0U-(V&1U))&0xedb88320U);}
  return ~V;
}
static CAS_U32 Known(CAS_U64 V) {
  return (V>=1 && V<=5) || V==0x10 || (V>=0x20 && V<=0x22) ||
         (V>=0x40 && V<=0x47) || ((V>=0x50 && V<=0x56) || V==0x5f || (V>=0x60 && V<=0x66)) || V==0x7f || V==0xe0;
}
static void Str(CAS_EARLY_TEXT *T,const char *S) {
  CAS_U32 N=0;while(S[N])++N;CasEarlyTextFeed(T,(const unsigned char *)S,N);
}
static void Hex(CAS_EARLY_TEXT *T,CAS_U64 V) {
  static const char Digits[]="0123456789ABCDEF";
  unsigned char S[16];CAS_U32 I;
  for(I=0;I<16;++I)S[I]=(unsigned char)Digits[(V>>(60-I*4))&15];
  CasEarlyTextFeed(T,S,16);
}
static void Pair(CAS_EARLY_TEXT *T,const char *L,CAS_U64 A,const char *R,CAS_U64 B) {
  Str(T,L);Hex(T,A);Str(T,R);Hex(T,B);Str(T,"\n");
}
CAS_U32 CasTraceDescribe(const unsigned char *A,const unsigned char *B,
                        CAS_U64 CurrentBoot,CAS_U64 ExpectedTag,CAS_EARLY_TEXT *T) {
  CAS_U32 I,Stable=1,H,R,Tag,E=0,Platform=0,Flags=0;CAS_U64 X=0;
  for(I=0;I<CAS_TRACE_RECOVERY_BYTES;++I)if(A[I]!=B[I])Stable=0;
  H=Stable && Q(A,0)==0x363048534143ULL && Q(A,8)==1 && Q(A,16)==256 && Q(A,24)>0 &&
    Q(A,32)>0 && (CAS_U32)Q(A,232)==Crc(A,232);
  R=H && Q(A,248)==~Q(A,240) && Known(Q(A,240));
  Tag=H && Q(A,152)==ExpectedTag;
  if(R && Tag && Q(A,240)==0xe0 && Q(A,256)==0x315845534143ULL &&
     Q(A,376)==~Q(A,256) && Q(A,264)==Q(A,24) && Q(A,272)==Q(A,152) &&
     Q(A,280)<16 && Known(Q(A,360)) && Q(A,360)!=0xe0) {
    for(I=264;I<368;I+=8)X^=Q(A,I);
    E=X==Q(A,368);
  }
  if(R && Tag && Q(A,384)==0x31504c534143ULL && Q(A,504)==~Q(A,384) &&
     Q(A,392)==Q(A,24) && Q(A,400)==Q(A,152) && !(Q(A,408)&~0x7fULL)) {
    X=0;for(I=392;I<496;I+=8)X^=Q(A,I);
    Platform=X==Q(A,496);
  }
  if(H)Flags|=1;if(R)Flags|=2;if(Tag)Flags|=4;if(E)Flags|=8;if(Platform)Flags|=16;
  CasEarlyTextReset(T);
  Str(T,"CAS READ ONLY XNU TRACE RECOVERY 002\n");
  Str(T,"NO UEFI NO KERNEL EXECUTION\n");
  Str(T,"CURRENT BOOT ");Hex(T,CurrentBoot);Str(T,"\n");
  Str(T,H?"HEADER CRC ABI STABLE OK ":"HEADER INVALID NO STAGE ACCEPTED ");
  Str(T,R?"RAW OK\n":"RAW INVALID\n");
  Str(T,"RECORD BOOT ");Hex(T,Q(A,24));Str(T,"\n");
  Str(T,"TAG ");Hex(T,Q(A,152));Str(T,Tag?" MATCH\n":" FOREIGN\n");
  Pair(T,"C ",Q(A,40)," RAW ",Q(A,240));
  Pair(T,"PA ",Q(A,56)," ARGS ",Q(A,64));
  Pair(T,"ENTRY ",Q(A,72)," SIZE ",Q(A,80));
  if(Platform && Q(A,240)!=0xe0) {
    Str(T,"PLATFORM PACKET OK MASK ");Hex(T,Q(A,408));Str(T,"\n");
    Pair(T,"DT BYTES ",(CAS_U32)Q(A,416)," CPU COUNT ",Q(A,416)>>32);
    Pair(T,"PE HZ ",Q(A,424)," CNTFRQ ",Q(A,432));
    Pair(T,"GICD ",Q(A,448)," GICR ",Q(A,456));
    Pair(T,"NVRAM ",Q(A,464)," DAIF ",Q(A,472));
    Pair(T,"MPIDR ",Q(A,440)," TICKS ",Q(A,480));
  } else {
    if(E && Platform) { Str(T,"EXCEPTION OK PLATFORM MASK ");Hex(T,Q(A,408));Str(T,"\n"); }
    else Str(T,E?"EXCEPTION ID XOR INVERSE STABLE OK\n":"EXCEPTION NOT ACCEPTED\n");
    Pair(T,"VECTOR ",Q(A,280)," PREV ",Q(A,360));
    Pair(T,"ESR ",Q(A,288)," FAR ",Q(A,296));
    Pair(T,"ELR ",Q(A,304)," SPSR ",Q(A,312));
    Pair(T,"SP ",Q(A,352)," TCR ",Q(A,328));
    Pair(T,"TTBR0 ",Q(A,336)," TTBR1 ",Q(A,344));
  }
  Str(T,"HOLD POWER UNTIL RESTART TO ANDROID");
  return Flags;
}

#ifndef CAS_TRACE_RECOVERY_HOST_TEST
void CasTraceRecoverEarly(void) {
  unsigned char A[CAS_TRACE_RECOVERY_BYTES],B[CAS_TRACE_RECOVERY_BYTES];CAS_U32 I;
  const volatile unsigned char *P=(void *)(CAS_U64)(CAS_LOG_BASE+CAS_HANDOFF_LOG_OFFSET);
  const volatile CAS_BOOT_INFO *Info=(void *)(CAS_U64)CAS_LOG_BASE;
  CAS_EARLY_TEXT Text;CAS_DISPLAY_STATE *S=CasDisplayState();
  // This hook follows CasValidateBoot and the already accepted display init.
  // Do not initialize/repair/clear any byte in the retained mailbox itself.
  for(I=0;I<CAS_TRACE_RECOVERY_BYTES;++I)A[I]=P[I];
  __asm__ volatile("dsb sy" ::: "memory");
  for(I=0;I<CAS_TRACE_RECOVERY_BYTES;++I)B[I]=P[I];
  __asm__ volatile("dsb sy" ::: "memory");
  (void)CasTraceDescribe(A,B,Info->Reserved[0],CAS_TRACE_RECOVERY_TAG,&Text);
  if(S->Magic==CAS_DISPLAY_MAGIC && S->Ready && !S->Stopped) {
    (void)CasEarlyTextRender(&Text,&S->Framebuffer);CasDisplayRefresh();
  }
  // Terminal by design: the caller also branches to a terminal WFE loop.
}

#endif
