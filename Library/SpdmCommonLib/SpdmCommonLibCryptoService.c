/** @file
  SPDM common library.
  It follows the SPDM Specification.

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SpdmCommonLibInternal.h"

/**
  This function generates the certificate chain hash.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SlotIndex                    The slot index of the certificate chain.
  @param  Signature                    The buffer to store the certificate chain hash.

  @retval TRUE  certificate chain hash is generated.
  @retval FALSE certificate chain hash is not generated.
**/
BOOLEAN
SpdmGenerateCertChainHash (
  IN     SPDM_DEVICE_CONTEXT          *SpdmContext,
  IN     UINTN                        SlotIndex,
     OUT UINT8                        *Hash
  )
{
  ASSERT (SlotIndex < SpdmContext->LocalContext.SlotCount);
  SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, SpdmContext->LocalContext.CertificateChain[SlotIndex], SpdmContext->LocalContext.CertificateChainSize[SlotIndex], Hash);
  return TRUE;
}

/**
  This function verifies the digest.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  Digest                       The digest data buffer.
  @param  DigestSize                   Size in bytes of the digest data buffer.

  @retval TRUE  digest verification pass.
  @retval FALSE digest verification fail.
**/
BOOLEAN
SpdmVerifyDigest (
  IN SPDM_DEVICE_CONTEXT          *SpdmContext,
  IN VOID                         *Digest,
  IN UINTN                        DigestSize
  )
{
  UINTN                                     HashSize;
  UINT8                                     CertBufferHash[MAX_HASH_SIZE];
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;

  CertBuffer = SpdmContext->LocalContext.PeerCertChainProvision;
  CertBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize;
  if ((CertBuffer != NULL) && (CertBufferSize != 0)) {
    HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);
    SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, CertBuffer, CertBufferSize, CertBufferHash);

    if (CompareMem (Digest, CertBufferHash, HashSize) != 0) {
      DEBUG((DEBUG_INFO, "!!! VerifyDigest - FAIL !!!\n"));
      return FALSE;
    }
  }

  DEBUG((DEBUG_INFO, "!!! VerifyDigest - PASS !!!\n"));

  return TRUE;
}

/**
  This function verifies the certificate chain.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  CertificateChain             The certificate chain data buffer.
  @param  CertificateChainSize         Size in bytes of the certificate chain data buffer.

  @retval TRUE  certificate chain verification pass.
  @retval FALSE certificate chain verification fail.
**/
BOOLEAN
SpdmVerifyCertificateChain (
  IN SPDM_DEVICE_CONTEXT          *SpdmContext,
  IN VOID                         *CertificateChain,
  IN UINTN                        CertificateChainSize
  )
{
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;
  UINTN                                     HashSize;
  UINT8                                     *RootCertHash;
  UINTN                                     RootCertHashSize;
  BOOLEAN                                   Result;

  Result = SpdmVerifyCertificateChainData (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, CertificateChain, CertificateChainSize);
  if (!Result) {
    return FALSE;
  }

  RootCertHash = SpdmContext->LocalContext.PeerRootCertHashProvision;
  RootCertHashSize = SpdmContext->LocalContext.PeerRootCertHashProvisionSize;
  CertBuffer = SpdmContext->LocalContext.PeerCertChainProvision;
  CertBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize;

  if ((RootCertHash != NULL) && (RootCertHashSize != 0)) {
    HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);
    if (RootCertHashSize != HashSize) {
      DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL (hash size mismatch) !!!\n"));
      return FALSE;
    }
    if (CompareMem ((UINT8 *)CertificateChain + sizeof(SPDM_CERT_CHAIN), RootCertHash, HashSize) != 0) {
      DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL (root hash mismatch) !!!\n"));
      return FALSE;
    }
  } else if ((CertBuffer != NULL) && (CertBufferSize != 0)) {
    if (CertBufferSize != CertificateChainSize) {
      DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL !!!\n"));
      return FALSE;
    }
    if (CompareMem (CertificateChain, CertBuffer, CertificateChainSize) != 0) {
      DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - FAIL !!!\n"));
      return FALSE;
    }
  }

  DEBUG((DEBUG_INFO, "!!! VerifyCertificateChain - PASS !!!\n"));
  SpdmContext->ConnectionInfo.PeerCertChainBufferSize = CertificateChainSize;
  CopyMem (SpdmContext->ConnectionInfo.PeerCertChainBuffer, CertificateChain, CertificateChainSize);

  return TRUE;
}

/**
  This function generates the challenge signature based upon M1M2 for authentication.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  IsRequester                  Indicate of the signature generation for a requester or a responder.
  @param  ResponseMessage              The response message buffer.
  @param  ResponseMessageSize          Size in bytes of the response message buffer.
  @param  Signature                    The buffer to store the challenge signature.

  @retval TRUE  challenge signature is generated.
  @retval FALSE challenge signature is not generated.
**/
BOOLEAN
SpdmGenerateChallengeAuthSignature (
  IN     SPDM_DEVICE_CONTEXT        *SpdmContext,
  IN     BOOLEAN                    IsRequester,
  IN     VOID                       *ResponseMessage,
  IN     UINTN                      ResponseMessageSize,
     OUT UINT8                      *Signature
  )
{
  UINT8                         HashData[MAX_HASH_SIZE];
  BOOLEAN                       Result;
  UINTN                         SignatureSize;
  UINT32                        HashSize;
  RETURN_STATUS                 Status;

  SignatureSize = GetSpdmAsymSize (SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo);
  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  if (IsRequester) {
    Status = AppendManagedBuffer (&SpdmContext->Transcript.MessageMutC, ResponseMessage, ResponseMessageSize);
    if (RETURN_ERROR(Status)) {
      return FALSE;
    }
    Status = AppendManagedBuffer (&SpdmContext->Transcript.M1M2, GetManagedBuffer(&SpdmContext->Transcript.MessageMutB), GetManagedBufferSize(&SpdmContext->Transcript.MessageMutB));
    if (RETURN_ERROR(Status)) {
      return FALSE;
    }
    Status = AppendManagedBuffer (&SpdmContext->Transcript.M1M2, GetManagedBuffer(&SpdmContext->Transcript.MessageMutC), GetManagedBufferSize(&SpdmContext->Transcript.MessageMutC));
    if (RETURN_ERROR(Status)) {
      return FALSE;
    }

    DEBUG((DEBUG_INFO, "Calc MessageMutB Data :\n"));
    InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageMutB), GetManagedBufferSize(&SpdmContext->Transcript.MessageMutB));

    DEBUG((DEBUG_INFO, "Calc MessageMutC Data :\n"));
    InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageMutC), GetManagedBufferSize(&SpdmContext->Transcript.MessageMutC));

    SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, GetManagedBuffer(&SpdmContext->Transcript.M1M2), GetManagedBufferSize(&SpdmContext->Transcript.M1M2), HashData);
    DEBUG((DEBUG_INFO, "Calc M1M2 Hash - "));
    InternalDumpData (HashData, HashSize);
    DEBUG((DEBUG_INFO, "\n"));
    
    Result = SpdmRequesterDataSignFunc (
              SpdmContext->ConnectionInfo.Algorithm.ReqBaseAsymAlg,
              HashData,
              HashSize,
              Signature,
              &SignatureSize
              );
  } else {
    Status = AppendManagedBuffer (&SpdmContext->Transcript.MessageC, ResponseMessage, ResponseMessageSize);
    if (RETURN_ERROR(Status)) {
      return FALSE;
    }
    Status = AppendManagedBuffer (&SpdmContext->Transcript.M1M2, GetManagedBuffer(&SpdmContext->Transcript.MessageA), GetManagedBufferSize(&SpdmContext->Transcript.MessageA));
    if (RETURN_ERROR(Status)) {
      return FALSE;
    }
    Status = AppendManagedBuffer (&SpdmContext->Transcript.M1M2, GetManagedBuffer(&SpdmContext->Transcript.MessageB), GetManagedBufferSize(&SpdmContext->Transcript.MessageB));
    if (RETURN_ERROR(Status)) {
      return FALSE;
    }
    Status = AppendManagedBuffer (&SpdmContext->Transcript.M1M2, GetManagedBuffer(&SpdmContext->Transcript.MessageC), GetManagedBufferSize(&SpdmContext->Transcript.MessageC));
    if (RETURN_ERROR(Status)) {
      return FALSE;
    }

    DEBUG((DEBUG_INFO, "Calc MessageA Data :\n"));
    InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageA), GetManagedBufferSize(&SpdmContext->Transcript.MessageA));

    DEBUG((DEBUG_INFO, "Calc MessageB Data :\n"));
    InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageB), GetManagedBufferSize(&SpdmContext->Transcript.MessageB));

    DEBUG((DEBUG_INFO, "Calc MessageC Data :\n"));
    InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageC), GetManagedBufferSize(&SpdmContext->Transcript.MessageC));

    SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, GetManagedBuffer(&SpdmContext->Transcript.M1M2), GetManagedBufferSize(&SpdmContext->Transcript.M1M2), HashData);
    DEBUG((DEBUG_INFO, "Calc M1M2 Hash - "));
    InternalDumpData (HashData, HashSize);
    DEBUG((DEBUG_INFO, "\n"));
    
    Result = SpdmResponderDataSignFunc (
              SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo,
              HashData,
              HashSize,
              Signature,
              &SignatureSize
              );
  }

  return Result;
}

