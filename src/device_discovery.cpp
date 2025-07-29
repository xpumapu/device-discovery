#include "device_discovery.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <json-c/json.h>
#include <set>
#include <algorithm>

DeviceDiscovery::DeviceDiscovery(const std::string& interface) 
    : db(nullptr), interface_name(interface), network_range("192.168.10.0/24") {
}

DeviceDiscovery::~DeviceDiscovery() {
    if (db) {
        sqlite3_close(db);
    }
}

bool DeviceDiscovery::initialize() {
    return initDatabase();
}

bool DeviceDiscovery::initDatabase() {
    int rc = sqlite3_open("/tmp/device_discovery.db", &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS devices (
            mac_address TEXT PRIMARY KEY,
            ip_address TEXT,
            hostname TEXT,
            manufacturer TEXT,
            device_type TEXT,
            open_ports TEXT,
            is_online INTEGER,
            last_seen INTEGER,
            first_seen INTEGER
        );
    )";
    
    char* err_msg = nullptr;
    rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    
    return true;
}

void DeviceDiscovery::startDiscovery() {
    std::cout << "Starting device discovery on " << interface_name << std::endl;
    
    auto arp_devices = scanArpTable();

    //auto ping_devices = pingSweep(network_range);
    auto ping_arp_devices = pingArp(arp_devices);

    for (const std::string& ping_arp : ping_arp_devices) {
        std::cout << " ping_arp_device " << ping_arp << std::endl;
    }
    
    std::set<std::string> all_ips;
    all_ips.insert(arp_devices.begin(), arp_devices.end());
    all_ips.insert(ping_arp_devices.begin(), ping_arp_devices.end());
    
    for (const auto& ip : all_ips) {
        DeviceInfo device;
        device.ip_address = ip;
        device.mac_address = getMacFromArp(ip);
        device.hostname = getHostname(ip);
        device.manufacturer = getManufacturer(device.mac_address);
        device.open_ports = scanPorts(ip);
        device.device_type = classifyDevice(device);
        device.is_online = true;
        device.last_seen = time(nullptr);
        device.first_seen = time(nullptr);

        if (!device.mac_address.empty()) {
            discovered_devices[device.mac_address] = device;
            saveDevice(device);
        }
    }
    
    std::cout << "Discovery complete. Found " << discovered_devices.size() << " devices." << std::endl;
}

std::vector<std::string> DeviceDiscovery::scanArpTable() {
    std::vector<std::string> devices;
    std::string arp_output = executeCommand("cat /proc/net/arp | tail -n +2");
    
    std::istringstream iss(arp_output);
    std::string line;
    while (std::getline(iss, line)) {
        auto tokens = split(line, ' ');
        if (tokens.size() >= 1 && isValidIP(tokens[0])) {
            devices.push_back(tokens[0]);
        }
    }
    
    return devices;
}

std::vector<std::string> DeviceDiscovery::pingSweep(const std::string& network) {
    std::vector<std::string> active_devices;
    auto ip_range = generateIPRange(network);
    
    for (const auto& ip : ip_range) {
        std::string cmd = "ping -c 1 -W 1 " + ip + " > /dev/null 2>&1";
        if (system(cmd.c_str()) == 0) {
            active_devices.push_back(ip);
        }
    }
    
    return active_devices;
}

std::vector<std::string> DeviceDiscovery::pingArp(const std::vector<std::string>& arp_ips) {
    std::vector<std::string> active_arp_devices;

    for (const auto& ip : arp_ips) {
        std::string cmd = "ping -c 1 -W 1 " + ip + " > /dev/null 2>&1";
        if (system(cmd.c_str()) == 0) {
            active_arp_devices.push_back(ip);
        }
    }

    return active_arp_devices;
}

std::vector<int> DeviceDiscovery::scanPorts(const std::string& ip) {
    std::vector<int> open_ports;
    std::vector<int> common_ports = {22, 23, 53, 80, 443, 993, 995};
    
    for (int port : common_ports) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            open_ports.push_back(port);
        }
        
        close(sock);
    }
    
    return open_ports;
}

std::string DeviceDiscovery::getMacFromArp(const std::string& ip) {
    std::string cmd = "cat /proc/net/arp | tail -n +2 | grep " + ip;
    std::string output = executeCommand(cmd);
    std::string mac;
    
    size_t start = output.find_first_of(':');
    if (start != std::string::npos) {
        mac = output.substr(start - 2, 17);
        if (mac.length() > 0 ) {
            return mac;
        }
    }
    
    return "";
}

std::string DeviceDiscovery::getHostname(const std::string& ip) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    
    char hostname[256];
    if (getnameinfo((struct sockaddr*)&addr, sizeof(addr), hostname, sizeof(hostname), nullptr, 0, 0) == 0) {
        return std::string(hostname);
    }
    
    return "";
}

std::string DeviceDiscovery::getManufacturer(const std::string& mac) {
    if (mac.length() < 8) return "Unknown";
    
    std::string oui = mac.substr(0, 8);
    std::transform(oui.begin(), oui.end(), oui.begin(), ::toupper);
    
    std::map<std::string, std::string> oui_map = {
        {"00:0C:29", "VMware"},
        {"08:00:27", "VirtualBox"},
        {"B8:27:EB", "Raspberry Pi"},
        {"DC:A6:32", "Raspberry Pi"},
        {"E4:5F:01", "Raspberry Pi"},
        {"00:16:3E", "Xen"}
    };
    
    if (oui_map.find(oui) != oui_map.end()) {
        return oui_map[oui];
    }
    
    return "Unknown";
}

