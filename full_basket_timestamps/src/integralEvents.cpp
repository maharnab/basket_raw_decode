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

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " [binary_file_directory_path] [output_base_dir]" << std::endl;
        return 1;
    }

    const char* filePath = argv[1];
    std::string base_dir = argv[2];
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

    if (std::filesystem::exists(filePath) && !std::filesystem::is_directory(filePath)) {
        if (std::string(filePath).size() >= 5 && std::string(filePath).substr(std::string(filePath).size() - 5) == ".data") {
            std::ifstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                std::cerr << "Failed to open the file." << std::endl;
                return 1;
            }

            int64_t bufferSize = 4;
            std::vector<char> buffer(bufferSize);
            int64_t bytesRead = 0;
            unsigned int sync_word_evnt = 0x2A50D5AF; //Event

            int state = 0;
            int eventNumber, adcPayloadLength;
            uint64_t timeStampNanoSec, timeStampSec_ns;
            int64_t wordOffset = 0;
            std::stringstream ss;
            std::vector<uint32_t> words;

            // Find the total size of the file
            file.seekg(0, file.end);
            int64_t totalSize = file.tellg();
            file.seekg(0, file.beg);

            std::string fileName = filePath;
            fileName = fileName.substr(fileName.find_last_of("/\\") + 1);

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

                    // Extract timestamp for output path (only for first event)
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
                        std::string filename = std::filesystem::path(filePath).filename().string();
                        size_t reduced_pos = filename.find("_reduced");
                        if (reduced_pos != std::string::npos) filename.erase(reduced_pos, 8);
                        size_t data_pos = filename.rfind(".data");
                        if (data_pos != std::string::npos) filename.erase(data_pos);
                        std::filesystem::path out_dir = std::filesystem::path(base_dir) / year / year_month / year_month_day / filename;
                        std::filesystem::create_directories(out_dir);
                        outputFilePath = (out_dir / (filename + ".root")).string();
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

                    // Collect timestamps for all device_ids in this event
                    std::vector<uint64_t> event_timestamps;

                    while (static_cast<std::vector<unsigned int>::size_type>(wordOffset) < (words.size() - 1)) {
                        adcPayloadLength = static_cast<int>(words[2 + wordOffset] & 0xFFFFFF) / 4;
                        timeStampSec_ns = static_cast<uint64_t>(words[4 + wordOffset]) * 1e9;
                        timeStampNanoSec = static_cast<uint64_t>((words[5 + wordOffset] & 0xFFFFFFFC) >> 2);
                        uint64_t timestamp = timeStampSec_ns + timeStampNanoSec;
                        event_timestamps.push_back(timestamp);
                        wordOffset += (2 + adcPayloadLength);
                    }

                    // Compute min, max, and diff
                    if (!event_timestamps.empty()) {
                        min_timestamp = *std::min_element(event_timestamps.begin(), event_timestamps.end());
                        max_timestamp = *std::max_element(event_timestamps.begin(), event_timestamps.end());
                        diff_timestamp = max_timestamp - min_timestamp;
                        // Calculate multiplicity: only count device_ids with one or more channel with ADC values
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

                    // reset the state, buffers and clear the words vector
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

    if (output_initialized) {
        rootFile->Write();
        rootFile->Close();
        delete rootFile; // Cleanup
    }

    // Print multiplicity statistics
    if (total_events > 0) {
        std::cout << "\nMultiplicity statistics (as % of total events):" << std::endl;
        for (const auto& kv : multiplicity_counts) {
            double percent = 100.0 * kv.second / total_events;
            std::cout << "Multiplicity " << kv.first << ": " << kv.second << " events (" << std::fixed << std::setprecision(4) << percent << "%)" << std::endl;
        }
    }
    return 0;
}