/**
  This function verifies the certificate chain hash.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  CertificateChainHash         The certificate chain hash data buffer.
  @param  CertificateChainHashSize     Size in bytes of the certificate chain hash data buffer.

  @retval TRUE  hash verification pass.
  @retval FALSE hash verification fail.
**/
BOOLEAN
SpdmVerifyCertificateChainHash (
  IN SPDM_DEVICE_CONTEXT          *SpdmContext,
  IN VOID                         *CertificateChainHash,
  IN UINTN                        CertificateChainHashSize
  )
{
  UINTN                                     HashSize;
  UINT8                                     CertBufferHash[MAX_HASH_SIZE];
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;

  if (SpdmContext->ConnectionInfo.PeerCertChainBufferSize != 0) {
    CertBuffer = SpdmContext->ConnectionInfo.PeerCertChainBuffer;
    CertBufferSize = SpdmContext->ConnectionInfo.PeerCertChainBufferSize;
  } else if (SpdmContext->LocalContext.PeerCertChainProvisionSize != 0) {
    CertBuffer = SpdmContext->LocalContext.PeerCertChainProvision;
    CertBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize;
  } else {
    return FALSE;
  }

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, CertBuffer, CertBufferSize, CertBufferHash);

  if (HashSize != CertificateChainHashSize) {
    DEBUG((DEBUG_INFO, "!!! VerifyCertificateChainHash - FAIL !!!\n"));
    return FALSE;
  }
  if (CompareMem (CertificateChainHash, CertBufferHash, CertificateChainHashSize) != 0) {
    DEBUG((DEBUG_INFO, "!!! VerifyCertificateChainHash - FAIL !!!\n"));
    return FALSE;
  }
  DEBUG((DEBUG_INFO, "!!! VerifyCertificateChainHash - PASS !!!\n"));
  return TRUE;
}

