#include "device_discovery.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -i <interface>  Network interface (default: br-lan)" << std::endl;
    std::cout << "  -n <network>    Network range (default: 192.168.1.0/24)" << std::endl;
    std::cout << "  -m              Continuous monitoring mode" << std::endl;
    std::cout << "  -j <file>       Export results to JSON file" << std::endl;
    std::cout << "  -h              Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string interface = "br-lan";
    std::string network = "192.168.10.0/24";
    std::string json_file;
    bool continuous_mode = false;
    
    int opt;
    while ((opt = getopt(argc, argv, "i:n:mj:h")) != -1) {
        switch (opt) {
            case 'i':
                interface = optarg;
                break;
            case 'n':
                network = optarg;
                break;
            case 'm':
                continuous_mode = true;
                break;
            case 'j':
                json_file = optarg;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    DeviceDiscovery discovery(interface);
    
    if (!discovery.initialize()) {
        std::cerr << "Failed to initialize device discovery" << std::endl;
        return 1;
    }
    
    discovery.setNetworkRange(network);
    
    std::cout << "Device Discovery Tool v1.0" << std::endl;
    std::cout << "Interface: " << interface << std::endl;
    std::cout << "Network: " << network << std::endl;
    std::cout << "=========================" << std::endl;
    
    discovery.startDiscovery();
    discovery.printDevices();
    
    if (!json_file.empty()) {
        discovery.exportToJson(json_file);
        std::cout << "Results exported to " << json_file << std::endl;
    }
    
    if (continuous_mode) {
        std::cout << "Starting continuous monitoring..." << std::endl;
        discovery.continuousMonitoring();
    }
    
    return 0;
}