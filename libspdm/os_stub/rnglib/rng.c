/** @file

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <base.h>
#include <stdlib.h>

/**
  Generates a 64-bit random number.

  if rand is NULL, then ASSERT().

  @param[out] rand_data     buffer pointer to store the 64-bit random value.

  @retval TRUE         Random number generated successfully.
  @retval FALSE        Failed to generate the random number.

**/
boolean
get_random_number_64 (
  OUT     uint64                    *rand_data
  )
{
  uint8  *ptr;

  ptr = (uint8 *)rand_data;
  ptr[0] = (uint8)rand();
  ptr[1] = (uint8)rand();
  ptr[2] = (uint8)rand();
  ptr[3] = (uint8)rand();
  ptr[4] = (uint8)rand();
  ptr[5] = (uint8)rand();
  ptr[6] = (uint8)rand();
  ptr[7] = (uint8)rand();

  return TRUE;
}
