// SPDX-License-Identifier: BSD-2-Clause-Patent
// Independently written bounded decoder. Wire-format sources and version limits
// are identified in docs/cas-diagnostics.md. No upstream driver code is included.
#include "SmemCore.h"
static CD_U32 L16(const CD_U8 *P) {return (CD_U32)P[0]|((CD_U32)P[1]<<8);}
static CD_U32 L32(const CD_U8 *P) {return L16(P)|(L16(P+2)<<16);}
static CD_U64 L64(const CD_U8 *P) {return L32(P)|((CD_U64)L32(P+4)<<32);}
static void S32(CD_U8 *P,CD_U32 V) {CD_U32 I;for(I=0;I<4;++I)P[I]=(CD_U8)(V>>(I*8));}
static int Zero(const CD_U8 *P,CD_U32 N) {CD_U32 I;for(I=0;I<N;++I)if(P[I])return 0;return 1;}
static int Read(CD_SMEM_WORK *W,CD_U32 Offset,CD_U32 Length,CD_U8 *Out) {
  CD_U32 I,At=W->Compare ? W->Cursor : W->Used;
  if(!Length || Length>4096 || Offset>CD_SMEM_BYTES || Length>CD_SMEM_BYTES-Offset)return CdSmemBounds;
  if(++W->Reads>1200 || At>CD_SMEM_TRACE-8 || Length>CD_SMEM_TRACE-8-At)return CdSmemBudget;
  if(W->Compare && (At+8+Length>W->Used || L32(W->Trace+At)!=Offset || L32(W->Trace+At+4)!=Length))return CdSmemChanging;
  if(W->Read(W->Context,Offset,Length,Out))return CdSmemRead;
  if(W->Compare) {
    for(I=0;I<Length;++I)if(W->Trace[At+8+I]!=Out[I])return CdSmemChanging;
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
  if(Version!=11 && Version!=12)return CdSmemVersion;
  GET(192,16,B);
  if(L32(B)!=1 || L32(B+12)!=0)return CdSmemFormat;
  if(Version==11) {
    Free=L32(B+4);
    if(Free<8400 || Free>CD_SMEM_BYTES-4096 || (Free&7))return CdSmemBounds;
    GET(208+402*16,16,B);
    if(!L32(B))return CdSmemMissing;
    if(L32(B)!=1 || L32(B+12)!=0)return CdSmemFormat;
    Off=L32(B+4);Size=L32(B+8);
    if(Off<8400 || (Off&7) || !Size || (Size&7) || Off>Free || Size>Free-Off)return CdSmemBounds;
  } else {
    // v12 global partition directory is confined to the final 4 KiB.
    GET(CD_SMEM_BYTES-4096,32,B);
    if(L32(B)!=0x434f5424U || L32(B+4)!=1 || !Zero(B+12,20))return CdSmemFormat;
    Count=L32(B+8);
    if(!Count || Count>(4096-32-20)/48)return CdSmemBounds;
    for(I=0;I<Count;++I) {
      CD_U8 E[48];GET(CD_SMEM_BYTES-4096+32+I*48,48,E);
      if(L16(E+12)!=0xfffe || L16(E+14)!=0xfffe)continue;
      if(Found++ || L32(E+8) || !Zero(E+20,28))return CdSmemFormat;
      Part=L32(E);PartSize=L32(E+4);Size=L32(E+16);
      if(Part<8400 || (Part&7) || Part>CD_SMEM_BYTES-4096 || PartSize<32 ||
          PartSize>CD_SMEM_BYTES-4096-Part || Size<16 || Size>4096 || (Size&(Size-1)))return CdSmemBounds;
    }
    if(!Found)return CdSmemMissing;
    GET(Part,32,B);
    if(L32(B)!=0x54525024U || L16(B+4)!=0xfffe || L16(B+6)!=0xfffe ||
        L32(B+8)!=PartSize || !Zero(B+20,12))return CdSmemFormat;
    Free=L32(B+12);Limit=L32(B+16);
    if(Free<32 || Free>Limit || Limit>PartSize || (Free&7) || (Limit&7))return CdSmemBounds;
    Found=0;Off=Size=0;Pos=32;
    // Read allocation headers only; other items' payloads are never read.
    while(Pos<Free) {
      CD_U32 Bytes,Padding,HeaderPad,Item;
      if(Free-Pos<16)return CdSmemBounds;
      GET(Part+Pos,16,B);
      if(L16(B)!=0xa5a5 || L32(B+12)!=0)return CdSmemFormat;
      Item=L16(B+2);Bytes=L32(B+4);Padding=L16(B+8);HeaderPad=L16(B+10);
      if(!Bytes || (Bytes&7) || (HeaderPad&7) || Padding>=Bytes || HeaderPad>Free-Pos-16 ||
          Bytes>Free-Pos-16-HeaderPad)return CdSmemBounds;
      Next=Pos+16+HeaderPad+Bytes;
      if(Item==402) {
        if(Found++)return CdSmemFormat;
        Off=Part+Pos+16+HeaderPad;Size=Bytes-Padding;
      }
      Pos=Next;
    }
    // Cached items require a different coherency contract; never fall back to
    // an unchecked cached/auxiliary pointer or to a historical RAM bank.
    if(!Found)return CdSmemMissing;
  }
  if(Size<24 || Size>4096)return CdSmemBounds;
  Info->ItemOffset=Off;Info->ItemSize=Size;
  GET(Off,Size,W->Table);
  return CdSmemValid;
}
static int Parse(CD_SMEM_WORK *W,CD_SMEM_INFO *Info) {
  CD_U32 I,J,Version,Count,Stride;const CD_U8 *T=W->Table;
  if(L32(T)!=0x9da5e0a8U || L32(T+4)!=0xaf9ec4e2U || L32(T+12) || L32(T+20))return CdSmemFormat;
  Version=L32(T+8);Info->TableVersion=Version;
  if(Version<1 || Version>3)return CdSmemVersion;
  Stride=Version==1 ? 64 : 72;Count=L32(T+16);
  if(!Count || Count>CD_SMEM_PARTS || Count>(Info->ItemSize-24)/Stride)return CdSmemBounds;
  Info->Count=Count;
  for(I=0;I<Count;++I) {
    const CD_U8 *P=T+24+I*Stride;CD_SMEM_PART *R=&Info->Parts[I];CD_RANGE Range;
    R->Base=L64(P+16);R->Size=L64(P+24);R->Available=Version==1 ? R->Size : L64(P+64);
    R->Attribute=L32(P+32);R->Category=L32(P+36);R->Domain=L32(P+40);R->Type=L32(P+44);
    Range.Base=R->Base;Range.Size=R->Size;
    if(!CasDiagRangeValid(Range,4096) || R->Base+R->Size>(1ULL<<48) ||
        R->Available>R->Size || (R->Available&4095))return CdSmemBounds;
    // Duplicate/overlapping entries are ambiguous even when both say SDRAM.
    for(J=0;J<I;++J)if(R->Base<Info->Parts[J].Base+Info->Parts[J].Size &&
        Info->Parts[J].Base<R->Base+R->Size)return CdSmemFormat;
  }
  return CdSmemValid;
}
int CasDiagSmemRead(CD_SMEM_READ Reader,void *Context,CD_SMEM_WORK *W,CD_SMEM_INFO *Info) {
  int Status;CD_U32 I,Pass;CD_U8 *Bytes=(CD_U8 *)Info;
  if(!Reader || !W || !Info)return CdSmemRead;
  for(I=0;I<sizeof(*Info);++I)Bytes[I]=0;
  W->Read=Reader;W->Context=Context;W->Used=W->Cursor=W->Reads=W->Compare=0;
  for(Pass=0;Pass<2;++Pass) {
    W->Compare=Pass;W->Cursor=W->Reads=0;
    Status=Locate(W,Info);if(Status)return Status;
    Status=Parse(W,Info);if(Status)return Status;
  }
  if(W->Cursor!=W->Used)return CdSmemChanging;
  Info->Reads=W->Reads;Info->TraceBytes=W->Used;
  return CdSmemValid;
}