std::string DeviceDiscovery::classifyDevice(const DeviceInfo& device) {

    if (device.manufacturer.find("Raspberry") != std::string::npos) {
        return "Single Board Computer";
    }
    
    for (int port : device.open_ports) {
        switch (port) {
            case 22: return "Server/Router";
            case 80:
            case 443: return "Web Server";
            case 993:
            case 995: return "Mail Server";
        }
    }
    
    return "Unknown Device";
}

bool DeviceDiscovery::saveDevice(const DeviceInfo& device) {
    const char* sql = R"(
        INSERT OR REPLACE INTO devices 
        (mac_address, ip_address, hostname, manufacturer, device_type, open_ports, is_online, last_seen, first_seen)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    std::string ports_str;
    for (size_t i = 0; i < device.open_ports.size(); ++i) {
        if (i > 0) ports_str += ",";
        ports_str += std::to_string(device.open_ports[i]);
    }
    
    sqlite3_bind_text(stmt, 1, device.mac_address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, device.ip_address.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, device.hostname.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, device.manufacturer.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, device.device_type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, ports_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, device.is_online ? 1 : 0);
    sqlite3_bind_int64(stmt, 8, device.last_seen);
    sqlite3_bind_int64(stmt, 9, device.first_seen);
    
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return result == SQLITE_DONE;
}

void DeviceDiscovery::printDevices() {
    for (const auto& pair : discovered_devices) {
        const auto& device = pair.second;
        std::cout << "MAC: " << device.mac_address << std::endl;
        std::cout << "IP: " << device.ip_address << std::endl;
        std::cout << "Hostname: " << device.hostname << std::endl;
        std::cout << "Manufacturer: " << device.manufacturer << std::endl;
        std::cout << "Type: " << device.device_type << std::endl;
        std::cout << "Open Ports: ";
        for (int port : device.open_ports) {
            std::cout << port << " ";
        }
        std::cout << std::endl << "---" << std::endl;
    }
}

std::string executeCommand(const std::string& command) {
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
    } else {
        result = "pipe failed";
    }
    return result;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

bool DeviceDiscovery::isValidIP(const std::string& ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 0;
}

void DeviceDiscovery::setNetworkRange(const std::string& range) {
    network_range = range;
}

void DeviceDiscovery::continuousMonitoring() {
    std::cout << "Starting continuous monitoring mode..." << std::endl;
    while (true) {
        sleep(300);
        std::cout << "Rescanning network..." << std::endl;
        startDiscovery();
    }
}

void DeviceDiscovery::exportToJson(const std::string& filename) {
    json_object* root = json_object_new_object();
    json_object* devices_array = json_object_new_array();
    
    for (const auto& pair : discovered_devices) {
        const auto& device = pair.second;
        json_object* device_obj = json_object_new_object();
        
        json_object_object_add(device_obj, "mac_address", json_object_new_string(device.mac_address.c_str()));
        json_object_object_add(device_obj, "ip_address", json_object_new_string(device.ip_address.c_str()));
        json_object_object_add(device_obj, "hostname", json_object_new_string(device.hostname.c_str()));
        json_object_object_add(device_obj, "manufacturer", json_object_new_string(device.manufacturer.c_str()));
        json_object_object_add(device_obj, "device_type", json_object_new_string(device.device_type.c_str()));
        json_object_object_add(device_obj, "is_online", json_object_new_boolean(device.is_online));
        json_object_object_add(device_obj, "last_seen", json_object_new_int64(device.last_seen));
        json_object_object_add(device_obj, "first_seen", json_object_new_int64(device.first_seen));
        
        json_object* ports_array = json_object_new_array();
        for (int port : device.open_ports) {
            json_object_array_add(ports_array, json_object_new_int(port));
        }
        json_object_object_add(device_obj, "open_ports", ports_array);
        
        json_object_array_add(devices_array, device_obj);
    }
    
    json_object_object_add(root, "devices", devices_array);
    json_object_object_add(root, "scan_time", json_object_new_int64(time(nullptr)));
    
    std::ofstream file(filename);
    if (file.is_open()) {
        file << json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
        file.close();
    }
    
    json_object_put(root);
}

std::vector<DeviceInfo> DeviceDiscovery::getDevices() {
    std::vector<DeviceInfo> devices;
    for (const auto& pair : discovered_devices) {
        devices.push_back(pair.second);
    }
    return devices;
}

std::vector<std::string> DeviceDiscovery::generateIPRange(const std::string& network) {
    std::vector<std::string> ips;

    size_t slash = network.find('/');
    if (slash != std::string::npos) {
        std::string base = network.substr(0, slash);
        size_t last_dot = base.rfind('.');
        if (last_dot != std::string::npos) {
            std::string subnet = base.substr(0, last_dot + 1);
            for (int i = 1; i < 255; ++i) {
                ips.push_back(subnet + std::to_string(i));
            }
        }
    }
    return ips;
}