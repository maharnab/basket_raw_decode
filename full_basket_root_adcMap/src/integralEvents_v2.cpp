#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

#include <nlohmann/json.hpp>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleWriter.hxx>

using json = nlohmann::json;

static constexpr double BIN_WIDTH_NS = 16.0;

struct ChannelEntry {
    uint32_t device_id;
    uint64_t timestamp;
    int channel_number;
    int channel_value;
    int t50_time_ps;
};

struct OutputState {
    bool initialized = false;
    std::string output_file_path;
    std::unique_ptr<ROOT::RNTupleWriter> writer;
    std::shared_ptr<int> field_event_number;
    std::shared_ptr<uint32_t> field_device_id;
    std::shared_ptr<uint64_t> field_timestamp;
    std::shared_ptr<int> field_channel_number;
    std::shared_ptr<int> field_channel_value;
    std::shared_ptr<int> field_t50_time;
};

struct MultiFileLabel {
    std::string first_num = "unknown";
    std::string last_num = "unknown";
};

static inline uint32_t read_u32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, sizeof(uint32_t));
    return v;
}

static double compute_t50_ns(const std::vector<int>& adc, double pedestal) {
    const int n = static_cast<int>(adc.size()) - 1;
    if (n < 2) return -1.0;

    int min_idx = 0;
    double min_val = adc[1] - pedestal;
    for (int i = 1; i < n; ++i) {
        double val = adc[i + 1] - pedestal;
        if (val < min_val) {
            min_val = val;
            min_idx = i;
        }
    }

    const double half_min = min_val * 0.5;
    for (int i = 0; i < min_idx; ++i) {
        double y_curr = adc[i + 1] - pedestal;
        double y_next = adc[i + 2] - pedestal;
        if (y_curr >= half_min && y_next < half_min) {
            double x1 = (i + 0.5) * BIN_WIDTH_NS;
            double x2 = (i + 1.5) * BIN_WIDTH_NS;
            return x1 + (half_min - y_curr) * (x2 - x1) / (y_next - y_curr);
        }
    }

    return -1.0;
}

static std::vector<std::string> load_adc_map(const std::string& json_path, int basket_num) {
    std::ifstream ifs(json_path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open ADC map: " + json_path);
    }

    json j;
    ifs >> j;

    for (const auto& entry : j) {
        if (entry.contains("basket") && entry["basket"].get<int>() == basket_num) {
            std::vector<std::string> addrs;
            addrs.reserve(12);
            for (int i = 1; i <= 12; ++i) {
                std::string key = std::to_string(i);
                if (entry.contains(key) && !entry[key].get<std::string>().empty()) {
                    std::string a = entry[key].get<std::string>();
                    std::transform(a.begin(), a.end(), a.begin(), ::tolower);
                    addrs.push_back(a);
                } else {
                    addrs.push_back("");
                }
            }
            return addrs;
        }
    }

    throw std::runtime_error("Basket " + std::to_string(basket_num) + " not found in ADC map");
}

static std::vector<uint32_t> build_adc_id_table(const std::vector<std::string>& addrs) {
    std::vector<uint32_t> table;
    table.reserve(addrs.size());
    for (const auto& s : addrs) {
        table.push_back(s.empty() ? 0 : static_cast<uint32_t>(std::stoul(s, nullptr, 16)));
    }
    return table;
}

static inline int find_adc_order(const std::vector<uint32_t>& id_table, uint32_t device_id) {
    for (size_t i = 0; i < id_table.size(); ++i) {
        if (id_table[i] == device_id) return static_cast<int>(i) + 1;
    }
    return -1;
}

static std::string extract_data_suffix_number(const std::string& file_name) {
    size_t dot = file_name.rfind(".data");
    if (dot == std::string::npos) return "unknown";
    size_t under = file_name.rfind('_', dot);
    if (under == std::string::npos || under + 1 >= dot) return "unknown";
    return file_name.substr(under + 1, dot - under - 1);
}

static MultiFileLabel build_multi_file_label(const std::vector<std::string>& data_files) {
    MultiFileLabel label;
    if (data_files.empty()) return label;
    const std::string first = std::filesystem::path(data_files.front()).filename().string();
    const std::string last = std::filesystem::path(data_files.back()).filename().string();
    label.first_num = extract_data_suffix_number(first);
    label.last_num = extract_data_suffix_number(last);
    return label;
}

