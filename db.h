#pragma once

#include <iostream>
#include <sqlite3.h>
#include <string>
#include <random>
#include <chrono>
#include "picosha2.h"

class DB
{
public:
	DB()
	{
        std::cout << "StartDB" << std::endl;

        char* errMsg = nullptr;

        // Open database
        if (sqlite3_open("license.db", &db))
        {
            std::cerr << "Can't open DB: " << sqlite3_errmsg(db) << "\n";
            return;
        }

        // Create table
        const char* createTableSQL = "CREATE TABLE IF NOT EXISTS license (id INTEGER PRIMARY KEY, unique_id TEXT UNIQUE, email TEXT, date TEXT, option TEXT);";
        if (sqlite3_exec(db, createTableSQL, 0, 0, &errMsg) != SQLITE_OK)
        {
            std::cerr << "Create table error: " << errMsg << "\n";
            sqlite3_free(errMsg);
        }
	}

    ~DB()
    {
        sqlite3_close(db);
    }

    std::string getUnixTimestampAsString()
    {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto timestamp = duration_cast<seconds>(now.time_since_epoch()).count();
        return std::to_string(timestamp);
    }

    void CreateNewEntry(std::string uniqueId, std::string email)
    {
        //std::string key = generateRandomString();
        //std::string hash = picosha2::hash256_hex_string(entry + key);
        std::string option = "VALID";
        std::string date = getUnixTimestampAsString();
        const char* sql = R"(
                              INSERT INTO license (unique_id, email, date, option)
                              VALUES (?, ?, ?, ?)
                              ON CONFLICT(unique_id) DO UPDATE SET
                                unique_id = excluded.unique_id,
                                email = excluded.email,
                                date = excluded.date,
                                option = excluded.option;
                            )";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
        }
        else
        {
            sqlite3_bind_text(stmt, 1, uniqueId.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, date.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, option.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
                std::cerr << "Execution failed: " << sqlite3_errmsg(db) << "\n";
            }
            else
            {
                std::cout << "Inserted successfully.\n";
            }

            // Finalize the statement
            sqlite3_finalize(stmt);
        }
    }

    void UpdateLicenseToValid(const std::string& uniqueId)
    {
        const char* sql = "UPDATE license SET option = 'VALID' WHERE unique_id = ?";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
            return;
        }

        if (sqlite3_bind_text(stmt, 1, uniqueId.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
        {
            std::cerr << "Bind failed: " << sqlite3_errmsg(db) << "\n";
            sqlite3_finalize(stmt);
            return;
        }

        // Execute the update
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            std::cerr << "Update failed: " << sqlite3_errmsg(db) << "\n";
        }
        else
        {
            std::cout << "License updated to VALID for unique_id: " << uniqueId << "\n";
        }

        sqlite3_finalize(stmt);
    }

    bool IsLicenseValid(const std::string& uniqueId)
    {
        std::cout << "IsLicenseValid, unique_id: " << uniqueId << std::endl;
        const char* sql = "SELECT option FROM license WHERE unique_id = ? LIMIT 1";

        sqlite3_stmt* stmt = nullptr;
        bool isValid = false;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "Prepare failed: " << sqlite3_errmsg(db) << "\n";
            return false;
        }

        if (sqlite3_bind_text(stmt, 1, uniqueId.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
        {
            std::cerr << "Bind failed: " << sqlite3_errmsg(db) << "\n";
            sqlite3_finalize(stmt);
            return false;
        }

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            std::cout << "IsLicenseValid 3 " << std::endl;
            const unsigned char* opt = sqlite3_column_text(stmt, 0);
            std::cout << "IS_VALID, option: " << opt << std::endl;

            if (opt && std::string(reinterpret_cast<const char*>(opt)) == "VALID")
            {
                isValid = true;
            }
            std::cout << "IS_VALID, isValid: " << (isValid ? "T" : "F") << std::endl;
        }

        sqlite3_finalize(stmt);

        return isValid;
    }

    void DumpLicenseTable()
    {
        const char* sql = "SELECT unique_id, email, date, option FROM license";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        {
            std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << "\n";
            return;
        }

        std::cout << "------------ BEGIN License Table ------------" << std::endl;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char* unique_id = sqlite3_column_text(stmt, 0);
            const unsigned char* email = sqlite3_column_text(stmt, 1);
            const unsigned char* date = sqlite3_column_text(stmt, 2);
            const unsigned char* option = sqlite3_column_text(stmt, 3);

            std::cout << "UniequeId: " << (unique_id ? reinterpret_cast<const char*>(unique_id) : "(null)") << "\n"
                << "Email: " << (email ? reinterpret_cast<const char*>(email) : "(null)") << "\n"
                << "Date: " << (date ? reinterpret_cast<const char*>(date) : "(null)") << "\n"
                << "Option: " << (option ? reinterpret_cast<const char*>(option) : "(null)") << "\n"
                << "------------------------" << std::endl;
        }

        std::cout << "------------ END Table ------------" << std::endl;
        sqlite3_finalize(stmt);
    }

    /*
    void QueryExample()
    {

        // Query data
        const char* selectSQL = "SELECT id, name, age FROM people;";
        auto callback = [](void*, int argc, char** argv, char** colNames) -> int
            {
                for (int i = 0; i < argc; i++)
                {
                    std::cout << colNames[i] << ": " << (argv[i] ? argv[i] : "NULL") << "\n";
                }
                std::cout << "----\n";
                return 0;
            };

        char* errMsg = nullptr;
        if (sqlite3_exec(db, selectSQL, callback, 0, &errMsg) != SQLITE_OK)
        {
            std::cerr << "Select error: " << errMsg << "\n";
            sqlite3_free(errMsg);
        }
    }
    */

private:
    sqlite3* db;

    std::string generateRandomString(size_t length = 10)
    {
        const std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, charset.size() - 1);

        std::string result;
        for (size_t i = 0; i < length; ++i)
        {
            result += charset[dist(gen)];
        }
        return result;
    }
};
