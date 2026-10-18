#pragma once

#include <string>
#include <memory>
#include <mutex>

// Forward declarations for project classes used in signatures
namespace org { namespace minima { namespace objects { namespace base {
class MiniData;
} } } }

namespace org { namespace minima { namespace utils { namespace json {
class JSONObject;
class JSONArray;
} } } }

struct sqlite3; // forward declare SQLite C handle

namespace org {
namespace minima {
namespace utils {

class SqlDB {
public:
    SqlDB();
    virtual ~SqlDB();

    // The actual database file on disk (full path as a string)
    std::string getSQLFile() const;

    // Specify the location of the DB (creates parent dirs if needed)
    // Throws std::runtime_error on failure
    void loadDB(const std::string& zFile);

    // Encrypted load request - flag and password stored, but note:
    // Stock SQLite3 does not support encryption without extensions.
    // We still open the database normally and call createSQL().
    void loadEncryptedSQLDB(const std::string& zFile, const std::string& zPassword);

    // Is the DB connection open
    bool isOpen() const;

    // Reopen if necessary. Returns true if reopened, false otherwise.
    bool checkOpen();
    bool checkOpen(bool zLogs);

    // Force close the DB (if open)
    void hardCloseDB();

    // Save and close. If zCompact = true, VACUUM before closing.
    void saveDB(bool zCompact);

    // Backup to file (uses SQLite online backup API). zGZIP is ignored.
    void backupToFile(const std::string& zBackupFile);
    void backupToFile(const std::string& zBackupFile, bool zGZIP);

    // Restore from a DB file (uses SQLite online backup API). zGZIP is ignored.
    void restoreFromFile(const std::string& zRestoreFile);
    void restoreFromFile(const std::string& zRestoreFile, bool zGZIP);

    // Utility Functions
    // Parse JSON text stored in MiniData into a JSONObject.
    // Throws ParseException on parse errors if not an object.
    static std::unique_ptr<org::minima::utils::json::JSONObject>
    convertDataToJSONObject(const org::minima::objects::base::MiniData& zData);

    // Convert JSONObject to MiniData containing its textual JSON representation.
    static std::unique_ptr<org::minima::objects::base::MiniData>
    convertJSONObjectToData(const org::minima::utils::json::JSONObject& zJSON);

    // Only one thread can access the db at a time
    // Execute arbitrary SQL; returns a JSON response mirroring the Java structure:
    // { "sql": string, "status": bool, "results": bool, "count": int, "rows": JSONArray, "error": string? }
    std::unique_ptr<org::minima::utils::json::JSONObject> executeGenericSQL(const std::string& zSQL);

protected:
    // Perform the Create SQL (pure virtual)
    virtual void createSQL() = 0;

public:
    // Apply performance / durability pragmas to an arbitrary sqlite3 handle.
    // Called by SqlDB subclasses and by standalone SQLite users (TxPoWSqlDB).
    static void applySQLiteTuning(sqlite3* db);

private:
    // Helpers
    void openSQLite(const std::string& path);

    // Apply tuning pragmas to this object's connection.
    void applySQLiteTuning();

private:
    sqlite3* mSQLConnection;           // SQLite connection handle (nullptr if closed)
    std::string mSQLFile;              // actual DB file used
    std::string mSQLDBNoMV;            // base path used to open (same as mSQLFile in this port)

    bool mEncrypted;                   // compatibility flags (not enforced by stock SQLite)
    std::string mEncryptedPassword;

    mutable std::mutex mMutex;         // for synchronized executeGenericSQL
};

} // namespace utils
} // namespace minima
} // namespace org