#include <memory>
#include <boost/algorithm/string/replace.hpp>
#include <cryptopp/base64.h>
#include <fmt/format.h>
#include "common/file_util.h"
#include "nasc.h"

namespace NetworkClient::NASC {

    constexpr std::size_t TIMEOUT_SECONDS = 15;

    // Custom function to perform case-insensitive comparison
    bool IsEqualIgnoreCase(const std::string& str1, const std::string& str2) {
        if (str1.size() != str2.size()) {
            return false;
        }

        return std::equal(str1.begin(), str1.end(), str2.begin(), [](char a, char b) {
            return std::tolower(a) == std::tolower(b);
        });
    }

    void NASCClient::Initialize(const std::vector<u8>& cert, const std::vector<u8>& key) {
        Clear();

        const unsigned char* tmpCertPtr = cert.data();
        const unsigned char* tmpKeyPtr = key.data();

        client_cert = d2i_X509(nullptr, &tmpCertPtr, (long)cert.size());
        client_priv_key = d2i_PrivateKey(EVP_PKEY_RSA, nullptr, &tmpKeyPtr, (long)key.size());
    }

    void NASCClient::SetParameterImpl(const std::string& key, const std::vector<u8>& value) {
        using namespace CryptoPP;
        using Name::EncodingLookupArray;
        using Name::InsertLineBreaks;
        using Name::Pad;
        using Name::PaddingByte;

        std::string out;
        Base64Encoder encoder;
        AlgorithmParameters params =
            MakeParameters(EncodingLookupArray(), (const byte*)base64_dict.data())(
                InsertLineBreaks(), false)(Pad(), true)(PaddingByte(), (byte)'*');

        encoder.IsolatedInitialize(params);
        encoder.Attach(new StringSink(out));
        encoder.Put(value.data(), value.size());
        encoder.MessageEnd();

        parameters.emplace_back(key, out);
    }

