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
         (V>=0x40 && V<=0x47) || ((V>=0x50 && V<=0x56) || V==0x5f || (V>=0x60 && V<=0x66)) || (V>=0x70 && V<=0x7f) || V==0x90 || V==0xe0;
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

static CAS_U32 Packet(const unsigned char *A, CAS_U32 R, CAS_U32 Tag,
                      CAS_U32 At, CAS_U32 Words, CAS_U64 Magic) {
  CAS_U32 I; CAS_U64 X=0;
  if(!R || !Tag || Q(A,At)!=Magic || Q(A,At+(Words-1)*8)!=~Magic ||
     Q(A,At+8)!=Q(A,24) || Q(A,At+16)!=Q(A,152))return 0;
  for(I=1;I<Words-2;++I)X^=Q(A,At+I*8);
  return X==Q(A,At+(Words-2)*8);
}
static CAS_U64 Irq(const unsigned char *A,CAS_U32 Index) { return Q(A,552+Index*8); }
static void RuntimeLines(CAS_EARLY_TEXT *T,const unsigned char *A,CAS_U32 Run,CAS_U32 Interrupt) {
  Str(T,Run?"STARTUP OK ":"STARTUP INVALID ");
  Str(T,Interrupt?"IRQ PACKET OK\n":"IRQ NOT ACCEPTED\n");
  if(Run)Pair(T,"STEP ",Q(A,864)," SEEN ",Q(A,872));
  if(Interrupt) {
    Pair(T,"REGISTERED ",Irq(A,2)," GIC ",Irq(A,3));
    Pair(T,"IRQ ",Irq(A,4)," RETURN ",Irq(A,26));
    Pair(T,"EOI ",Irq(A,5)," HZ ",Irq(A,8));
    Pair(T,"FIRST ",Irq(A,27)," LAST ",Irq(A,28));
    Pair(T,"CVAL ",Irq(A,10)," CTL ",Irq(A,11));
    Pair(T,"INTID ",Irq(A,6)," MPIDR ",Irq(A,15));
  } else if(Run) {
    Pair(T,"DETAIL ",Q(A,880)," VBAR ",Q(A,888));
    Pair(T,"DAIF ",Q(A,896)," TICKS ",Q(A,904));
  }
}
static void PanicFormat(CAS_EARLY_TEXT *T,const unsigned char *A) {
  CAS_U32 I,At=0,Length=(CAS_U32)Q(A,1232);
  while(At<Length) {
    Str(T,"FMT ");
    for(I=0;I<48 && At<Length;++I,++At) {
      unsigned char C=A[1120+At];
      if(C<32 || C>126)C='.';
      CasEarlyTextFeed(T,&C,1);
    }
    Str(T,"\n");
  }
}

CAS_U32 CasTraceDescribe(const unsigned char *A,const unsigned char *B,
                        CAS_U64 CurrentBoot,CAS_U64 ExpectedTag,CAS_EARLY_TEXT *T) {
  CAS_U32 I,Stable=1,H,R,Tag,E=0,Platform=0,Interrupt=0,Run=0,Panic=0,Flags=0;CAS_U64 X=0;
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
  Interrupt=Packet(A,R,Tag,512,40,0x31515249534143ULL) && Q(A,544)==32 && Irq(A,0)==1;
  Run=Packet(A,R,Tag,832,16,0x314e5552534143ULL) && Q(A,856)>0 && Q(A,864)==Q(A,240) &&
      ((Q(A,864)>=0x70 && Q(A,864)<=0x7d) || Q(A,864)==0x90) && !(Q(A,872)&~0x80003fffULL);
  Panic=Packet(A,R,Tag,1024,32,0x31434e50534143ULL) && Q(A,240)==0x90 &&
        Q(A,1048)>0 && Known(Q(A,1104)) && Q(A,1104)!=0x90 && Q(A,1232)<=111 && A[1120+(CAS_U32)Q(A,1232)]==0;
  if(H)Flags|=1;if(R)Flags|=2;if(Tag)Flags|=4;if(E)Flags|=8;if(Platform)Flags|=16;
  if(Interrupt)Flags|=32;if(Run)Flags|=64;if(Panic)Flags|=128;
  CasEarlyTextReset(T);
  Str(T,"CAS READ ONLY XNU TRACE RECOVERY 003\n");
  Pair(T,"CURRENT ",CurrentBoot," RECORD ",Q(A,24));
  Str(T,H?"HEADER CRC ABI STABLE OK ":"HEADER INVALID NO STAGE ACCEPTED ");
  Str(T,R?"RAW OK\n":"RAW INVALID\n");
  Str(T,"TAG ");Hex(T,Q(A,152));Str(T,Tag?" MATCH\n":" FOREIGN\n");
  Pair(T,"C ",Q(A,40)," RAW ",Q(A,240));
  Pair(T,"PA ",Q(A,56)," ARGS ",Q(A,64));
  Pair(T,"ENTRY ",Q(A,72)," SIZE ",Q(A,80));
  if(Panic) {
    Pair(T,"PANIC OK PREV ",Q(A,1104)," WHY ",Q(A,1056));
    Pair(T,"CALLER ",Q(A,1048)," ELR ",Q(A,1080));
    Pair(T,"ESR ",Q(A,1064)," FAR ",Q(A,1072));
    PanicFormat(T,A);
    RuntimeLines(T,A,Run,Interrupt);
    Pair(T,"SPSR ",Q(A,1088)," DAIF ",Q(A,1096));
    if(Run)Pair(T,"VBAR ",Q(A,888)," DETAIL ",Q(A,880));
  } else if((Run || Interrupt) && Q(A,240)!=0xe0) {
    RuntimeLines(T,A,Run,Interrupt);
    if(Platform) {
      Pair(T,"PE HZ ",Q(A,424)," CNTFRQ ",Q(A,432));
      Pair(T,"DT BYTES ",(CAS_U32)Q(A,416)," CPU COUNT ",Q(A,416)>>32);
      Pair(T,"GICD ",Q(A,448)," GICR ",Q(A,456));
    }
  } else if(Platform && Q(A,240)!=0xe0) {
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
  while(T->Row<CAS_EARLY_ROWS-1)Str(T,"\n");
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
