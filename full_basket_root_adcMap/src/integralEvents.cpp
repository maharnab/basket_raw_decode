
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <fstream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <filesystem>
#include <string>
#include <chrono>
#include <nlohmann/json.hpp>
#include "TFile.h"
#include "TTree.h"

using json = nlohmann::json;

std::vector<std::string> get_adc_addresses_from_json(const std::string& json_file, int basket_num) {
    std::ifstream ifs(json_file);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open adcMap.json file: " + json_file);
    }
    json j;
    ifs >> j;
    for (const auto& basket : j) {
        if (basket.contains("basket")) {
            int json_basket_num = basket["basket"].get<int>();
            if (json_basket_num == basket_num) {
                std::vector<std::string> addresses;
                for (int i = 1; i <= 12; ++i) {
                    std::string key = std::to_string(i);
                    if (basket.contains(key) && !basket[key].get<std::string>().empty()) {
                        std::string addr = basket[key].get<std::string>();
                        // preserve leading zeros, convert to lowercase for consistency
                        std::transform(addr.begin(), addr.end(), addr.begin(), ::tolower);
                        addresses.push_back(addr);
                    } else {
                        addresses.push_back("");
                    }
                }
                return addresses;
            }
        }
    }
    throw std::runtime_error("Basket number not found in adcMap.json: " + std::to_string(basket_num));
}