    NASCClient::NASCResult NASCClient::Perform() {
        std::unique_ptr<httplib::SSLClient> cli;
        httplib::Request request;
        NASCResult res;

        if (client_cert == nullptr || client_priv_key == nullptr) {
            res.log_message = "Missing or invalid client certificate or key.";
            return res;
        }

        cli = std::make_unique<httplib::SSLClient>(nasc_url, 443, client_cert, client_priv_key);
        cli->set_connection_timeout(TIMEOUT_SECONDS);
        cli->set_read_timeout(TIMEOUT_SECONDS);
        cli->set_write_timeout(TIMEOUT_SECONDS);
        cli->enable_server_certificate_verification(false);

        if (!cli->is_valid()) {
            res.log_message = fmt::format("Invalid URL \"{}\"", nasc_url);
            return res;
        }

        std::string header_param;

        if (GetParameter(parameters, "gameid", header_param)) {
            request.set_header("X-GameId", header_param);
        }
        header_param.clear();
        if (GetParameter(parameters, "fpdver", header_param)) {
            request.set_header("User-Agent", fmt::format("CTR FPD/00{}", header_param));
        }

        request.set_header("Content-Type", "application/x-www-form-urlencoded");

        request.method = "POST";
        request.path = "/ac";

        for (auto it = parameters.begin(); it != parameters.end(); ++it) {
            if (it != parameters.begin()) { request.body += "&"; }
            request.body += it->first;
            request.body += "=";
            request.body += httplib::detail::encode_query_param(it->second);
        }

        boost::replace_all(request.body, "*", "%2A");

        httplib::Result result = cli->send(request);
        LOG_INFO(Service_FRD, request.body.c_str());
        if (!result) {
            res.log_message =
                fmt::format("Request to \"{}\" returned error {}", nasc_url, (int)result.error());
            return res;
        }

        httplib::Response response = result.value();

        res.http_status = response.status;
        if (response.status != 200) {
            res.log_message =
                fmt::format("Request to \"{}\" returned status {}", nasc_url, response.status);
            return res;
        }
        LOG_INFO(Service_FRD, "test5");
        LOG_INFO(Service_FRD, response.body.c_str());

        auto content_type = response.get_header_value("content-type");
        if (content_type == "" || content_type == "text/plain") {
            res.log_message = "Unknown response body from NASC server";
            return res;
        }

        NASCParams out_parameters;
        std::set<std::string> cache;
        httplib::detail::split(response.body.data(), response.body.data() + response.body.size(), '&', [&](const char *b, const char *e) {
            std::string kv(b, e);
            if (cache.find(kv) != cache.end()) { return; }
            cache.insert(kv);

            std::string key;
            std::string val;
            httplib::detail::split(b, e, '=', [&](const char *b2, const char *e2) {
            if (key.empty()) {
                key.assign(b2, e2);
            } else {
                val.assign(b2, e2);
            }
            });

            if (!key.empty()) {
            out_parameters.emplace_back(httplib::detail::decode_url(key, true), httplib::detail::decode_url(val, true));
            }
        });

        int nasc_result;
        if (!GetParameter(out_parameters, "returncd", nasc_result)) {
            res.log_message = "NASC response missing \"returncd\"";
            return res;
        }

        res.result = static_cast<u8>(nasc_result);
        if (nasc_result != 1) {
            res.log_message = fmt::format("NASC login failed with code 002-{:04d}", nasc_result);
            return res;
        }
        LOG_INFO(Service_FRD, "test7");

        std::string locator;
        if (!GetParameter(out_parameters, "locator", locator)) {
            res.log_message = "NASC response missing \"locator\"";
            return res;
        }

        auto delimiter = locator.find(":");
        if (delimiter == locator.npos) {
            res.log_message = "NASC response \"locator\" missing port delimiter";
            return res;
        }
        LOG_INFO(Service_FRD, "test8");
        res.server_address = locator.substr(0, delimiter);
        std::string port_str = locator.substr(delimiter + 1);
        try {
            res.server_port = (u16)std::stoi(port_str);
        } catch (std::exception const&) {
        }

        std::string tokenName("token");
        auto token = std::find_if(out_parameters.begin(), out_parameters.end(), [tokenName](const std::pair<std::string, std::string>& p) {
            return p.first == tokenName;
        });
        if (token == out_parameters.end()) {
            res.log_message = "NASC response missing \"locator\"";
            return res;
        }
        LOG_INFO(Service_FRD, "test9");

        res.auth_token = token->second;

        long long server_time;
        if (!GetParameter(out_parameters, "datetime", server_time)) {
            res.log_message = "NASC response missing \"datetime\"";
            return res;
        }
        res.time_stamp = server_time;
        LOG_INFO(Service_FRD, "test10");
        return res;
    }

    bool NASCClient::GetParameter(const NASCParams& param, const std::string& key,
                                  std::string& out) {
        using namespace CryptoPP;
        using Name::DecodingLookupArray;
        using Name::Pad;
        using Name::PaddingByte;

        auto field = std::find_if(param.begin(), param.end(), [key](const std::pair<std::string, std::string>& p) {
            return p.first == key;
        });
        if (field == param.end())
            return false;

        Base64Decoder decoder;
        int lookup[256];
        Base64Decoder::InitializeDecodingLookupArray(lookup, (const byte*)base64_dict.data(), 64,
                                                     false);
        AlgorithmParameters params = MakeParameters(DecodingLookupArray(), (const int*)lookup);

        decoder.IsolatedInitialize(params);
        decoder.Attach(new StringSink(out));
        decoder.Put(reinterpret_cast<const byte*>(field->second.data()), field->second.size());
        decoder.MessageEnd();
        return true;
    }

    bool NASCClient::GetParameter(const NASCParams& param, const std::string& key, int& out) {
        std::string out_str;
        if (!GetParameter(param, key, out_str)) {
            return false;
        }
        try {
            out = std::stoi(out_str);
            return true;
        } catch (std::exception const&) {
            return false;
        }
    }

    bool NASCClient::GetParameter(const NASCParams& param, const std::string& key,
                                  long long& out) {
        std::string out_str;
        if (!GetParameter(param, key, out_str)) {
            return false;
        }
        try {
            out = std::stoll(out_str);
            return true;
        } catch (std::exception const&) {
            return false;
        }
    }
} // namespace NetworkClient::NASC