static void init_output(
    OutputState& out,
    const std::string& base_dir,
    const std::string& input_file,
    uint64_t first_timestamp_ns,
    bool use_file_list,
    int basket_num,
    const MultiFileLabel& list_label
) {
    std::time_t t = static_cast<std::time_t>(first_timestamp_ns / 1000000000ULL);
    std::tm* tm_ptr = std::localtime(&t);
    char year_month_day[11];
    std::strftime(year_month_day, sizeof(year_month_day), "%Y-%m-%d", tm_ptr);

    std::filesystem::path out_dir = std::filesystem::path(base_dir) / year_month_day;
    std::string out_name;

    if (use_file_list) {
        out_name = "basket" + std::to_string(basket_num) + "_" +
                   list_label.first_num + "-" + list_label.last_num +
                   "_v2.root";
    } else {
        std::string stem = std::filesystem::path(input_file).filename().string();
        size_t reduced_pos = stem.find("_reduced");
        if (reduced_pos != std::string::npos) stem.erase(reduced_pos, 8);
        size_t data_pos = stem.rfind(".data");
        if (data_pos != std::string::npos) stem.erase(data_pos);
        out_dir /= stem;
        out_name = stem + "_v2.root";
    }

    std::filesystem::create_directories(out_dir);
    out.output_file_path = (out_dir / out_name).string();

    auto model = ROOT::RNTupleModel::Create();
    out.field_event_number = model->MakeField<int>("event_number");
    out.field_device_id = model->MakeField<uint32_t>("device_id");
    out.field_timestamp = model->MakeField<uint64_t>("timestamp");
    out.field_channel_number = model->MakeField<int>("channel_number");
    out.field_channel_value = model->MakeField<int>("channel_value");
    out.field_t50_time = model->MakeField<int>("t50_time");
    out.writer = ROOT::RNTupleWriter::Recreate(std::move(model), "events", out.output_file_path);
    out.initialized = true;
}

