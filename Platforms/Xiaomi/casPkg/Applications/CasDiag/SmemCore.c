// SPDX-License-Identifier: BSD-2-Clause-Patent
// Independently written bounded decoder. Wire-format sources and version limits
// are identified in docs/cas-diagnostics.md. No upstream driver code is included.
#include "SmemCore.h"
static CD_U32 L16(const CD_U8 *P) {return (CD_U32)P[0]|((CD_U32)P[1]<<8);}
static CD_U32 L32(const CD_U8 *P) {return L16(P)|(L16(P+2)<<16);}
static CD_U64 L64(const CD_U8 *P) {return L32(P)|((CD_U64)L32(P+4)<<32);}
static void S32(CD_U8 *P,CD_U32 V) {CD_U32 I;for(I=0;I<4;++I)P[I]=(CD_U8)(V>>(I*8));}
static int Zero(const CD_U8 *P,CD_U32 N) {CD_U32 I;for(I=0;I<N;++I)if(P[I])return 0;return 1;}
#define REFUSE(Bad,Code,Id) do {if(Bad) {W->Check=(Id);return (Code);}} while(0)
static int Read(CD_SMEM_WORK *W,CD_U32 Offset,CD_U32 Length,CD_U8 *Out) {
  CD_U32 I,At=W->Compare ? W->Cursor : W->Used;
  W->ReadOffset=Offset;W->ReadLength=Length;W->SampleBytes=0;
  for(I=0;I<sizeof(W->Sample);++I)W->Sample[I]=0;
  REFUSE(!Length || Length>4096 || Offset>CD_SMEM_BYTES || Length>CD_SMEM_BYTES-Offset,CdSmemBounds,1);
  REFUSE(++W->Reads>1200 || At>CD_SMEM_TRACE-8 || Length>CD_SMEM_TRACE-8-At,CdSmemBudget,2);
  REFUSE(W->Compare && (At+8+Length>W->Used || L32(W->Trace+At)!=Offset || L32(W->Trace+At+4)!=Length),CdSmemChanging,3);
  REFUSE(W->Read(W->Context,Offset,Length,Out),CdSmemRead,4);
  // Out==Table identifies the RAM item payload. Metadata reads use local
  // arrays of at most 48 bytes; sample those only, never the item payload.
  if(Out!=W->Table && Length<=sizeof(W->Sample)) {
    for(I=0;I<Length;++I)W->Sample[I]=Out[I];
    W->SampleBytes=Length;
  }
  if(W->Compare) {
    for(I=0;I<Length;++I)REFUSE(W->Trace[At+8+I]!=Out[I],CdSmemChanging,5);
    W->Cursor=At+8+Length;
  } else {
    S32(W->Trace+At,Offset);S32(W->Trace+At+4,Length);
    for(I=0;I<Length;++I)W->Trace[At+8+I]=Out[I];
    W->Used=At+8+Length;
  }
  return CdSmemValid;
}
#define GET(O,N,B) do {int Status=Read(W,(O),(N),(B));if(Status)return Status;} while(0)
static int Locate(CD_SMEM_WORK *W,CD_SMEM_INFO *Info) {
  CD_U8 B[32];CD_U32 Version,Off,Size,Free,I,Count,Part=0,PartSize=0,Found=0,Limit,Pos,Next;
  GET(92,4,B);Version=L32(B)>>16;Info->SmemVersion=Version;
  REFUSE(Version!=11 && Version!=12,CdSmemVersion,10);
  GET(192,16,B);
  REFUSE(L32(B)!=1 || L32(B+12)!=0,CdSmemFormat,11);
  if(Version==11) {
    Free=L32(B+4);
    REFUSE(Free<8400 || Free>CD_SMEM_BYTES-4096 || (Free&7),CdSmemBounds,12);
    GET(208+402*16,16,B);
    REFUSE(!L32(B),CdSmemMissing,13);
    REFUSE(L32(B)!=1 || L32(B+12)!=0,CdSmemFormat,14);
    Off=L32(B+4);Size=L32(B+8);
    REFUSE(Off<8400 || (Off&7) || !Size || (Size&7) || Off>Free || Size>Free-Off,CdSmemBounds,15);
  } else {
    // v12 global partition directory is confined to the final 4 KiB.
    GET(CD_SMEM_BYTES-4096,32,B);
    REFUSE(L32(B)!=0x434f5424U || L32(B+4)!=1 || !Zero(B+12,20),CdSmemFormat,20);
    Count=L32(B+8);
    REFUSE(!Count || Count>(4096-32-20)/48,CdSmemBounds,21);
    for(I=0;I<Count;++I) {
      CD_U8 E[48];GET(CD_SMEM_BYTES-4096+32+I*48,48,E);
      if(L16(E+12)!=0xfffe || L16(E+14)!=0xfffe)continue;
      // Linux's v12 directory contract leaves flags unused. They describe the
      // partition, not RAM-row access permissions; never derive ownership here.
      REFUSE(Found++ || !Zero(E+20,28),CdSmemFormat,22);
      Part=L32(E);PartSize=L32(E+4);Size=L32(E+16);
      W->PartitionOffset=Part;W->PartitionSize=PartSize;
      W->PartitionFlags=L32(E+8);W->PartitionCacheline=Size;
      // The legacy 8400-byte allocation table is v11-only. In this v12
      // profile reserve the first/last 4KiB and bound every partition read.
      if(Part<4096 || (Part&7) || Part>CD_SMEM_BYTES-4096 || PartSize<32 ||
          PartSize>CD_SMEM_BYTES-4096-Part || Size<16 || Size>4096 || (Size&(Size-1))) {W->Check=23;return CdSmemBounds;}
    }
    REFUSE(!Found,CdSmemMissing,24);
    GET(Part,32,B);
    if(L32(B)!=0x54525024U || L16(B+4)!=0xfffe || L16(B+6)!=0xfffe ||
        L32(B+8)!=PartSize || !Zero(B+20,12)) {W->Check=25;return CdSmemFormat;}
    Free=L32(B+12);Limit=L32(B+16);
    REFUSE(Free<32 || Free>Limit || Limit>PartSize || (Free&7) || (Limit&7),CdSmemBounds,26);
    Found=0;Off=Size=0;Pos=32;
    // Read allocation headers only; other items' payloads are never read.
    while(Pos<Free) {
      CD_U32 Bytes,Padding,HeaderPad,Item;
      REFUSE(Free-Pos<16,CdSmemBounds,27);
      GET(Part+Pos,16,B);
      REFUSE(L16(B)!=0xa5a5 || L32(B+12)!=0,CdSmemFormat,28);
      Item=L16(B+2);Bytes=L32(B+4);Padding=L16(B+8);HeaderPad=L16(B+10);
      if(!Bytes || (Bytes&7) || (HeaderPad&7) || Padding>=Bytes || HeaderPad>Free-Pos-16 ||
          Bytes>Free-Pos-16-HeaderPad) {W->Check=29;return CdSmemBounds;}
      Next=Pos+16+HeaderPad+Bytes;
      if(Item==402) {
        REFUSE(Found++,CdSmemFormat,30);
        Off=Part+Pos+16+HeaderPad;Size=Bytes-Padding;
      }
      Pos=Next;
    }
    // Cached items require a different coherency contract; never fall back to
    // an unchecked cached/auxiliary pointer or to a historical RAM bank.
    REFUSE(!Found,CdSmemMissing,31);
  }
  REFUSE(Size<24 || Size>4096,CdSmemBounds,32);
  Info->ItemOffset=Off;Info->ItemSize=Size;
  GET(Off,Size,W->Table);
  return CdSmemValid;
}
static int Parse(CD_SMEM_WORK *W,CD_SMEM_INFO *Info) {
  CD_U32 I,J,Version,Count,Stride;const CD_U8 *T=W->Table;
  REFUSE(L32(T)!=0x9da5e0a8U || L32(T+4)!=0xaf9ec4e2U || L32(T+12) || L32(T+20),CdSmemFormat,40);
  Version=L32(T+8);Info->TableVersion=Version;
  REFUSE(Version<1 || Version>3,CdSmemVersion,41);
  Stride=Version==1 ? 64 : 72;Count=L32(T+16);
  REFUSE(!Count || Count>CD_SMEM_PARTS || Count>(Info->ItemSize-24)/Stride,CdSmemBounds,42);
  Info->Count=Count;
  for(I=0;I<Count;++I) {
    const CD_U8 *P=T+24+I*Stride;CD_SMEM_PART *R=&Info->Parts[I];CD_RANGE Range;
    R->Base=L64(P+16);R->Size=L64(P+24);R->Available=Version==1 ? R->Size : L64(P+64);
    R->Attribute=L32(P+32);R->Category=L32(P+36);R->Domain=L32(P+40);R->Type=L32(P+44);
    W->ParsedRows=I+1;
    Range.Base=R->Base;Range.Size=R->Size;
    if(!CasDiagRangeValid(Range,4096) || R->Base+R->Size>(1ULL<<48) ||
        R->Available>R->Size || (R->Available&4095)) {W->Check=43;return CdSmemBounds;}
    // Duplicate/overlapping entries are ambiguous even when both say SDRAM.
    for(J=0;J<I;++J)if(R->Base<Info->Parts[J].Base+Info->Parts[J].Size &&
        Info->Parts[J].Base<R->Base+R->Size) {
      W->OverlapA=I;W->OverlapB=J;W->Check=44;return CdSmemFormat;
    }
  }
  return CdSmemValid;
}
int CasDiagSmemRead(CD_SMEM_READ Reader,void *Context,CD_SMEM_WORK *W,CD_SMEM_INFO *Info) {
  int Status;CD_U32 I,Pass;CD_U8 *Bytes=(CD_U8 *)Info;
  if(!Reader || !W || !Info)return CdSmemRead;
  for(I=0;I<sizeof(*Info);++I)Bytes[I]=0;
  W->Read=Reader;W->Context=Context;W->Used=W->Cursor=W->Reads=W->Compare=0;
  W->Check=W->ReadOffset=W->ReadLength=W->SampleBytes=0;
  W->PartitionOffset=W->PartitionSize=W->PartitionFlags=W->PartitionCacheline=0;
  for(I=0;I<sizeof(W->Sample);++I)W->Sample[I]=0;
  for(Pass=0;Pass<2;++Pass) {
    W->Compare=Pass;W->Cursor=W->Reads=0;
    W->ParsedRows=0;W->OverlapA=W->OverlapB=CD_SMEM_PARTS;
    Status=Locate(W,Info);if(Status)return Status;
    Status=Parse(W,Info);if(Status)return Status;
  }
  REFUSE(W->Cursor!=W->Used,CdSmemChanging,50);
  Info->Reads=W->Reads;Info->TraceBytes=W->Used;
  return CdSmemValid;
}
const char *CasDiagSmemCheckName(CD_U32 Check) {
  switch(Check) {
    case 0:return "none";
    case 1:return "read-bounds";case 2:return "read-budget";
    case 3:return "read-order";case 4:return "read-callback";case 5:return "read-changing";
    case 10:return "heap-version";case 11:return "primary-header";
    case 12:return "legacy-free";case 13:return "legacy-missing";
    case 14:return "legacy-header";case 15:return "legacy-bounds";
    case 20:return "directory-header";case 21:return "directory-count";
    case 22:return "directory-entry";case 23:return "directory-bounds";
    case 24:return "global-missing";case 25:return "partition-header";
    case 26:return "partition-bounds";case 27:return "item-tail";
    case 28:return "item-header";case 29:return "item-bounds";
    case 30:return "item-duplicate";case 31:return "item-missing";case 32:return "item-size";
    case 40:return "ram-header";case 41:return "ram-version";case 42:return "ram-count";
    case 43:return "ram-bounds";case 44:return "ram-overlap";case 50:return "trace-end";
    default:return "unknown";
  }
}
