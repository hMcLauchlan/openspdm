/** @file
  SPDM common library.
  It follows the SPDM Specification.

Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SpdmRequesterLibInternal.h"

#pragma pack(1)

typedef struct {
  SPDM_MESSAGE_HEADER  Header;
  UINT8                CertChainHash[MAX_HASH_SIZE];
  UINT8                Nonce[SPDM_NONCE_SIZE];
  UINT8                MeasurementSummaryHash[MAX_HASH_SIZE];
  UINT16               OpaqueLength;
  UINT8                OpaqueData[MAX_SPDM_OPAQUE_DATA_SIZE];
  UINT8                Signature[MAX_ASYM_KEY_SIZE];
} SPDM_CHALLENGE_AUTH_RESPONSE_MAX;

#pragma pack()

/**
  This function sends CHALLENGE
  to authenticate the device based upon the key in one slot.

  This function verifies the signature in the challenge auth.

  If basic mutual authentication is requested from the responder,
  this function also perform the basic mutual authentication.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SlotNum                      The number of slot for the challenge.
  @param  MeasurementHashType          The type of the measurement hash.
  @param  MeasurementHash              A pointer to a destination buffer to store the measurement hash.

  @retval RETURN_SUCCESS               The challenge auth is got successfully.
  @retval RETURN_DEVICE_ERROR          A device error occurs when communicates with the device.
  @retval RETURN_SECURITY_VIOLATION    Any verification fails.
**/
RETURN_STATUS
TrySpdmChallenge (
  IN     VOID                 *Context,
  IN     UINT8                SlotNum,
  IN     UINT8                MeasurementHashType,
     OUT VOID                 *MeasurementHash
  )
{
  RETURN_STATUS                             Status;
  BOOLEAN                                   Result;
  SPDM_CHALLENGE_REQUEST                    SpdmRequest;
  SPDM_CHALLENGE_AUTH_RESPONSE_MAX          SpdmResponse;
  UINTN                                     SpdmResponseSize;
  UINT8                                     *Ptr;
  VOID                                      *CertChainHash;
  UINTN                                     HashSize;
  VOID                                      *ServerNonce;
  VOID                                      *MeasurementSummaryHash;
  UINT16                                    OpaqueLength;
  VOID                                      *Opaque;
  VOID                                      *Signature;
  UINTN                                     SignatureSize;
  SPDM_DEVICE_CONTEXT                       *SpdmContext;
  SPDM_CHALLENGE_AUTH_RESPONSE_ATTRIBUTE    AuthAttribute;

  SpdmContext = Context;
  if (((SpdmContext->SpdmCmdReceiveState & SPDM_NEGOTIATE_ALGORITHMS_RECEIVE_FLAG) == 0) ||
      ((SpdmContext->SpdmCmdReceiveState & SPDM_GET_CAPABILITIES_RECEIVE_FLAG) == 0) ||
      ((SpdmContext->SpdmCmdReceiveState & SPDM_GET_DIGESTS_RECEIVE_FLAG) == 0)) {
    return RETURN_DEVICE_ERROR;
  }
  if ((SpdmContext->ConnectionInfo.Capability.Flags & SPDM_GET_CAPABILITIES_RESPONSE_FLAGS_CHAL_CAP) == 0) {
    return RETURN_DEVICE_ERROR;
  }

  if ((SlotNum >= MAX_SPDM_SLOT_COUNT) && (SlotNum != 0xFF)) {
    return RETURN_INVALID_PARAMETER;
  }
  if ((SlotNum == 0xFF) && (SpdmContext->LocalContext.PeerCertChainProvisionSize == 0)) {
    return RETURN_INVALID_PARAMETER;
  }

  SpdmContext->ErrorState = SPDM_STATUS_ERROR_DEVICE_NO_CAPABILITIES;

  if (SpdmIsVersionSupported (SpdmContext, SPDM_MESSAGE_VERSION_11)) {
    SpdmRequest.Header.SPDMVersion = SPDM_MESSAGE_VERSION_11;
  } else {
    SpdmRequest.Header.SPDMVersion = SPDM_MESSAGE_VERSION_10;
  }
  SpdmRequest.Header.RequestResponseCode = SPDM_CHALLENGE;
  SpdmRequest.Header.Param1 = SlotNum;
  SpdmRequest.Header.Param2 = MeasurementHashType;
  SpdmGetRandomNumber (SPDM_NONCE_SIZE, SpdmRequest.Nonce);
  DEBUG((DEBUG_INFO, "ClientNonce - "));
  InternalDumpData (SpdmRequest.Nonce, SPDM_NONCE_SIZE);
  DEBUG((DEBUG_INFO, "\n"));
  Status = SpdmSendSpdmRequest (SpdmContext, NULL, sizeof(SpdmRequest), &SpdmRequest);
  if (RETURN_ERROR(Status)) {
    return RETURN_DEVICE_ERROR;
  }

  //
  // Cache data
  //
  AppendManagedBuffer (&SpdmContext->Transcript.MessageC, &SpdmRequest, sizeof(SpdmRequest));

  SpdmResponseSize = sizeof(SpdmResponse);
  ZeroMem (&SpdmResponse, sizeof(SpdmResponse));
  Status = SpdmReceiveSpdmResponse (SpdmContext, NULL, &SpdmResponseSize, &SpdmResponse);
  if (RETURN_ERROR(Status)) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponseSize < sizeof(SPDM_MESSAGE_HEADER)) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponse.Header.RequestResponseCode == SPDM_ERROR) {
    Status = SpdmHandleErrorResponseMain(SpdmContext, NULL, &SpdmContext->Transcript.MessageC, sizeof(SpdmRequest), &SpdmResponseSize, &SpdmResponse, SPDM_CHALLENGE, SPDM_CHALLENGE_AUTH, sizeof(SPDM_CHALLENGE_AUTH_RESPONSE_MAX));
    if (RETURN_ERROR(Status)) {
      return Status;
    }
  } else if (SpdmResponse.Header.RequestResponseCode != SPDM_CHALLENGE_AUTH) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponseSize < sizeof(SPDM_CHALLENGE_AUTH_RESPONSE)) {
    return RETURN_DEVICE_ERROR;
  }
  if (SpdmResponseSize > sizeof(SpdmResponse)) {
    return RETURN_DEVICE_ERROR;
  }
  *(UINT8 *)&AuthAttribute = SpdmResponse.Header.Param1;
  if (SlotNum == 0xFF) {
    if (AuthAttribute.SlotNum != 0xF) {
      return RETURN_DEVICE_ERROR;
    }
    if (SpdmResponse.Header.Param2 != 0) {
      return RETURN_DEVICE_ERROR;
    }
  } else {
    if (AuthAttribute.SlotNum != SlotNum) {
      return RETURN_DEVICE_ERROR;
    }
    if (SpdmResponse.Header.Param2 != (1 << SlotNum)) {
      return RETURN_DEVICE_ERROR;
    }
  }
  if (AuthAttribute.BasicMutAuthReq == 1) {
    if ((SpdmContext->ConnectionInfo.Capability.Flags & SPDM_GET_CAPABILITIES_REQUEST_FLAGS_MUT_AUTH_CAP) == 0) {
      return RETURN_DEVICE_ERROR;
    }
  }
  HashSize = GetSpdmHashSize (SpdmContext);
  SignatureSize = GetSpdmAsymSize (SpdmContext);

  if (SpdmResponseSize <= sizeof(SPDM_CHALLENGE_AUTH_RESPONSE) +
                          HashSize +
                          SPDM_NONCE_SIZE +
                          HashSize +
                          sizeof(UINT16)) {
    return RETURN_DEVICE_ERROR;
  }

  Ptr = SpdmResponse.CertChainHash;

  CertChainHash = Ptr;
  Ptr += HashSize;
  DEBUG((DEBUG_INFO, "CertChainHash (0x%x) - ", HashSize));
  InternalDumpData (CertChainHash, HashSize);
  DEBUG((DEBUG_INFO, "\n"));
  Result = SpdmVerifyCertificateChainHash (SpdmContext, CertChainHash, HashSize);
  if (!Result) {
    SpdmContext->ErrorState = SPDM_STATUS_ERROR_CERTIFICATE_FAILURE;
    return RETURN_SECURITY_VIOLATION;
  }

  ServerNonce = Ptr;
  DEBUG((DEBUG_INFO, "ServerNonce (0x%x) - ", SPDM_NONCE_SIZE));
  InternalDumpData (ServerNonce, SPDM_NONCE_SIZE);
  DEBUG((DEBUG_INFO, "\n"));
  Ptr += SPDM_NONCE_SIZE;

  MeasurementSummaryHash = Ptr;
  Ptr += HashSize;
  DEBUG((DEBUG_INFO, "MeasurementSummaryHash (0x%x) - ", HashSize));
  InternalDumpData (MeasurementSummaryHash, HashSize);
  DEBUG((DEBUG_INFO, "\n"));

  OpaqueLength = *(UINT16 *)Ptr;
  Ptr += sizeof(UINT16);

  if (SpdmResponseSize < sizeof(SPDM_CHALLENGE_AUTH_RESPONSE) +
                         HashSize +
                         SPDM_NONCE_SIZE +
                         HashSize +
                         sizeof(UINT16) +
                         OpaqueLength +
                         SignatureSize) {
    return RETURN_DEVICE_ERROR;
  }
  SpdmResponseSize = sizeof(SPDM_CHALLENGE_AUTH_RESPONSE) +
                     HashSize +
                     SPDM_NONCE_SIZE +
                     HashSize +
                     sizeof(UINT16) +
                     OpaqueLength +
                     SignatureSize;
  AppendManagedBuffer (&SpdmContext->Transcript.MessageC, &SpdmResponse, SpdmResponseSize - SignatureSize);
  AppendManagedBuffer (&SpdmContext->Transcript.M1M2, GetManagedBuffer(&SpdmContext->Transcript.MessageA), GetManagedBufferSize(&SpdmContext->Transcript.MessageA));
  AppendManagedBuffer (&SpdmContext->Transcript.M1M2, GetManagedBuffer(&SpdmContext->Transcript.MessageB), GetManagedBufferSize(&SpdmContext->Transcript.MessageB));
  AppendManagedBuffer (&SpdmContext->Transcript.M1M2, GetManagedBuffer(&SpdmContext->Transcript.MessageC), GetManagedBufferSize(&SpdmContext->Transcript.MessageC));

  Opaque = Ptr;
  Ptr += OpaqueLength;
  DEBUG((DEBUG_INFO, "Opaque (0x%x):\n", OpaqueLength));
  InternalDumpHex (Opaque, OpaqueLength);

  Signature = Ptr;
  DEBUG((DEBUG_INFO, "Signature (0x%x):\n", SignatureSize));
  InternalDumpHex (Signature, SignatureSize);
  Result = SpdmVerifyChallengeAuthSignature (SpdmContext, TRUE, Signature, SignatureSize);
  if (!Result) {
    SpdmContext->ErrorState = SPDM_STATUS_ERROR_CERTIFICATE_FAILURE;
    return RETURN_SECURITY_VIOLATION;
  }

  SpdmContext->ErrorState = SPDM_STATUS_SUCCESS;

  ResetManagedBuffer (&SpdmContext->Transcript.M1M2);

  if (MeasurementHash != NULL) {
    CopyMem (MeasurementHash, MeasurementSummaryHash, HashSize);
  }

  if (AuthAttribute.BasicMutAuthReq == 1) {
    DEBUG((DEBUG_INFO, "BasicMutAuth :\n"));
    Status = SpdmEncapsulatedRequest (SpdmContext, NULL, 0, NULL);
    DEBUG ((DEBUG_INFO, "SpdmChallenge - SpdmEncapsulatedRequest - %p\n", Status));
    if (RETURN_ERROR(Status)) {
      SpdmContext->ErrorState = SPDM_STATUS_ERROR_CERTIFICATE_FAILURE;
      return RETURN_SECURITY_VIOLATION;
    }
  }

  SpdmContext->SpdmCmdReceiveState |= SPDM_CHALLENGE_RECEIVE_FLAG;
  SpdmContext->ConnectionInfo.ConnectionState = SpdmConnectionStateAuthenticated;

  return RETURN_SUCCESS;
}