/**
  This function verifies the challenge signature based upon M1M2.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  IsRequester                  Indicate of the signature verification for a requester or a responder.
  @param  SignData                     The signature data buffer.
  @param  SignDataSize                 Size in bytes of the signature data buffer.

  @retval TRUE  signature verification pass.
  @retval FALSE signature verification fail.
**/
BOOLEAN
SpdmVerifyChallengeAuthSignature (
  IN  SPDM_DEVICE_CONTEXT          *SpdmContext,
  IN  BOOLEAN                      IsRequester,
  IN  VOID                         *SignData,
  IN  UINTN                        SignDataSize
  )
{
  UINTN                                     HashSize;
  UINT8                                     HashData[MAX_HASH_SIZE];
  BOOLEAN                                   Result;
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;
  VOID                                      *Context;
  UINT8                                     *CertChainBuffer;
  UINTN                                     CertChainBufferSize;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  if (IsRequester) {
    DEBUG((DEBUG_INFO, "MessageA Data :\n"));
    InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageA), GetManagedBufferSize(&SpdmContext->Transcript.MessageA));

    DEBUG((DEBUG_INFO, "MessageB Data :\n"));
    InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageB), GetManagedBufferSize(&SpdmContext->Transcript.MessageB));

    DEBUG((DEBUG_INFO, "MessageC Data :\n"));
    InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageC), GetManagedBufferSize(&SpdmContext->Transcript.MessageC));
  } else {
    DEBUG((DEBUG_INFO, "MessageMutB Data :\n"));
    InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageMutB), GetManagedBufferSize(&SpdmContext->Transcript.MessageMutB));

    DEBUG((DEBUG_INFO, "MessageMutC Data :\n"));
    InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageMutC), GetManagedBufferSize(&SpdmContext->Transcript.MessageMutC));
  }

  SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, GetManagedBuffer(&SpdmContext->Transcript.M1M2), GetManagedBufferSize(&SpdmContext->Transcript.M1M2), HashData);
  DEBUG((DEBUG_INFO, "M1M2 Hash - "));
  InternalDumpData (HashData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  if (SpdmContext->ConnectionInfo.PeerCertChainBufferSize != 0) {
    CertChainBuffer = (UINT8 *)SpdmContext->ConnectionInfo.PeerCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertChainBufferSize = SpdmContext->ConnectionInfo.PeerCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else if (SpdmContext->LocalContext.PeerCertChainProvisionSize != 0) {
    CertChainBuffer = (UINT8 *)SpdmContext->LocalContext.PeerCertChainProvision + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertChainBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else {
    return FALSE;
  }


  //
  // Get leaf cert from cert chain
  //
  Result = X509GetCertFromCertChain (CertChainBuffer, CertChainBufferSize, -1,  &CertBuffer, &CertBufferSize);
  if (!Result) {
    return FALSE;
  }

  if (IsRequester) {
    Result = SpdmAsymGetPublicKeyFromX509 (SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo, CertBuffer, CertBufferSize, &Context);
    if (!Result) {
      return FALSE;
    }

    Result = SpdmAsymVerify (
              SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo,
              Context,
              HashData,
              HashSize,
              SignData,
              SignDataSize
              );
    SpdmAsymFree (SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo, Context);
  } else {
    Result = SpdmReqAsymGetPublicKeyFromX509 (SpdmContext->ConnectionInfo.Algorithm.ReqBaseAsymAlg, CertBuffer, CertBufferSize, &Context);
    if (!Result) {
      return FALSE;
    }

    Result = SpdmReqAsymVerify (
              SpdmContext->ConnectionInfo.Algorithm.ReqBaseAsymAlg,
              Context,
              HashData,
              HashSize,
              SignData,
              SignDataSize
              );
    SpdmReqAsymFree (SpdmContext->ConnectionInfo.Algorithm.ReqBaseAsymAlg, Context);
  }

  if (!Result) {
    DEBUG((DEBUG_INFO, "!!! VerifyChallengeSignature - FAIL !!!\n"));
    return FALSE;
  }
  DEBUG((DEBUG_INFO, "!!! VerifyChallengeSignature - PASS !!!\n"));

  return TRUE;
}

/**
  This function calculate the measurement summary hash.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  MeasurementSummaryHashType   The type of the measurement summary hash.
  @param  MeasurementSummaryHash       The buffer to store the measurement summary hash.

  @retval TRUE  measurement summary hash is generated.
  @retval FALSE measurement summary hash is not generated.
**/
BOOLEAN
SpdmGenerateMeasurementSummaryHash (
  IN     SPDM_DEVICE_CONTEXT  *SpdmContext,
  IN     UINT8                MeasurementSummaryHashType,
     OUT UINT8                *MeasurementSummaryHash
  )
{
  UINT8                         MeasurementData[MAX_SPDM_MEASUREMENT_RECORD_SIZE];
  UINTN                         Index;
  SPDM_MEASUREMENT_BLOCK_DMTF   *CachedMeasurmentBlock;
  UINTN                         MeasurmentDataSize;
  UINTN                         MeasurmentBlockSize;
  UINT8                         DeviceMeasurement[MAX_SPDM_MEASUREMENT_RECORD_SIZE];
  UINT8                         DeviceMeasurementCount;
  UINTN                         DeviceMeasurementSize;
  BOOLEAN                       Ret;

  switch (MeasurementSummaryHashType) {
  case SPDM_CHALLENGE_REQUEST_NO_MEASUREMENT_SUMMARY_HASH:
    ZeroMem (MeasurementSummaryHash, GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo));
    break;

  case SPDM_CHALLENGE_REQUEST_TCB_COMPONENT_MEASUREMENT_HASH:
  case SPDM_CHALLENGE_REQUEST_ALL_MEASUREMENTS_HASH:

    DeviceMeasurementSize = sizeof(DeviceMeasurement);
    Ret = SpdmMeasurementCollectionFunc (
            SpdmContext->ConnectionInfo.Algorithm.MeasurementSpec,
            SpdmContext->ConnectionInfo.Algorithm.MeasurementHashAlgo,
            &DeviceMeasurementCount,
            DeviceMeasurement,
            &DeviceMeasurementSize
            );
    if (!Ret) {
      return Ret;
    }

    ASSERT(DeviceMeasurementCount <= MAX_SPDM_MEASUREMENT_BLOCK_COUNT);

    MeasurmentDataSize = 0;
    CachedMeasurmentBlock = (VOID *)DeviceMeasurement;
    for (Index = 0; Index < DeviceMeasurementCount; Index++) {
      MeasurmentBlockSize = sizeof(SPDM_MEASUREMENT_BLOCK_COMMON_HEADER) + CachedMeasurmentBlock->MeasurementBlockCommonHeader.MeasurementSize;
      ASSERT (CachedMeasurmentBlock->MeasurementBlockCommonHeader.MeasurementSize == sizeof(SPDM_MEASUREMENT_BLOCK_DMTF_HEADER) + CachedMeasurmentBlock->MeasurementBlockDmtfHeader.DMTFSpecMeasurementValueSize);
      MeasurmentDataSize += CachedMeasurmentBlock->MeasurementBlockCommonHeader.MeasurementSize;
      CachedMeasurmentBlock = (VOID *)((UINTN)CachedMeasurmentBlock + MeasurmentBlockSize);
    }

    ASSERT (MeasurmentDataSize <= MAX_SPDM_MEASUREMENT_RECORD_SIZE);

    CachedMeasurmentBlock = (VOID *)DeviceMeasurement;
    MeasurmentDataSize = 0;
    for (Index = 0; Index < DeviceMeasurementCount; Index++) {
      MeasurmentBlockSize = sizeof(SPDM_MEASUREMENT_BLOCK_COMMON_HEADER) + CachedMeasurmentBlock->MeasurementBlockCommonHeader.MeasurementSize;
      if ((MeasurementSummaryHashType == SPDM_CHALLENGE_REQUEST_ALL_MEASUREMENTS_HASH) ||
          ((CachedMeasurmentBlock->MeasurementBlockDmtfHeader.DMTFSpecMeasurementValueType & SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_MASK) == SPDM_MEASUREMENT_BLOCK_MEASUREMENT_TYPE_IMMUTABLE_ROM)) {
        CopyMem (&MeasurementData[MeasurmentDataSize], &CachedMeasurmentBlock->MeasurementBlockDmtfHeader, CachedMeasurmentBlock->MeasurementBlockCommonHeader.MeasurementSize);
      }
      MeasurmentDataSize += CachedMeasurmentBlock->MeasurementBlockCommonHeader.MeasurementSize;
      CachedMeasurmentBlock = (VOID *)((UINTN)CachedMeasurmentBlock + MeasurmentBlockSize);
    }
    SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, MeasurementData, MeasurmentDataSize, MeasurementSummaryHash);
    break;
  default:
    return FALSE;
    break;
  }
  return TRUE;
}

/**
  This function creates the measurement signature to response message based upon L1L2.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  ResponseMessage              The measurement response message with empty signature to be filled.
  @param  ResponseMessageSize          Total size in bytes of the response message including signature.

  @retval TRUE  measurement signature is created.
  @retval FALSE measurement signature is not created.
**/
BOOLEAN
SpdmGenerateMeasurementSignature (
  IN     SPDM_DEVICE_CONTEXT    *SpdmContext,
  IN OUT VOID                   *ResponseMessage,
  IN     UINTN                  ResponseMessageSize
  )
{
  UINT8                         *Ptr;
  UINTN                         MeasurmentSigSize;
  UINTN                         SignatureSize;
  BOOLEAN                       Result;
  UINT8                         HashData[MAX_HASH_SIZE];
  UINT32                        HashSize;
  RETURN_STATUS                 Status;

  SignatureSize = GetSpdmAsymSize (SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo);
  MeasurmentSigSize = SPDM_NONCE_SIZE +
                      sizeof(UINT16) +
                      SpdmContext->LocalContext.OpaqueMeasurementRspSize +
                      SignatureSize;
  ASSERT (ResponseMessageSize > MeasurmentSigSize);
  Ptr = (VOID *)((UINT8 *)ResponseMessage + ResponseMessageSize - MeasurmentSigSize);
  
  SpdmGetRandomNumber (SPDM_NONCE_SIZE, Ptr);
  Ptr += SPDM_NONCE_SIZE;

  *(UINT16 *)Ptr = (UINT16)SpdmContext->LocalContext.OpaqueMeasurementRspSize;
  Ptr += sizeof(UINT16);
  CopyMem (Ptr, SpdmContext->LocalContext.OpaqueMeasurementRsp, SpdmContext->LocalContext.OpaqueMeasurementRspSize);
  Ptr += SpdmContext->LocalContext.OpaqueMeasurementRspSize;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  Status = AppendManagedBuffer (&SpdmContext->Transcript.L1L2, ResponseMessage, ResponseMessageSize - SignatureSize);
  if (RETURN_ERROR(Status)) {
    return FALSE;
  }

  DEBUG((DEBUG_INFO, "Calc L1L2 Data :\n"));
  InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.L1L2), GetManagedBufferSize(&SpdmContext->Transcript.L1L2));

  SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, GetManagedBuffer(&SpdmContext->Transcript.L1L2), GetManagedBufferSize(&SpdmContext->Transcript.L1L2), HashData);
  DEBUG((DEBUG_INFO, "Calc L1L2 Hash - "));
  InternalDumpData (HashData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));
  
  Result = SpdmResponderDataSignFunc (
             SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo,
             HashData,
             HashSize,
             Ptr,
             &SignatureSize
             );
  return Result;
}

