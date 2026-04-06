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

#include "TFile.h"
#include "TTree.h"

int main(int argc, char* argv[]) {

    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " [binary_file_directory_path] [minPosition_lower] [output_base_dir]" << std::endl;
        return 1;
    }

    const char* filePath = argv[1];
    int minPosition_lower = atoi(argv[2]);              // 25 is default
    std::string base_dir = argv[3];
    auto start = std::chrono::high_resolution_clock::now();

    std::string outputFilePath;
    TFile* rootFile = nullptr;
    TTree* tree = nullptr;
    int32_t event_number;
    uint32_t device_id;
    uint64_t timestamp; // ROOT expects ULong64_t for 'l' type
    int32_t channel_number;
    int32_t channel_value;
    bool output_initialized = false;

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
            unsigned int sync_word_evnt = 0x2A50D5AF;

            int state = 0;
            int eventNumber, channelNumber, adcPayloadLength;
            uint64_t timeStampNanoSec, timeStampSec_ns;
            int64_t wordOffset = 0;
            std::string deviceSerialNumber = "";
            std::stringstream ss;
            std::vector<uint32_t> words;

            file.seekg(0, file.end);
            int64_t totalSize = file.tellg();
            file.seekg(0, file.beg);

            std::string fileName = filePath;
            fileName = fileName.substr(fileName.find_last_of("/\\") + 1);

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

                if (bufferSize==4){
                    uint32_t word;
                    word =  static_cast<uint8_t>(buffer[3]) << 24 |
                            static_cast<uint8_t>(buffer[2]) << 16 |
                            static_cast<uint8_t>(buffer[1]) << 8 |
                            static_cast<uint8_t>(buffer[0]);
                    if (word == sync_word_evnt && state == 0) {
                        state += 1;
                    }
                    else if (state == 1){
                        bufferSize = static_cast<int>(word);
                        buffer.resize(bufferSize);
                    }
                }
                else if (bufferSize>4){
                    for (int i = 0; i < bufferSize/4; i++) {
                        uint32_t word;
                        word =  static_cast<uint8_t>(buffer[3+4*i]) << 24 |
                                static_cast<uint8_t>(buffer[2+4*i]) << 16 |
                                static_cast<uint8_t>(buffer[1+4*i]) << 8 |
                                static_cast<uint8_t>(buffer[0+4*i]);
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
                        char year_month_day[11];
                        std::strftime(year_month_day, sizeof(year_month_day), "%Y-%m-%d", tm_ptr);
                        std::string filename = std::filesystem::path(filePath).filename().string();
                        size_t reduced_pos = filename.find("_reduced");
                        if (reduced_pos != std::string::npos) filename.erase(reduced_pos, 8);
                        size_t data_pos = filename.rfind(".data");
                        if (data_pos != std::string::npos) filename.erase(data_pos);
                        std::filesystem::path out_dir = std::filesystem::path(base_dir) / year_month_day;
                        std::filesystem::create_directories(out_dir);
                        outputFilePath = (out_dir / (filename + ".root")).string();
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

                    int adcNumber = 0;

                    while (static_cast<std::vector<unsigned int>::size_type>(wordOffset) < (words.size() - 1)) {
                        device_id = words[1 + wordOffset];
                        adcPayloadLength = static_cast<int>(words[2 + wordOffset] & 0xFFFFFF) / 4;
                        timeStampSec_ns = static_cast<uint64_t>(words[4 + wordOffset]) *1e9;
                        timeStampNanoSec = static_cast<uint64_t>((words[5 + wordOffset] & 0xFFFFFFFC) >> 2);
                        timestamp = timeStampSec_ns + timeStampNanoSec;

                        if (adcPayloadLength > 5){
                            uint32_t ch_details = words[8 + wordOffset];
                            uint8_t ch_details_last_byte = ch_details & 0xFF;
                            waveform_words = (ch_details_last_byte  - 1) / 4 + 1;

                            for (int j = 0; j < (adcPayloadLength-5)/waveform_words; j++) {
                                std::vector<int> adcValues;
                                for(int k = 0; k < waveform_words; k++) {
                                    if (k == 0){
                                        channelNumber = static_cast<int>((words[8 + j*waveform_words + k + wordOffset] >> 24) + 1);
                                        adcValues.push_back(channelNumber);
                                    }
                                    if (k > 2){
                                        int16_t left = static_cast<int16_t>(words[8 + j*waveform_words + k + wordOffset] >> 16) & 0xFFFF;
                                        int16_t right = static_cast<int16_t>(words[8 + j*waveform_words + k + wordOffset]) & 0xFFFF;
                                        adcValues.push_back(left - 30000);
                                        adcValues.push_back(right - 30000);
                                    }
                                }

                                auto minElement = std::min_element(adcValues.begin(), adcValues.end());
                                int minPosition = std::distance(adcValues.begin(), minElement);

                                if (*minElement < -64) {
                                    if (minPosition > minPosition_lower && minPosition < minPosition_lower+30) {
                                        std::vector<int> slicedVector(adcValues.begin() + minPosition - 4, adcValues.begin() + minPosition + 16);

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
                                        channel_number = adcValues[0] + int(adcNumber)*64;
                                        channel_value = integral * -1;

                                        tree->Fill();
                                    }
                                }
                            }
                        }
                        adcNumber++;
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

    if (output_initialized) {
        rootFile->Write();
        rootFile->Close();
        delete rootFile; // Cleanup
    }
    return 0;
}