/**
  This function sends CHALLENGE
  to authenticate the device based upon the key in one slot.

  This function verifies the signature in the challenge auth.

  If basic mutual authentication is requested from the responder,
  this function also perform the basic mutual authentication.

  @param  SpdmContext                  A pointer to the SPDM context.
  @param  SlotNum                      The number of slot for the challenge.
  @param  MeasurementHashType          The type of the measurement hash.
  @param  MeasurementHash              A pointer to a destination buffer to store the measurement hash.

  @retval RETURN_SUCCESS               The challenge auth is got successfully.
  @retval RETURN_DEVICE_ERROR          A device error occurs when communicates with the device.
  @retval RETURN_SECURITY_VIOLATION    Any verification fails.
**/
RETURN_STATUS
EFIAPI
SpdmChallenge (
  IN     VOID                 *Context,
  IN     UINT8                SlotNum,
  IN     UINT8                MeasurementHashType,
     OUT VOID                 *MeasurementHash
  )
{
  SPDM_DEVICE_CONTEXT    *SpdmContext;
  UINTN                   Retry;
  RETURN_STATUS           Status;

  SpdmContext = Context;
  Retry = SpdmContext->RetryTimes;
  do {
    Status = TrySpdmChallenge(SpdmContext, SlotNum, MeasurementHashType, MeasurementHash);
    if (RETURN_NO_RESPONSE != Status) {
      return Status;
    }
  } while (Retry-- != 0);

  return Status;
}

