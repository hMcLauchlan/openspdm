/** @file
  EDKII SpdmIo Stub

  Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SpdmResponderLibInternal.h"

BOOLEAN
SpdmCalculateCertChainHash (
  IN  SPDM_DEVICE_CONTEXT  *SpdmContext,
  IN  UINT8                SlotNum,
  OUT UINT8                *CertChainHash
  )
{
  UINT32                        HashSize;
  HASH_ALL                      HashFunc;
  
  HashSize = GetSpdmHashSize (SpdmContext);
  HashFunc = GetSpdmHashFunc (SpdmContext);

  HashFunc (SpdmContext->LocalContext.CertificateChain[SlotNum], SpdmContext->LocalContext.CertificateChainSize[SlotNum], CertChainHash);
  return TRUE;
}

BOOLEAN
CalculateMeasurementSummaryHash (
  IN  SPDM_DEVICE_CONTEXT  *SpdmContext,
  IN  UINT8                MeasurementSummaryHashType,
  OUT UINT8                *MeasurementSummaryHash
  )
{
  UINT8                         MeasurementData[MAX_HASH_SIZE * MAX_SPDM_MEASUREMENT_BLOCK_COUNT];
  UINTN                         Index;
  UINTN                         LocalIndex;
  UINT32                        HashSize;
  HASH_ALL                      HashFunc;
  UINTN                         MeasurmentBlockSize;
  SPDM_MEASUREMENT_BLOCK_DMTF   *CachedMeasurmentBlock;
  
  HashSize = GetSpdmMeasurementHashSize (SpdmContext);
  HashFunc = GetSpdmMeasurementHashFunc (SpdmContext);
  
  MeasurmentBlockSize = sizeof(SPDM_MEASUREMENT_BLOCK_DMTF) + HashSize;

  ASSERT(SpdmContext->LocalContext.DeviceMeasurementCount <= MAX_SPDM_MEASUREMENT_BLOCK_COUNT);

  switch (MeasurementSummaryHashType) {
  case SPDM_CHALLENGE_REQUEST_NO_MEASUREMENT_SUMMARY_HASH:
    ZeroMem (MeasurementSummaryHash, HashSize);
    break;
  case SPDM_CHALLENGE_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH:
    CachedMeasurmentBlock = SpdmContext->LocalContext.DeviceMeasurement;
    LocalIndex = 0;
    for (Index = 0; Index < SpdmContext->LocalContext.DeviceMeasurementCount; Index++) {
      switch (CachedMeasurmentBlock->MeasurementBlockDmtfHeader.DMTFSpecMeasurementValueType) {
      case SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_IMMUTABLE_ROM:
        CopyMem (
          &MeasurementData[HashSize * LocalIndex],
          (CachedMeasurmentBlock + 1),
          HashSize
          );
        LocalIndex ++;
        break;
      default:
        break;
      }
      CachedMeasurmentBlock = (VOID *)((UINTN)CachedMeasurmentBlock + MeasurmentBlockSize);
    }
    HashFunc (MeasurementData, HashSize * LocalIndex, MeasurementSummaryHash);
    break;
  case SPDM_CHALLENGE_REQUEST_ALL_MEASUREMENTS_HASH:
    CachedMeasurmentBlock = SpdmContext->LocalContext.DeviceMeasurement;
    for (Index = 0; Index < SpdmContext->LocalContext.DeviceMeasurementCount; Index++) {
      CopyMem (
        &MeasurementData[HashSize * Index],
        (CachedMeasurmentBlock + 1),
        HashSize
        );
      CachedMeasurmentBlock = (VOID *)((UINTN)CachedMeasurmentBlock + MeasurmentBlockSize);
    }
    HashFunc (MeasurementData, HashSize * SpdmContext->LocalContext.DeviceMeasurementCount, MeasurementSummaryHash);
    break;
  default:
    return FALSE;
    break;
  }
  return TRUE;
}

BOOLEAN
SpdmGenerateChallengeSignature (
  IN  SPDM_DEVICE_CONTEXT        *SpdmContext,
  IN  VOID                       *ResponseMessage,
  IN  UINTN                      ResponseMessageSize,
  OUT UINT8                      *Signature
  )
{
  VOID                          *RsaContext;
  UINT8                         HashData[MAX_HASH_SIZE];
  BOOLEAN                       Result;
  UINTN                         SignatureSize;
  UINT32                        HashSize;
  HASH_ALL                      HashFunc;
  
  if (SpdmContext->LocalContext.PrivatePem == NULL) {
    return FALSE;
  }

  SignatureSize = GetSpdmAsymSize (SpdmContext);
  HashSize = GetSpdmHashSize (SpdmContext);
  HashFunc = GetSpdmHashFunc (SpdmContext);

  Result = RsaGetPrivateKeyFromPem (SpdmContext->LocalContext.PrivatePem, SpdmContext->LocalContext.PrivatePemSize, NULL, &RsaContext);
  if (!Result) {
    return FALSE;
  }

  AppendManagedBuffer (&SpdmContext->Transcript.MessageC, ResponseMessage, ResponseMessageSize);
  AppendManagedBuffer (&SpdmContext->Transcript.M1M2, GetManagedBuffer(&SpdmContext->Transcript.MessageA), GetManagedBufferSize(&SpdmContext->Transcript.MessageA));
  AppendManagedBuffer (&SpdmContext->Transcript.M1M2, GetManagedBuffer(&SpdmContext->Transcript.MessageB), GetManagedBufferSize(&SpdmContext->Transcript.MessageB));
  AppendManagedBuffer (&SpdmContext->Transcript.M1M2, GetManagedBuffer(&SpdmContext->Transcript.MessageC), GetManagedBufferSize(&SpdmContext->Transcript.MessageC));

  DEBUG((DEBUG_INFO, "Calc MessageA Data :\n"));
  InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageA), GetManagedBufferSize(&SpdmContext->Transcript.MessageA));
 
  DEBUG((DEBUG_INFO, "Calc MessageB Data :\n"));
  InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageB), GetManagedBufferSize(&SpdmContext->Transcript.MessageB));

  DEBUG((DEBUG_INFO, "Calc MessageC Data :\n"));
  InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageC), GetManagedBufferSize(&SpdmContext->Transcript.MessageC));

  HashFunc (GetManagedBuffer(&SpdmContext->Transcript.M1M2), GetManagedBufferSize(&SpdmContext->Transcript.M1M2), HashData);
  DEBUG((DEBUG_INFO, "Calc M1M2 Hash - "));
  InternalDumpData (HashData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));
  
  Result = RsaPkcs1Sign (
             RsaContext,
             HashData,
             HashSize,
             Signature,
             &SignatureSize
             );
  RsaFree (RsaContext);

  return Result;
}

RETURN_STATUS
EFIAPI
SpdmGetResponseChallenge (
  IN     SPDM_DEVICE_CONTEXT  *SpdmContext,
  IN     UINTN                RequestSize,
  IN     VOID                 *Request,
  IN OUT UINTN                *ResponseSize,
     OUT VOID                 *Response
  )
{
  SPDM_CHALLENGE_REQUEST            *SpdmRequest;
  SPDM_CHALLENGE_AUTH_RESPONSE      *SpdmResponse;
  BOOLEAN                           Result;
  UINTN                             SignatureSize;
  UINT8                             SlotNum;
  UINT32                            HashSize;
  UINT8                             *Ptr;
  UINTN                             TotalSize;
  
  SpdmRequest = Request;
  SlotNum = SpdmRequest->Header.Param1;

  if (SlotNum > SpdmContext->LocalContext.SlotCount) {
    SpdmGenerateErrorResponse (SpdmContext, SPDM_ERROR_CODE_INVALID_REQUEST, 0, ResponseSize, Response);
    return RETURN_SUCCESS;
  }

  SignatureSize = GetSpdmAsymSize (SpdmContext);
  HashSize = GetSpdmHashSize (SpdmContext);

  TotalSize = sizeof(SPDM_CHALLENGE_AUTH_RESPONSE) +
              HashSize +
              SPDM_NONCE_SIZE +
              HashSize +
              sizeof(UINT16) +
              DEFAULT_OPAQUE_LENGTH +
              SignatureSize;

  ASSERT (*ResponseSize >= TotalSize);
  *ResponseSize = TotalSize;
  ZeroMem (Response, *ResponseSize);
  SpdmResponse = Response;

  SpdmResponse->Header.SPDMVersion = SPDM_MESSAGE_VERSION_10;
  SpdmResponse->Header.RequestResponseCode = SPDM_CHALLENGE_AUTH;
  SpdmResponse->Header.Param1 = SlotNum;
  SpdmResponse->Header.Param2 = (1 << SlotNum);

  Ptr = (VOID *)(SpdmResponse + 1);
  SpdmCalculateCertChainHash (SpdmContext, SlotNum, Ptr);
  Ptr += HashSize;

  GetRandomNumber (SPDM_NONCE_SIZE, Ptr);
  Ptr += SPDM_NONCE_SIZE;

  Result = CalculateMeasurementSummaryHash (SpdmContext, SpdmRequest->Header.Param2, Ptr);
  if (!Result) {
    SpdmGenerateErrorResponse (SpdmContext, SPDM_ERROR_CODE_INVALID_REQUEST, 0, ResponseSize, Response);
    return RETURN_SUCCESS;
  }
  Ptr += HashSize;

  *(UINT16 *)Ptr = DEFAULT_OPAQUE_LENGTH;
  Ptr += sizeof(UINT16);
  SetMem (Ptr, DEFAULT_OPAQUE_LENGTH, DEFAULT_OPAQUE_DATA);
  Ptr += DEFAULT_OPAQUE_LENGTH;
  
  //
  // Calc Sign
  //
  Result = SpdmGenerateChallengeSignature (SpdmContext, SpdmResponse, (UINTN)Ptr - (UINTN)SpdmResponse, Ptr);
  if (!Result) {
    SpdmGenerateErrorResponse (SpdmContext, SPDM_ERROR_CODE_UNSUPPORTED_REQUEST, SPDM_CHALLENGE_AUTH, ResponseSize, Response);
    return RETURN_SUCCESS;
  }
  Ptr += SignatureSize;

  return RETURN_SUCCESS;
}
