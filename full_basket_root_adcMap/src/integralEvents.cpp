

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <map>
#include <filesystem>
#include <string>
#include <chrono>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include "TFile.h"
#include "TTree.h"

using json = nlohmann::json;

std::vector<uint32_t> get_adc_addresses_from_json(const std::string& json_file, int basket_num) {
    std::ifstream ifs(json_file);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open adcMap.json file: " + json_file);
    }
    json j;
    ifs >> j;
    std::cerr << "[DEBUG] get_adc_addresses_from_json: Looking for basket_num " << basket_num << std::endl;
    for (const auto& basket : j) {
        if (basket.contains("basket")) {
            int json_basket_num = basket["basket"].get<int>();
            std::cerr << "[DEBUG] Checking basket in JSON: " << json_basket_num << std::endl;
            if (json_basket_num == basket_num) {
                std::cerr << "[DEBUG] Matched basket_num " << basket_num << " in JSON." << std::endl;
                std::vector<uint32_t> addresses;
                for (int i = 1; i <= 12; ++i) {
                    std::string key = std::to_string(i);
                    if (basket.contains(key) && !basket[key].get<std::string>().empty()) {
                        addresses.push_back(static_cast<uint32_t>(std::stoul(basket[key].get<std::string>(), nullptr, 16)));
                    } else {
                        addresses.push_back(0); // or throw, or skip, depending on your needs
                    }
                }
                return addresses;
            }
        }
    }
    std::cerr << "[DEBUG] No matching basket_num found in JSON for " << basket_num << std::endl;
    throw std::runtime_error("Basket number not found in adcMap.json: " + std::to_string(basket_num));
}

