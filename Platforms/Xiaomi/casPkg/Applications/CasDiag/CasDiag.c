// SPDX-License-Identifier: BSD-2-Clause-Patent
// RAM diagnostics only. No block I/O writes, variables, GCD import or ExitBootServices.
#include <Uefi.h>
#include <Guid/Fdt.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/MemoryAttribute.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/FdtLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <CasPlatform.h>
#include "DiagCore.h"
#include "SmemCore.h"
#ifndef CAS_XNU_DIAGNOSTICS
#error CasDiag requires the independent diagnostic firmware profile
#endif
#define BUILD_ID "cas-diag-003"
STATIC CD_RANGE mBanks[CD_MAX_RANGES],mSmem[CD_MAX_RANGES],mFree[CD_MAX_RANGES],mReserved[CD_MAX_RANGES];
STATIC CD_U32 mBankCount,mSmemCount,mFreeCount,mReservedCount,mComplete,mFlags;
STATIC CD_U8 mPayload[CD_LOG_CAPACITY-CD_LOG_HEADER],mEncoded[CD_LOG_CAPACITY];
STATIC CD_U32 mUsed;
STATIC CD_U64 mSequence=1,mBootStamp;
STATIC CD_LOG_INFO mPrevious;
STATIC INTN mPreviousStatus;
STATIC CAS_BOOT_INFO *mBoot;
STATIC BOOLEAN mDropped;
STATIC CD_SMEM_WORK mSmemWork;
STATIC CD_SMEM_INFO mSmemInfo;
STATIC BOOLEAN mSmemMapped;