static void print_usage() {
    std::cout << "\nUsage: ./integralEvents_v2 [options] <datafile> <minPosition_lower> <output_path> <basket_number>\n"
              << "       For multiple files, use -F <file_list.txt> instead of <datafile>.\n"
              << "\nOptions:\n"
              << "  -M <adcMap.json>         Path to ADC map JSON file (default: ../adcMap.json)\n"
              << "  -F <file_list.txt>       Path to file containing list of .data files to process\n"
              << "  --max-events <N>         Maximum number of global events to process\n"
              << "\n";
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        print_usage();
        return 0;
    }

    std::string adc_map_json = "../adcMap.json";
    std::string file_list_path;
    std::vector<std::string> raw_positional;
    bool use_file_list = false;
    int64_t max_events = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-M") {
            if (i + 1 >= argc) {
                std::cerr << "[ERROR] -M requires a path.\n";
                return 1;
            }
            adc_map_json = argv[++i];
        } else if (arg == "-F") {
            if (i + 1 >= argc) {
                std::cerr << "[ERROR] -F requires a file list path.\n";
                return 1;
            }
            use_file_list = true;
            file_list_path = argv[++i];
        } else if (arg == "--max-events") {
            if (i + 1 >= argc) {
                std::cerr << "[ERROR] --max-events requires a value.\n";
                return 1;
            }
            max_events = std::stoll(argv[++i]);
        } else if (arg.empty() || arg[0] != '-') {
            raw_positional.push_back(arg);
        }
    }

    std::vector<std::string> data_files;
    int min_position_lower = 0;
    std::string base_dir;
    int basket_num = -1;

    if (use_file_list) {
        if (raw_positional.size() != 3) {
            std::cerr << "[ERROR] Expected positional args: <minPosition_lower> <output_path> <basket_number> when using -F.\n";
            print_usage();
            return 1;
        }
        min_position_lower = std::stoi(raw_positional[0]);
        base_dir = raw_positional[1];
        basket_num = std::stoi(raw_positional[2]);

        std::ifstream flist(file_list_path);
        if (!flist.is_open()) {
            std::cerr << "[ERROR] Could not open file list: " << file_list_path << std::endl;
            return 1;
        }
        std::string line;
        while (std::getline(flist, line)) {
            if (!line.empty()) data_files.push_back(line);
        }
    } else {
        if (raw_positional.size() != 4) {
            std::cerr << "[ERROR] Expected positional args: <datafile> <minPosition_lower> <output_path> <basket_number>.\n";
            print_usage();
            return 1;
        }
        data_files.push_back(raw_positional[0]);
        min_position_lower = std::stoi(raw_positional[1]);
        base_dir = raw_positional[2];
        basket_num = std::stoi(raw_positional[3]);
    }

    if (data_files.empty()) {
        std::cerr << "[ERROR] No input .data files to process.\n";
        return 1;
    }

    std::vector<std::string> adc_addrs;
    try {
        adc_addrs = load_adc_map(adc_map_json, basket_num);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }
    const std::vector<uint32_t> adc_id_table = build_adc_id_table(adc_addrs);

    const MultiFileLabel list_label = build_multi_file_label(data_files);
    OutputState output;

    std::vector<int> adc;
    adc.reserve(256);
    std::vector<ChannelEntry> event_entries;
    event_entries.reserve(768);

    constexpr uint32_t SYNC = 0x2A50D5AF;
    auto wall_start = std::chrono::high_resolution_clock::now();
    int64_t n_filled = 0;
    int64_t event_number_offset = 0;
    bool reached_max_events = false;

    for (const auto& file_path : data_files) {
        if (reached_max_events) break;

        if (!std::filesystem::exists(file_path) || std::filesystem::is_directory(file_path)) {
            std::cerr << "[ERROR] Not a valid file: " << file_path << std::endl;
            return 1;
        }
        if (file_path.size() < 5 || file_path.substr(file_path.size() - 5) != ".data") {
            std::cerr << "[ERROR] Expected .data file: " << file_path << std::endl;
            return 1;
        }

        int fd = open(file_path.c_str(), O_RDONLY);
        if (fd < 0) {
            perror("open");
            return 1;
        }

        struct stat st {};
        if (fstat(fd, &st) != 0) {
            perror("fstat");
            close(fd);
            return 1;
        }

        const size_t file_size = static_cast<size_t>(st.st_size);
        void* map_addr = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (map_addr == MAP_FAILED) {
            perror("mmap");
            close(fd);
            return 1;
        }
        madvise(map_addr, file_size, MADV_SEQUENTIAL);

        const uint8_t* raw = static_cast<const uint8_t*>(map_addr);
        int last_pct = -1;
        size_t pos = 0;
        int wf_words = 0;
        int64_t file_max_local_event = -1;

        std::cout << "\n[ " << std::time(nullptr) << " ] : Processing "
                  << std::filesystem::path(file_path).filename().string()
                  << " (" << file_size / (1024 * 1024) << " MiB)" << std::endl;

        while (pos + 8 <= file_size) {
            int pct = static_cast<int>(100.0 * static_cast<double>(pos) / static_cast<double>(file_size));
            if (pct != last_pct) {
                double elapsed = std::chrono::duration<double>(
                    std::chrono::high_resolution_clock::now() - wall_start
                ).count();
                std::cout << "[ " << std::time(nullptr) << " ] : "
                          << std::setw(2) << pct << " % completed in "
                          << std::fixed << std::setprecision(1)
                          << std::setw(6) << elapsed << " s @ "
                          << std::setw(6) << (pos / 1e6 / std::max(elapsed, 1e-9))
                          << " MB/s." << std::endl;
                last_pct = pct;
            }

            if (read_u32(raw + pos) != SYNC) {
                pos += 4;
                continue;
            }
            pos += 4;

            const uint32_t payload_bytes = read_u32(raw + pos);
            pos += 4;
            if (pos + payload_bytes > file_size) break;

            const uint8_t* pl = raw + pos;
            const size_t nw = payload_bytes / 4;
            auto pw = [&](size_t i) -> uint32_t { return read_u32(pl + i * 4); };

            const int event_num_local = static_cast<int>(pw(0));
            if (event_num_local > file_max_local_event) file_max_local_event = event_num_local;
            const int event_num_global = static_cast<int>(event_number_offset) + event_num_local;
            if (max_events > 0 && event_num_global >= max_events) {
                reached_max_events = true;
                break;
            }

            if (!output.initialized && nw > 5) {
                const uint64_t first_ts = static_cast<uint64_t>(pw(4)) * 1000000000ULL
                                        + static_cast<uint64_t>((pw(5) & 0xFFFFFFFC) >> 2);
                init_output(output, base_dir, file_path, first_ts, use_file_list, basket_num, list_label);
            }

            event_entries.clear();
            bool skip_event = false;
            size_t w_off = 0;

            while (w_off + 3 <= nw && !skip_event) {
                const uint32_t device_id = pw(1 + w_off);
                const int adc_len = static_cast<int>(pw(2 + w_off) & 0x00FFFFFF) / 4;
                const size_t next_off = w_off + 2 + static_cast<size_t>(adc_len);
                if (next_off > nw) break;

                const int adc_order = find_adc_order(adc_id_table, device_id);
                if (adc_order < 0) {
                    std::cerr << "[ERROR] ADC address not found for device_id 0x"
                              << std::hex << std::setw(8) << std::setfill('0') << device_id
                              << std::dec << " in basket " << basket_num << std::endl;
                    munmap(map_addr, file_size);
                    close(fd);
                    return 2;
                }

                if (adc_len > 5) {
                    if (wf_words == 0) {
                        int dw = adc_len - 5;
                        for (int cand = 5; cand <= dw; ++cand) {
                            if (dw % cand != 0) continue;
                            int nc = dw / cand;
                            if (nc < 1 || nc > 64) continue;
                            bool ok = true;
                            for (int jj = 0; jj < nc; ++jj) {
                                int ch = static_cast<int>((pw(8 + jj * cand + w_off) >> 24) + 1);
                                if (ch < 1 || ch > 64) {
                                    ok = false;
                                    break;
                                }
                            }
                            if (ok) {
                                wf_words = cand;
                                break;
                            }
                        }
                        if (wf_words == 0) {
                            w_off = next_off;
                            continue;
                        }
                        std::cout << "Detected " << (wf_words - 3) * 2
                                  << " samples/channel (" << wf_words << " words)" << std::endl;
                    }

                    const int num_ch = (adc_len - 5) / wf_words;
                    const int pedestal_bins = std::max(1, (wf_words + 5) / 6);

                    for (int j = 0; j < num_ch; ++j) {
                        adc.clear();
                        const size_t ch_base = 8 + static_cast<size_t>(j) * wf_words + w_off;

                        adc.push_back(static_cast<int>((pw(ch_base) >> 24) + 1));

                        const uint32_t ch_tai_sec = pw(ch_base + 1);
                        const uint32_t ch_tai_nsec = (pw(ch_base + 2) & 0xFFFFFFFC) >> 2;
                        const uint64_t ch_timestamp =
                            static_cast<uint64_t>(ch_tai_sec) * 1000000000ULL + ch_tai_nsec;

                        for (int k = 3; k < wf_words; ++k) {
                            uint32_t w = pw(ch_base + k);
                            adc.push_back(static_cast<int16_t>(w >> 16) - 30000);
                            adc.push_back(static_cast<int16_t>(w & 0xFFFF) - 30000);
                        }

                        auto min_it = std::min_element(adc.begin(), adc.end());
                        const int min_pos = static_cast<int>(min_it - adc.begin());
                        if (*min_it >= -100) continue;
                        if (min_pos <= min_position_lower || min_pos >= min_position_lower + 30) continue;

                        const int start_idx = min_pos - 4;
                        const int end_idx = min_pos + 16;
                        if (start_idx < 0 || end_idx > static_cast<int>(adc.size())) {
                            skip_event = true;
                            break;
                        }

                        double pedestal = 0.0;
                        const int ped_count = std::min(pedestal_bins, static_cast<int>(adc.size()) - 1);
                        if (ped_count > 0) {
                            double sum = 0.0;
                            for (int i = 1; i <= ped_count; ++i) sum += adc[i];
                            pedestal = sum / static_cast<double>(ped_count);
                        }

                        const int ped_int = static_cast<int>(pedestal);
                        int integral = 0;
                        for (int i = start_idx; i < end_idx; ++i) integral += adc[i] - ped_int;

                        const int ch_out = adc[0] + (adc_order - 1) * 64;
                        const double t50_ns = compute_t50_ns(adc, pedestal);
                        const int t50_ps = (t50_ns >= 0.0) ? static_cast<int>(t50_ns * 1000.0 + 0.5) : -1;

                        event_entries.push_back(ChannelEntry{device_id, ch_timestamp, ch_out, -integral, t50_ps});
                    }
                }

                w_off = next_off;
            }

            if (!skip_event && output.initialized) {
                for (const auto& e : event_entries) {
                    *output.field_event_number = event_num_global;
                    *output.field_device_id = e.device_id;
                    *output.field_timestamp = e.timestamp;
                    *output.field_channel_number = e.channel_number;
                    *output.field_channel_value = e.channel_value;
                    *output.field_t50_time = e.t50_time_ps;
                    output.writer->Fill();
                    ++n_filled;
                }
            }

            pos += payload_bytes;
        }

        if (file_max_local_event >= 0) {
            event_number_offset += (file_max_local_event + 1);
        }

        munmap(map_addr, file_size);
        close(fd);
    }

    if (output.initialized) {
        output.writer.reset();
    }

    const double total = std::chrono::duration<double>(
        std::chrono::high_resolution_clock::now() - wall_start
    ).count();
    std::cout << "\n[ " << std::time(nullptr) << " ] : Done in "
              << std::fixed << std::setprecision(1) << total << " s; "
              << n_filled << " entries written to "
              << (output.initialized ? std::filesystem::path(output.output_file_path).filename().string() : "<none>")
              << std::endl;

    return 0;
}