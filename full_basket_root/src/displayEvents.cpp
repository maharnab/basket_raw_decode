// Display events between two event numbers from a .data file in the specified format
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <algorithm>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " [data_file_path] [start_event_number] [end_event_number]" << std::endl;
        return 1;
    }
    const char* filePath = argv[1];
    int start_event = std::stoi(argv[2]);
    int end_event = std::stoi(argv[3]);
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
    int eventNumber, adcPayloadLength;
    uint64_t timeStampNanoSec, timeStampSec_ns;
    int64_t wordOffset = 0;
    std::string deviceSerialNumber = "";
    std::stringstream ss;
    std::vector<uint32_t> words;
    int waveform_words;
    while (file.read(buffer.data(), bufferSize)) {
        if (bufferSize==4){
            uint32_t word = static_cast<uint8_t>(buffer[3]) << 24 |
                            static_cast<uint8_t>(buffer[2]) << 16 |
                            static_cast<uint8_t>(buffer[1]) << 8 |
                            static_cast<uint8_t>(buffer[0]);
            if (word == sync_word_evnt && state == 0) {
                state += 1;
            } else if (state == 1){
                bufferSize = static_cast<int>(word);
                buffer.resize(bufferSize);
            }
        } else if (bufferSize>4){
            for (int i = 0; i < bufferSize/4; i++) {
                uint32_t word = static_cast<uint8_t>(buffer[3+4*i]) << 24 |
                                static_cast<uint8_t>(buffer[2+4*i]) << 16 |
                                static_cast<uint8_t>(buffer[1+4*i]) << 8 |
                                static_cast<uint8_t>(buffer[0+4*i]);
                words.push_back(word);
            }
            wordOffset = 0;
            eventNumber = static_cast<int>(words[0]);
            if (eventNumber >= start_event && eventNumber < end_event) {
                std::cout << "Event " << eventNumber << std::endl;
                int adcNumber = 0;
                while (static_cast<std::vector<unsigned int>::size_type>(wordOffset) < (words.size() - 1)) {
                    ss << std::hex << words[1 + wordOffset];
                    deviceSerialNumber = ss.str();
                    adcPayloadLength = static_cast<int>(words[2 + wordOffset] & 0xFFFFFF) / 4;
                    timeStampSec_ns = static_cast<uint64_t>(words[4 + wordOffset]) * 1000000000ULL;
                    timeStampNanoSec = static_cast<uint64_t>((words[5 + wordOffset] & 0xFFFFFFFC) >> 2);
                    uint64_t time_ns = timeStampSec_ns + timeStampNanoSec;
                    std::vector<std::vector<int>> all_adcValues;
                    std::vector<int> all_channelNumbers;
                    if (adcPayloadLength > 5){
                        uint32_t ch_details = words[8 + wordOffset];
                        uint8_t ch_details_last_byte = ch_details & 0xFF;
                        waveform_words = (ch_details_last_byte  - 1) / 4 + 1;
                        int n_channels = (adcPayloadLength-5)/waveform_words;
                        std::cout << std::string(4, ' ') << "Device " << deviceSerialNumber << " " << time_ns << std::endl;
                        for (int j = 0; j < n_channels; j++) {
                            std::vector<int> adcValues;
                            int ch_num = 0;
                            for(int k = 0; k < waveform_words; k++) {
                                if (k == 0){
                                    ch_num = static_cast<int>((words[8 + j*waveform_words + k + wordOffset] >> 24) + 1 + int(adcNumber)*64);
                                    adcValues.push_back(ch_num);
                                }
                                if (k > 2){
                                    int16_t left = static_cast<int16_t>(words[8 + j*waveform_words + k + wordOffset] >> 16) & 0xFFFF;
                                    int16_t right = static_cast<int16_t>(words[8 + j*waveform_words + k + wordOffset]) & 0xFFFF;
                                    adcValues.push_back(left - 30000);
                                    adcValues.push_back(right - 30000);
                                }
                            }
                            std::cout << std::string(8, ' ') << "" << ch_num << " ";
                            for (size_t idx = 1; idx < adcValues.size(); ++idx) {
                                std::cout << adcValues[idx];
                                if (idx != adcValues.size()-1) std::cout << " ";
                            }
                            std::cout << std::endl;
                        }
                    }
                    ss = std::stringstream();
                    wordOffset += (2 + adcPayloadLength);
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
    file.close();
    return 0;
}
