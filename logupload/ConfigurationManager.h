#include <string>
#include <unordered_map>

#define LOG_UPLOAD_SUCCESS 0
#define LOG_UPLOAD_FAILED  1      
#define LOG_UPLOAD_ABORTED 2 

struct UploadConfig {
    std::string uploadProtocol;
    std::string uploadUrl;
    std::string deviceMac;
    std::string firmwareVersion;
    int uploadFlag = 0;
    int dcmFlag = 0;
    int rrdFlag = 0;
    int triggerType = 0;
    int numUploadAttempts = 3;
    int curlTimeout = 10;
    int curlTlsTimeout = 30;
    bool encryptionEnabled = false;
    bool mtlsEnabled = false;
    bool ocspEnabled = false;
};

class ConfigurationManager {
public:
    // Load both config files in order, as per shell script
    bool loadAllConfigs(); // Loads /etc/include.properties, then /etc/device.properties

    // Loads a single property file (for completeness)
    bool loadFile(const std::string& path);

    // Parses argv, populates outConfig, overlays/confirms with properties
    bool parseArgs(int argc, char* argv[], UploadConfig& outConfig);

    // Fetches loaded property
    std::string getProperty(const std::string& key) const;

private:
    std::unordered_map<std::string, std::string> properties_;

    static std::string trim(const std::string& s);
};
