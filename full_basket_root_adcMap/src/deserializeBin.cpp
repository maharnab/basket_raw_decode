#include <iostream>
#include <fstream>
#include <cstdint>
#include <string>
#include <set>
#include <tuple>
#include <iomanip>
#include "TFile.h"
#include "TTree.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_root_file>" << std::endl;
        return 1;
    }

    const char* inputFilePath = argv[1];

    // Open ROOT file
    TFile* file = TFile::Open(inputFilePath, "READ");
    if (!file || file->IsZombie()) {
        std::cerr << "Failed to open the input ROOT file." << std::endl;
        return 1;
    }

    TTree* tree = nullptr;
    file->GetObject("events", tree);
    if (!tree) {
        std::cerr << "Failed to get TTree 'events' from file." << std::endl;
        file->Close();
        return 1;
    }

    int32_t event_number;
    uint32_t device_id;
    ULong64_t timestamp;
    int32_t channel_number;
    int32_t channel_value;

    tree->SetBranchAddress("event_number", &event_number);
    tree->SetBranchAddress("device_id", &device_id);
    tree->SetBranchAddress("timestamp", &timestamp);
    tree->SetBranchAddress("channel_number", &channel_number);
    tree->SetBranchAddress("channel_value", &channel_value);

    // Create output file path by replacing .root with .txt
    std::string outputFilePath = inputFilePath;
    size_t pos = outputFilePath.find(".root");
    if (pos != std::string::npos) {
        outputFilePath.replace(pos, 5, ".txt");
    } else {
        outputFilePath += ".txt";
    }
    std::ofstream output(outputFilePath);
    if (!output.is_open()) {
        std::cerr << "Failed to open the output file." << std::endl;
        file->Close();
        return 1;
    }

    Long64_t nentries = tree->GetEntries();
    // Limit to first 100 events
    // Long64_t max_events = 1000;
    // if (nentries > max_events) nentries = max_events;
    int32_t prev_event = -1;
    // To store all (channel_number, device_id, timestamp) for current event
    std::vector<std::tuple<int32_t, uint32_t, uint64_t, int32_t>> event_channels;
    for (Long64_t i = 0; i < nentries; ++i) {
        tree->GetEntry(i);
        if (event_number != prev_event && prev_event != -1) {
            // Output previous event
            // Count unique (channel_number, device_id, timestamp)
            std::set<std::tuple<int32_t, uint32_t, uint64_t>> unique_channels;
            for (const auto& t : event_channels) {
                unique_channels.emplace(std::get<0>(t), std::get<1>(t), std::get<2>(t));
            }
            output << prev_event << " " << unique_channels.size() << std::endl;
            for (const auto& t : event_channels) {
                double ts_sec = static_cast<double>(std::get<2>(t)) / 1e9;
                output << std::get<0>(t) << " " << std::fixed << std::setprecision(9) << ts_sec << " " << std::get<3>(t) << std::endl;
            }
            event_channels.clear();
        }
        event_channels.emplace_back(channel_number, device_id, timestamp, channel_value);
        prev_event = event_number;
    }
    // Output last event
    if (!event_channels.empty()) {
        std::set<std::tuple<int32_t, uint32_t, uint64_t>> unique_channels;
        for (const auto& t : event_channels) {
            unique_channels.emplace(std::get<0>(t), std::get<1>(t), std::get<2>(t));
        }
        output << prev_event << " " << unique_channels.size() << std::endl;
        for (const auto& t : event_channels) {
            double ts_sec = static_cast<double>(std::get<2>(t)) / 1e9;
            output << std::get<0>(t) << " " << std::fixed << std::setprecision(9) << ts_sec << " " << std::get<3>(t) << std::endl;
        }
    }
    output.close();
    file->Close();
    return 0;
}