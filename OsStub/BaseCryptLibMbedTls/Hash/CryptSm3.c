/** @file
  SM3 Digest Wrapper Implementations over mbedTLS

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "InternalCryptLib.h"

/**
  Retrieves the size, in bytes, of the context buffer required for SM3 hash operations.

  @return  The size, in bytes, of the context buffer required for SM3 hash operations.

**/
UINTN
EFIAPI
Sm3GetContextSize (
  VOID
  )
{
  return 0;
}

/**
  Initializes user-supplied memory pointed by Sm3Context as SM3 hash context for
  subsequent use.

  If Sm3Context is NULL, then return FALSE.

  @param[out]  Sm3Context  Pointer to SM3 context being initialized.

  @retval TRUE   SM3 context initialization succeeded.
  @retval FALSE  SM3 context initialization failed.

**/
BOOLEAN
EFIAPI
Sm3Init (
  OUT  VOID  *Sm3Context
  )
{
  return FALSE;
}

/**
  Makes a copy of an existing SM3 context.

  If Sm3Context is NULL, then return FALSE.
  If NewSm3Context is NULL, then return FALSE.
  If this interface is not supported, then return FALSE.

  @param[in]  Sm3Context     Pointer to SM3 context being copied.
  @param[out] NewSm3Context  Pointer to new SM3 context.

  @retval TRUE   SM3 context copy succeeded.
  @retval FALSE  SM3 context copy failed.
  @retval FALSE  This interface is not supported.

**/
BOOLEAN
EFIAPI
Sm3Duplicate (
  IN   CONST VOID  *Sm3Context,
  OUT  VOID        *NewSm3Context
  )
{
  return FALSE;
}

/**
  Digests the input data and updates SM3 context.

  This function performs SM3 digest on a data buffer of the specified size.
  It can be called multiple times to compute the digest of long or discontinuous data streams.
  SM3 context should be already correctly initialized by Sm3Init(), and should not be finalized
  by Sm3Final(). Behavior with invalid context is undefined.

  If Sm3Context is NULL, then return FALSE.

  @param[in, out]  Sm3Context     Pointer to the SM3 context.
  @param[in]       Data           Pointer to the buffer containing the data to be hashed.
  @param[in]       DataSize       Size of Data buffer in bytes.

  @retval TRUE   SM3 data digest succeeded.
  @retval FALSE  SM3 data digest failed.

**/
BOOLEAN
EFIAPI
Sm3Update (
  IN OUT  VOID        *Sm3Context,
  IN      CONST VOID  *Data,
  IN      UINTN       DataSize
  )
{
  return FALSE;
}

/**
  Completes computation of the SM3 digest value.

  This function completes SM3 hash computation and retrieves the digest value into
  the specified memory. After this function has been called, the SM3 context cannot
  be used again.
  SM3 context should be already correctly initialized by Sm3Init(), and should not be
  finalized by Sm3Final(). Behavior with invalid SM3 context is undefined.

  If Sm3Context is NULL, then return FALSE.
  If HashValue is NULL, then return FALSE.

  @param[in, out]  Sm3Context     Pointer to the SM3 context.
  @param[out]      HashValue      Pointer to a buffer that receives the SM3 digest
                                  value (32 bytes).

  @retval TRUE   SM3 digest computation succeeded.
  @retval FALSE  SM3 digest computation failed.

**/
BOOLEAN
EFIAPI
Sm3Final (
  IN OUT  VOID   *Sm3Context,
  OUT     UINT8  *HashValue
  )
{
  return FALSE;
}

/**
  Computes the SM3 message digest of a input data buffer.

  This function performs the SM3 message digest of a given data buffer, and places
  the digest value into the specified memory.

  If this interface is not supported, then return FALSE.

  @param[in]   Data        Pointer to the buffer containing the data to be hashed.
  @param[in]   DataSize    Size of Data buffer in bytes.
  @param[out]  HashValue   Pointer to a buffer that receives the SM3 digest
                           value (32 bytes).

  @retval TRUE   SM3 digest computation succeeded.
  @retval FALSE  SM3 digest computation failed.
  @retval FALSE  This interface is not supported.

**/
BOOLEAN
EFIAPI
Sm3HashAll (
  IN   CONST VOID  *Data,
  IN   UINTN       DataSize,
  OUT  UINT8       *HashValue
  )
{
  return FALSE;
}