STATIC VOID Line(CONST CHAR8 *Format, ...) {
  CHAR8 Text[256];VA_LIST Args;UINTN N;
  VA_START(Args,Format);AsciiVSPrint(Text,sizeof(Text),Format,Args);VA_END(Args);
  N=AsciiStrLen(Text);Print(L"%a",Text);
  if(N<=sizeof(mPayload)-mUsed) {CopyMem(mPayload+mUsed,Text,N);mUsed+=(CD_U32)N;}
  else if(!mDropped) {Print(L"LOG CAPACITY REACHED; no ownership pass\r\n");mDropped=TRUE;mComplete=0;}
}
STATIC VOID Persist(VOID) {
  CD_U32 N,I;volatile UINT8 *P=(VOID *)(UINTN)(CAS_LOG_BASE+CAS_DIAG_LOG_OFFSET);
  N=CasDiagLogEncode(mEncoded,sizeof(mEncoded),mPayload,mUsed,mSequence,mBootStamp,mFlags,BUILD_ID);
  if(!N) {Print(L"Log encode refused\r\n");return;}
  // Existing firmware-owned NC/XN slot. Invalidate magic first, commit it last.
  for(I=0;I<8;++I)P[I]=0;
  MemoryFence();
  for(I=8;I<N;++I)P[I]=mEncoded[I];
  MemoryFence();
  for(I=0;I<8;++I)P[I]=mEncoded[I];
  MemoryFence();
}
STATIC VOID LoadPrevious(VOID) {
  UINTN I;volatile UINT8 *P=(VOID *)(UINTN)(CAS_LOG_BASE+CAS_DIAG_LOG_OFFSET);
  for(I=0;I<sizeof(mEncoded);++I)mEncoded[I]=P[I];
  mPreviousStatus=CasDiagLogValidate(mEncoded,sizeof(mEncoded),BUILD_ID,&mPrevious);
  if(mPreviousStatus==CdLogValid) {
    CopyMem(mPayload,mEncoded+CD_LOG_HEADER,mPrevious.Length);mUsed=mPrevious.Length;
    if(mPrevious.Sequence<MAX_UINT64)mSequence=mPrevious.Sequence+1;
  }
}
STATIC BOOLEAN Add(CD_RANGE *R,CD_U32 *Count,UINT64 Base,UINT64 Size) {
  CD_RANGE Value={Base,Size};
  if(*Count>=CD_MAX_RANGES || !CasDiagRangeValid(Value,1))return FALSE;
  R[(*Count)++]=Value;return TRUE;
}
STATIC UINT64 Be64(CONST UINT8 *P) {
  UINT64 V=0;UINTN I;for(I=0;I<8;++I)V=(V<<8)|P[I];return V;
}
STATIC BOOLEAN Reg(CONST VOID *Fdt,INT32 Node,CD_RANGE *R,CD_U32 *Count,CONST CHAR8 *Kind) {
  INT32 N,I;CONST UINT8 *P=FdtGetProp(Fdt,Node,"reg",&N);
  if(P==NULL || N<=0 || N%16) {Line("FDT %a: unresolved/invalid reg node=%d\n",Kind,Node);return FALSE;}
  for(I=0;I<N;I+=16) {
    UINT64 Base=Be64(P+I),Size=Be64(P+I+8);
    Line("FDT %a base=%016Lx size=%016Lx node=%d\n",Kind,Base,Size,Node);
    if(!Add(R,Count,Base,Size))return FALSE;
  }
  return TRUE;
}
STATIC BOOLEAN FdtFacts(VOID) {
  VOID *Fdt=NULL;INT32 Node,Reserved,N;INTN I,Count;CONST CHAR8 *Type;CONST VOID *Ranges;
  BOOLEAN Good=TRUE;UINT32 Crc;
  if(EFI_ERROR(EfiGetSystemConfigurationTable(&gFdtTableGuid,&Fdt)) ||
      (UINTN)Fdt!=CAS_FDT_BASE || FdtCheckHeader(Fdt) || FdtTotalSize(Fdt)!=mBoot->FdtSize ||
      FdtAddressCells(Fdt,0)!=2 || FdtSizeCells(Fdt,0)!=2) {
    Line("FDT unavailable or outside validated cas handoff\n");return FALSE;
  }
  if(EFI_ERROR(gBS->CalculateCrc32(Fdt,(UINTN)mBoot->FdtSize,&Crc)))return FALSE;
  Line("FDT current-copy bytes=%Lu crc32=%08x\n",mBoot->FdtSize,Crc);
  for(Node=FdtFirstSubnode(Fdt,0);Node>=0;Node=FdtNextSubnode(Fdt,Node)) {
    Type=FdtGetProp(Fdt,Node,"device_type",&N);
    if(Type && N==7 && CompareMem(Type,"memory",7)==0)Good=Reg(Fdt,Node,mBanks,&mBankCount,"RAM")&&Good;
  }
  if(Node!=-1)Good=FALSE;
  Count=FdtGetNumberOfReserveMapEntries(Fdt);
  if(Count<0 || Count>CD_MAX_RANGES)Good=FALSE;
  else for(I=0;I<Count;++I) {
    UINT64 Base,Size;
    if(FdtGetReserveMapEntry(Fdt,I,&Base,&Size) || !Add(mReserved,&mReservedCount,Base,Size))Good=FALSE;
    else Line("FDT reserve-map base=%016Lx size=%016Lx\n",Base,Size);
  }
  Reserved=FdtPathOffset(Fdt,"/reserved-memory");
  if(Reserved<0 || FdtAddressCells(Fdt,Reserved)!=2 || FdtSizeCells(Fdt,Reserved)!=2)return FALSE;
  Ranges=FdtGetProp(Fdt,Reserved,"ranges",&N);
  if(!Ranges || N!=0)Good=FALSE;
  for(Node=FdtFirstSubnode(Fdt,Reserved);Node>=0;Node=FdtNextSubnode(Fdt,Node))
    Good=Reg(Fdt,Node,mReserved,&mReservedCount,"reserved")&&Good;
  if(Node!=-1)Good=FALSE;
  return Good && mBankCount>0;
}
STATIC int SmemRead(void *Context,CD_U32 Offset,CD_U32 Length,CD_U8 *Output) {
  volatile CONST UINT8 *P=(VOID *)(UINTN)(CAS_SMEM_BASE+Offset);CD_U32 I;(VOID)Context;
  if(!mSmemMapped || Offset>CAS_SMEM_SIZE || Length>CAS_SMEM_SIZE-Offset)return 1;
  MemoryFence();for(I=0;I<Length;++I)Output[I]=P[I];MemoryFence();return 0;
}
STATIC BOOLEAN RamFacts(VOID) {
  VOID *Fdt=NULL;INT32 Node,Region,N,Reserved;CONST UINT8 *P;UINT32 Phandle,I;int Status;
  EFI_MEMORY_ATTRIBUTE_PROTOCOL *Attributes=NULL;UINT64 Permissions;
  // The exact current FDT provider must resolve to the mapped reserved region.
  if(!mSmemMapped || EFI_ERROR(EfiGetSystemConfigurationTable(&gFdtTableGuid,&Fdt)) ||
      (UINTN)Fdt!=CAS_FDT_BASE || FdtCheckHeader(Fdt) || FdtTotalSize(Fdt)!=mBoot->FdtSize)return FALSE;
  Node=FdtNodeOffsetByCompatible(Fdt,-1,"qcom,smem");
  if(Node<0 || FdtNodeOffsetByCompatible(Fdt,Node,"qcom,smem")!=-1)return FALSE;
  P=FdtGetProp(Fdt,Node,"memory-region",&N);if(!P || N!=4)return FALSE;
  Phandle=(UINT32)P[0]<<24|(UINT32)P[1]<<16|(UINT32)P[2]<<8|P[3];
  Region=FdtNodeOffsetByPhandle(Fdt,Phandle);Reserved=FdtPathOffset(Fdt,"/reserved-memory");
  if(Region<0 || Reserved<0 || FdtParentOffset(Fdt,Region)!=Reserved ||
      FdtAddressCells(Fdt,Reserved)!=2 || FdtSizeCells(Fdt,Reserved)!=2)return FALSE;
  P=FdtGetProp(Fdt,Reserved,"ranges",&N);if(!P || N!=0)return FALSE;
  P=FdtGetProp(Fdt,Region,"reg",&N);
  if(!P || N!=16 || Be64(P)!=CAS_SMEM_BASE || Be64(P+8)!=CAS_SMEM_SIZE)return FALSE;
  P=FdtGetProp(Fdt,Region,"no-map",&N);if(!P || N!=0)return FALSE;
  if(EFI_ERROR(gBS->LocateProtocol(&gEfiMemoryAttributeProtocolGuid,NULL,(VOID **)&Attributes)) ||
      !Attributes || !Attributes->GetMemoryAttributes ||
      EFI_ERROR(Attributes->GetMemoryAttributes(Attributes,CAS_SMEM_BASE,CAS_SMEM_SIZE,&Permissions)) ||
      (Permissions&(EFI_MEMORY_RO|EFI_MEMORY_XP|EFI_MEMORY_RP))!=(EFI_MEMORY_RO|EFI_MEMORY_XP))return FALSE;
  Status=CasDiagSmemRead(SmemRead,NULL,&mSmemWork,&mSmemInfo);
  Line("SMEM readonly base=%016Lx bytes=%Lx status=%d heap-version=%u table-version=%u item-offset=%x item-size=%u\n",
       (UINT64)CAS_SMEM_BASE,(UINT64)CAS_SMEM_SIZE,Status,mSmemInfo.SmemVersion,mSmemInfo.TableVersion,mSmemInfo.ItemOffset,mSmemInfo.ItemSize);
  if(Status!=CdSmemValid)return FALSE;
  Line("SMEM equal bounded observations reads=%u trace-bytes=%u (no atomic-snapshot claim)\n",mSmemInfo.Reads,mSmemInfo.TraceBytes);
  for(I=0;I<mSmemInfo.Count;++I) {
    CD_SMEM_PART *R=&mSmemInfo.Parts[I];
    Line("SMEM part=%u base=%016Lx size=%016Lx available=%016Lx attr=%x category=%u domain=%u type=%u\n",
         I,R->Base,R->Size,R->Available,R->Attribute,R->Category,R->Domain,R->Type);
    if(R->Category==14 && R->Type==1 && R->Attribute==1 && (R->Domain==1 || R->Domain==3) && R->Available) {
      if(!Add(mSmem,&mSmemCount,R->Base,R->Available))return FALSE;
      if(R->Available<R->Size && !Add(mReserved,&mReservedCount,R->Base+R->Available,R->Size-R->Available))return FALSE;
    } else if(!Add(mReserved,&mReservedCount,R->Base,R->Size))return FALSE;
  }
  return mSmemCount>0;
}
STATIC BOOLEAN GopFacts(VOID) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop=NULL;EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;UINT64 Need;
  if(EFI_ERROR(gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,NULL,(VOID **)&Gop)) ||
      !Gop || !Gop->Mode || !Gop->Mode->Info || Gop->Mode->SizeOfInfo<sizeof(*Info))return FALSE;
  Info=Gop->Mode->Info;
  if(!Info->HorizontalResolution || !Info->VerticalResolution || Info->PixelsPerScanLine<Info->HorizontalResolution ||
      Info->PixelsPerScanLine>16384 || Info->VerticalResolution>16384 || Info->PixelFormat>=PixelBltOnly)return FALSE;
  Need=(UINT64)Info->PixelsPerScanLine*Info->VerticalResolution*4;
  Line("GOP base=%016Lx size=%016Lx width=%u height=%u stride-pixels=%u format=%u\n",
    Gop->Mode->FrameBufferBase,(UINT64)Gop->Mode->FrameBufferSize,Info->HorizontalResolution,
    Info->VerticalResolution,Info->PixelsPerScanLine,Info->PixelFormat);
  if(Gop->Mode->FrameBufferSize<Need || !Gop->Mode->FrameBufferBase)return FALSE;
  return Add(mReserved,&mReservedCount,Gop->Mode->FrameBufferBase,Gop->Mode->FrameBufferSize);
}
STATIC BOOLEAN UefiFacts(BOOLEAN Verbose) {
  EFI_MEMORY_DESCRIPTOR *Map=NULL,*D;UINTN Size=0,Key,Stride=sizeof(*D),Capacity,I,Attempt;UINT32 Version;
  EFI_STATUS Status;BOOLEAN Good=TRUE;
  mFreeCount=0;mSmemMapped=FALSE;
  Status=gBS->GetMemoryMap(&Size,NULL,&Key,&Stride,&Version);
  if(Status!=EFI_BUFFER_TOO_SMALL || Stride<sizeof(*D) || Stride>4096 || Size>0x100000)return FALSE;
  for(Attempt=0;Attempt<4;++Attempt) {
    Capacity=Size+16*Stride;Map=AllocatePool(Capacity);if(!Map)return FALSE;Size=Capacity;
    Status=gBS->GetMemoryMap(&Size,Map,&Key,&Stride,&Version);
    if(Status!=EFI_BUFFER_TOO_SMALL)break;
    FreePool(Map);Map=NULL;
    if(Size>0x100000 || Stride<sizeof(*D) || Stride>4096)return FALSE;
  }
  if(EFI_ERROR(Status) || Version!=EFI_MEMORY_DESCRIPTOR_VERSION || !Map || Stride<sizeof(*D) || Stride>4096 || !Size || Size>Capacity || Size%Stride || Size/Stride>1024) {
    if(Map)FreePool(Map);return FALSE;
  }
  if(Verbose)Line("UEFI map version=%u descriptor=%Lu bytes=%Lu key=%Lu\n",Version,(UINT64)Stride,(UINT64)Size,(UINT64)Key);
  for(I=0;I<Size/Stride;++I) {
    UINT64 Bytes;CD_RANGE R;
    D=(VOID *)((UINT8 *)Map+I*Stride);
    if(D->NumberOfPages>MAX_UINT64/4096) {Good=FALSE;continue;}
    Bytes=D->NumberOfPages*4096;R.Base=D->PhysicalStart;R.Size=Bytes;
    if(!CasDiagRangeValid(R,4096))Good=FALSE;
    if(Verbose)Line("UEFI type=%u phys=%016Lx virt=%016Lx bytes=%016Lx attr=%016Lx\n",D->Type,D->PhysicalStart,D->VirtualStart,Bytes,D->Attribute);
    if(D->Type==EfiReservedMemoryType && (D->Attribute&EFI_MEMORY_UC) && !(D->Attribute&EFI_MEMORY_RP) &&
        CasDiagRangeValid(R,4096) && R.Base<=CAS_SMEM_BASE && R.Base+R.Size>=CAS_SMEM_BASE+CAS_SMEM_SIZE)mSmemMapped=TRUE;
    if(D->Type==EfiConventionalMemory && (D->Attribute&EFI_MEMORY_WB) &&
        !(D->Attribute&(EFI_MEMORY_RUNTIME|EFI_MEMORY_RP|EFI_MEMORY_RO)))
      Good=Add(mFree,&mFreeCount,D->PhysicalStart,Bytes)&&Good;
  }
  FreePool(Map);return Good;
}
STATIC VOID Enumerate(VOID) {
  mBankCount=mSmemCount=mFreeCount=mReservedCount=mComplete=0;
  Line("\nENUM build=%a boot=%016Lx entry-EL=%Lu SCTLR=%016Lx\n",BUILD_ID,mBootStamp,mBoot->EntryEl,mBoot->EntrySctlr);
  // This low envelope covers the firmware, input corridors, stack and logs.
  Add(mReserved,&mReservedCount,0,0x100000000ULL);
  if(FdtFacts())mComplete|=1;else Line("FDT incomplete: probe refused\n");
  if(GopFacts())mComplete|=8;else Line("GOP incomplete: probe refused\n");
  if(UefiFacts(TRUE))mComplete|=4;else Line("UEFI map incomplete: probe refused\n");
  if(RamFacts())mComplete|=2;else Line("SMEM source incomplete: probe refused\n");
  if(mDropped)mComplete=0;
  Line("ENUM complete-mask=%u required=%u banks=%u SMEM=%u conventional=%u exclusions=%u\n",mComplete,CD_FACTS_COMPLETE,mBankCount,mSmemCount,mFreeCount,mReservedCount);
  Persist();
}
STATIC int ProbeAllocate(void *Context,CD_RANGE R,CD_U64 *Actual) {
  EFI_PHYSICAL_ADDRESS Address=R.Base;EFI_STATUS S;
  (VOID)Context;S=gBS->AllocatePages(AllocateAddress,EfiLoaderData,(UINTN)(R.Size/4096),&Address);
  Line("HIGH AllocateAddress base=%016Lx size=%016Lx status=%r\n",Address,R.Size,S);*Actual=Address;return EFI_ERROR(S);
}
STATIC int ProbeWrite(void *Context,CD_RANGE R,CD_U32 Pass) {
  volatile CD_U64 *Words=(VOID *)(UINTN)R.Base;CD_U64 I;(VOID)Context;
  for(I=0;I<R.Size/8;++I)Words[I]=CasDiagPattern(R.Base+I*8,Pass);return 0;
}
STATIC int ProbeCache(void *Context,CD_RANGE R) {
  (VOID)Context;WriteBackInvalidateDataCacheRange((VOID *)(UINTN)R.Base,(UINTN)R.Size);MemoryFence();return 0;
}
STATIC int ProbeRead(void *Context,CD_RANGE R,CD_U32 Pass) {
  volatile CD_U64 *Words=(VOID *)(UINTN)R.Base;CD_U64 I;(VOID)Context;
  for(I=0;I<R.Size/8;++I)if(Words[I]!=CasDiagPattern(R.Base+I*8,Pass)) {Line("HIGH mismatch pass=%u offset=%016Lx\n",Pass,I*8);return 1;}
  Line("HIGH pass=%u readback=OK\n",Pass);return 0;
}
STATIC int ProbeClear(void *Context,CD_RANGE R) {
  volatile CD_U64 *Words=(VOID *)(UINTN)R.Base;CD_U64 I;(VOID)Context;
  for(I=0;I<R.Size/8;++I)Words[I]=0;return 0;
}
STATIC int ProbeFree(void *Context,CD_RANGE R) {
  EFI_STATUS S;(VOID)Context;S=gBS->FreePages(R.Base,(UINTN)(R.Size/4096));Line("HIGH release status=%r\n",S);return EFI_ERROR(S);
}
STATIC VOID Probe(VOID) {
  CD_RANGE C={0x100000000ULL,CD_REQUEST};INTN Gate;CD_PROBE_RESULT Result;
  STATIC CONST CD_PROBE_IO Io={ProbeAllocate,ProbeWrite,ProbeCache,ProbeRead,ProbeClear,ProbeFree};
  Enumerate();if(!UefiFacts(FALSE))mComplete&=~4U;
  Gate=CasDiagEligible(C,mBanks,mBankCount,mSmem,mSmemCount,mFree,mFreeCount,mReserved,mReservedCount,mComplete);
  Result=CasDiagRunProbe(C,(int)Gate,&Io,NULL);
  if(Result.Result==CdProbePass) {mFlags|=CD_PROBE_PASSED;Line("HIGH ALLOCATION/READBACK/RELEASE PASS (this run only)\n");}
  else {mFlags|=CD_PROBE_REFUSED;Line("HIGH REFUSED/FAILED gate=%d mask=%u stage=%d cleanup=%d release=%d\n",Gate,mComplete,Result.Result,Result.CleanupFailed,Result.ReleaseFailed);}
  Persist();
}
STATIC EFI_INPUT_KEY Key(VOID) {
  EFI_INPUT_KEY K;UINTN Index;ZeroMem(&K,sizeof(K));
  for(;;)if(!EFI_ERROR(gBS->WaitForEvent(1,&gST->ConIn->WaitForKey,&Index)) && !EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn,&K)))return K;
}
STATIC VOID View(VOID) {
  UINTN Offset=0,Start,Lines;CHAR8 Part[240];EFI_INPUT_KEY K;
  do {
    gST->ConOut->ClearScreen(gST->ConOut);Print(L"CAS DIAG recovered/current log\r\n");
    Print(L"Previous status=%d build=%a seq=%Lu flags=%u boot=%Lx\r\n",mPreviousStatus,mPrevious.Build,mPrevious.Sequence,mPrevious.Flags,mPrevious.BootStamp);
    Print(L"Current boot=%Lx; %s\r\n",mBootStamp,mPreviousStatus==CdLogValid ? (mPrevious.BootStamp==mBootStamp ? L"same boot" : L"prior-boot candidate; persistence unverified") : L"no matching valid previous record");
    for(Lines=0;Lines<20 && Offset<mUsed;++Lines) {
      Start=Offset;while(Offset<mUsed && mPayload[Offset]!='\n' && Offset-Start<sizeof(Part)-2)++Offset;
      if(Offset<mUsed && mPayload[Offset]=='\n')++Offset;
      CopyMem(Part,mPayload+Start,Offset-Start);Part[Offset-Start]=0;Print(L"%a",Part);
    }
    Print(L"\r\nPower: next/end    Hold Power: back\r\n");K=Key();
  }while(K.ScanCode!=SCAN_ESC && Offset<mUsed);
}
EFI_STATUS EFIAPI CasDiagEntry(EFI_HANDLE ImageHandle,EFI_SYSTEM_TABLE *SystemTable) {
  EFI_INPUT_KEY K;UINTN Selected=0;CONST CHAR16 *Items[]={L"Enumerate current facts",L"Probe owned high 256 MiB (guards first)",L"Read recovered/current log",L"Mark and request warm reset",L"Return to UniXNU"};
  (VOID)ImageHandle;(VOID)SystemTable;
  mBoot=(VOID *)(UINTN)CAS_LOG_BASE;
  if(!gST->ConOut || !gST->ConIn)return EFI_NOT_READY;
  // App is specific to the bounded cas firmware. Arbitrary UEFI launches refuse.
  // The profile places this fixed control region in the UEFI map; validate it before use.
  { EFI_MEMORY_DESCRIPTOR *Map=NULL;UINTN Size=0,KeyValue,Stride=sizeof(*Map),Capacity,I;UINT32 Version;BOOLEAN Found=FALSE;EFI_STATUS S;
    S=gBS->GetMemoryMap(&Size,NULL,&KeyValue,&Stride,&Version);
    if(S!=EFI_BUFFER_TOO_SMALL || Size>0x100000 || Stride<sizeof(*Map) || Stride>4096)return EFI_UNSUPPORTED;
    Capacity=Size+16*Stride;Map=AllocatePool(Capacity);if(!Map)return EFI_OUT_OF_RESOURCES;Size=Capacity;
    S=gBS->GetMemoryMap(&Size,Map,&KeyValue,&Stride,&Version);
    if(!EFI_ERROR(S) && Stride>=sizeof(*Map) && Stride<=4096 && Size<=Capacity && Size%Stride==0)
      for(I=0;I<Size/Stride;++I) {EFI_MEMORY_DESCRIPTOR *D=(VOID *)((UINT8 *)Map+I*Stride);
        if(D->Type==EfiReservedMemoryType && D->PhysicalStart==CAS_LOG_BASE && D->NumberOfPages==CAS_LOG_SIZE/4096 && !(D->Attribute&EFI_MEMORY_RP))Found=TRUE;}
    FreePool(Map);if(!Found)return EFI_UNSUPPORTED;
  }
  if(mBoot->Magic!=CAS_BOOT_MAGIC || mBoot->Version!=CAS_BOOT_VERSION || mBoot->Size!=sizeof(*mBoot) ||
      mBoot->FdtCopy!=CAS_FDT_BASE || mBoot->FdtSize<40 || mBoot->FdtSize>CAS_FDT_SIZE ||
      !mBoot->Reserved[0] || (mBoot->EntryEl!=1 && mBoot->EntryEl!=2))return EFI_UNSUPPORTED;
  mBootStamp=mBoot->Reserved[0];LoadPrevious();Line("\nAPP ENTRY build=%a boot=%016Lx previous-status=%d\n",BUILD_ID,mBootStamp,mPreviousStatus);
  gBS->SetWatchdogTimer(0,0,0,NULL);
  for(;;) {
    UINTN I;gST->ConOut->ClearScreen(gST->ConOut);Print(L"CAS DIAG cas-diag-003 (XNU not loaded)\r\n\r\n");
    for(I=0;I<ARRAY_SIZE(Items);++I)Print(L"%s %s\r\n",I==Selected ? L">" : L" ",Items[I]);
    Print(L"\r\nVolume: move   Power: select   Hold Power: return\r\n");K=Key();
    if(K.ScanCode==SCAN_ESC) {Line("APP RETURN\n");Persist();return EFI_SUCCESS;}
    if(K.ScanCode==SCAN_UP)Selected=(Selected+ARRAY_SIZE(Items)-1)%ARRAY_SIZE(Items);
    else if(K.ScanCode==SCAN_DOWN)Selected=(Selected+1)%ARRAY_SIZE(Items);
    else if(K.UnicodeChar==L'\r') {
      gST->ConOut->ClearScreen(gST->ConOut);
      if(Selected==0) {Enumerate();View();}
      else if(Selected==1) {Probe();View();}
      else if(Selected==2)View();
      else if(Selected==3) {
        Line("CONTROLLED WARM RESET REQUESTED; survival not assumed\n");mFlags|=CD_WARM_REQUESTED;Persist();
        gRT->ResetSystem(EfiResetWarm,EFI_SUCCESS,0,NULL);Line("ResetSystem returned unexpectedly\n");Persist();View();
      } else {Line("APP RETURN\n");Persist();return EFI_SUCCESS;}
    }
  }
}
