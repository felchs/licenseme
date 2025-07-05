#pragma once

#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>

using json = nlohmann::json;

std::string base64Encode(const std::string& input)
{
    static const char* base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string encoded;
    int val = 0, valb = -6;

    for (unsigned char c : input)
    {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6)
    {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (encoded.size() % 4)
    {
        encoded.push_back('=');
    }

    return encoded;
}

std::string getAccessToken(const std::string& clientId, const std::string& clientSecret, bool sandbox = true)
{
    httplib::SSLClient cli(sandbox ? "api.sandbox.paypal.com" : "api.paypal.com", 443);
    cli.set_follow_location(true);

    std::string auth = base64Encode(clientId + ":" + clientSecret);
    httplib::Headers headers = {
        { "Authorization", "Basic " + auth },
        { "Content-Type", "application/x-www-form-urlencoded" }
    };

    std::string body = "grant_type=client_credentials";

    auto res = cli.Post("/v1/oauth2/token", headers, body, "application/x-www-form-urlencoded");

    if (!res || res->status != 200)
    {
        std::cerr << "Failed to get token. Status: " << (res ? res->status : 0) << "\n";
        return "";
    }

    auto json_body = json::parse(res->body);
    return json_body["access_token"];
}

bool verifyOrder(const std::string& orderId, const std::string& accessToken, bool sandbox = true)
{
    httplib::SSLClient cli(sandbox ? "api.sandbox.paypal.com" : "api.paypal.com", 443);
    cli.set_follow_location(true);

    httplib::Headers headers = {
        { "Authorization", "Bearer " + accessToken }
    };

    std::string path = "/v2/checkout/orders/" + orderId;
    auto res = cli.Get(path.c_str(), headers);

    if (!res || res->status != 200)
    {
        std::cerr << "Failed to verify payment. Status: " << (res ? res->status : 0) << "\n";
        return 0;
    }

    std::cout << "verifyOK on order: " << orderId << std::endl;

    //json::parse(res->body);
    return 1;
}

bool verifyOrder(const std::string& orderId)
{
    std::string clientId = "AcmUlUxLsdw-GhQPQ5n6bT-oXlX-HFFMNNdLY3qUAdDo_a4H3ALkXaOHLKL4VKk99S8gRGLBq2RZH0vL";
    std::string clientSecret = "EP5NnC8_P-COm9bShgj7fpw8Hk_PPHvreiaILb7gJGpIyxPGP9hLmncxM30-5EemEsFqKvWH2H0Ay4vj";

    std::string token = getAccessToken(clientId, clientSecret);
    std::cout << "token: " << token << std::endl;
    if (token.empty())
    {
        std::cerr << "Token retrieval failed.\n";
        return 0;
    }

    bool orderOk = verifyOrder(orderId, token);

    std::cout << "orderOK: " << (orderOk ? "YES" : "NO") << std::endl;
    if (!orderOk)
    {
        std::cerr << "Order verification failed.\n";
        return 0;
    }

    std::cout << "returning OK" << std::endl;
    //std::cout << "Status: " << order["status"] << "\n";
    //std::cout << "Email: " << order["payer"]["email_address"] << "\n";
    return 1;
}