int main(int argc, char* argv[]) {

    // Get the directory of the running binary
    char exePath[4096];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    std::string bin_dir = ".";
    if (len != -1) {
        exePath[len] = '\0';
        bin_dir = std::filesystem::path(exePath).parent_path();
    }
    std::string adc_map_json = bin_dir + "/../adcMap.json";
    int arg_offset = 0;
    bool use_file_list = false;
    std::string file_list_path;
    std::vector<std::string> data_files;
    // minPosition_lower, base_dir, basket_num are declared below, do not redeclare here

    // ...existing code...


    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-M") {
            if (i + 1 < argc) {
                adc_map_json = argv[i + 1];
                arg_offset = i + 1;
                i++;
            }
        } else if (std::string(argv[i]) == "-F") {
            if (i + 1 < argc) {
                use_file_list = true;
                file_list_path = argv[i + 1];
                arg_offset = i + 1;
                i++;
            }
        }
    }

    int minPosition_lower = -1;
    std::string base_dir;
    int basket_num = -1;
    int canary = 0xDEADBEEF;
    std::cerr << "[DEBUG] basket_num address at start of main: " << &basket_num << ", value: " << basket_num << std::endl;
    std::cerr << "[DEBUG] canary address at start of main: " << &canary << ", value: 0x" << std::hex << canary << std::dec << std::endl;

    if (use_file_list) {
        // -F mode: [other options] -F [file_list.txt] [minPosition_lower] [output_base_dir] [basket_number]
        if (argc < arg_offset + 4) {
            std::cerr << "Usage: " << argv[0] << " [-M adcMap_snap.json] -F [file_list.txt] [minPosition_lower] [output_base_dir] [basket_number]" << std::endl;
            return 1;
        }
        minPosition_lower = atoi(argv[arg_offset + 1]);
        base_dir = argv[arg_offset + 2];
        basket_num = std::stoi(argv[arg_offset + 3]);
        // Read file list
        std::ifstream flist(file_list_path);
        if (!flist.is_open()) {
            std::cerr << "Failed to open file list: " << file_list_path << std::endl;
            return 1;
        }
        std::string line;
        while (std::getline(flist, line)) {
            if (!line.empty()) data_files.push_back(line);
        }
        flist.close();
        if (data_files.empty()) {
            std::cerr << "No data files found in list." << std::endl;
            return 1;
        }
        // Print debug info after all variables are initialized (file list mode)
        std::cerr << "[DEBUG] Program parameters:" << std::endl;
        std::cerr << "  adc_map_json: " << adc_map_json << std::endl;
        std::cerr << "  minPosition_lower: " << minPosition_lower << std::endl;
        std::cerr << "  base_dir: " << base_dir << std::endl;
        std::cerr << "  basket_num: " << basket_num << " (address: " << &basket_num << ")" << std::endl;
        std::cerr << "  canary: 0x" << std::hex << canary << std::dec << " (address: " << &canary << ")" << std::endl;
        std::cerr << "  use_file_list: true" << std::endl;
        std::cerr << "  file_list_path: " << file_list_path << std::endl;
        std::cerr << "  data_files:" << std::endl;
        for (const auto& f : data_files) {
            std::cerr << "    " << f << std::endl;
        }
    } else {
        // Single file mode
        if (argc > 2 && std::string(argv[1]) == "-M") {
            if (argc < 7) {
                std::cerr << "Usage: " << argv[0] << " -M [adcMap_snap.json] [binary_file_directory_path] [minPosition_lower] [output_base_dir] [basket_number]" << std::endl;
                return 1;
            }
            arg_offset = 2;
        } else if (argc < 5) {
            std::cerr << "Usage: " << argv[0] << " [binary_file_directory_path] [minPosition_lower] [output_base_dir] [basket_number]" << std::endl;
            std::cerr << "   or: " << argv[0] << " -M [adcMap_snap.json] [binary_file_directory_path] [minPosition_lower] [output_base_dir] [basket_number]" << std::endl;
            return 1;
        }
        data_files.push_back(argv[1 + arg_offset]);
        minPosition_lower = atoi(argv[2 + arg_offset]);
        base_dir = argv[3 + arg_offset];
        basket_num = std::stoi(argv[4 + arg_offset]);
        // Print debug info after all variables are initialized (single file mode)
        std::cerr << "[DEBUG] Program parameters:" << std::endl;
        std::cerr << "  adc_map_json: " << adc_map_json << std::endl;
        std::cerr << "  minPosition_lower: " << minPosition_lower << std::endl;
        std::cerr << "  base_dir: " << base_dir << std::endl;
        std::cerr << "  basket_num: " << basket_num << " (address: " << &basket_num << ")" << std::endl;
        std::cerr << "  canary: 0x" << std::hex << canary << std::dec << " (address: " << &canary << ")" << std::endl;
        std::cerr << "  use_file_list: false" << std::endl;
        std::cerr << "  data_files:" << std::endl;
        for (const auto& f : data_files) {
            std::cerr << "    " << f << std::endl;
        }
    }

    // Print debug info after all variables are initialized
    std::cerr << "[DEBUG] Program parameters (final):" << std::endl;
    std::cerr << "  adc_map_json: " << adc_map_json << std::endl;
    std::cerr << "  minPosition_lower: " << minPosition_lower << std::endl;
    std::cerr << "  base_dir: " << base_dir << std::endl;
    std::cerr << "  basket_num: " << basket_num << " (address: " << &basket_num << ")" << std::endl;
    std::cerr << "  canary: 0x" << std::hex << canary << std::dec << " (address: " << &canary << ")" << std::endl;
    std::cerr << "  use_file_list: " << (use_file_list ? "true" : "false") << std::endl;
    if (use_file_list) {
        std::cerr << "  file_list_path: " << file_list_path << std::endl;
    }
    std::cerr << "  data_files:" << std::endl;
    for (const auto& f : data_files) {
        std::cerr << "    " << f << std::endl;
    }

    std::vector<uint32_t> adc_addresses;
    try {
        adc_addresses = get_adc_addresses_from_json(adc_map_json, basket_num);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    auto start = std::chrono::high_resolution_clock::now();

    std::string outputFilePath;
    TFile* rootFile = nullptr;
    TTree* tree = nullptr;
    int32_t event_number; // Will be used as global event number
    // int32_t file_event_number; // If you want to keep per-file event number, uncomment this
    uint32_t device_id;
    uint64_t timestamp; // ROOT expects ULong64_t for 'l' type
    int32_t channel_number;
    int32_t channel_value;
    bool output_initialized = false;

    // Determine output file name if using file list
    if (use_file_list) {
        // Get first and last file names
        std::string first_file = std::filesystem::path(data_files.front()).filename().string();
        std::string last_file = std::filesystem::path(data_files.back()).filename().string();
        // Extract prefix and last number
        size_t dash_pos = first_file.find_last_of("_");
        size_t dot_pos = last_file.rfind(".data");
        std::string prefix = (dash_pos != std::string::npos) ? first_file.substr(0, dash_pos) : first_file;
        std::string first_num = (dash_pos != std::string::npos) ? first_file.substr(dash_pos + 1, first_file.find(".data") - dash_pos - 1) : "";
        std::string last_num = (dot_pos != std::string::npos) ? last_file.substr(last_file.find_last_of("_") + 1, dot_pos - last_file.find_last_of("_") - 1) : "";
        std::string out_name = prefix + "_" + first_num + "-" + last_num + ".root";
        // Use first file's timestamp for directory structure
        std::string first_file_path = data_files.front();
        std::ifstream file(first_file_path, std::ios::binary);
        uint64_t first_timestamp = 0;
        if (file.is_open()) {
            // Read enough to get timestamp (skip to event header)
            int64_t bufferSize = 4;
            std::vector<char> buffer(bufferSize);
            int state = 0;
            unsigned int sync_word_evnt = 0x2A50D5AF;
            std::vector<uint32_t> words;
            while (file.read(buffer.data(), bufferSize)) {
                if (bufferSize == 4) {
                    uint32_t word = static_cast<uint8_t>(buffer[3]) << 24 |
                                    static_cast<uint8_t>(buffer[2]) << 16 |
                                    static_cast<uint8_t>(buffer[1]) << 8 |
                                    static_cast<uint8_t>(buffer[0]);
                    if (word == sync_word_evnt && state == 0) {
                        state++;
                    } else if (state == 1) {
                        bufferSize = static_cast<int>(word);
                        buffer.resize(bufferSize);
                    }
                } else if (bufferSize > 4) {
                    for (int i = 0; i < bufferSize / 4; i++) {
                        uint32_t word = static_cast<uint8_t>(buffer[3 + 4 * i]) << 24 |
                                        static_cast<uint8_t>(buffer[2 + 4 * i]) << 16 |
                                        static_cast<uint8_t>(buffer[1 + 4 * i]) << 8 |
                                        static_cast<uint8_t>(buffer[0 + 4 * i]);
                        words.push_back(word);
                    }
                    if (words.size() > 5) {
                        uint64_t timeStampSec_ns = static_cast<uint64_t>(words[4]) * 1e9;
                        uint64_t timeStampNanoSec = static_cast<uint64_t>((words[5] & 0xFFFFFFFC) >> 2);
                        first_timestamp = timeStampSec_ns + timeStampNanoSec;
                    }
                    break;
                }
            }
            file.close();
        }
        std::time_t t = first_timestamp / 1000000000ULL;
        std::tm* tm_ptr = std::localtime(&t);
        char year[5], year_month[8], year_month_day[11];
        std::strftime(year, sizeof(year), "%Y", tm_ptr);
        std::strftime(year_month, sizeof(year_month), "%Y-%m", tm_ptr);
        std::strftime(year_month_day, sizeof(year_month_day), "%Y-%m-%d", tm_ptr);
        std::string filename = prefix + "_" + first_num;
        std::filesystem::path out_dir = std::filesystem::path(base_dir) / year / year_month / year_month_day / filename;
        std::filesystem::create_directories(out_dir);
        outputFilePath = (out_dir / out_name).string();
    }


    int32_t global_event_number = 0;
    for (size_t file_idx = 0; file_idx < data_files.size(); ++file_idx) {
        const std::string& filePath = data_files[file_idx];
        if (std::filesystem::exists(filePath) && !std::filesystem::is_directory(filePath)) {
            if (filePath.size() >= 5 && filePath.substr(filePath.size() - 5) == ".data") {
                std::ifstream file(filePath, std::ios::binary);
                if (!file.is_open()) {
                    std::cerr << "Failed to open the file: " << filePath << std::endl;
                    return 1;
                }

                int64_t bufferSize = 4;
                std::vector<char> buffer(bufferSize);
                int64_t bytesRead = 0;
                unsigned int sync_word_evnt = 0x2A50D5AF;

                int state = 0;
                int channelNumber, adcPayloadLength;
                uint64_t timeStampNanoSec, timeStampSec_ns;
                int64_t wordOffset = 0;
                std::string deviceSerialNumber = "";
                std::stringstream ss;
                std::vector<uint32_t> words;

                // Find the total size of the file
                file.seekg(0, file.end);
                int64_t totalSize = file.tellg();
                file.seekg(0, file.beg);

                std::string fileName = std::filesystem::path(filePath).filename().string();

                std::cout << std::endl;
                std::time_t now1 = std::time(nullptr);
                std::cout << "[ " << now1 << " ] : " << "Processing the file: " << fileName << std::endl;
                int waveform_words;
                int lastPercentage = -1;

                while (bytesRead < totalSize && file.read(buffer.data(), bufferSize)) {
                    int percentage = (double)bytesRead / totalSize * 100;
                    if (percentage != lastPercentage) {
                        auto end = std::chrono::high_resolution_clock::now();
                        std::chrono::duration<double> elapsed = end - start;
                        double rate = (double)bytesRead / 1e6 / elapsed.count();
                        std::time_t now2 = std::time(nullptr);
                        std::cout << "[ " << now2 << " ] : " << std::setw(2) << std::setfill(' ') << percentage
                                    << " % completed in " << std::setw(5) << std::setfill(' ') << std::fixed << std::setprecision(1) << elapsed.count()
                                    << " s @ " << std::setw(4) << std::setfill(' ') << std::fixed << std::setprecision(1) << rate << " MB/s." << std::endl;
                        lastPercentage = percentage;
                    }

                    if (bufferSize == 4) {
                        uint32_t word = static_cast<uint8_t>(buffer[3]) << 24 |
                                        static_cast<uint8_t>(buffer[2]) << 16 |
                                        static_cast<uint8_t>(buffer[1]) << 8 |
                                        static_cast<uint8_t>(buffer[0]);
                        if (word == sync_word_evnt && state == 0) {
                            state += 1;
                        } else if (state == 1) {
                            bufferSize = static_cast<int>(word);
                            buffer.resize(bufferSize);
                        }
                    } else if (bufferSize > 4) {
                        for (int i = 0; i < bufferSize / 4; i++) {
                            uint32_t word = static_cast<uint8_t>(buffer[3 + 4 * i]) << 24 |
                                            static_cast<uint8_t>(buffer[2 + 4 * i]) << 16 |
                                            static_cast<uint8_t>(buffer[1 + 4 * i]) << 8 |
                                            static_cast<uint8_t>(buffer[0 + 4 * i]);
                            words.push_back(word);
                        }

                        wordOffset = 0;
                        // eventNumber = static_cast<int>(words[0]); // Removed unused variable
                        // event_number = eventNumber; // Remove this, use global_event_number instead
                        // Only initialize output on first event of first file (for single file mode, this is always true)
                        if (!output_initialized) {
                            uint64_t first_timestamp = 0;
                            if (words.size() > 5) {
                                uint64_t timeStampSec_ns = static_cast<uint64_t>(words[4]) * 1e9;
                                uint64_t timeStampNanoSec = static_cast<uint64_t>((words[5] & 0xFFFFFFFC) >> 2);
                                first_timestamp = timeStampSec_ns + timeStampNanoSec;
                            }
                            std::time_t t = first_timestamp / 1000000000ULL;
                            std::tm* tm_ptr = std::localtime(&t);
                            char year[5], year_month[8], year_month_day[11];
                            std::strftime(year, sizeof(year), "%Y", tm_ptr);
                            std::strftime(year_month, sizeof(year_month), "%Y-%m", tm_ptr);
                            std::strftime(year_month_day, sizeof(year_month_day), "%Y-%m-%d", tm_ptr);
                            std::string filename;
                            std::string out_name;
                            if (use_file_list) {
                                // Already determined outputFilePath above
                                filename = std::filesystem::path(data_files.front()).filename().string();
                                out_name = std::filesystem::path(outputFilePath).filename().string();
                            } else {
                                filename = std::filesystem::path(filePath).filename().string();
                                size_t reduced_pos = filename.find("_reduced");
                                if (reduced_pos != std::string::npos) filename.erase(reduced_pos, 8);
                                size_t data_pos = filename.rfind(".data");
                                if (data_pos != std::string::npos) filename.erase(data_pos);
                                out_name = filename + ".root";
                                std::filesystem::path out_dir = std::filesystem::path(base_dir) / year / year_month / year_month_day / filename;
                                std::filesystem::create_directories(out_dir);
                                outputFilePath = (out_dir / out_name).string();
                            }
                            rootFile = new TFile(outputFilePath.c_str(), "RECREATE");
                            if (!rootFile->IsOpen()) {
                                std::cerr << "Failed to open the output ROOT file at " << outputFilePath << std::endl;
                                return 1;
                            }
                            tree = new TTree("events", "Event data");
                            tree->SetAutoSave(0);
                            tree->Branch("event_number", &event_number, "event_number/I");
                            tree->Branch("device_id", &device_id, "device_id/i");
                            tree->Branch("timestamp", &timestamp, "timestamp/l");
                            tree->Branch("channel_number", &channel_number, "channel_number/I");
                            tree->Branch("channel_value", &channel_value, "channel_value/I");
                            output_initialized = true;
                        }

                        while (static_cast<std::vector<unsigned int>::size_type>(wordOffset) < (words.size() - 1)) {

                            // Bounds check before accessing words vector
                            if ((size_t)(1 + wordOffset) >= words.size() || (size_t)(2 + wordOffset) >= words.size() || (size_t)(4 + wordOffset) >= words.size() || (size_t)(5 + wordOffset) >= words.size()) {
                                std::cerr << "[ERROR] Out-of-bounds access detected in event header parsing. wordOffset=" << wordOffset << ", words.size()=" << words.size() << std::endl;
                                std::cerr << "[ERROR] basket_num address: " << &basket_num << ", value: " << basket_num << std::endl;
                                exit(3);
                            }
                            device_id = words[1 + wordOffset];
                            adcPayloadLength = static_cast<int>(words[2 + wordOffset] & 0xFFFFFF) / 4;
                            timeStampSec_ns = static_cast<uint64_t>(words[4 + wordOffset]) * 1e9;
                            timeStampNanoSec = static_cast<uint64_t>((words[5 + wordOffset] & 0xFFFFFFFC) >> 2);
                            timestamp = timeStampSec_ns + timeStampNanoSec;

                            int adcOrder = -1;
                            for (size_t idx = 0; idx < adc_addresses.size(); ++idx) {
                                if (device_id == adc_addresses[idx]) {
                                    adcOrder = static_cast<int>(idx) + 1;
                                    break;
                                }
                            }
                            if (adcOrder == -1) {
                                std::cerr << "[DEBUG] Entered ADC address error block. basket_num: " << basket_num << ", canary: 0x" << std::hex << canary << std::dec << std::endl;
                                std::cerr << "[DEBUG] basket_num address: " << &basket_num << ", canary address: " << &canary << std::endl;
                                std::cerr << "[DEBUG] basket_num after address print: " << basket_num << ", canary: 0x" << std::hex << canary << std::dec << std::endl;
                                std::cerr << "[DEBUG] adc_addresses vector: ";
                                for (const auto& addr : adc_addresses) {
                                    std::cerr << std::hex << std::setw(8) << std::setfill('0') << addr << " ";
                                    std::cerr << " [basket_num: " << std::dec << basket_num << ", canary: 0x" << std::hex << canary << std::dec << "] ";
                                }
                                std::cerr << std::endl;
                                std::cerr << "[DEBUG] basket_num after adc_addresses vector print: " << basket_num << ", canary: 0x" << std::hex << canary << std::dec << std::endl;
                                std::cerr << "[DEBUG] basket_num just before error print: " << basket_num << ", canary: 0x" << std::hex << canary << std::dec << std::endl;
                                std::cerr << "ADC addresses for basket (DEBUG MARKER) " << basket_num << ":" << std::endl;
                                for (const auto& addr : adc_addresses) {
                                    std::cerr << std::hex << std::setw(8) << std::setfill('0') << addr << " [basket_num: " << std::dec << basket_num << ", canary: 0x" << std::hex << canary << std::dec << "]" << std::endl;
                                    std::cerr << "[DEBUG] basket_num after printing addr: " << basket_num << ", canary: 0x" << std::hex << canary << std::dec << std::endl;
                                }
                                std::cerr << "[DEBUG] basket_num at end of error block: " << basket_num << ", canary: 0x" << std::hex << canary << std::dec << std::endl;
                                exit(2);
                            }

                            if (adcPayloadLength > 5) {
                                if ((size_t)(8 + wordOffset) >= words.size()) {
                                    std::cerr << "[ERROR] Out-of-bounds access detected in ch_details. wordOffset=" << wordOffset << ", words.size()=" << words.size() << std::endl;
                                    std::cerr << "[ERROR] basket_num address: " << &basket_num << ", value: " << basket_num << std::endl;
                                    exit(4);
                                }
                                uint32_t ch_details = words[8 + wordOffset];
                                uint8_t ch_details_last_byte = ch_details & 0xFF;
                                waveform_words = (ch_details_last_byte - 1) / 4 + 1;

                                int n_waveform_loops = (adcPayloadLength - 5) / waveform_words;
                                for (int j = 0; j < n_waveform_loops; j++) {
                                    std::vector<int> adcValues;
                                    for (int k = 0; k < waveform_words; k++) {
                                        size_t idx = 8 + j * waveform_words + k + wordOffset;
                                        if (idx >= words.size()) {
                                            std::cerr << "[ERROR] Out-of-bounds access detected in waveform loop. idx=" << idx << ", words.size()=" << words.size() << std::endl;
                                            std::cerr << "[ERROR] basket_num address: " << &basket_num << ", value: " << basket_num << std::endl;
                                            exit(5);
                                        }
                                        if (k == 0) {
                                            channelNumber = static_cast<int>((words[idx] >> 24) + 1);
                                            adcValues.push_back(channelNumber);
                                        }
                                        if (k > 2) {
                                            int16_t left = static_cast<int16_t>(words[idx] >> 16) & 0xFFFF;
                                            int16_t right = static_cast<int16_t>(words[idx]) & 0xFFFF;
                                            adcValues.push_back(left - 30000);
                                            adcValues.push_back(right - 30000);
                                        }
                                    }

                                    auto minElement = std::min_element(adcValues.begin(), adcValues.end());
                                    int minPosition = std::distance(adcValues.begin(), minElement);

                                    if (*minElement < -100) {
                                        if (minPosition > minPosition_lower && minPosition < minPosition_lower + 30) {
                                            int start_idx = minPosition - 4;
                                            int end_idx = minPosition + 16;
                                            if (start_idx < 0 || end_idx > static_cast<int>(adcValues.size())) {
                                                std::cerr << "[ERROR] Out-of-bounds access in slicedVector. minPosition=" << minPosition << ", adcValues.size()=" << adcValues.size() << std::endl;
                                                std::cerr << "[ERROR] slicedVector indices: start_idx=" << start_idx << ", end_idx=" << end_idx << std::endl;
                                                std::cerr << "[ERROR] basket_num address: " << &basket_num << ", value: " << basket_num << std::endl;
                                                exit(6);
                                            }
                                            std::cerr << "[DEBUG] Constructing slicedVector: start_idx=" << start_idx << ", end_idx=" << end_idx << ", adcValues.size()=" << adcValues.size() << std::endl;
                                            std::vector<int> slicedVector(adcValues.begin() + start_idx, adcValues.begin() + end_idx);

                                            double pedestal = 0.0;
                                            if (adcValues.size() > 10) {
                                                pedestal = std::accumulate(adcValues.begin() + 1, adcValues.begin() + 11, 0.0) / 10.0;
                                            }

                                            std::vector<int> slicedVectorPedSub;
                                            slicedVectorPedSub.reserve(slicedVector.size());
                                            for (const auto& val : slicedVector) {
                                                slicedVectorPedSub.push_back(val - static_cast<int>(pedestal));
                                            }

                                            int integral = std::accumulate(slicedVectorPedSub.begin(), slicedVectorPedSub.end(), 0);
                                            channel_number = adcValues[0] + int(adcOrder - 1) * 64;
                                            channel_value = integral * -1;

                                            event_number = global_event_number;
                                            global_event_number++;
                                            tree->Fill();
                                        }
                                    }
                                }
                            }
                            wordOffset += (2 + adcPayloadLength);
                        }

                        words.clear();
                        bufferSize = 4;
                        buffer.resize(bufferSize);
                        state = 0;
                        ss = std::stringstream();
                    }

                    bytesRead += file.gcount();
                }
                file.close();
            }
        }
    }

    if (output_initialized) {
        rootFile->Write();
        rootFile->Close();
        delete rootFile;
    }
    return 0;
}