/**
  This function verifies the measurement signature based upon L1L2.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SignData                     The signature data buffer.
  @param  SignDataSize                 Size in bytes of the signature data buffer.

  @retval TRUE  signature verification pass.
  @retval FALSE signature verification fail.
**/
BOOLEAN
SpdmVerifyMeasurementSignature (
  IN SPDM_DEVICE_CONTEXT          *SpdmContext,
  IN VOID                         *SignData,
  IN UINTN                        SignDataSize
  )
{
  UINTN                                     HashSize;
  UINT8                                     HashData[MAX_HASH_SIZE];
  BOOLEAN                                   Result;
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;
  VOID                                      *Context;
  UINT8                                     *CertChainBuffer;
  UINTN                                     CertChainBufferSize;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  DEBUG((DEBUG_INFO, "L1L2 Data :\n"));
  InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.L1L2), GetManagedBufferSize(&SpdmContext->Transcript.L1L2));

  SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, GetManagedBuffer(&SpdmContext->Transcript.L1L2), GetManagedBufferSize(&SpdmContext->Transcript.L1L2), HashData);
  DEBUG((DEBUG_INFO, "L1L2 Hash - "));
  InternalDumpData (HashData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  if (SpdmContext->ConnectionInfo.PeerCertChainBufferSize != 0) {
    CertChainBuffer = (UINT8 *)SpdmContext->ConnectionInfo.PeerCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertChainBufferSize = SpdmContext->ConnectionInfo.PeerCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else if (SpdmContext->LocalContext.PeerCertChainProvisionSize != 0) {
    CertChainBuffer = (UINT8 *)SpdmContext->LocalContext.PeerCertChainProvision + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertChainBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else {
    return FALSE;
  }

  //
  // Get leaf cert from cert chain
  //
  Result = X509GetCertFromCertChain (CertChainBuffer, CertChainBufferSize, -1,  &CertBuffer, &CertBufferSize);
  if (!Result) {
    return FALSE;
  }

  Result = SpdmAsymGetPublicKeyFromX509 (SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo, CertBuffer, CertBufferSize, &Context);
  if (!Result) {
    return FALSE;
  }

  Result = SpdmAsymVerify (
             SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo,
             Context,
             HashData,
             HashSize,
             SignData,
             SignDataSize
             );
  SpdmAsymFree (SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo, Context);
  if (!Result) {
    DEBUG((DEBUG_INFO, "!!! VerifyMeasurementSignature - FAIL !!!\n"));
    return FALSE;
  }

  DEBUG((DEBUG_INFO, "!!! VerifyMeasurementSignature - PASS !!!\n"));
  return TRUE;
}

BOOLEAN
SpdmCalculateTHCurrAK (
  IN     SPDM_DEVICE_CONTEXT       *SpdmContext,
  IN     SPDM_SESSION_INFO         *SessionInfo,
  IN     UINT8                     *CertBuffer, OPTIONAL
  IN     UINTN                     CertBufferSize, OPTIONAL
     OUT LARGE_MANAGED_BUFFER      *THCurr
  )
{
  UINT8                         CertBufferHash[MAX_HASH_SIZE];
  UINT32                        HashSize;
  RETURN_STATUS                 Status;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  InitManagedBuffer (THCurr, MAX_SPDM_MESSAGE_BUFFER_SIZE);

  DEBUG((DEBUG_INFO, "MessageA Data :\n"));
  InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageA), GetManagedBufferSize(&SpdmContext->Transcript.MessageA));
  Status = AppendManagedBuffer (THCurr, GetManagedBuffer(&SpdmContext->Transcript.MessageA), GetManagedBufferSize(&SpdmContext->Transcript.MessageA));
  if (RETURN_ERROR(Status)) {
    return FALSE;
  }

  if (CertBuffer != NULL) {
    DEBUG((DEBUG_INFO, "THMessageCt Data :\n"));
    InternalDumpHex (CertBuffer, CertBufferSize);
    SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, CertBuffer, CertBufferSize, CertBufferHash);
    Status = AppendManagedBuffer (THCurr, CertBufferHash, HashSize);
    if (RETURN_ERROR(Status)) {
      return FALSE;
    }
  }

  DEBUG((DEBUG_INFO, "MessageK Data :\n"));
  InternalDumpHex (GetManagedBuffer(&SessionInfo->SessionTranscript.MessageK), GetManagedBufferSize(&SessionInfo->SessionTranscript.MessageK));
  Status = AppendManagedBuffer (THCurr, GetManagedBuffer(&SessionInfo->SessionTranscript.MessageK), GetManagedBufferSize(&SessionInfo->SessionTranscript.MessageK));
  if (RETURN_ERROR(Status)) {
    return FALSE;
  }

  return TRUE;
}

BOOLEAN
SpdmCalculateTHCurrAKF (
  IN     SPDM_DEVICE_CONTEXT       *SpdmContext,
  IN     SPDM_SESSION_INFO         *SessionInfo,
  IN     UINT8                     *CertBuffer, OPTIONAL
  IN     UINTN                     CertBufferSize, OPTIONAL
  IN     UINT8                     *MutCertBuffer, OPTIONAL
  IN     UINTN                     MutCertBufferSize, OPTIONAL
     OUT LARGE_MANAGED_BUFFER      *THCurr
  )
{
  UINT8                         CertBufferHash[MAX_HASH_SIZE];
  UINT8                         MutCertBufferHash[MAX_HASH_SIZE];
  UINT32                        HashSize;
  RETURN_STATUS                 Status;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  InitManagedBuffer (THCurr, MAX_SPDM_MESSAGE_BUFFER_SIZE);

  DEBUG((DEBUG_INFO, "MessageA Data :\n"));
  InternalDumpHex (GetManagedBuffer(&SpdmContext->Transcript.MessageA), GetManagedBufferSize(&SpdmContext->Transcript.MessageA));
  Status = AppendManagedBuffer (THCurr, GetManagedBuffer(&SpdmContext->Transcript.MessageA), GetManagedBufferSize(&SpdmContext->Transcript.MessageA));
  if (RETURN_ERROR(Status)) {
    return FALSE;
  }

  if (CertBuffer != NULL) {
    DEBUG((DEBUG_INFO, "THMessageCt Data :\n"));
    InternalDumpHex (CertBuffer, CertBufferSize);
    SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, CertBuffer, CertBufferSize, CertBufferHash);
    Status = AppendManagedBuffer (THCurr, CertBufferHash, HashSize);
    if (RETURN_ERROR(Status)) {
      return FALSE;
    }
  }

  DEBUG((DEBUG_INFO, "MessageK Data :\n"));
  InternalDumpHex (GetManagedBuffer(&SessionInfo->SessionTranscript.MessageK), GetManagedBufferSize(&SessionInfo->SessionTranscript.MessageK));
  Status = AppendManagedBuffer (THCurr, GetManagedBuffer(&SessionInfo->SessionTranscript.MessageK), GetManagedBufferSize(&SessionInfo->SessionTranscript.MessageK));
  if (RETURN_ERROR(Status)) {
    return FALSE;
  }

  if (MutCertBuffer != NULL) {
    DEBUG((DEBUG_INFO, "THMessageCM Data :\n"));
    InternalDumpHex (MutCertBuffer, MutCertBufferSize);
    SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, MutCertBuffer, MutCertBufferSize, MutCertBufferHash);
    Status = AppendManagedBuffer (THCurr, MutCertBufferHash, HashSize);
    if (RETURN_ERROR(Status)) {
      return FALSE;
    }
  }

  DEBUG((DEBUG_INFO, "MessageF Data :\n"));
  InternalDumpHex (GetManagedBuffer(&SessionInfo->SessionTranscript.MessageF), GetManagedBufferSize(&SessionInfo->SessionTranscript.MessageF));
  Status = AppendManagedBuffer (THCurr, GetManagedBuffer(&SessionInfo->SessionTranscript.MessageF), GetManagedBufferSize(&SessionInfo->SessionTranscript.MessageF));
  if (RETURN_ERROR(Status)) {
    return FALSE;
  }

  return TRUE;
}

