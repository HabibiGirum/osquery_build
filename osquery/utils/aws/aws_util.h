/**
 * Copyright (c) 2014-present, The osquery authors
 *
 * This source code is licensed as defined by the LICENSE file found in the
 * root directory of this source tree.
 *
 * SPDX-License-Identifier: (Apache-2.0 OR GPL-2.0-only)
 */

#pragma once

#include <memory>
#include <string>
#include <utility>

#include <aws/core/Region.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpClient.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/core/http/standard/StandardHttpResponse.h>
#include <aws/core/utils/StringUtils.h>

#include <aws/ec2/EC2Client.h>
#include <aws/firehose/FirehoseClient.h>
#include <aws/kinesis/KinesisClient.h>
#include <aws/sts/STSClient.h>

#include <boost/optional/optional.hpp>
#include <boost/property_tree/ptree.hpp>

#include <osquery/utils/status/status.h>

// This macro from the Windows headers is used to map the GetMessage
// name to either GetMessageW or GetMessageA depending on the UNICODE
// define. We have to undefine this because it causes a method in the
// AWS sdk to be renamed, causing a compilation error.
#if defined(WIN32) && defined(GetMessage)
#undef GetMessage
#endif

namespace osquery {

using RegionName = const char* const;

/// EC2 instance latestmetadata URL
extern const std::string kEc2MetadataUrl;

/// Hypervisor UUID file
extern const std::string kHypervisorUuid;

/// URL resource to request IMDSv2 API token
extern const std::string kImdsTokenResource;

/// Token header to be used in HTTP GET requests for IMDSv2 API calls
extern const std::string kImdsTokenHeader;

/// Header for specifying TTL for IMDSv2 API token
extern const std::string kImdsTokenTtlHeader;

/// Default TTL value for IMDSv2 API token
extern const std::string kImdsTokenTtlDefaultValue;

/**
 * @brief Client factory for the Osquery HTTP client
 */
class OsqueryHttpClientFactory : public Aws::Http::HttpClientFactory {
 public:
  std::shared_ptr<Aws::Http::HttpClient> CreateHttpClient(
      const Aws::Client::ClientConfiguration& clientConfiguration)
      const override;
  std::shared_ptr<Aws::Http::HttpRequest> CreateHttpRequest(
      const Aws::String& uri,
      Aws::Http::HttpMethod method,
      const Aws::IOStreamFactory& streamFactory) const override;
  std::shared_ptr<Aws::Http::HttpRequest> CreateHttpRequest(
      const Aws::Http::URI& uri,
      Aws::Http::HttpMethod method,
      const Aws::IOStreamFactory& streamFactory) const override;
};

/**
 * @brief Osquery implementation of AWS HTTP Client
 *
 * AWS uses a libcurl HTTP client by default. We prefer not to use libcurl, so
 * we provide this HTTP client implementation for the AWS SDK to use when
 * querying the APIs. The SDK must be built with -DNO_HTTP_CLIENT=1 to prevent
 * it looking for libcurl when static linking.
 */
class OsqueryHttpClient : public Aws::Http::HttpClient {
 public:
  OsqueryHttpClient() : HttpClient() {}

