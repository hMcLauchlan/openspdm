/**
@file
UEFI OS based application.

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SpdmDump.h"

VOID
DumpPciDoePacket (
  IN VOID    *Buffer,
  IN UINTN   BufferSize,
  IN BOOLEAN Truncated
  )
{
  PCI_DOE_DATA_OBJECT_HEADER  *PciDoeHeader;
  if (BufferSize < sizeof(PCI_DOE_DATA_OBJECT_HEADER)) {
    return ;
  }
  PciDoeHeader = Buffer;

  printf ("PCI_DOE(%d, %d) ", PciDoeHeader->VendorId, PciDoeHeader->DataObjectType);

  if (PciDoeHeader->VendorId != PCI_DOE_VENDOR_ID_PCISIG) {
    return ;
  }
  switch (PciDoeHeader->DataObjectType) {
  case PCI_DOE_DATA_OBJECT_TYPE_SPDM:
    DumpSpdmPacket ((UINT8 *)Buffer + sizeof(PCI_DOE_DATA_OBJECT_HEADER), BufferSize - sizeof(PCI_DOE_DATA_OBJECT_HEADER), Truncated);
    break;
  case PCI_DOE_DATA_OBJECT_TYPE_SECURED_SPDM:
    DumpSecuredSpdmPacket ((UINT8 *)Buffer + sizeof(PCI_DOE_DATA_OBJECT_HEADER), BufferSize - sizeof(PCI_DOE_DATA_OBJECT_HEADER), Truncated);
    break;
  case PCI_DOE_DATA_OBJECT_TYPE_DOE_DISCOVERY:
    // TBD
    break;
  default:
    break;
  }
}