/**
  This function generates the key exchange signature based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  Signature                    The buffer to store the key exchange signature.

  @retval TRUE  key exchange signature is generated.
  @retval FALSE key exchange signature is not generated.
**/
BOOLEAN
SpdmGenerateKeyExchangeRspSignature (
  IN     SPDM_DEVICE_CONTEXT       *SpdmContext,
  IN     SPDM_SESSION_INFO         *SessionInfo,
     OUT UINT8                     *Signature
  )
{
  UINT8                         HashData[MAX_HASH_SIZE];
  UINT8                         *CertBuffer;
  UINTN                         CertBufferSize;
  BOOLEAN                       Result;
  UINTN                         SignatureSize;
  UINT32                        HashSize;
  LARGE_MANAGED_BUFFER          THCurr;

  SignatureSize = GetSpdmAsymSize (SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo);
  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  if ((SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer == NULL) || (SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize == 0)) {
    return FALSE;
  }
  CertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
  CertBufferSize = SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);

  Result = SpdmCalculateTHCurrAK (SpdmContext, SessionInfo, CertBuffer, CertBufferSize, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), HashData);
  DEBUG((DEBUG_INFO, "THCurr Hash - "));
  InternalDumpData (HashData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  Result = SpdmResponderDataSignFunc (
             SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo,
             HashData,
             HashSize,
             Signature,
             &SignatureSize
             );

  return Result;
}

/**
  This function generates the key exchange HMAC based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  Hmac                         The buffer to store the key exchange HMAC.

  @retval TRUE  key exchange HMAC is generated.
  @retval FALSE key exchange HMAC is not generated.
**/
BOOLEAN
SpdmGenerateKeyExchangeRspHmac (
  IN     SPDM_DEVICE_CONTEXT       *SpdmContext,
  IN     SPDM_SESSION_INFO         *SessionInfo,
     OUT UINT8                     *Hmac
  )
{
  UINT8                         HmacData[MAX_HASH_SIZE];
  UINT8                         *CertBuffer;
  UINTN                         CertBufferSize;
  UINT32                        HashSize;
  LARGE_MANAGED_BUFFER          THCurr;
  BOOLEAN                       Result;

  if ((SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer == NULL) || (SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize == 0)) {
    return FALSE;
  }

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  CertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
  CertBufferSize = SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);

  Result = SpdmCalculateTHCurrAK (SpdmContext, SessionInfo, CertBuffer, CertBufferSize, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHmacAllWithResponseFinishedKey (SessionInfo->SecuredMessageContext, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), HmacData);
  DEBUG((DEBUG_INFO, "THCurr Hmac - "));
  InternalDumpData (HmacData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  CopyMem (Hmac, HmacData, HashSize);

  return TRUE;
}