  std::shared_ptr<Aws::Http::HttpResponse> MakeRequest(
      const std::shared_ptr<Aws::Http::HttpRequest>& request_ptr,
      Aws::Utils::RateLimits::RateLimiterInterface* readLimiter,
      Aws::Utils::RateLimits::RateLimiterInterface* writeLimiter)
      const override;
};

/**
 * @brief AWS credentials provider that uses osquery config flags
 *
 * This provider uses the config flags aws_access_key_id and
 * aws_secret_access_key. If only one flag is specified, it returns empty
 * strings so that another provider in the chain can be tried.
 */
class OsqueryFlagsAWSCredentialsProvider
    : public Aws::Auth::AWSCredentialsProvider {
 public:
  OsqueryFlagsAWSCredentialsProvider() : AWSCredentialsProvider() {}

  /// Retrieve credentials from configurations.
  Aws::Auth::AWSCredentials GetAWSCredentials() override;
};

/**
 * @brief AWS credentials provider that uses STS assume role auth
 *
 * This provider delegates temp AWS STS credentials via assume role
 * for an AWS arn.
 */
class OsquerySTSAWSCredentialsProvider
    : public Aws::Auth::AWSCredentialsProvider {
 public:
  OsquerySTSAWSCredentialsProvider() : AWSCredentialsProvider() {}

  /// Retrieve credentials from STS.
  Aws::Auth::AWSCredentials GetAWSCredentials() override;

 private:
  /// Internal API client.
  std::shared_ptr<Aws::STS::STSClient> client_{nullptr};

  /// Configuration details.
  Aws::String access_key_id_;
  Aws::String secret_access_key_;
  Aws::String session_token_;

  /// Time when the last-most-recent credentials will expire.
  uint64_t token_expire_time_{0};
};

/**
 * @brief AWS credentials provider chain that prioritizes osquery config
 *
 * This provider attempts to find credentials in the following order, returning
 * the first non-empty credentials it finds:
 * 1. AWS STS auth (via OsquerySTSAWSCredentialsProvider)
 * 2. osquery flags (via OsqueryFlagsAWSCredentialsProvider)
 * 3. Profile from the AWS profile file (iff --aws_profile_name is specified)
 * 4. Environment variables (AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY)
 * 5. "default" profile in AWS profile file
 * 6. Profile from the EC2 Instance Metadata Service
 */
class OsqueryAWSCredentialsProviderChain
    : public Aws::Auth::AWSCredentialsProviderChain {
 public:
  OsqueryAWSCredentialsProviderChain(bool sts = true);
};

/**
 * @brief Initialize the AWS SDK
 *
 * This function is intended to be called from the ::setUp() method of logger
 * plugins that use the AWS SDK. It initializes the SDK, instructing it to use
 * our custom OsqueryHttpClientFactory. This function may be called more than
 * once, but initializing will only occur on the first call.
 */
void initAwsSdk();

/**
 * @brief Returns a token for use with Instance Metadata Service (IMDSv2) API
 *
 * This method makes an HTTP PUT request with kImdsTokenTtlHeader
 * to request a token, which can subsequently used to make GET requests to the
 * Instance Metadata Service endpoint.
 * If it fails to retrieve the token, it does FLAGS_aws_imdsv2_request_attempts
 * attempts, with an interval of FLAGS_aws_imdsv2_request_interval, which scales
 * quadratically.
 *
 * @return token as a string if successful, boost::none if not
 */
boost::optional<std::string> getIMDSToken();

/**
 * @brief Returns EC2 instance ID and region of this machine.
 *
 * Returns the instance ID and region by making an HTTP request to the EC2
 * metadata service. If there's any error in retrieving the instance id or
 * region, they will be empty. One has to ensure that the instance is an EC2 one
 * before calling this method, using isEc2Instance.
 *
 * @return Pair of instance ID and region if successful, otherwise boost::none
 */
boost::optional<std::pair<std::string, std::string>> getInstanceIDAndRegion();

/**
 * @brief Retrieve the configured Aws::Region
 *
 * The region can come from the aws_region flag, the aws_sts_region flag
 * if the sts parameter is true, or the local AWS profile,
 * if no flag was specified.
 *
 * @param region The output string containing the region name
 * @param sts If to retrieve the STS region in the aws_sts_region flag or not
 * @param validate_region If the function should validate the region name found
 * against a list of known values.
 *
 * @return 0 if successful, 1 if the region was not recognized.
 */
Status getAWSRegion(std::string& region,
                    bool sts = false,
                    bool validate_region = true);

/**
 * @brief Set HTTP/HTTPS proxy information on the AWS ClientConfiguration
 * using relevant flags for scheme, host, port, username, and password
 *
 * This is a no-op if the 'aws_enable_proxy' flag is not set to true.
 *
 * @param config Pointer to Aws::Client::ClientConfiguration struct
 *  on which to set the proxy values
 */
void setAWSProxy(Aws::Client::ClientConfiguration& config);

Status setAwsClientConfig(const std::string& region,
                          const std::string& service_type,
                          const std::string& endpoint_override,
                          bool sts,
                          Aws::Client::ClientConfiguration& config);

/**
 * @brief Instantiate an AWS client with the appropriate osquery configs,
 *
 * This will pull the region and authentication configs from the appropriate
 * places (i.e. using getAWSRegion, OsqueryAWSCredentialsProviderChain),
 * instantiating whichever type of AWS client is passed as the template
 * parameter.
 *
 * @param client Pointer to the client object to instantiate.
 * @param region AWS region to connect to. If not specified, will try to figure
 * out based on the configuration flags and AWS profile.
 * @param endpoint_override Custom AWS service endpoint.
 *
 * @return 0 if successful, 1 if there was a problem reading configs.
 */
template <class Client>
Status makeAWSClient(std::shared_ptr<Client>& client,
                     const std::string& region = "",
                     bool sts = true,
                     const std::string& endpoint_override = "") {
  std::string service_type;

  Aws::Client::ClientConfiguration client_config;

  if constexpr (std::is_same_v<Client, Aws::Kinesis::KinesisClient>) {
    service_type = "kinesis";
  } else if constexpr (std::is_same_v<Client, Aws::Firehose::FirehoseClient>) {
    service_type = "firehose";
  } else if constexpr (std::is_same_v<Client, Aws::STS::STSClient>) {
    service_type = "sts";
  } else if constexpr (std::is_same_v<Client, Aws::EC2::EC2Client>) {
    service_type = "ec2";
  }

  Status s = setAwsClientConfig(
      region, service_type, endpoint_override, sts, client_config);

  if (!s.ok()) {
    return s;
  }

  client = std::make_shared<Client>(
      std::make_shared<OsqueryAWSCredentialsProviderChain>(sts), client_config);
  return Status::success();
}

/**
 * @brief Parses an input string as JSON, appends a "log_type" key to the
 * dictionary, and serializes it, mutating the input
 *
 * @param log_type The type of log (as passed to a logger plugin's send
 * function)
 * @param log The input to be mutated with the appended "log_type" JSON key
 *
 * @return 0 if successful, 1 if there were issues
 */
Status appendLogTypeToJson(const std::string& log_type, std::string& log);

/**
 * @brief Enable the FIPS endpoint for the specified region
 *
 * @param config AWS client configuration to be modified to enable FIPS
 */
void enableFIPSInClientConfig(const std::string& service,
                              Aws::Client::ClientConfiguration& config);
} // namespace osquery
