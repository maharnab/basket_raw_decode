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
            unsigned int sync_word_evnt = 0x2A50D5AF;                                                                       //Event

            int state = 0;
            int eventNumber, channelNumber, adcPayloadLength;
            uint64_t timeStampNanoSec, timeStampSec_ns;
            int64_t wordOffset = 0;
            std::string deviceSerialNumber = "";
            std::stringstream ss;
            std::vector<uint32_t> words;

            // Find the total size of the file
            file.seekg(0, file.end);
            int64_t totalSize = file.tellg(); // Get the total size of the file
            file.seekg(0, file.beg);
            // totalSize = 10000000;

            // get the file name from the path
            std::string fileName = filePath;
            fileName = fileName.substr(fileName.find_last_of("/\\") + 1); //Extract the file name from the path

            // Newline at the beginning and message
            std::cout << std::endl;
            std::time_t now1 = std::time(nullptr);
            std::cout << "[ " << now1 << " ] : " << "Processing the file: " << fileName << std::endl;
            // int counter = 0;
            int waveform_words;

            int lastPercentage = -1; // Initialize to -1 so the progress bar is displayed the first time

            while (bytesRead < totalSize && file.read(buffer.data(), bufferSize)) {    // loop over the file until the end of the file
                // Calculate the percentage of bytes read
                int percentage = (double)bytesRead / totalSize * 100;
                if (percentage != lastPercentage) {
                    auto end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> elapsed = end - start;
                    double rate = (double)bytesRead / 1e6 / elapsed.count();
                    std::time_t now2 = std::time(nullptr);
                    std::cout << "[ " << now2 << " ] : " << std::setw(2) << std::setfill(' ') << percentage
                                << " % completed in " << std::setw(5) << std::setfill(' ') << std::fixed << std::setprecision(1) << elapsed.count()
                                << " s @ " << std::setw(4) << std::setfill(' ') << std::fixed << std::setprecision(1) << rate << " MB/s." << std::endl;
                    lastPercentage = percentage; // Update lastPercentage
                }

                if (bufferSize==4){
                    uint32_t word;

                    // combine the bytes in the buffer to form a 32-bit word in little-endian format
                    word =  static_cast<uint8_t>(buffer[3]) << 24 |
                            static_cast<uint8_t>(buffer[2]) << 16 |
                            static_cast<uint8_t>(buffer[1]) << 8 |
                            static_cast<uint8_t>(buffer[0]);

                    // compare the word with the sync words
                    if (word == sync_word_evnt && state == 0) {
                        state += 1;
                    }
                    else if (state == 1){
                        // convert this word to integer and use it as bufferSize for next iteration of the while loop (event size in bytes)
                        bufferSize = static_cast<int>(word);
                        buffer.resize(bufferSize);
                    }
                }
                else if (bufferSize>4){
                    for (int i = 0; i < bufferSize/4; i++) {    // get 4 bytes from the buffer at a time in a loop
                        uint32_t word;
                        // convert the 4 bytes to a 32-bit word in little-endian format and store it in a vector
                        word =  static_cast<uint8_t>(buffer[3+4*i]) << 24 |
                                static_cast<uint8_t>(buffer[2+4*i]) << 16 |
                                static_cast<uint8_t>(buffer[1+4*i]) << 8 |
                                static_cast<uint8_t>(buffer[0+4*i]);
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
                        tree->SetAutoSave(0);       // gets rid of multiple events;22 and events;23 entries
                        tree->Branch("event_number", &event_number, "event_number/I");
                        tree->Branch("device_id", &device_id, "device_id/i");
                        tree->Branch("timestamp", &timestamp, "timestamp/l");
                        tree->Branch("channel_number", &channel_number, "channel_number/I");
                        tree->Branch("channel_value", &channel_value, "channel_value/I");
                        output_initialized = true;
                    }

                    int adcNumber = 0;

                    while (static_cast<std::vector<unsigned int>::size_type>(wordOffset) < (words.size() - 1)) {
                        device_id = words[1 + wordOffset]; // Store as uint32_t
                        adcPayloadLength = static_cast<int>(words[2 + wordOffset] & 0xFFFFFF) / 4;  // get the adc payload length from the word and convert it to integer
                        timeStampSec_ns = static_cast<uint64_t>(words[4 + wordOffset]) *1e9;                                                  // get the timestamp from the word and convert it to integer
                        timeStampNanoSec = static_cast<uint64_t>((words[5 + wordOffset] & 0xFFFFFFFC) >> 2);                         // get the timestamp from the word and convert it to integer
                        timestamp = timeStampSec_ns + timeStampNanoSec;    // add the timestamp in seconds and nanoseconds to get the final timestamp

                        if (adcPayloadLength > 5){
                            uint32_t ch_details = words[8 + wordOffset];    // get the channel details from the first word of the adc payload
                            uint8_t ch_details_last_byte = ch_details & 0xFF; // Last byte
                            waveform_words = (ch_details_last_byte  - 1) / 4 + 1;

                            for (int j = 0; j < (adcPayloadLength-5)/waveform_words; j++) {     // loop over the adc payload
                                std::vector<int> adcValues;     // create a vector to store the adc channel values for each adc
                                for(int k = 0; k < waveform_words; k++) {   // loop over the 33 or 17 or number of words that are set for waveform of each ADC words for each adc
                                    if (k == 0){    // if the word is the first word
                                        channelNumber = static_cast<int>((words[8 + j*waveform_words + k + wordOffset] >> 24) + 1);     // get the channel number from the word and convert it to integer
                                        adcValues.push_back(channelNumber);     // store the channel number in the vector
                                    }
                                    if (k > 2){     // 3rd word onwards
                                        int16_t left = static_cast<int16_t>(words[8 + j*waveform_words + k + wordOffset] >> 16) & 0xFFFF;   // get the left adc value from the word and convert it to integer
                                        int16_t right = static_cast<int16_t>(words[8 + j*waveform_words + k + wordOffset]) & 0xFFFF;    // get the right adc value from the word and convert it to integer
                                        adcValues.push_back(left - 30000);      // store the left adc value in the vector
                                        adcValues.push_back(right - 30000);     // store the right adc value in the vector
                                    }
                                }

                                auto minElement = std::min_element(adcValues.begin(), adcValues.end());      // find the minimum element in the vector
                                int minPosition = std::distance(adcValues.begin(), minElement);      // find the position of the minimum element in the vector

                                if (*minElement < -64) {    // if the minimum element is less than -64
                                    if (minPosition > minPosition_lower && minPosition < minPosition_lower+30) {    // if the minimum element is in the range of 31-5 to 31+20
                                        std::vector<int> slicedVector(adcValues.begin() + minPosition - 4, adcValues.begin() + minPosition + 16);   // slice the vector from 31-4 to 31+21

                                        // Calculate pedestal from first 10 waveform values (excluding channel number)
                                        double pedestal = 0.0;
                                        if (adcValues.size() > 10) {
                                            pedestal = std::accumulate(adcValues.begin() + 1, adcValues.begin() + 11, 0.0) / 10.0;
                                        }

                                        // Subtract pedestal from each element in slicedVector
                                        std::vector<int> slicedVectorPedSub;
                                        slicedVectorPedSub.reserve(slicedVector.size());
                                        for (const auto& val : slicedVector) {
                                            slicedVectorPedSub.push_back(val - static_cast<int>(pedestal));
                                        }

                                        int integral = std::accumulate(slicedVectorPedSub.begin(), slicedVectorPedSub.end(), 0);     // calculate the integral of the pedestal-subtracted sliced vector
                                        channel_number = adcValues[0] + int(adcNumber)*64;
                                        channel_value = integral * -1;

                                        // Fill the TTree with the flattened data
                                        tree->Fill();
                                    }
                                }
                            }
                        }
                        adcNumber++;    // increment the adc number
                        wordOffset += (2 + adcPayloadLength);                                                               // increment the wordOffset
                    }

                    // reset the state, buffers and clear the words vector
                    words.clear();
                    bufferSize = 4;
                    buffer.resize(bufferSize);
                    state = 0;
                    ss = std::stringstream();
                }

                // Update the bytesRead
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
