# SPDX-License-Identifier: BSD-2-Clause-Patent
# Explicit cas/SM8250 source-only configuration. No bulk Qcom/Silicium includes.
[Defines]
  PLATFORM_NAME = cas
  PLATFORM_GUID = 33cd870e-412b-4344-8606-386b4ce9abf1
  PLATFORM_VERSION = 0.1
  DSC_SPECIFICATION = 0x0001001B
  OUTPUT_DIRECTORY = $(CAS_BUILD_DIR)
  SUPPORTED_ARCHITECTURES = AARCH64
  BUILD_TARGETS = DEBUG
  SKUID_IDENTIFIER = DEFAULT
  FLASH_DEFINITION = casPkg/cas-upstream.fdf
  DEFINE CAS_BUILD_DIR = Build/casUpstream001
  DEFINE SOC_TYPE = 0
  DEFINE ENABLE_SECUREBOOT = 0
  DEFINE FD_BASE = 0x9f000000
  DEFINE FD_SIZE = 0x00800000
  DEFINE FD_BLOCKS = 0x800

[BuildOptions]
  # Do not embed output-directory-dependent CodeView/PDB records in EFI images.
  *_CLANGPDB_*_DLINK_FLAGS = /DEBUG:NONE
  *_CLANGPDB_*_CC_FLAGS = -Wno-unused-const-variable -Wno-sometimes-uninitialized -Wno-unaligned-access -Wno-deprecated-non-prototype
  *_CLANGPDB_AARCH64_CC_FLAGS = -march=armv8.2-a+crypto+rcpc -D CLANG -D ENABLE_SECUREBOOT=0 -D CAS_XNU_DIAGNOSTICS=1 -D CAS_UPSTREAM_PROFILE=1 -D __DEPRECATED_AARCH64_4K_RUNTIME_GRANULARITY
[BuildOptions.common.EDKII.SEC]
  *_CLANGPDB_*_DLINK_FLAGS = /ALIGN:0x1000 /FILEALIGN:0x1000
[BuildOptions.common.EDKII.DXE_CORE,BuildOptions.common.EDKII.DXE_DRIVER,BuildOptions.common.EDKII.UEFI_APPLICATION]
  *_CLANGPDB_*_DLINK_FLAGS = /ALIGN:0x1000
[BuildOptions.common.EDKII.DXE_RUNTIME_DRIVER]
  *_CLANGPDB_*_DLINK_FLAGS = /ALIGN:0x10000

!include MdePkg/MdeLibs.dsc.inc

[PcdsFixedAtBuild]
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVendor|L"UniXNU cas development"
  gEfiMdeModulePkgTokenSpaceGuid.PcdFirmwareVersionString|L"cas-upstream-001"
  gEfiMdeModulePkgTokenSpaceGuid.PcdEmuVariableNvModeEnable|TRUE
  gEfiMdeModulePkgTokenSpaceGuid.PcdResetOnMemoryTypeInformationChange|FALSE
  gEfiMdeModulePkgTokenSpaceGuid.PcdMaxVariableSize|0x2000
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x2F
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x8000004F
  gEfiMdePkgTokenSpaceGuid.PcdReportStatusCodePropertyMask|0
  gEfiMdePkgTokenSpaceGuid.PcdPerformanceLibraryPropertyMask|0
  gEfiMdePkgTokenSpaceGuid.PcdUefiLibMaxPrintBufferSize|1024
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000
  gArmTokenSpaceGuid.PcdSystemMemorySize|0x39900000
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0x9f900000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000
  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|3
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x17a00000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x17a60000
  gArmTokenSpaceGuid.PcdMonitorConduitHvc|FALSE
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiACPIReclaimMemory|0
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiACPIMemoryNVS|0
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiReservedMemoryType|0
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiRuntimeServicesData|300
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiRuntimeServicesCode|150
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiBootServicesCode|1000
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiBootServicesData|800
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiLoaderCode|10
  gEmbeddedTokenSpaceGuid.PcdMemoryTypeEfiLoaderData|0
[PcdsFeatureFlag]
  gEmbeddedTokenSpaceGuid.PcdPrePiProduceMemoryTypeInformationHob|TRUE
  gEfiMdePkgTokenSpaceGuid.PcdComponentNameDisable|TRUE
  gEfiMdePkgTokenSpaceGuid.PcdDriverDiagnosticsDisable|TRUE
  gEfiMdePkgTokenSpaceGuid.PcdComponentName2Disable|TRUE
  gEfiMdePkgTokenSpaceGuid.PcdDriverDiagnostics2Disable|TRUE

!include casPkg/Include/Libraries.upstream.dsc.inc

[Components]
  casPkg/Sec/Sec.inf {
    <LibraryClasses>
      NULL|MdeModulePkg/Library/LzmaCustomDecompressLib/LzmaCustomDecompressLib.inf
  }
!include casPkg/Include/Components.upstream.dsc.inc
