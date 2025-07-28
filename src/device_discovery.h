#ifndef DEVICE_DISCOVERY_H
#define DEVICE_DISCOVERY_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sqlite3.h>
#include <pcap.h>

struct DeviceInfo {
    std::string mac_address;
    std::string ip_address;
    std::string hostname;
    std::string manufacturer;
    std::string device_type;
    std::vector<int> open_ports;
    bool is_online;
    time_t last_seen;
    time_t first_seen;
};

class DeviceDiscovery {
private:
    sqlite3* db;
    std::string interface_name;
    std::string network_range;
    std::map<std::string, DeviceInfo> discovered_devices;
    
    bool initDatabase();
    bool saveDevice(const DeviceInfo& device);
    bool loadDevices();

    std::vector<std::string> scanArpTable();
    std::vector<std::string> pingSweep(const std::string& network);
    std::vector<int> scanPorts(const std::string& ip);
    std::string getMacFromArp(const std::string& ip);
    std::string getHostname(const std::string& ip);
    std::string getManufacturer(const std::string& mac);
    std::string classifyDevice(const DeviceInfo& device);
    

    bool isValidIP(const std::string& ip);
    std::vector<std::string> generateIPRange(const std::string& network);
    
public:
    DeviceDiscovery(const std::string& interface = "br-lan");
    ~DeviceDiscovery();
    
    bool initialize();
    void startDiscovery();
    void continuousMonitoring();
    void exportToJson(const std::string& filename);
    void printDevices();
    std::vector<DeviceInfo> getDevices();
    void setNetworkRange(const std::string& range);
};

std::string executeCommand(const std::string& command);
std::vector<std::string> split(const std::string& str, char delimiter);
bool fileExists(const std::string& filename);

#endif // DEVICE_DISCOVERY_H