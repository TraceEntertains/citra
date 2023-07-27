// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <httplib.h>
#include "common/common_types.h"
#include "map"
#include "string"
#include "vector"

using NASCParams = std::list<std::pair<std::string, std::string>>;
namespace NetworkClient::NASC {
    class NASCClient {
    public:
        struct NASCResult {
            u8 result = 0;
            int http_status;
            std::string log_message;

            std::string server_address;
            u16 server_port;
            std::string auth_token;
            u64 time_stamp;
        };

        NASCClient(const std::string& url, const std::vector<u8>& cert, const std::vector<u8>& key)
            : nasc_url(url) {
            Initialize(cert, key);
        }

        ~NASCClient() {
            if (client_cert) {
                X509_free(client_cert);
                client_cert = nullptr;
            }
            if (client_priv_key) {
                EVP_PKEY_free(client_priv_key);
                client_priv_key = nullptr;
            }
        }

        void Clear() {
            parameters.clear();
        }


        void SetParameter(const std::string& key, int value) {
            SetParameter(key, std::to_string(value));
        }

        void SetParameter(const std::string& key, const std::string& value) {
            SetParameter(key, std::vector<u8>(value.cbegin(), value.cend()));
        }

        void SetParameter(const std::string& key, const std::vector<u8>& value) {
            SetParameterImpl(key, value);
        }

        NASCResult Perform();

    private:
        const std::string base64_dict =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-";

        std::string nasc_url;
        X509* client_cert = nullptr;
        EVP_PKEY* client_priv_key = nullptr;

        void Initialize(const std::vector<u8>& cert, const std::vector<u8>& key);

        NASCParams parameters;

        void SetParameterImpl(const std::string& key, const std::vector<u8>& value);

        bool GetParameter(const NASCParams& param, const std::string& key, std::string& out);
        bool GetParameter(const NASCParams& param, const std::string& key, int& out);
        bool GetParameter(const NASCParams& param, const std::string& key, long long& out);
    };
} // namespace NetworkClient::NASC