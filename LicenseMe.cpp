#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <optional>
#include <algorithm>
#include "db.h"
#include "paypal.h"

#define IS_LICENSE_VALID "IS_LICENSE_VALID"
#define REGISTER_ENTRY "REGISTER_ENTRY"
#define UPDATE_LICENSE_TO_VALID "UPDATE_LICENSE_TO_VALID"
#define DUMP_LIST "DUMP_LIST"

bool do_log = true;

std::string PrivilegeReadKey;
std::string PrivilegeWriteKey;

void SetupPrivilegeKeys()
{
    std::ifstream file("properties.txt");
    std::string line;

    if (!file)
    {
        std::cerr << "Failed to open properties.txt\n";
        return;
    }

    while (std::getline(file, line))
    {
        // Remove whitespace
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());

        // Fix typo: privilleges -> privileges
        std::string fixedLine = line;
        size_t typoPos = fixedLine.find("privilleges");
        if (typoPos != std::string::npos)
        {
            fixedLine.replace(typoPos, 11, "privileges");
        }

        // Parse key=value
        size_t eq = fixedLine.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = fixedLine.substr(0, eq);
        std::string value = fixedLine.substr(eq + 1);

        if (key == "read_privileges")
        {
            PrivilegeReadKey = value;
        }
        else if (key == "write_privileges")
        {
            PrivilegeWriteKey = value;
        }
    }

    if (!PrivilegeReadKey.empty() && !PrivilegeWriteKey.empty())
    {
        std::cout << "Privileges loaded successfully.\n";
    }
    else
    {
        std::cerr << "Missing read or write privilege key.\n";
    }
}

std::vector<std::string> SplitByPipe(const std::string& input) 
{
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;

    while (std::getline(ss, item, '|')) 
    {
        result.push_back(item);
    }

    return result;
}

bool IsPrevillegeOnRequest(const httplib::Request& req, boolean read) 
{
    for (const auto& header : req.headers)
    {
        if (read)
        {
            if (header.first == "pri"
                && header.second == PrivilegeReadKey)
            {
                return true;
            }
        }
        else
        {
            if (header.first == "pri"
                && header.second == PrivilegeWriteKey)
            {
                return true;
            }
        }
    }
    
    return false;
}

int main()
{
    SetupPrivilegeKeys();

    DB db;

    std::cout << "Starting server...\n";

    httplib::Server server;

    // Handle POST and GET requests to /log
    server.Get("/log", [](const httplib::Request& req, httplib::Response& res) {
        std::cout << "Received GET request\n";
        std::cout << "Path: " << req.path << "\n";
        std::cout << "Headers:\n";
        for (const auto& header : req.headers) 
        {
            std::cout << "  " << header.first << ": " << header.second << "\n";
        }

        res.set_content("GET request logged.\n", "text/plain");
        });

    server.Post("/license", [&db](const httplib::Request& req, httplib::Response& res) {
        std::cout << "---------------------------------------------------------------------" << std::endl;
        try
        {
            if (do_log)
            {
                std::cout << "Received POST request\n";
                std::cout << "Path: " << req.path << "\n";
                std::cout << "Body: " << req.body << "\n";
                std::cout << "Headers:\n";
            }

            if (IsPrevillegeOnRequest(req, true))
            {
                std::cout << "Pri Read OK" << std::endl;

                for (const auto& header : req.headers)
                {
                    std::cout << "header.key: " << header.first << ", header.value: " << header.second << std::endl;
                    if (header.first == IS_LICENSE_VALID)
                    {
                        bool isValid = db.IsLicenseValid(header.second);
                        res.set_content(isValid ? "YES" : "NO", "text/plain");
                    }
                }
            }
            else if (IsPrevillegeOnRequest(req, false))
            {
                std::cout << "Pri Write OK" << std::endl;

                for (const auto& header : req.headers)
                {
                    std::cout << "header.key: " << header.first << ", header.value: " << header.second << std::endl;
                    if (header.first == REGISTER_ENTRY)
                    {
                        std::vector<std::string> splited = SplitByPipe(header.second);
                        std::string orderId = splited[0];
                        std::string email = splited[1];
                        std::string uniqueId = splited[2];
                        if (verifyOrder(orderId))
                        {
                            db.CreateNewEntry(uniqueId, email);
                            res.set_content("Registered.", "text/plain");
                            std::cout << "REGISTER_ENTRY ALL GOOD" << std::endl;
                        }
                    }
                    else if (header.first == UPDATE_LICENSE_TO_VALID)
                    {
                        std::vector<std::string> splited = SplitByPipe(header.second);
                        std::string entry = splited[0];
                        db.UpdateLicenseToValid(entry);
                    }
                    else if (header.first == DUMP_LIST)
                    {
                        db.DumpLicenseTable();
                    }
                }
            }
            else
            {
                std::cout << "pri OK" << std::endl;
                res.set_content("NOT OK", "text/plain");
            }
        }
        catch (const std::exception& ex)
        {
            std::cout << "exception: " << ex.what() << std::endl;
            res.set_content(std::string("Internal error: ") + ex.what(), "text/plain");
        }
        catch (...)
        {
            std::cout << "Unknown error." << std::endl;
            res.set_content("Unknown error occurred", "text/plain");
        }

        });

    std::cout << "Server started at 80\n";
    server.listen("0.0.0.0", 80);

    return 0;
}