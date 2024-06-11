// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "proto_processing_lib/proto_scrubber/constants.h"

#include <cstdint>

#include "absl/base/attributes.h"

namespace proto_processing_lib::proto_scrubber {

ABSL_CONST_INIT const char* const kTypeServiceBaseUrl = "type.googleapis.com";

ABSL_CONST_INIT const char* const kGoogleProdServiceBaseUrl =
    "type.googleprod.com";

ABSL_CONST_INIT const char* const kOptionLegacyTokenUnit =
    "proto2.MethodOptions.legacy_token_unit";

ABSL_CONST_INIT const char* const kOptionLegacyClientInitialTokens =
    "proto2.MethodOptions.legacy_client_initial_tokens";

ABSL_CONST_INIT const char* const kOptionLegacyServerInitialTokens =
    "proto2.MethodOptions.legacy_server_initial_tokens";

ABSL_CONST_INIT const char* const kOptionEnvelopeStream =
    "envelope.framework.option.is_stream";

ABSL_CONST_INIT const char* const kOptionStreamType =
    "proto2.MethodOptions.stream_type";

ABSL_CONST_INIT const char* const kOptionLegacyStreamType =
    "proto2.MethodOptions.legacy_stream_type";

ABSL_CONST_INIT const char* const kOptionLegacyResultType =
    "proto2.MethodOptions.legacy_result_type";

ABSL_CONST_INIT const char* const kMigrationOptionFieldOptionName =
    "google.api.field_migration";

ABSL_CONST_INIT const char* const kOperationType =
    "google.longrunning.Operation";

ABSL_CONST_INIT const char* const kStructType = "google.protobuf.Struct";

ABSL_CONST_INIT const char* const kStructFieldsEntryType =
    "google.protobuf.Struct.FieldsEntry";

ABSL_CONST_INIT const char* const kStructValueType = "google.protobuf.Value";

ABSL_CONST_INIT const char* const kStructListValueType =
    "google.protobuf.ListValue";

ABSL_CONST_INIT const char* const kStructTypeUrl =
    "type.googleapis.com/google.protobuf.Struct";

ABSL_CONST_INIT const char* const kStructValueTypeUrl =
    "type.googleapis.com/google.protobuf.Value";

ABSL_CONST_INIT const char* const kStructListValueTypeUrl =
    "type.googleapis.com/google.protobuf.ListValue";

ABSL_CONST_INIT const char* const kStringValueTypeUrl =
    "type.googleapis.com/google.protobuf.StringValue";

ABSL_CONST_INIT const char* const kAnyType = "google.protobuf.Any";

ABSL_CONST_INIT const char* const kProtoMapEntryName =
    "proto2.MessageOptions.map_entry";
ABSL_CONST_INIT const char* const kProtoMapValueFieldName = "value";

ABSL_CONST_INIT const char* const kDiscoveryServiceName =
    "discovery.googleapis.com";
ABSL_CONST_INIT const char* const kDiscoveryApiName =
    "google.discovery.Discovery";
ABSL_CONST_INIT const char* const kDiscoveryApiPrefix = "google.discovery.";
ABSL_CONST_INIT const char* const kLegacyDiscoveryUrlPathPrefix =
    "/discovery/v1/apis/";

ABSL_CONST_INIT const char* const kDiscoveryUdsPath =
    "unix:/tmp/google.discovery.uds";

ABSL_CONST_INIT const char* const kESFSidecarUdsPath =
    "unix:/tmp/google.esfsidecar.uds";

ABSL_CONST_INIT const char* const kHttpBodyTypeUrl =
    "type.googleapis.com/google.api.HttpBody";

ABSL_CONST_INIT const char* const kHttpBodyName = "google.api.HttpBody";

ABSL_CONST_INIT const char* const kQueryParamPrettyPrint1 = "$prettyPrint";
ABSL_CONST_INIT const char* const kQueryParamPrettyPrint2 = "$prettyprint";
ABSL_CONST_INIT const char* const kQueryParamPrettyPrint3 = "$pp";
ABSL_CONST_INIT const char* const kQueryParamPrettyPrint4 = "prettyPrint";
ABSL_CONST_INIT const char* const kQueryParamPrettyPrint5 = "pp";
ABSL_CONST_INIT const char* const kQueryParamPrettyPrint6 = "prettyprint";

ABSL_CONST_INIT const char* const kQueryParamUnique = "$unique";

ABSL_CONST_INIT const char* const kQueryParameterTrace = "$trace";
// To match Apiary.
ABSL_CONST_INIT const char* const kQueryParameterTrace2 = "trace";

ABSL_CONST_INIT const char* const kTraceHttpResponseHeader = "X-GOOG-TRACE-ID";

ABSL_CONST_INIT const char* const kFieldMaskParameter1 = "$fields";
ABSL_CONST_INIT const char* const kFieldMaskParameter2 = "fields";

ABSL_CONST_INIT const char* const kQueryParameterKey1 = "$key";
ABSL_CONST_INIT const char* const kQueryParameterKey2 = "key";
ABSL_CONST_INIT const char* const kQueryParameterKeyRegex = "\\$?key";

ABSL_CONST_INIT const char* const kQueryParameterCallback1 = "$callback";
ABSL_CONST_INIT const char* const kQueryParameterCallback2 = "callback";

ABSL_CONST_INIT const char* const kQueryParameterErrorFormat1 = "$.xgafv";
ABSL_CONST_INIT const char* const kQueryParameterErrorFormat2 = "$apiFormat";
ABSL_CONST_INIT const char* const kQueryParameterErrorFormat3 = ".xgafv";

ABSL_CONST_INIT const char* const kQueryParameterQuotaUser1 = "$quotaUser";
ABSL_CONST_INIT const char* const kQueryParameterQuotaUser2 = "quotaUser";

ABSL_CONST_INIT const char* const kQueryParameterAlt1 = "$alt";
ABSL_CONST_INIT const char* const kQueryParameterAlt2 = "alt";

ABSL_CONST_INIT const char* const kQueryParameterAccessToken = "access_token";
ABSL_CONST_INIT const char* const kQueryParameterOauthToken = "oauth_token";
ABSL_CONST_INIT const char* const kQueryParameterBearerToken = "bearer_token";
ABSL_CONST_INIT const char* const kQueryParameterTokenRegex = ".*token";
ABSL_CONST_INIT const char* const kQueryParameterXOauth = "xoauth";

ABSL_CONST_INIT const char* const kQueryParameterUploadProtocol1 =
    "$upload_protocol";
ABSL_CONST_INIT const char* const kQueryParameterUploadProtocol2 =
    "upload_protocol";

ABSL_CONST_INIT const char* const kQueryParameterUploadType1 = "upload_type";
ABSL_CONST_INIT const char* const kQueryParameterUploadType2 = "uploadType";

ABSL_CONST_INIT const char* const kDataAccessReasonKey =
    "x-google-dataaccessreason-bin";
ABSL_CONST_INIT const char* const kIamAuthorizationTokenKey =
    "x-goog-iam-authorization-token";
ABSL_CONST_INIT const char* const kEndUserCredsKey =
    "x-google-endusercreds-bin";

ABSL_CONST_INIT const char* const kQueryParameterUserIp1 = "$userIp";
ABSL_CONST_INIT const char* const kQueryParameterUserIp2 = "userIp";
ABSL_CONST_INIT const char* const kQueryParameterUserIpRegex = "\\$?userIp";

ABSL_CONST_INIT const char* const kPasswordRegex = "passw(or)?d";

ABSL_CONST_INIT const char* const kQueryParamWebChannelAid = "AID";
ABSL_CONST_INIT const char* const kQueryParamWebChannelCi = "CI";
ABSL_CONST_INIT const char* const kQueryParamWebChannelTo = "TO";
ABSL_CONST_INIT const char* const kQueryParamWebChannelRid = "RID";
ABSL_CONST_INIT const char* const kQueryParamWebChannelSid = "SID";
ABSL_CONST_INIT const char* const kQueryParamWebChannelType = "TYPE";
ABSL_CONST_INIT const char* const kQueryParamWebChannelVer = "VER";
ABSL_CONST_INIT const char* const kQueryParamWebChannelCver = "CVER";
ABSL_CONST_INIT const char* const kQueryParamWebChannelModel = "MODEL";
ABSL_CONST_INIT const char* const kQueryParamWebChannelRetryId = "t";
ABSL_CONST_INIT const char* const kQueryParamWebChannelOriginTrial = "ot";
ABSL_CONST_INIT const char* const kQueryParamGSessionId = "gsessionid";
ABSL_CONST_INIT const char* const kQueryParamHttpSessionId =
    "X-HTTP-Session-Id";

ABSL_CONST_INIT const char* const kQueryParamJsRandom = "zx";

ABSL_CONST_INIT const char* const kQueryParamHttpMethod = "$httpMethod";

ABSL_CONST_INIT const char* const kQueryParamHttpHeaders = "$httpHeaders";

ABSL_CONST_INIT const char* const kQueryParamContentType = "$ct";

ABSL_CONST_INIT const char* const kQueryParamReq = "$req";
ABSL_CONST_INIT const char* const kQueryParamReqRegex = "\\$req";

ABSL_CONST_INIT const char* const kQueryParamOutputDefaults = "$outputDefaults";

ABSL_CONST_INIT const char* const kQueryParamVisibilities = "$visibilities";

ABSL_CONST_INIT const char* const kQueryParamApiVersion = "$apiVersion";

ABSL_CONST_INIT const char* const kQueryParamUserProject = "$userProject";

ABSL_CONST_INIT const char* const kQueryParamGoogleCloudResourcePrefix =
    "google_cloud_resource_prefix";

ABSL_CONST_INIT const char* const kStubby4WrapperPrefix = "stubby4:";

ABSL_CONST_INIT const char* const kFormUrlEncodedContentType =
    "application/x-www-form-urlencoded";

ABSL_CONST_INIT const char* const kProtobufContentType =
    "application/x-protobuf";

ABSL_CONST_INIT const char* const kJSPBContentType =
    "application/json+protobuf";

ABSL_CONST_INIT const char* const kJSPBGWTContentType =
    "application/protobuf+json; kind=gwt";

ABSL_CONST_INIT const char* const kJSPBFAVAContentType =
    "application/protobuf+json; kind=fava";

ABSL_CONST_INIT const char* const kJSONContentType = "application/json";

ABSL_CONST_INIT const char* const kMultipartFormDataContentType =
    "multipart/form-data";

ABSL_CONST_INIT const char* const kMultipartMixedContentType =
    "multipart/mixed";

ABSL_CONST_INIT const char* const kContentIdKey = "Content-ID";

ABSL_CONST_INIT const char* const kMediaMethodPrefix = "google.bytestream";

ABSL_CONST_INIT const char* const kFieldMaskContext =
    "google.rpc.context.FieldMaskContext";

ABSL_CONST_INIT const char* const kGrpcApiKeyHeader = "X-Goog-Api-Key";

ABSL_CONST_INIT const char* const kGrpcFieldMaskHeader = "X-Goog-FieldMask";

ABSL_CONST_INIT const char* const kGrpcVisibilitiesHeader =
    "X-Goog-Visibilities";

ABSL_CONST_INIT const char* const kGrpcApiVersionHeader = "X-Goog-Api-Version";

ABSL_CONST_INIT const char* const kGrpcQuotaUserHeader = "X-Goog-Quota-User";

// TODO(wenboz): once the spec is reviewed and finalized, move this constant to
// google3 base
ABSL_CONST_INIT const char* const kResponseCompressionHeader =
    "x-response-encoding";

ABSL_CONST_INIT const char* const kHttpMethodOverrideHeader =
    "X-HTTP-Method-Override";

ABSL_CONST_INIT const char* const kApiClientHeader = "X-Goog-Api-Client";

ABSL_CONST_INIT const char* const kUserProject = "X-Goog-User-Project";

ABSL_CONST_INIT const char* const kRequestReasonHeader =
    "X-Goog-Request-Reason";

ABSL_CONST_INIT const char* const kXAndroidPackage = "X-Android-Package";

ABSL_CONST_INIT const char* const kXAndroidCert = "X-Android-Cert";

ABSL_CONST_INIT const char* const kXIosBundleId = "X-Ios-Bundle-Identifier";

ABSL_CONST_INIT const char* const kXGoogSpatulaHeader = "X-Goog-Spatula";

ABSL_CONST_INIT const char* const kXGoogPageIdHeaderName = "X-Goog-PageId";

// LINT.IfChange
ABSL_CONST_INIT const char* const kGrpcSideChannelHeaderPrefix = "x-goog-ext-";
// LINT.ThenChange(//depot/google3/java/com/google/frameworks/client/data/sidechannel/FrontendRequestHeaders.java)

ABSL_CONST_INIT const char* const kProductCuiHeader =
    "x-goog-ext-353267353-bin";

ABSL_CONST_INIT const char* const kJSPBProductCuiHeader =
    "x-goog-ext-353267353-jspb";

ABSL_CONST_INIT const char* const kRequestQosHeader =
    "x-goog-ext-174067345-bin";

ABSL_CONST_INIT const char* const kGoogleCloudResourcePrefixHeader =
    "google-cloud-resource-prefix";

ABSL_CONST_INIT const char* const kXGoogleAuthUserHeader = "x-goog-authuser";

ABSL_CONST_INIT const char* const kXGoogFirstPartyReauthHeader =
    "X-Goog-First-Party-Reauth";

ABSL_CONST_INIT const char* const kXGoogRequestParams = "x-goog-request-params";

ABSL_CONST_INIT const char* const kXGoogleRlsData = "x-google-rls-data";

ABSL_CONST_INIT const char* const kGrpcWebUrlPrefix = "/$rpc";
ABSL_CONST_INIT const char* const kGrpcWebUrlPrefixWithSlash = "/$rpc/";
ABSL_CONST_INIT const char* const kGrpcWebApiClient = "grpc-web";

ABSL_CONST_INIT const char* const kXAcceptContentTransferEncoding =
    "X-Accept-Content-Transfer-Encoding";

ABSL_CONST_INIT const char* const kContentTransferEncoding =
    "Content-Transfer-Encoding";

ABSL_CONST_INIT const char* const kXWebchannelContentType =
    "X-WebChannel-Content-Type";

ABSL_CONST_INIT const char* const kUnifiedLoggingService =
    "unified-logging.googleapis.com";

ABSL_CONST_INIT const char* const kLoadSheddingErrorCategory = "Load Shedding";

ABSL_CONST_INIT const char* const kUntrustedPeerErrorCategory =
    "Cloud Virtual Machine IP with MDB based Loas peering";

ABSL_CONST_INIT const char* const kUntrustedDelegationErrorCategory =
    "Untrusted Delegation";

ABSL_CONST_INIT const char* const kGoogEncodeResponseIfExecutable =
    "X-Goog-Encode-Response-If-Executable";
ABSL_CONST_INIT const char* const kGoogSafetyEncoding =
    "X-Goog-Safety-Encoding";
ABSL_CONST_INIT const char* const kBase64Encoding = "base64";

ABSL_CONST_INIT const char* const kGoogSafetyContentType =
    "X-Goog-Safety-Content-Type";

ABSL_CONST_INIT const char* const kRejectedReason = "X-Rejected-Reason";

ABSL_CONST_INIT const char* const kGrpcPeerDelegationChain =
    "x-google-peer-delegation-chain-bin";

ABSL_CONST_INIT const char* const kXGenoaScottyAgentResponseInfo =
    "X-Genoa-Scotty-Agent-Response-Info";

ABSL_CONST_INIT const char* const kXGoogleGenoaScottyAgentResponseInfo =
    "X-Google-Genoa-Scotty-Agent-Response-Info";

ABSL_CONST_INIT const char* const kUntrustedHttpProxy = "untrusted-http-proxy";

ABSL_CONST_INIT const char* const
    kAuditingDirectiveCloudAuditRequestAndResponse =
        "CLOUD_AUDIT_REQUEST_AND_RESPONSE";
ABSL_CONST_INIT const char* const kAuditingDirectiveCloudAuditRequestOnly =
    "CLOUD_AUDIT_REQUEST_ONLY";
ABSL_CONST_INIT const char* const kAuditingDirectiveAudit = "AUDIT";
ABSL_CONST_INIT const char* const kAuditingDirectiveAuditRedact =
    "AUDIT_REDACT";
ABSL_CONST_INIT const char* const kAuditingDirectiveAuditSize = "AUDIT_SIZE";
ABSL_CONST_INIT const char* const kAuditingDirectiveQuery = "QUERY";
ABSL_CONST_INIT const char* const kAuditingDirectiveReadAction = "READ_ACTION";
ABSL_CONST_INIT const char* const kAuditingDirectiveWriteAction =
    "WRITE_ACTION";
ABSL_CONST_INIT const char* const kGinAuditingDirective = "GIN_AUDIT";
ABSL_CONST_INIT const char* const kGinAuditingDirectiveGinAuditExempt =
    "GIN_AUDIT_EXEMPT";
ABSL_CONST_INIT const char* const kGinAccessingProcessFamily =
    "gin.googleprod.com/accessing_process_family";
ABSL_CONST_INIT const char* const kGinAccessingProcessName =
    "gin.googleprod.com/accessing_process_name";

ABSL_CONST_INIT const char* const kGinResourceScope =
    "one-platform-api-resource";

ABSL_CONST_INIT const char* const kGrpcBackendAddressPrefix = "grpc:";

ABSL_CONST_INIT const char* const kMixedBackendAddressPrefix = "mixed:";

ABSL_CONST_INIT const char* const kGdataErrorsTypeName = "gdata.Errors";

ABSL_CONST_INIT const char* const kGdataMediaTypeName = "gdata.Media";
ABSL_CONST_INIT const char* const kMediaRequestInfoTypeName =
    "apiserving.MediaRequestInfo";
ABSL_CONST_INIT const char* const kMediaResponseInfoTypeName =
    "apiserving.MediaResponseInfo";

ABSL_CONST_INIT const char* const kDebugInfoTypeName = "google.rpc.DebugInfo";

ABSL_CONST_INIT const char* const kMediaDefaultContentType =
    "application/octet-stream";

ABSL_CONST_INIT const char* const kGdataTraceTypeName = "gdata.TraceRecords";

ABSL_CONST_INIT const char* const kProjectContext =
    "google.rpc.context.ProjectContext";
ABSL_CONST_INIT const char* const kClientProjectContext =
    "google.rpc.context.ClientProjectContext";
ABSL_CONST_INIT const char* const kOriginContext =
    "google.rpc.context.OriginContext";
ABSL_CONST_INIT const char* const kHttpHeaderContext =
    "google.rpc.context.HttpHeaderContext";
ABSL_CONST_INIT const char* const kAbuseContext =
    "google.rpc.context.AbuseContext";
ABSL_CONST_INIT const char* const kAttributeContext =
    "google.rpc.context.AttributeContext";
ABSL_CONST_INIT const char* const kVisibilityContext =
    "google.rpc.context.VisibilityContext";
ABSL_CONST_INIT const char* const kConditionRequestContext =
    "google.rpc.context.ConditionRequestContext";
ABSL_CONST_INIT const char* const kConditionResponseContext =
    "google.rpc.context.ConditionResponseContext";
ABSL_CONST_INIT const char* const kSystemParameterContext =
    "google.rpc.context.SystemParameterContext";
ABSL_CONST_INIT const char* const kApiaryMigrationContext =
    "google.rpc.context.ApiaryMigrationContext";
ABSL_CONST_INIT const char* const kMediationRequestContext =
    "google.rpc.context.MediationRequestContext";

ABSL_CONST_INIT const char* const kUtf8ByteOrderMark = "\xEF\xBB\xBF";

ABSL_CONST_INIT const char* const kHttpBatchServiceName =
    "apiserving.internal.Batch";

ABSL_CONST_INIT const char* const kHttpBatchMethodName =
    "apiserving.internal.Batch.Batch";

ABSL_CONST_INIT const char* const kRpcBatchMethodName =
    "google.rpc.batch.Batch.Execute";

ABSL_CONST_INIT const char* const kAuthSubHeaderValuePrefix = "AuthSub ";

ABSL_CONST_INIT const char* const kSherlogContextHeaderName =
    "x-goog-sherlog-context";
ABSL_CONST_INIT const char* const kSherlogLinkHeaderName =
    "x-goog-sherlog-link";

ABSL_CONST_INIT const char* const kSkipVPCServiceControls = "SKIP_VPC_SC";
ABSL_CONST_INIT const char* const kFailClosedLocationPolicy =
    "FAIL_CLOSED_LOCATION_POLICY";
ABSL_CONST_INIT const char* const kUseCustomResourceCallback =
    "USE_CUSTOM_RESOURCE_CALLBACK";
ABSL_CONST_INIT const char* const kUseCpeFullCallback = "USE_CPE_FULL_CALLBACK";
ABSL_CONST_INIT const char* const kProxiedByCloudESF = "PROXIED_BY_CLOUD_ESF";

// TODO(ethantzang): Remove it once ubermint finished migration.
//
// If UberMint mode is enabled and this header is set to "1", then explicitly
// use UberMint for the access token exchange regardless of the rollout
// percentage.
//
// If UberMint mode is enabled and this header is set to "0", then explicitly
// DO NOT use UberMint for the access token exchange regardless of the rollout
// percentage
ABSL_CONST_INIT const char* const kCloudUbermintEnabledHeaderName =
    "X-ESF-Use-Cloud-UberMint-If-Enabled";

ABSL_CONST_INIT const char* const kProjectOverrideHeaderKey =
    "X-Google-Project-Override";
ABSL_CONST_INIT const char* const kProjectOverrideApiKeyValue = "apikey";
ABSL_CONST_INIT const char* const kProjectOverrideLoasValue = "loas";

ABSL_CONST_INIT const char* const kGrpcHealthCheckMethod =
    "/grpc.health.v1.Health/Check";

ABSL_CONST_INIT const char* const kUberProxySignedUpTickHeaderName =
    "X-UberProxy-Signed-UpTick";

ABSL_CONST_INIT const char* const kErrorCategoryRequestPipelineSuffix =
    "-REQUEST";

ABSL_CONST_INIT const char* const kErrorCategoryErrorPipelineSuffix = "-ERROR";

ABSL_CONST_INIT const char* const kErrorCategoryInternalRedirectSuffix =
    "(GFE_REDIRECT_ON_ERROR)";

ABSL_CONST_INIT const char* const kErrorCategoryErrorFallbackSuffix =
    "(APIARY_FALLBACK_ON_ERROR)";

ABSL_CONST_INIT const char* const kErrorCategoryErrorFallbackFailedSuffix =
    "(APIARY_FALLBACK_ON_ERROR_FAILED)";

ABSL_CONST_INIT const char* const kApiaryRedirectProjectPropertyName =
    "_APIARY_MIGRATION_REDIRECT_PERCENTAGE";

ABSL_CONST_INIT const char* const
    kApiaryErrorRedirectOverrideProjectPropertyName =
        "_APIARY_MIGRATION_ERROR_REDIRECT_OVERRIDE";

ABSL_CONST_INIT const char* const kErrorCategoryprojectBlacklistRedirectSuffix =
    "(INTERNAL_REDIRECT_ON_PROJECT_BLACKLIST)";

ABSL_CONST_INIT const char* const kErrorCategoryJsonFixup =
    "JSON-FIXUP-ORIGINAL-ERROR";

ABSL_CONST_INIT const char* const kDebugTrackingID = "X-Debug-Tracking-ID";

ABSL_CONST_INIT const char* const kCredentialIDPrefixApikey = "apikey:";
ABSL_CONST_INIT const char* const kCredentialIDPrefixOauth2 = "oauth2:";
ABSL_CONST_INIT const char* const kCredentialIDPrefixServiceaccount =
    "serviceaccount:";

ABSL_CONST_INIT const char* const kForceJsonErrorFormatForV1ErrorsHeader =
    "X-Google-Force-Json-Error-Format-For-v1-Errors";

// LINT.IfChange
ABSL_CONST_INIT const char* const kEsfSidecarServiceNameHeader =
    "x-esf-sidecar-service-name-header";
ABSL_CONST_INIT const char* const kEsfSidecarMethodNameHeader =
    "x-esf-sidecar-method-name-header";
ABSL_CONST_INIT const char* const kEsfSidecarProcessRequestMethodName =
    "ProcessRequest";
ABSL_CONST_INIT const char* const kEsfSidecarProcessRequestFullMethodName =
    "/tech.env.framework.sidecar.ESFSidecar/ProcessRequest";
ABSL_CONST_INIT const char* const kEsfSidecarProcessResponseMethodName =
    "ProcessResponse";
ABSL_CONST_INIT const char* const kEsfSidecarProcessResponseFullMethodName =
    "/tech.env.framework.sidecar.ESFSidecar/ProcessResponse";
// LINT.ThenChange(//depot/google3/tech/internal/env/framework/sidecar/esf_sidecar.proto)
ABSL_CONST_INIT const char* const kEsfSidecarWrapperGraphPrefix = "ESF.Sidecar";

ABSL_CONST_INIT const char* const kAxtLevel = "axt.googleprod.com/axt_level";
ABSL_CONST_INIT const char* const kDisableAxtEnforcement =
    "DISABLE_AXT_ENFORCEMENT";

// The Horizontal Onboarding related constants.
// go/one-horizontal-resource-metadata-annotation
//
// CAIS integration Flag.
ABSL_CONST_INIT const char* const kCAISIntegration = "ENABLE_CAIS_INTEGRATION";

// CARGO integration Flag.
ABSL_CONST_INIT const char* const kCARGOIntegration =
    "ENABLE_CARGO_INTEGRATION";

// Eventarc integration Flag.
ABSL_CONST_INIT const char* const kEventarcIntegration =
    "ENABLE_EVENTARC_INTEGRATION";

// Service level feature flag for eventarc integration
ABSL_CONST_INIT const char* const kEventarcFeatureFlag =
    "eventarc.googleapis.com/enable-eventarc-integration";

// The operation type of the method.
ABSL_CONST_INIT const char* const kMethodType = "method_type";

ABSL_CONST_INIT const char* const kSelectorName = "name";

// The operation type values supported by Horizontal Onboarding.
ABSL_CONST_INIT const char* const kMethodTypeCreate = "CREATE";
ABSL_CONST_INIT const char* const kMethodTypeUpdate = "UPDATE";
ABSL_CONST_INIT const char* const kMethodTypeDelete = "DELETE";
ABSL_CONST_INIT const char* const kMethodTypeUnDelete = "UNDELETE";

// The metadata policy type supported by Horizontal Onboarding.
ABSL_CONST_INIT const char* const kMetadataPolicyTypeResource = "ESF_RESOURCE";
ABSL_CONST_INIT const char* const kMetadataPolicyTypeLocation = "LOCATION";
ABSL_CONST_INIT const char* const kMetadataPolicyTypeEventTime = "EVENT_TIME";
ABSL_CONST_INIT const char* const kMetadataPolicyTypeCreateTime = "CREATE_TIME";
ABSL_CONST_INIT const char* const kMetadataPolicyTypeDeleteTime = "DELETE_TIME";
ABSL_CONST_INIT const char* const kMetadataPolicyTypeUid = "UID";

// The metadata policy value_extractors keywords supported by Horizontal
// Onboarding.
ABSL_CONST_INIT const char* const kMetadataPolicyExtractorClockTime =
    "CLOCK_TIME";
ABSL_CONST_INIT const char* const kMetadataPolicyExtractorCallback = "CALLBACK";

// The annotations supported by One Horizontal.
ABSL_CONST_INIT const char* const kIamPolicyNameKey =
    "iam.googleapis.com/policy_name";
ABSL_CONST_INIT const char* const kCloudResourceContainerKey =
    "cloud.googleapis.com/resource_container";

// The backend services supported by One Horizontal.
ABSL_CONST_INIT const char* const kServiceCAIS = "CAIS";
ABSL_CONST_INIT const char* const kServiceCARGO = "CARGO";
ABSL_CONST_INIT const char* const kServiceEventarc = "EVENTARC";

ABSL_CONST_INIT const char* const kGlobalResourceLocation = "global";

// The type url of google.longrunning.Operations.
ABSL_CONST_INIT const char* const kLongRunningOperationType =
    "type.googleapis.com/google.longrunning.Operation";

ABSL_CONST_INIT const char* const kRoleNoDelegation = "";

ABSL_CONST_INIT const char* const kESFInvalidMethod = "ESF_INVALID_METHOD";

ABSL_CONST_INIT const char* const kCloudStorageUploadIdHeader =
    "X-GUploader-UploadID";

ABSL_CONST_INIT const char* const kStreamBodyProtoContentType =
    "application/x-protobuf; type=google.rpc.streambody";

ABSL_CONST_INIT const int32_t kDefaultDirectPathProductID = 75249418;

}  // namespace proto_processing_lib::proto_scrubber