/**
  This function verifies the key exchange signature based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  SignData                     The signature data buffer.
  @param  SignDataSize                 Size in bytes of the signature data buffer.

  @retval TRUE  signature verification pass.
  @retval FALSE signature verification fail.
**/
BOOLEAN
SpdmVerifyKeyExchangeRspSignature (
  IN SPDM_DEVICE_CONTEXT          *SpdmContext,
  IN SPDM_SESSION_INFO            *SessionInfo,
  IN VOID                         *SignData,
  IN INTN                         SignDataSize
  )
{
  UINTN                                     HashSize;
  UINT8                                     HashData[MAX_HASH_SIZE];
  BOOLEAN                                   Result;
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;
  VOID                                      *Context;
  LARGE_MANAGED_BUFFER                      THCurr;
  UINT8                                     *CertChainBuffer;
  UINTN                                     CertChainBufferSize;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  if (SpdmContext->ConnectionInfo.PeerCertChainBufferSize != 0) {
    CertChainBuffer = (UINT8 *)SpdmContext->ConnectionInfo.PeerCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertChainBufferSize = SpdmContext->ConnectionInfo.PeerCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else if (SpdmContext->LocalContext.PeerCertChainProvisionSize != 0) {
    CertChainBuffer = (UINT8 *)SpdmContext->LocalContext.PeerCertChainProvision + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertChainBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else {
    return FALSE;
  }

  Result = SpdmCalculateTHCurrAK (SpdmContext, SessionInfo, CertChainBuffer, CertChainBufferSize, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), HashData);
  DEBUG((DEBUG_INFO, "THCurr Hash - "));
  InternalDumpData (HashData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  //
  // Get leaf cert from cert chain
  //
  Result = X509GetCertFromCertChain (CertChainBuffer, CertChainBufferSize, -1,  &CertBuffer, &CertBufferSize);
  if (!Result) {
    return FALSE;
  }

  Result = SpdmAsymGetPublicKeyFromX509 (SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo, CertBuffer, CertBufferSize, &Context);
  if (!Result) {
    return FALSE;
  }

  Result = SpdmAsymVerify (
             SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo,
             Context,
             HashData,
             HashSize,
             SignData,
             SignDataSize
             );
  SpdmAsymFree (SpdmContext->ConnectionInfo.Algorithm.BaseAsymAlgo, Context);
  if (!Result) {
    DEBUG((DEBUG_INFO, "!!! VerifyKeyExchangeSignature - FAIL !!!\n"));
    return FALSE;
  }
  DEBUG((DEBUG_INFO, "!!! VerifyKeyExchangeSignature - PASS !!!\n"));

  return TRUE;
}

/**
  This function verifies the key exchange HMAC based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  HmacData                     The HMAC data buffer.
  @param  HmacDataSize                 Size in bytes of the HMAC data buffer.

  @retval TRUE  HMAC verification pass.
  @retval FALSE HMAC verification fail.
**/
BOOLEAN
SpdmVerifyKeyExchangeRspHmac (
  IN     SPDM_DEVICE_CONTEXT  *SpdmContext,
  IN     SPDM_SESSION_INFO    *SessionInfo,
  IN     VOID                 *HmacData,
  IN     UINTN                HmacDataSize
  )
{
  UINTN                                     HashSize;
  UINT8                                     CalcHmacData[MAX_HASH_SIZE];
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;
  LARGE_MANAGED_BUFFER                      THCurr;
  BOOLEAN                                   Result;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);
  ASSERT(HashSize == HmacDataSize);

  if (SpdmContext->ConnectionInfo.PeerCertChainBufferSize != 0) {
    CertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.PeerCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertBufferSize = SpdmContext->ConnectionInfo.PeerCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else if (SpdmContext->LocalContext.PeerCertChainProvisionSize != 0) {
    CertBuffer = (UINT8 *)SpdmContext->LocalContext.PeerCertChainProvision + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else {
    return FALSE;
  }

  Result = SpdmCalculateTHCurrAK (SpdmContext, SessionInfo, CertBuffer, CertBufferSize, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHmacAllWithResponseFinishedKey (SessionInfo->SecuredMessageContext, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), CalcHmacData);
  DEBUG((DEBUG_INFO, "THCurr Hmac - "));
  InternalDumpData (CalcHmacData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  if (CompareMem (CalcHmacData, HmacData, HashSize) != 0) {
    DEBUG((DEBUG_INFO, "!!! VerifyKeyExchangeHmac - FAIL !!!\n"));
    return FALSE;
  }
  DEBUG((DEBUG_INFO, "!!! VerifyKeyExchangeHmac - PASS !!!\n"));

  return TRUE;
}

/**
  This function generates the finish signature based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  Signature                    The buffer to store the finish signature.

  @retval TRUE  finish signature is generated.
  @retval FALSE finish signature is not generated.
**/
BOOLEAN
SpdmGenerateFinishReqSignature (
  IN     SPDM_DEVICE_CONTEXT       *SpdmContext,
  IN     SPDM_SESSION_INFO         *SessionInfo,
     OUT UINT8                     *Signature
  )
{
  UINT8                         HashData[MAX_HASH_SIZE];
  UINT8                         *CertBuffer;
  UINTN                         CertBufferSize;
  UINT8                         *MutCertBuffer;
  UINTN                         MutCertBufferSize;
  BOOLEAN                       Result;
  UINTN                         SignatureSize;
  UINT32                        HashSize;
  LARGE_MANAGED_BUFFER          THCurr;

  if ((SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer == NULL) || (SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize == 0)) {
    return FALSE;
  }

  SignatureSize = GetSpdmReqAsymSize (SpdmContext->ConnectionInfo.Algorithm.ReqBaseAsymAlg);
  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  if (SpdmContext->ConnectionInfo.PeerCertChainBufferSize != 0) {
    CertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.PeerCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertBufferSize = SpdmContext->ConnectionInfo.PeerCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else if (SpdmContext->LocalContext.PeerCertChainProvisionSize != 0) {
    CertBuffer = (UINT8 *)SpdmContext->LocalContext.PeerCertChainProvision + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else {
    return FALSE;
  }

  MutCertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
  MutCertBufferSize = SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);

  Result = SpdmCalculateTHCurrAKF (SpdmContext, SessionInfo, CertBuffer, CertBufferSize, MutCertBuffer, MutCertBufferSize, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), HashData);
  DEBUG((DEBUG_INFO, "THCurr Hash - "));
  InternalDumpData (HashData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  Result = SpdmRequesterDataSignFunc (
             SpdmContext->ConnectionInfo.Algorithm.ReqBaseAsymAlg,
             HashData,
             HashSize,
             Signature,
             &SignatureSize
             );

  return Result;
}

/**
  This function generates the finish HMAC based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  Hmac                         The buffer to store the finish HMAC.

  @retval TRUE  finish HMAC is generated.
  @retval FALSE finish HMAC is not generated.
**/
BOOLEAN
SpdmGenerateFinishReqHmac (
  IN     SPDM_DEVICE_CONTEXT  *SpdmContext,
  IN     SPDM_SESSION_INFO    *SessionInfo,
     OUT VOID                 *Hmac
  )
{
  UINTN                                     HashSize;
  UINT8                                     CalcHmacData[MAX_HASH_SIZE];
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;
  UINT8                                     *MutCertBuffer;
  UINTN                                     MutCertBufferSize;
  LARGE_MANAGED_BUFFER                      THCurr;
  BOOLEAN                                   Result;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  if (SpdmContext->ConnectionInfo.PeerCertChainBufferSize != 0) {
    CertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.PeerCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertBufferSize = SpdmContext->ConnectionInfo.PeerCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else if (SpdmContext->LocalContext.PeerCertChainProvisionSize != 0) {
    CertBuffer = (UINT8 *)SpdmContext->LocalContext.PeerCertChainProvision + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else {
    return FALSE;
  }

  if (SessionInfo->MutAuthRequested) {
    if ((SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer == NULL) || (SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize == 0)) {
      return FALSE;
    }
    MutCertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
    MutCertBufferSize = SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else {
    MutCertBuffer = NULL;
    MutCertBufferSize = 0;
  }

  Result = SpdmCalculateTHCurrAKF (SpdmContext, SessionInfo, CertBuffer, CertBufferSize, MutCertBuffer, MutCertBufferSize, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHmacAllWithRequestFinishedKey (SessionInfo->SecuredMessageContext, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), CalcHmacData);
  DEBUG((DEBUG_INFO, "THCurr Hmac - "));
  InternalDumpData (CalcHmacData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  CopyMem (Hmac, CalcHmacData, HashSize);

  return TRUE;
}

/**
  This function verifies the finish signature based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  SignData                     The signature data buffer.
  @param  SignDataSize                 Size in bytes of the signature data buffer.

  @retval TRUE  signature verification pass.
  @retval FALSE signature verification fail.
**/
BOOLEAN
SpdmVerifyFinishReqSignature (
  IN SPDM_DEVICE_CONTEXT          *SpdmContext,
  IN SPDM_SESSION_INFO            *SessionInfo,
  IN VOID                         *SignData,
  IN INTN                         SignDataSize
  )
{
  UINTN                                     HashSize;
  UINT8                                     HashData[MAX_HASH_SIZE];
  BOOLEAN                                   Result;
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;
  UINT8                                     *MutCertBuffer;
  UINTN                                     MutCertBufferSize;
  UINT8                                     *MutCertChainBuffer;
  UINTN                                     MutCertChainBufferSize;
  VOID                                      *Context;
  LARGE_MANAGED_BUFFER                      THCurr;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  if ((SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer == NULL) || (SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize == 0)) {
    return FALSE;
  }
  CertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
  CertBufferSize = SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);

  if (SpdmContext->ConnectionInfo.PeerCertChainBufferSize != 0) {
    MutCertChainBuffer = (UINT8 *)SpdmContext->ConnectionInfo.PeerCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
    MutCertChainBufferSize = SpdmContext->ConnectionInfo.PeerCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else if (SpdmContext->LocalContext.PeerCertChainProvisionSize != 0) {
    MutCertChainBuffer = (UINT8 *)SpdmContext->LocalContext.PeerCertChainProvision + sizeof(SPDM_CERT_CHAIN) + HashSize;
    MutCertChainBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else {
    return FALSE;
  }

  Result = SpdmCalculateTHCurrAKF (SpdmContext, SessionInfo, CertBuffer, CertBufferSize, MutCertChainBuffer, MutCertChainBufferSize, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHashAll (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), HashData);
  DEBUG((DEBUG_INFO, "THCurr Hash - "));
  InternalDumpData (HashData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  //
  // Get leaf cert from cert chain
  //
  Result = X509GetCertFromCertChain (MutCertChainBuffer, MutCertChainBufferSize, -1,  &MutCertBuffer, &MutCertBufferSize);
  if (!Result) {
    return FALSE;
  }

  Result = SpdmReqAsymGetPublicKeyFromX509 (SpdmContext->ConnectionInfo.Algorithm.ReqBaseAsymAlg, MutCertBuffer, MutCertBufferSize, &Context);
  if (!Result) {
    return FALSE;
  }

  Result = SpdmReqAsymVerify (
             SpdmContext->ConnectionInfo.Algorithm.ReqBaseAsymAlg,
             Context,
             HashData,
             HashSize,
             SignData,
             SignDataSize
             );
  SpdmReqAsymFree (SpdmContext->ConnectionInfo.Algorithm.ReqBaseAsymAlg, Context);
  if (!Result) {
    DEBUG((DEBUG_INFO, "!!! VerifyFinishSignature - FAIL !!!\n"));
    return FALSE;
  }
  DEBUG((DEBUG_INFO, "!!! VerifyFinishSignature - PASS !!!\n"));

  return TRUE;
}

/**
  This function verifies the finish HMAC based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  HmacData                     The HMAC data buffer.
  @param  HmacDataSize                 Size in bytes of the HMAC data buffer.

  @retval TRUE  HMAC verification pass.
  @retval FALSE HMAC verification fail.
**/
BOOLEAN
SpdmVerifyFinishReqHmac (
  IN  SPDM_DEVICE_CONTEXT  *SpdmContext,
  IN  SPDM_SESSION_INFO    *SessionInfo,
  IN  UINT8                *Hmac,
  IN  UINTN                HmacSize
  )
{
  UINT8                         HmacData[MAX_HASH_SIZE];
  UINT8                         *CertBuffer;
  UINTN                         CertBufferSize;
  UINT8                         *MutCertBuffer;
  UINTN                         MutCertBufferSize;
  UINTN                         HashSize;
  LARGE_MANAGED_BUFFER          THCurr;
  BOOLEAN                       Result;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);
  ASSERT (HmacSize == HashSize);

  if ((SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer == NULL) || (SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize == 0)) {
    return FALSE;
  }
  CertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
  CertBufferSize = SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);

  if (SessionInfo->MutAuthRequested) {
    if (SpdmContext->ConnectionInfo.PeerCertChainBufferSize != 0) {
      MutCertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.PeerCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
      MutCertBufferSize = SpdmContext->ConnectionInfo.PeerCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
    } else if (SpdmContext->LocalContext.PeerCertChainProvisionSize != 0) {
      MutCertBuffer = (UINT8 *)SpdmContext->LocalContext.PeerCertChainProvision + sizeof(SPDM_CERT_CHAIN) + HashSize;
      MutCertBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
    } else {
      return FALSE;
    }
  } else {
    MutCertBuffer = NULL;
    MutCertBufferSize = 0;
  }

  Result = SpdmCalculateTHCurrAKF (SpdmContext, SessionInfo, CertBuffer, CertBufferSize, MutCertBuffer, MutCertBufferSize, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHmacAllWithRequestFinishedKey (SessionInfo->SecuredMessageContext, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), HmacData);
  DEBUG((DEBUG_INFO, "THCurr Hmac - "));
  InternalDumpData (HmacData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  if (CompareMem(Hmac, HmacData, HashSize) != 0) {
    DEBUG((DEBUG_INFO, "!!! VerifyFinishHmac - FAIL !!!\n"));
    return FALSE;
  }
  DEBUG((DEBUG_INFO, "!!! VerifyFinishHmac - PASS !!!\n"));
  return TRUE;
}

/**
  This function generates the finish HMAC based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  Hmac                         The buffer to store the finish HMAC.

  @retval TRUE  finish HMAC is generated.
  @retval FALSE finish HMAC is not generated.
**/
BOOLEAN
SpdmGenerateFinishRspHmac (
  IN     SPDM_DEVICE_CONTEXT       *SpdmContext,
  IN     SPDM_SESSION_INFO         *SessionInfo,
     OUT UINT8                     *Hmac
  )
{
  UINT8                         HmacData[MAX_HASH_SIZE];
  UINT8                         *CertBuffer;
  UINTN                         CertBufferSize;
  UINT8                         *MutCertBuffer;
  UINTN                         MutCertBufferSize;
  UINT32                        HashSize;
  LARGE_MANAGED_BUFFER          THCurr;
  BOOLEAN                       Result;

  if ((SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer == NULL) || (SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize == 0)) {
    return FALSE;
  }

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  CertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
  CertBufferSize = SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);

  if (SessionInfo->MutAuthRequested) {
    if (SpdmContext->ConnectionInfo.PeerCertChainBufferSize != 0) {
      MutCertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.PeerCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
      MutCertBufferSize = SpdmContext->ConnectionInfo.PeerCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
    } else if (SpdmContext->LocalContext.PeerCertChainProvisionSize != 0) {
      MutCertBuffer = (UINT8 *)SpdmContext->LocalContext.PeerCertChainProvision + sizeof(SPDM_CERT_CHAIN) + HashSize;
      MutCertBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
    } else {
      return FALSE;
    }
  } else {
    MutCertBuffer = NULL;
    MutCertBufferSize = 0;
  }

  Result = SpdmCalculateTHCurrAKF (SpdmContext, SessionInfo, CertBuffer, CertBufferSize, MutCertBuffer, MutCertBufferSize, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHmacAllWithResponseFinishedKey (SessionInfo->SecuredMessageContext, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), HmacData);
  DEBUG((DEBUG_INFO, "THCurr Hmac - "));
  InternalDumpData (HmacData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  CopyMem (Hmac, HmacData, HashSize);

  return TRUE;
}

/**
  This function verifies the finish HMAC based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  HmacData                     The HMAC data buffer.
  @param  HmacDataSize                 Size in bytes of the HMAC data buffer.

  @retval TRUE  HMAC verification pass.
  @retval FALSE HMAC verification fail.
**/
BOOLEAN
SpdmVerifyFinishRspHmac (
  IN     SPDM_DEVICE_CONTEXT  *SpdmContext,
  IN     SPDM_SESSION_INFO    *SessionInfo,
  IN     VOID                 *HmacData,
  IN     UINTN                HmacDataSize
  )
{
  UINTN                                     HashSize;
  UINT8                                     CalcHmacData[MAX_HASH_SIZE];
  UINT8                                     *CertBuffer;
  UINTN                                     CertBufferSize;
  UINT8                                     *MutCertBuffer;
  UINTN                                     MutCertBufferSize;
  LARGE_MANAGED_BUFFER                      THCurr;
  BOOLEAN                                   Result;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);
  ASSERT(HashSize == HmacDataSize);

  if (SpdmContext->ConnectionInfo.PeerCertChainBufferSize != 0) {
    CertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.PeerCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertBufferSize = SpdmContext->ConnectionInfo.PeerCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else if (SpdmContext->LocalContext.PeerCertChainProvisionSize != 0) {
    CertBuffer = (UINT8 *)SpdmContext->LocalContext.PeerCertChainProvision + sizeof(SPDM_CERT_CHAIN) + HashSize;
    CertBufferSize = SpdmContext->LocalContext.PeerCertChainProvisionSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else {
    return FALSE;
  }

  if (SessionInfo->MutAuthRequested) {
    if (SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize == 0) {
      return FALSE;
    }
    MutCertBuffer = (UINT8 *)SpdmContext->ConnectionInfo.LocalUsedCertChainBuffer + sizeof(SPDM_CERT_CHAIN) + HashSize;
    MutCertBufferSize = SpdmContext->ConnectionInfo.LocalUsedCertChainBufferSize - (sizeof(SPDM_CERT_CHAIN) + HashSize);
  } else {
    MutCertBuffer = NULL;
    MutCertBufferSize = 0;
  }

  Result = SpdmCalculateTHCurrAKF (SpdmContext, SessionInfo, CertBuffer, CertBufferSize, MutCertBuffer, MutCertBufferSize, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHmacAllWithResponseFinishedKey (SessionInfo->SecuredMessageContext, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), CalcHmacData);
  DEBUG((DEBUG_INFO, "THCurr Hmac - "));
  InternalDumpData (CalcHmacData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  if (CompareMem (CalcHmacData, HmacData, HashSize) != 0) {
    DEBUG((DEBUG_INFO, "!!! VerifyFinishRspHmac - FAIL !!!\n"));
    return FALSE;
  }
  DEBUG((DEBUG_INFO, "!!! VerifyFinishRspHmac - PASS !!!\n"));

  return TRUE;
}

/**
  This function generates the PSK exchange HMAC based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  Hmac                         The buffer to store the PSK exchange HMAC.

  @retval TRUE  PSK exchange HMAC is generated.
  @retval FALSE PSK exchange HMAC is not generated.
**/
BOOLEAN
SpdmGeneratePskExchangeRspHmac (
  IN     SPDM_DEVICE_CONTEXT       *SpdmContext,
  IN     SPDM_SESSION_INFO         *SessionInfo,
     OUT UINT8                     *Hmac
  )
{
  UINT8                         HmacData[MAX_HASH_SIZE];
  UINT32                        HashSize;
  LARGE_MANAGED_BUFFER          THCurr;
  BOOLEAN                       Result;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  Result = SpdmCalculateTHCurrAK (SpdmContext, SessionInfo, NULL, 0, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHmacAllWithResponseFinishedKey (SessionInfo->SecuredMessageContext, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), HmacData);
  DEBUG((DEBUG_INFO, "THCurr Hmac - "));
  InternalDumpData (HmacData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  CopyMem (Hmac, HmacData, HashSize);

  return TRUE;
}

/**
  This function verifies the PSK exchange HMAC based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  HmacData                     The HMAC data buffer.
  @param  HmacDataSize                 Size in bytes of the HMAC data buffer.

  @retval TRUE  HMAC verification pass.
  @retval FALSE HMAC verification fail.
**/
BOOLEAN
SpdmVerifyPskExchangeRspHmac (
  IN     SPDM_DEVICE_CONTEXT  *SpdmContext,
  IN     SPDM_SESSION_INFO    *SessionInfo,
  IN     VOID                 *HmacData,
  IN     UINTN                HmacDataSize
  )
{
  UINTN                                     HashSize;
  UINT8                                     CalcHmacData[MAX_HASH_SIZE];
  LARGE_MANAGED_BUFFER                      THCurr;
  BOOLEAN                                   Result;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);
  ASSERT(HashSize == HmacDataSize);

  Result = SpdmCalculateTHCurrAK (SpdmContext, SessionInfo, NULL, 0, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHmacAllWithResponseFinishedKey (SessionInfo->SecuredMessageContext, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), CalcHmacData);
  DEBUG((DEBUG_INFO, "THCurr Hmac - "));
  InternalDumpData (CalcHmacData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  if (CompareMem (CalcHmacData, HmacData, HashSize) != 0) {
    DEBUG((DEBUG_INFO, "!!! VerifyPskExchangeHmac - FAIL !!!\n"));
    return FALSE;
  }
  DEBUG((DEBUG_INFO, "!!! VerifyPskExchangeHmac - PASS !!!\n"));

  return TRUE;
}

/**
  This function generates the PSK finish HMAC based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  Hmac                         The buffer to store the finish HMAC.

  @retval TRUE  PSK finish HMAC is generated.
  @retval FALSE PSK finish HMAC is not generated.
**/
BOOLEAN
SpdmGeneratePskFinishReqHmac (
  IN     SPDM_DEVICE_CONTEXT          *SpdmContext,
  IN     SPDM_SESSION_INFO            *SessionInfo,
     OUT VOID                         *Hmac
  )
{
  UINTN                                     HashSize;
  UINT8                                     CalcHmacData[MAX_HASH_SIZE];
  LARGE_MANAGED_BUFFER                      THCurr;
  BOOLEAN                                   Result;

  InitManagedBuffer (&THCurr, MAX_SPDM_MESSAGE_BUFFER_SIZE);

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);

  Result = SpdmCalculateTHCurrAKF (SpdmContext, SessionInfo, NULL, 0, NULL, 0, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHmacAllWithRequestFinishedKey (SessionInfo->SecuredMessageContext, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), CalcHmacData);
  DEBUG((DEBUG_INFO, "THCurr Hmac - "));
  InternalDumpData (CalcHmacData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  CopyMem (Hmac, CalcHmacData, HashSize);

  return TRUE;
}

/**
  This function verifies the PSK finish HMAC based upon TH.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SessionInfo                  The session info of an SPDM session.
  @param  HmacData                     The HMAC data buffer.
  @param  HmacDataSize                 Size in bytes of the HMAC data buffer.

  @retval TRUE  HMAC verification pass.
  @retval FALSE HMAC verification fail.
**/
BOOLEAN
SpdmVerifyPskFinishReqHmac (
  IN  SPDM_DEVICE_CONTEXT       *SpdmContext,
  IN  SPDM_SESSION_INFO         *SessionInfo,
  IN  UINT8                     *Hmac,
  IN  UINTN                     HmacSize
  )
{
  UINT8                         HmacData[MAX_HASH_SIZE];
  UINT32                        HashSize;
  LARGE_MANAGED_BUFFER          THCurr;
  BOOLEAN                       Result;

  HashSize = GetSpdmHashSize (SpdmContext->ConnectionInfo.Algorithm.BaseHashAlgo);
  ASSERT (HmacSize == HashSize);

  Result = SpdmCalculateTHCurrAKF (SpdmContext, SessionInfo, NULL, 0, NULL, 0, &THCurr);
  if (!Result) {
    return FALSE;
  }

  SpdmHmacAllWithRequestFinishedKey (SessionInfo->SecuredMessageContext, GetManagedBuffer(&THCurr), GetManagedBufferSize(&THCurr), HmacData);
  DEBUG((DEBUG_INFO, "Calc THCurr Hmac - "));
  InternalDumpData (HmacData, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  if (CompareMem(Hmac, HmacData, HashSize) != 0) {
    DEBUG((DEBUG_INFO, "!!! VerifyPskFinishHmac - FAIL !!!\n"));
    return FALSE;
  }
  DEBUG((DEBUG_INFO, "!!! VerifyPskFinishHmac - PASS !!!\n"));
  return TRUE;
}
