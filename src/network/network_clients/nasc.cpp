#include <memory>
#include <boost/algorithm/string/replace.hpp>
#include <cryptopp/base64.h>
#include <fmt/format.h>
#include "common/file_util.h"
#include "nasc.h"

namespace NetworkClient::NASC {

    constexpr std::size_t TIMEOUT_SECONDS = 15;

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

        parameters.emplace(key, out);
    }

    NASCClient::NASCResult NASCClient::Perform() {
        std::unique_ptr<httplib::SSLClient> cli;
        httplib::Request request;
        NASCResult res;

        if (client_cert == nullptr || client_priv_key == nullptr) {
            res.log_message = "Missing or invalid client certificate or key.";
            return res;
        }

        cli = std::make_unique<httplib::SSLClient>(nasc_url.c_str(), 443, client_cert, client_priv_key);
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
            request.set_header("User-Agent", fmt::format("CTR FPD/{}", header_param));
        }
        request.set_header("Content-Type", "application/x-www-form-urlencoded");

        request.method = "POST";
        request.path = "/ac";
        request.body = httplib::detail::params_to_query_str(parameters);
        boost::replace_all(request.body, "*", "%2A");

        httplib::Result result = cli->send(request);
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

        auto content_type = response.headers.find("content-type");
        if (content_type == response.headers.end() ||
            content_type->second.find("text/plain") == content_type->second.npos) {
            res.log_message = "Unknown response body from NASC server";
            return res;
        }

        httplib::Params out_parameters;
        httplib::detail::parse_query_text(response.body, out_parameters);

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
        res.server_address = locator.substr(0, delimiter);
        std::string port_str = locator.substr(delimiter + 1);
        try {
            res.server_port = (u16)std::stoi(port_str);
        } catch (std::exception const&) {
        }

        auto token = out_parameters.find("token");
        if (token == out_parameters.end()) {
            res.log_message = "NASC response missing \"locator\"";
            return res;
        }

        res.auth_token = token->second;

        long long server_time;
        if (!GetParameter(out_parameters, "datetime", server_time)) {
            res.log_message = "NASC response missing \"datetime\"";
            return res;
        }
        res.time_stamp = server_time;
        return res;
    }

    bool NASCClient::GetParameter(const httplib::Params& param, const std::string& key,
                                  std::string& out) {
        using namespace CryptoPP;
        using Name::DecodingLookupArray;
        using Name::Pad;
        using Name::PaddingByte;

        auto field = param.find(key);
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

    bool NASCClient::GetParameter(const httplib::Params& param, const std::string& key, int& out) {
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

    bool NASCClient::GetParameter(const httplib::Params& param, const std::string& key,
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