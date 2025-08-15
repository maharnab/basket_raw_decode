#include <iostream>
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <map>
#include <set>
#include <filesystem>
#include <string>
#include <chrono>

#include "TFile.h"
#include "TTree.h"


int main(int argc, char* argv[]) {
    int arg_offset = 0;
    bool use_file_list = false;
    std::string file_list_path;
    std::vector<std::string> data_files;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-F") {
            if (i + 1 < argc) {
                use_file_list = true;
                file_list_path = argv[i + 1];
                arg_offset = i + 1;
                i++;
            }
        }
    }

    std::string base_dir;
    if (use_file_list) {
        // -F mode: -F [file_list.txt] [output_base_dir]
        if (argc < arg_offset + 2) {
            std::cerr << "Usage: " << argv[0] << " -F [file_list.txt] [output_base_dir]" << std::endl;
            return 1;
        }
        base_dir = argv[arg_offset + 1];
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
    } else {
        // Single file mode
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " [binary_file_directory_path] [output_base_dir]" << std::endl;
            return 1;
        }
        data_files.push_back(argv[1]);
        base_dir = argv[2];
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::string outputFilePath;
    TFile* rootFile = nullptr;
    TTree* tree = nullptr;
    int32_t event_number;
    uint64_t min_timestamp;
    uint64_t max_timestamp;
    uint64_t diff_timestamp;
    int32_t multiplicity;
    bool output_initialized = false;
    std::map<int, int> multiplicity_counts;
    int total_events = 0;

    // Determine output file name if using file list
    if (use_file_list) {
        std::string first_file = std::filesystem::path(data_files.front()).filename().string();
        std::string last_file = std::filesystem::path(data_files.back()).filename().string();
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
                int eventNumber, adcPayloadLength;
                uint64_t timeStampNanoSec, timeStampSec_ns;
                int64_t wordOffset = 0;
                std::stringstream ss;
                std::vector<uint32_t> words;

                file.seekg(0, file.end);
                int64_t totalSize = file.tellg();
                file.seekg(0, file.beg);

                std::string fileName = std::filesystem::path(filePath).filename().string();

                std::cout << std::endl;
                std::time_t now1 = std::time(nullptr);
                std::cout << "[ " << now1 << " ] : " << "Processing the file: " << fileName << std::endl;
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
                        uint32_t word;
                        word = static_cast<uint8_t>(buffer[3]) << 24 |
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
                            uint32_t word;
                            word = static_cast<uint8_t>(buffer[3 + 4 * i]) << 24 |
                                   static_cast<uint8_t>(buffer[2 + 4 * i]) << 16 |
                                   static_cast<uint8_t>(buffer[1 + 4 * i]) << 8 |
                                   static_cast<uint8_t>(buffer[0 + 4 * i]);
                            words.push_back(word);
                        }

                        wordOffset = 0;
                        eventNumber = static_cast<int>(words[0]);
                        event_number = eventNumber;

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
                            tree->Branch("event_number", &event_number, "event_number/I");
                            tree->Branch("min_timestamp", &min_timestamp, "min_timestamp/l");
                            tree->Branch("max_timestamp", &max_timestamp, "max_timestamp/l");
                            tree->Branch("diff_timestamp", &diff_timestamp, "diff_timestamp/l");
                            tree->Branch("multiplicity", &multiplicity, "multiplicity/I");
                            output_initialized = true;
                        }

                        std::vector<uint64_t> event_timestamps;

                        while (static_cast<std::vector<unsigned int>::size_type>(wordOffset) < (words.size() - 1)) {
                            adcPayloadLength = static_cast<int>(words[2 + wordOffset] & 0xFFFFFF) / 4;
                            timeStampSec_ns = static_cast<uint64_t>(words[4 + wordOffset]) * 1e9;
                            timeStampNanoSec = static_cast<uint64_t>((words[5 + wordOffset] & 0xFFFFFFFC) >> 2);
                            uint64_t timestamp = timeStampSec_ns + timeStampNanoSec;
                            event_timestamps.push_back(timestamp);
                            wordOffset += (2 + adcPayloadLength);
                        }

                        if (!event_timestamps.empty()) {
                            min_timestamp = *std::min_element(event_timestamps.begin(), event_timestamps.end());
                            max_timestamp = *std::max_element(event_timestamps.begin(), event_timestamps.end());
                            diff_timestamp = max_timestamp - min_timestamp;
                            std::set<uint32_t> unique_device_ids;
                            wordOffset = 0;
                            while (static_cast<std::vector<unsigned int>::size_type>(wordOffset) < (words.size() - 1)) {
                                uint32_t device_id = words[1 + wordOffset];
                                adcPayloadLength = static_cast<int>(words[2 + wordOffset] & 0xFFFFFF) / 4;
                                if (adcPayloadLength > 5) {
                                    unique_device_ids.insert(device_id);
                                }
                                wordOffset += (2 + adcPayloadLength);
                            }
                            multiplicity = unique_device_ids.size();
                            tree->Fill();
                            multiplicity_counts[multiplicity]++;
                            total_events++;
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

    if (total_events > 0) {
        std::cout << "\nMultiplicity statistics (as % of total events):" << std::endl;
        for (const auto& kv : multiplicity_counts) {
            double percent = 100.0 * kv.second / total_events;
            std::cout << "Multiplicity " << kv.first << ": " << kv.second << " events (" << std::fixed << std::setprecision(4) << percent << "%)" << std::endl;
        }
    }
    return 0;
}