void print_usage() {
    std::cout << "\nUsage: ./integralEvents [options] <datafile> <minPosition_lower> <output_path> <basket_number>\n";
    std::cout << "       For multiple files, use -F <file_list.txt> instead of <datafile>.\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -M <adcMap.json>         Path to ADC map JSON file (default: ../adcMap.json)\n";
    std::cout << "  -F <file_list.txt>       Path to file containing list of .data files to process\n";
    std::cout << "  --max-events <N>         Maximum number of events to process (default: unlimited)\n";
    std::cout << "\nExample:\n";
    std::cout << "  ./integralEvents -M /path/to/adcMap.json /path/to/data.data $MIN_POSITION $OUTPUT_PATH $BASKET_NUMBER\n";
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        print_usage();
        return 0;
    }

    // Variable declarations

    // Argument variables
    std::string adc_map_json = "../adcMap.json";
    std::string file_list_path;
    std::vector<std::string> data_files;
    int minPosition_lower = 0;
    std::string base_dir;
    int basket_num = -1;
    int64_t max_events = -1;
    bool use_file_list = false;

    // Output/processing variables
    auto start = std::chrono::high_resolution_clock::now();
    bool output_initialized = false;
    std::string outputFilePath;
    TFile* rootFile = nullptr;
    TTree* tree = nullptr;
    int event_number = 0;
    uint32_t device_id = 0;
    uint64_t timestamp = 0;
    int channel_number = 0;
    int channel_value = 0;
    std::vector<std::string> adc_addresses;

    // Unified argument parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-M") {
            if (i + 1 < argc) {
                adc_map_json = argv[++i];
            } else {
                std::cerr << "[ERROR] -M requires a path to adcMap.json.\n";
                print_usage();
                return 1;
            }
        } else if (arg == "-F") {
            if (i + 1 < argc) {
                use_file_list = true;
                file_list_path = argv[++i];
            } else {
                std::cerr << "[ERROR] -F requires a file list path.\n";
                print_usage();
                return 1;
            }
        } else if (arg == "--max-events") {
            if (i + 1 < argc) {
                max_events = std::stoll(argv[++i]);
            } else {
                std::cerr << "[ERROR] --max-events requires a value.\n";
                print_usage();
                return 1;
            }
        } else if (arg[0] != '-') {
            data_files.push_back(arg);
        }
    }

    // Positional argument validation and assignment
    if (use_file_list) {
        if (data_files.size() < 3) {
            std::cerr << "[ERROR] Not enough positional arguments after -F <file_list.txt>.\n";
            print_usage();
            return 1;
        }
        minPosition_lower = std::stoi(data_files[data_files.size() - 3]);
        base_dir = data_files[data_files.size() - 2];
        basket_num = std::stoi(data_files[data_files.size() - 1]);
        data_files.resize(data_files.size() - 3); // Only file list files remain
    } else {
        if (data_files.size() < 4) {
            std::cerr << "[ERROR] Not enough positional arguments.\n";
            print_usage();
            return 1;
        }
        minPosition_lower = std::stoi(data_files[data_files.size() - 3]);
        base_dir = data_files[data_files.size() - 2];
        basket_num = std::stoi(data_files[data_files.size() - 1]);
        std::string datafile = data_files[0];
        data_files.clear();
        data_files.push_back(datafile);
    }

    // If -F was used, read file list and populate data_files
    if (use_file_list) {
        std::ifstream flist(file_list_path);
        if (!flist.is_open()) {
            std::cerr << "[ERROR] Could not open file list: " << file_list_path << std::endl;
            return 1;
        }
        std::string line;
        while (std::getline(flist, line)) {
            if (!line.empty()) data_files.push_back(line);
        }
        flist.close();
    }

    // For multi-file mode, set outputFilePath early
    if (use_file_list) {
        // Extract first and last .data filenames from the file list
        std::vector<std::string> filelist_entries;
        std::ifstream flist(file_list_path);
        if (flist.is_open()) {
            std::string line;
            while (std::getline(flist, line)) {
                if (!line.empty()) filelist_entries.push_back(line);
            }
            flist.close();
        }
        std::string first_file, last_file;
        if (!filelist_entries.empty()) {
            first_file = std::filesystem::path(filelist_entries.front()).filename().string();
            last_file = std::filesystem::path(filelist_entries.back()).filename().string();
        } else {
            first_file = "unknown";
            last_file = "unknown";
        }

        // Helper lambda to extract number before .data and after last _
        auto extract_num = [](const std::string& fname) -> std::string {
            size_t dot = fname.rfind(".data");
            if (dot == std::string::npos) return "unknown";
            size_t under = fname.rfind('_', dot);
            if (under == std::string::npos || under + 1 >= dot) return "unknown";
            return fname.substr(under + 1, dot - under - 1);
        };
        std::string first_num = extract_num(first_file);
        std::string last_num = extract_num(last_file);

        // Use date and base_dir as before
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm* tm_ptr = std::localtime(&now_c);
        char year_month_day[11];
        std::strftime(year_month_day, sizeof(year_month_day), "%Y-%m-%d", tm_ptr);

        // Use the same output dir logic as before
        std::filesystem::path out_dir = std::filesystem::path(base_dir) / year_month_day;
        std::filesystem::create_directories(out_dir);

        // Compose output file name: something_firstnum-lastnum.root
        std::string out_name = "basket" + std::to_string(basket_num) + "_" + first_num + "-" + last_num + ".root";
        outputFilePath = (out_dir / out_name).string();
    }

    // Load ADC addresses for the selected basket before processing files
    try {
        adc_addresses = get_adc_addresses_from_json(adc_map_json, basket_num);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }

    int32_t event_number_offset = 0;
    bool reached_max_events = false;
    for (size_t file_idx = 0; file_idx < data_files.size() && !reached_max_events; ++file_idx) {
        const std::string& filePath = data_files[file_idx];

        if (std::filesystem::exists(filePath) && !std::filesystem::is_directory(filePath)) {
            if (filePath.size() >= 5 && filePath.substr(filePath.size() - 5) == ".data") {
                std::ifstream file(filePath, std::ios::binary);
                if (!file.is_open()) {
                    std::cerr << "[ERROR] Failed to open the file: " << filePath << std::endl;
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

                while (bytesRead < totalSize && file.read(buffer.data(), bufferSize) && !reached_max_events) {
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
                                std::filesystem::path out_dir = std::filesystem::path(base_dir) / year_month_day / filename;
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

                        // Trust event block structure: each event is a block, process all ADCs for the event
                        // Use words[0] as event number, offset for multi-file
                        event_number = static_cast<int>(words[0]) + event_number_offset;

                        wordOffset = 0;
                        while (static_cast<std::vector<unsigned int>::size_type>(wordOffset) < (words.size() - 1) && !reached_max_events) {
                            device_id = words[1 + wordOffset];
                            std::stringstream device_id_ss;
                            device_id_ss << std::hex << std::setw(8) << std::setfill('0') << std::nouppercase << device_id;
                            std::string device_id_str = device_id_ss.str();
                            adcPayloadLength = static_cast<int>(words[2 + wordOffset] & 0xFFFFFF) / 4;
                            timeStampSec_ns = static_cast<uint64_t>(words[4 + wordOffset]) * 1e9;
                            timeStampNanoSec = static_cast<uint64_t>((words[5 + wordOffset] & 0xFFFFFFFC) >> 2);
                            timestamp = timeStampSec_ns + timeStampNanoSec;

                            int adcOrder = -1;
                            for (size_t idx = 0; idx < adc_addresses.size(); ++idx) {
                                if (device_id_str == adc_addresses[idx]) {
                                    adcOrder = static_cast<int>(idx) + 1;
                                    break;
                                }
                            }
                            if (adcOrder == -1) {
                                std::cerr << "[ERROR] ADC address not found for device_id '" << device_id_str << "' in basket_num " << basket_num << std::endl;
                                std::cerr << "[ERROR] ADC addresses for basket " << basket_num << ":\n";
                                for (size_t i = 0; i < adc_addresses.size(); ++i) {
                                    std::cerr << "  Channel " << (i+1) << ": '" << adc_addresses[i] << "'\n";
                                }
                                exit(2);
                            }

                            if (adcPayloadLength > 5) {
                                if ((size_t)(8 + wordOffset) >= words.size()) {
                                    std::cerr << "[ERROR] Out-of-bounds access in ch_details. wordOffset=" << wordOffset << ", words.size()=" << words.size() << std::endl;
                                    std::cerr << "[ERROR] basket_num=" << basket_num << std::endl;
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
                                            std::cerr << "[ERROR] Out-of-bounds access in waveform loop. idx=" << idx << ", words.size()=" << words.size() << std::endl;
                                            std::cerr << "[ERROR] basket_num=" << basket_num << std::endl;
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
                                                std::cerr << "[ERROR] basket_num=" << basket_num << std::endl;
                                                exit(6);
                                            }
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

                                            // event_number is already set for this event
                                            tree->Fill();
                                        }
                                    }
                                }
                            }
                            wordOffset += (2 + adcPayloadLength);

                            // Check if we've reached the max events
                            if (max_events > 0 && event_number >= max_events) {
                                reached_max_events = true;
                                break;
                            }
                        }

                        words.clear();
                        bufferSize = 4;
                        buffer.resize(bufferSize);
                        state = 0;
                        ss = std::stringstream();
                    }

                    bytesRead += file.gcount();
                }
                // For multi-file: update event_number_offset to last event number in this file
                if (!words.empty()) {
                    int last_event_number = static_cast<int>(words[0]);
                    event_number_offset += last_event_number;
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
