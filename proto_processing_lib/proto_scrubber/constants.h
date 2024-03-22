// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_CONSTANTS_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_CONSTANTS_H_

#include <cstdint>

// This file contains constants used by proto processing libraries.

namespace proto_processing_lib::proto_scrubber {

// Prefix for type URLs.
extern const char* const kTypeServiceBaseUrl;
// Size of "type.googleapis.com"
constexpr int64_t kTypeUrlSize = 19;

// TypeId of the request extension setting the `product_cui` Census tag.
// http://google3/production/insights/cui/attribution/product_cui_extension.proto
constexpr int64_t kProductCuiTypeId = 353267353;

// Prefix for type URLs that some internal APIs use. Majority use the above. All
// responses have above as type URL.
extern const char* const kGoogleProdServiceBaseUrl;

// Fully qualified name of proto stream option legacy_token_unit.
extern const char* const kOptionLegacyTokenUnit;

// Fully qualified name of proto method option legacy_client_initial_tokens.
extern const char* const kOptionLegacyClientInitialTokens;

// Fully qualified name of proto method option legacy_server_initial_tokens.
extern const char* const kOptionLegacyServerInitialTokens;

// Placeholder name for identifying that a given method is defined as a 'stream'
// instead of as 'rpc' in service definition proto.
extern const char* const kOptionEnvelopeStream;

// Fully qualified name of proto method option stream_type.
extern const char* const kOptionStreamType;

// Fully qualified name of proto method option legacy_stream_type.
extern const char* const kOptionLegacyStreamType;

// Fully qualified name of proto method option legacy_result_type.
extern const char* const kOptionLegacyResultType;

// Fully qualified name of proto field option field_migration.
extern const char* const kMigrationOptionFieldOptionName;

// Type string for google.longrunning.Operation
extern const char* const kOperationType;

// Type string for google.protobuf.Struct.
extern const char* const kStructType;

// Type string for google.protobuf.FieldsEntry.
extern const char* const kStructFieldsEntryType;

// Type string for struct.proto's google.protobuf.Value value type.
extern const char* const kStructValueType;

// Type string for struct.proto's google.protobuf.ListValue value type.
extern const char* const kStructListValueType;

// Type URL for struct value type google.protobuf.Struct.
extern const char* const kStructTypeUrl;

// Type URLfor struct.proto's google.protobuf.Value value type.
extern const char* const kStructValueTypeUrl;

// Type URL for struct value type google.protobuf.ListValue.
extern const char* const kStructListValueTypeUrl;

// Type URL for string wrapper type google.protobuf.StringValue
extern const char* const kStringValueTypeUrl;

// Type string for google.protobuf.Any
extern const char* const kAnyType;

// The constants for proto map.
extern const char* const kProtoMapEntryName;
extern const char* const kProtoMapValueFieldName;

extern const char* const kDiscoveryServiceName;

// Full service name for Discovery API
extern const char* const kDiscoveryApiName;
extern const char* const kDiscoveryApiPrefix;
extern const char* const kLegacyDiscoveryUrlPathPrefix;

// UDS path for Discovery Stubby2 server
extern const char* const kDiscoveryUdsPath;

// UDS path for ESF Sidecar
extern const char* const kESFSidecarUdsPath;

// A special type that binds incoming HTTP request body to the fields of this
// type.
extern const char* const kHttpBodyTypeUrl;

// Type string for google.api.HttpBody.
extern const char* const kHttpBodyName;

// Query parameters that control pretty-pretty behavior on JSON response.
extern const char* const kQueryParamPrettyPrint1;
extern const char* const kQueryParamPrettyPrint2;
extern const char* const kQueryParamPrettyPrint3;
extern const char* const kQueryParamPrettyPrint4;
extern const char* const kQueryParamPrettyPrint5;
extern const char* const kQueryParamPrettyPrint6;

// Cache-busting query parameter.
extern const char* const kQueryParamUnique;

// On-demand trace request query parameter.
extern const char* const kQueryParameterTrace;
extern const char* const kQueryParameterTrace2;

// The name of the HTTP response header whose value contains the trace id.
// It is only present when functional trace is explicitly requested by the
// caller of a HTTP request.
// TODO(apavan): When Tapper has a proper trace context propagation mechanism,
// remove this.
extern const char* const kTraceHttpResponseHeader;

// FieldMask (partial response) query parameters. "fields" is treated as a
// special parameter only when it is not bound to a request field.
extern const char* const kFieldMaskParameter1;
extern const char* const kFieldMaskParameter2;

// The name of the project "key" parameter. For more details, please refer to:
// https://developers.google.com/console/help/#WhatIsKey
extern const char* const kQueryParameterKey1;
extern const char* const kQueryParameterKey2;
extern const char* const kQueryParameterKeyRegex;

// JSONP callback query parameter.
extern const char* const kQueryParameterCallback1;
extern const char* const kQueryParameterCallback2;

// V1 or V2 error format selector parameter.
extern const char* const kQueryParameterErrorFormat1;
// A more human-readable version error format selector.
extern const char* const kQueryParameterErrorFormat2;
extern const char* const kQueryParameterErrorFormat3;

// Quota user query parameter.
extern const char* const kQueryParameterQuotaUser1;
extern const char* const kQueryParameterQuotaUser2;

// Query parameter specifying the data format of the response.
extern const char* const kQueryParameterAlt1;
extern const char* const kQueryParameterAlt2;

// OAuth-related query parameters.
extern const char* const kQueryParameterAccessToken;
extern const char* const kQueryParameterOauthToken;
extern const char* const kQueryParameterBearerToken;
extern const char* const kQueryParameterTokenRegex;
extern const char* const kQueryParameterXOauth;

// Upload protocol query parameter.
extern const char* const kQueryParameterUploadProtocol1;
extern const char* const kQueryParameterUploadProtocol2;

// Legacy upload type query parameter.
extern const char* const kQueryParameterUploadType1;
extern const char* const kQueryParameterUploadType2;

// Grpc reserved headers that are used to create rpc security policy by grpc
// backends.
extern const char* const kDataAccessReasonKey;
extern const char* const kIamAuthorizationTokenKey;
extern const char* const kEndUserCredsKey;

// User IP query parameter.
extern const char* const kQueryParameterUserIp1;
extern const char* const kQueryParameterUserIp2;
extern const char* const kQueryParameterUserIpRegex;

// Password query parameter regex. This matches passwd or password.
extern const char* const kPasswordRegex;

// WebChannel-specific special query parameters.
extern const char* const kQueryParamWebChannelAid;
extern const char* const kQueryParamWebChannelCi;
extern const char* const kQueryParamWebChannelTo;
extern const char* const kQueryParamWebChannelRid;
extern const char* const kQueryParamWebChannelSid;
extern const char* const kQueryParamWebChannelType;
extern const char* const kQueryParamWebChannelVer;
extern const char* const kQueryParamWebChannelCver;
extern const char* const kQueryParamWebChannelModel;
extern const char* const kQueryParamWebChannelRetryId;
extern const char* const kQueryParamWebChannelOriginTrial;
extern const char* const kQueryParamGSessionId;
extern const char* const kQueryParamHttpSessionId;
// Javascript specific query parameters. They are used by WebChannel too.
extern const char* const kQueryParamJsRandom;

// Query param used to override the HTTP method, just like the
// X-HTTP-Method-Override header.
extern const char* const kQueryParamHttpMethod;

// Query param used to override the HTTP headers.
extern const char* const kQueryParamHttpHeaders;

// Query param used to override content type, useful for XD4 requests.
extern const char* const kQueryParamContentType;

// Query param for holding request body.
extern const char* const kQueryParamReq;
extern const char* const kQueryParamReqRegex;

// Query param used to force output default values.
extern const char* const kQueryParamOutputDefaults;

// Query param used to override visibility labels.
extern const char* const kQueryParamVisibilities;

// Query param used to specify API version.
extern const char* const kQueryParamApiVersion;

// Query param used to specify user project.
extern const char* const kQueryParamUserProject;

// Query param used to specify the Google Cloud resource prefix (used for
// routing).
extern const char* const kQueryParamGoogleCloudResourcePrefix;

// The backend address prefix to use stubby4 wrapper.
extern const char* const kStubby4WrapperPrefix;

// Content-Type string for form-urlencoded type.
extern const char* const kFormUrlEncodedContentType;

// Content-Type string for application/x-protobuf or for responses when
// $alt=proto query parameter is set.
extern const char* const kProtobufContentType;

// Content-Type string for JSPB.
extern const char* const kJSPBContentType;

// Content-Type for using JSPB GWT.
extern const char* const kJSPBGWTContentType;

// Content-Type for using JSPB FAVA (Apps).
extern const char* const kJSPBFAVAContentType;

// Content-Type string for JSON.
extern const char* const kJSONContentType;

// Content-Type string for multipart/form-data.
extern const char* const kMultipartFormDataContentType;

// Content-Type string for multipart/mixed.
extern const char* const kMultipartMixedContentType;

// Content-Id string in Batch Request.
extern const char* const kContentIdKey;

// Project number is considered as unset with this value.
constexpr int64_t kProjectNumberUnsetValue = 0;

// Prefix for ByteStream method names.
extern const char* const kMediaMethodPrefix;

// FieldMask context type's full name.
extern const char* const kFieldMaskContext;

// GRPC header for API key.
extern const char* const kGrpcApiKeyHeader;

// GRPC header for selecting response field mask.
extern const char* const kGrpcFieldMaskHeader;

// GRPC header for visibilities. Equivalent to $visibilities query param.
extern const char* const kGrpcVisibilitiesHeader;

// GRPC header for API version. Equivalent to $apiVersion query param.
extern const char* const kGrpcApiVersionHeader;

// GRPC header for quota user.
extern const char* const kGrpcQuotaUserHeader;

// Header for http method override.
extern const char* const kHttpMethodOverrideHeader;

// Header for API client.
extern const char* const kApiClientHeader;

// Header for user project.
extern const char* const kUserProject;

// Header for request reason, used for audit logging.
extern const char* const kRequestReasonHeader;

// Header for android package name, used for api key restriction check.
extern const char* const kXAndroidPackage;

// Header for android certificate fingerprint, used for api key restriction
// check.
extern const char* const kXAndroidCert;

// Header for IOS bundle identifier, used for api key restriction check.
extern const char* const kXIosBundleId;

// Header for the new keyless project authentication (see go/kaapav).
extern const char* const kXGoogSpatulaHeader;

// Header for Plus Page Impersonation.
extern const char* const kXGoogPageIdHeaderName;

// Header prefix for gRPC sidechannel information.
// LINT.IfChange
extern const char* const kGrpcSideChannelHeaderPrefix;
// LINT.ThenChange(//depot/google3/java/com/google/frameworks/client/data/sidechannel/FrontendRequestHeaders.java)

// Header used to indicate the `product_cui` request extension.
// http://google3/production/insights/cui/attribution/product_cui_extension.proto
extern const char* const kProductCuiHeader;

// JSPB Header used to indicate the `product_cui` request extension sent from
// Boq Wiz
extern const char* const kJSPBProductCuiHeader;

// Header used to indicate the `request_qos` request extension.
// http://google3/frameworks/client/data/qos_extension.proto
extern const char* const kRequestQosHeader;

// Header used to indicate the resource being operated on.
extern const char* const kGoogleCloudResourcePrefixHeader;

// Header used to indicate Gaia session index for multiple login.
extern const char* const kXGoogleAuthUserHeader;

// Header used to pass additional parameters for gRPC requests in URL query
// format.
extern const char* const kXGoogRequestParams;

// Header used to pass headers received from the RLS server to AFEs.
extern const char* const kXGoogleRlsData;

// Header used to allow a first party client to pass reauth related metadata to
// one platform.
extern const char* const kXGoogFirstPartyReauthHeader;

// The URL prefix for GRPC-Web.
extern const char* const kGrpcWebUrlPrefix;
extern const char* const kGrpcWebUrlPrefixWithSlash;
extern const char* const kGrpcWebApiClient;

// Signal to the server that the response should be encoded. The value of this
// header specifies the expected encoding. Currently this is only used by
// grpc-web.
extern const char* const kXAcceptContentTransferEncoding;

// https://www.w3.org/Protocols/rfc1341/5_Content-Transfer-Encoding.html
extern const char* const kContentTransferEncoding;

// Response compression header.
extern const char* const kResponseCompressionHeader;

// The sub content-type for Webchannel
extern const char* const kXWebchannelContentType;

// The service name of Unified Logging Service.
extern const char* const kUnifiedLoggingService;

extern const char* const kLoadSheddingErrorCategory;
extern const char* const kUntrustedPeerErrorCategory;

extern const char* const kUntrustedDelegationErrorCategory;

// Base64 encoding for response safety request header name and value.
extern const char* const kGoogEncodeResponseIfExecutable;
extern const char* const kGoogSafetyEncoding;
extern const char* const kBase64Encoding;

// The safety content header name.
extern const char* const kGoogSafetyContentType;

// The rejected reason header name.
extern const char* const kRejectedReason;

// gRPC LOAS2 delegation header. See go/loas-impersonation-grpc for details.
extern const char* const kGrpcPeerDelegationChain;

// Custom header for Drive's Scotty Media Upload, b/235376652.
// This should only be used and set by Drive for backend batch response. Soon to
// be replaced by kXGoogleGenoaScottyAgentResponseInfo
extern const char* const kXGenoaScottyAgentResponseInfo;

// Custom header for Drive's Scotty Media Upload. This should only be used and
// set by Drive for backend batch response.
extern const char* const kXGoogleGenoaScottyAgentResponseInfo;

// The role ESF allows anyone to delegate to and GFE delegates to this role
// by default.
extern const char* const kUntrustedHttpProxy;

// The supported auditing directives.
extern const char* const kAuditingDirectiveCloudAuditRequestAndResponse;
extern const char* const kAuditingDirectiveCloudAuditRequestOnly;
extern const char* const kAuditingDirectiveAudit;
extern const char* const kAuditingDirectiveAuditRedact;
extern const char* const kAuditingDirectiveAuditSize;
extern const char* const kAuditingDirectiveQuery;
extern const char* const kAuditingDirectiveReadAction;
extern const char* const kAuditingDirectiveWriteAction;
extern const char* const kGinAuditingDirective;
extern const char* const kGinAuditingDirectiveGinAuditExempt;

// The supported auditing labels.
extern const char* const kGinAccessingProcessFamily;
extern const char* const kGinAccessingProcessName;

// Gin audit logging constants for all OnePlatform APIs.
extern const char* const kGinResourceScope;

// The backend address prefix to use the grpc backend proxy.
extern const char* const kGrpcBackendAddressPrefix;

// The backend address prefix to use the dual backend proxy.
extern const char* const kMixedBackendAddressPrefix;

// Type name for Apiary's gdata errors.
extern const char* const kGdataErrorsTypeName;

// Type name for Apiary's gdata media.
extern const char* const kGdataMediaTypeName;

// Type name for Apiary's media request info.
extern const char* const kMediaRequestInfoTypeName;

// Type name for Apiary's media response info.
extern const char* const kMediaResponseInfoTypeName;

// Type name for google.rpc.DebugInfo.
extern const char* const kDebugInfoTypeName;

// The default Content-Type to assume when not supplied in a request.
extern const char* const kMediaDefaultContentType;

// Gdata Type name for TraceRecords.
extern const char* const kGdataTraceTypeName;

// Contexts that can be requested via service config.
extern const char* const kProjectContext;
extern const char* const kClientProjectContext;
extern const char* const kOriginContext;
extern const char* const kHttpHeaderContext;
extern const char* const kAbuseContext;
extern const char* const kAttributeContext;
extern const char* const kVisibilityContext;
extern const char* const kConditionRequestContext;
extern const char* const kConditionResponseContext;
extern const char* const kSystemParameterContext;
extern const char* const kApiaryMigrationContext;
extern const char* const kMediationRequestContext;

// Prefix for UTF8 byte-order-mark.
extern const char* const kUtf8ByteOrderMark;

// Name of the apiserving.internal.Batch service.
extern const char* const kHttpBatchServiceName;

// Name of the apiserving.internal.Batch method.
extern const char* const kHttpBatchMethodName;

// Name of the google.rpc.batch method.
extern const char* const kRpcBatchMethodName;

// Used to identify an AuthSub request.
extern const char* const kAuthSubHeaderValuePrefix;

// Sherlog header for propagating context to connect logs from client-side and
// server-side.
extern const char* const kSherlogContextHeaderName;

// Sherlog header for propagating a link to Sherlog trace back to API consumers.
extern const char* const kSherlogLinkHeaderName;

// Permissible values of MethodPolicy.flags.
extern const char* const kSkipVPCServiceControls;
extern const char* const kFailClosedLocationPolicy;
extern const char* const kUseCustomResourceCallback;
extern const char* const kUseCpeFullCallback;
extern const char* const kProxiedByCloudESF;

// Temporary header for uber mint migration.
extern const char* const kCloudUbermintEnabledHeaderName;

// Project override constants. For more information, please refer to
// go/op:quotaoverride.
extern const char* const kProjectOverrideHeaderKey;
extern const char* const kProjectOverrideApiKeyValue;
extern const char* const kProjectOverrideLoasValue;

// gRPC health check method name.
extern const char* const kGrpcHealthCheckMethod;

// Header for UberProxy Signed UpTick Auth
extern const char* const kUberProxySignedUpTickHeaderName;

// Request pipeline suffix of wrapper key string.
extern const char* const kErrorCategoryRequestPipelineSuffix;
// Error pipeline suffix of wrapper key string.
extern const char* const kErrorCategoryErrorPipelineSuffix;
// Error category suffix to be appended if the request is internally redirected.
extern const char* const kErrorCategoryInternalRedirectSuffix;
// Error category suffix to be appended if the error fallback call is invoked.
extern const char* const kErrorCategoryErrorFallbackSuffix;
// Error category suffix to be appended if the error fallback call is supposed
// to be called but failed.
extern const char* const kErrorCategoryErrorFallbackFailedSuffix;
// Project property name for Apiary migration project blacklist redirect
// percentage.
extern const char* const kApiaryRedirectProjectPropertyName;
// Project property name for Apiary migration error redirection override.
extern const char* const kApiaryErrorRedirectOverrideProjectPropertyName;
// Error category suffix to be appended if the request is internally redirected
// because of project blacklist.
extern const char* const kErrorCategoryprojectBlacklistRedirectSuffix;
// Error category for a request invoking the JsonFixupService. The
// error_category field will be set to this for analysis purpose.
extern const char* const kErrorCategoryJsonFixup;

// Header name for debug tracking ID, which uses trace id (a.k.a. global id) and
// is unique(in a short period of time) for different requests. We could use
// this ID to link the client side debug info with server side info for a
// specific request. Design doc: go/pan-common-tracking-id
extern const char* const kDebugTrackingID;

// Valid credential id prefixes.
extern const char* const kCredentialIDPrefixApikey;
extern const char* const kCredentialIDPrefixOauth2;
extern const char* const kCredentialIDPrefixServiceaccount;

// HTTP header used to force the error format to be JSON regardless of the
// output format requested by the client. So, alt=proto with this header will
// cause non-error responses to be in proto format but errors to be in JSON
// format. This works only for v1 errors (i.e. use_v1_error_format is set).
extern const char* const kForceJsonErrorFormatForV1ErrorsHeader;

// ESF Sidecar related.
extern const char* const kEsfSidecarServiceNameHeader;
extern const char* const kEsfSidecarMethodNameHeader;
extern const char* const kEsfSidecarProcessRequestMethodName;
extern const char* const kEsfSidecarProcessRequestFullMethodName;
extern const char* const kEsfSidecarProcessResponseMethodName;
extern const char* const kEsfSidecarProcessResponseFullMethodName;
extern const char* const kEsfSidecarWrapperGraphPrefix;

extern const char* const kAxtLevel;
extern const char* const kDisableAxtEnforcement;

// Horizontal Onboarding annotations.
// go/one-horizontal-resource-metadata-annotation
//
// CAIS Integration Flag
extern const char* const kCAISIntegration;

// CARGO Integration Flag
extern const char* const kCARGOIntegration;

// Eventarc Integration Flag
extern const char* const kEventarcIntegration;

// Service level feature flag for eventarc integration
extern const char* const kEventarcFeatureFlag;

// The operation type of the method.
extern const char* const kMethodType;
// Selector of the field "name"
extern const char* const kSelectorName;

// The operation type values supported by CAIS.
extern const char* const kMethodTypeCreate;
extern const char* const kMethodTypeUpdate;
extern const char* const kMethodTypeDelete;
extern const char* const kMethodTypeUnDelete;

// The metadata policy type supported by CAIS.
extern const char* const kMetadataPolicyTypeResource;
extern const char* const kMetadataPolicyTypeLocation;
extern const char* const kMetadataPolicyTypeEventTime;
extern const char* const kMetadataPolicyTypeCreateTime;
extern const char* const kMetadataPolicyTypeDeleteTime;
extern const char* const kMetadataPolicyTypeUid;

// The metadata policy value_extractor keywords supported by CAIS.
extern const char* const kMetadataPolicyExtractorClockTime;
extern const char* const kMetadataPolicyExtractorCallback;

// The keys supported by One Horizontal.
extern const char* const kIamPolicyNameKey;
extern const char* const kCloudResourceContainerKey;

// The backend services supported by One Horizontal.
extern const char* const kServiceCAIS;
extern const char* const kServiceCARGO;
extern const char* const kServiceEventarc;

// The global resource location, used by One Horizontal.
extern const char* const kGlobalResourceLocation;

// The type url for google.longrunning.Operations.
extern const char* const kLongRunningOperationType;

// Indicates that no delegation role is specified. An empty delegation role (no
// delegation) indicates the delegation is valid.
extern const char* const kRoleNoDelegation;

// ESF generated invalid method
extern const char* const kESFInvalidMethod;

// A header used by the Cloud Storage team's APIs which contains a session ID
// for an upload or download.
extern const char* const kCloudStorageUploadIdHeader;

extern const char* const kStreamBodyProtoContentType;

// http://product/75249418.
extern const int32_t kDefaultDirectPathProductID;

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_CONSTANTS_H_
