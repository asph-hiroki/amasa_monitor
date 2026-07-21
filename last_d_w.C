#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <ctime>
#include <cstdio>

#include "TSystem.h"
#include "TStyle.h"
#include "TGraph.h"
#include "TMultiGraph.h"
#include "TLegend.h"
#include "TAxis.h"
#include "TFile.h"
#include "TCanvas.h"
#include "TList.h"
#include "TObject.h"
#include "TString.h"


const int N_CH = 10;

int CH_LIST[N_CH] = {
    1, 2, 3, 4, 5, 6, 7,
    17, 18, 19
};

const char* CH_NAME[N_CH] = {
    "ch1", "ch2", "ch3", "ch4", "ch5", "ch6", "ch7",
    "Any2", "Any3", "Any4"
};


struct Series {
    std::vector<double> x;
    std::vector<double> y;
};


struct AggValue {
    double sum;
    int n;

    AggValue() {
        sum = 0.0;
        n = 0;
    }
};


int channel_index(int ch) {
    for (int i = 0; i < N_CH; i++) {
        if (CH_LIST[i] == ch) return i;
    }
    return -1;
}


std::vector<std::string> find_d_files(const std::string& data_root) {
    std::vector<std::string> files;

    TString cmd = Form("find %s -type f -name '*.d' | sort", data_root.c_str());
    TString result = gSystem->GetFromPipe(cmd);

    std::stringstream ss(result.Data());
    std::string line;

    while (std::getline(ss, line)) {
        if (line.size() > 0) {
            files.push_back(line);
        }
    }

    return files;
}


time_t parse_utc_time_to_epoch(const std::string& date_str, const std::string& time_str) {
    int year, month, day;
    int hour, minute;
    double sec_double;

    if (sscanf(date_str.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
        return 0;
    }

    if (sscanf(time_str.c_str(), "%d:%d:%lf", &hour, &minute, &sec_double) != 3) {
        return 0;
    }

    int sec = (int)sec_double;

    struct tm t;
    t.tm_year = year - 1900;
    t.tm_mon  = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = sec;
    t.tm_isdst = 0;

    // Input time is UTC.
    return timegm(&t);
}


std::string format_time_utc(time_t epoch_utc) {
    struct tm* gm = gmtime(&epoch_utc);

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", gm);

    return std::string(buf);
}


std::string format_time_jst_from_utc(time_t epoch_utc) {
    time_t epoch_jst = epoch_utc + 9 * 3600;
    struct tm* gm = gmtime(&epoch_jst);

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S JST", gm);

    return std::string(buf);
}


std::string make_yymmdd_from_utc_as_jst(time_t epoch_utc) {
    time_t epoch_jst = epoch_utc + 9 * 3600;
    struct tm* gm = gmtime(&epoch_jst);

    char buf[32];
    strftime(buf, sizeof(buf), "%y%m%d", gm);

    return std::string(buf);
}


void setup_graph_style(TGraph* g, int idx) {
    int colors[] = {
        kRed + 1,
        kBlue + 1,
        kGreen + 2,
        kMagenta + 1,
        kCyan + 2,
        kOrange + 7,
        kBlack,
        kViolet + 1,
        kTeal + 2,
        kGray + 2
    };

    int ncolors = sizeof(colors) / sizeof(colors[0]);

    g->SetLineColor(colors[idx % ncolors]);
    g->SetMarkerColor(colors[idx % ncolors]);
    g->SetLineWidth(2);
    g->SetMarkerStyle(1);
}


void load_raw_data(
    const char* data_root,
    Series raw[],
    time_t& latest_epoch_utc,
    long& total_lines,
    long& s_lines,
    long& used_lines
) {
    latest_epoch_utc = 0;
    total_lines = 0;
    s_lines = 0;
    used_lines = 0;

    std::vector<std::string> files = find_d_files(data_root);

    std::cout << "Found " << files.size() << " .d files under " << data_root << std::endl;

    for (size_t ifile = 0; ifile < files.size(); ifile++) {
        std::ifstream fin(files[ifile].c_str());

        if (!fin) {
            std::cout << "Skip: " << files[ifile] << std::endl;
            continue;
        }

        std::string line;

        while (std::getline(fin, line)) {
            total_lines++;

            if (line.size() == 0) continue;

            std::stringstream ss(line);

            std::string run_id;
            std::string event_type;
            int event_number;
            std::string date_str;
            std::string time_str;

            ss >> run_id >> event_type >> event_number >> date_str >> time_str;

            if (!ss) continue;
            if (event_type != "S") continue;

            s_lines++;

            time_t epoch_utc = parse_utc_time_to_epoch(date_str, time_str);
            if (epoch_utc == 0) continue;

            if (epoch_utc > latest_epoch_utc) {
                latest_epoch_utc = epoch_utc;
            }

            /*
              Data timestamp is UT.
              For JSROOT display, store x as UTC + 9h.
              This makes ROOT/JSROOT time labels appear as JST.
            */
            double x_time = (double)epoch_utc;

            double values[N_CH];
            bool has_value[N_CH];

            for (int i = 0; i < N_CH; i++) {
                values[i] = 0.0;
                has_value[i] = false;
            }

            int ch;
            double value;

            while (ss >> ch >> value) {
                int idx = channel_index(ch);
                if (idx >= 0) {
                    values[idx] = value;
                    has_value[idx] = true;
                }
            }

            for (int i = 0; i < N_CH; i++) {
                if (has_value[i]) {
                    raw[i].x.push_back(x_time);
                    raw[i].y.push_back(values[i]);
                }
            }

            used_lines++;
        }
    }
}


void make_1min_mean_series(
    Series raw[],
    Series one_min[],
    double start_x,
    double end_x
) {
    for (int i = 0; i < N_CH; i++) {
        std::map<long, AggValue> bins;

        for (size_t j = 0; j < raw[i].x.size(); j++) {
            double x = raw[i].x[j];

            if (x < start_x) continue;
            if (x > end_x) continue;

            long bin = ((long)x / 60) * 60;

            bins[bin].sum += raw[i].y[j];
            bins[bin].n += 1;
        }

        for (std::map<long, AggValue>::iterator it = bins.begin(); it != bins.end(); ++it) {
            if (it->second.n <= 0) continue;

            double value = it->second.sum / it->second.n;

            one_min[i].x.push_back((double)it->first);
            one_min[i].y.push_back(value);
        }
    }
}


TGraph* make_graph(Series one_min[], int idx) {
    int n = one_min[idx].x.size();

    if (n <= 0) return 0;

    TGraph* g = new TGraph(n, &one_min[idx].x[0], &one_min[idx].y[0]);

    g->SetName(Form("g_%s_1min_mean", CH_NAME[idx]));
    g->SetTitle(Form("%s 1 min mean;Time JST;1 min mean count rate [counts/sec]", CH_NAME[idx]));

    setup_graph_style(g, idx);

    return g;
}


TMultiGraph* make_multigraph(
    Series one_min[],
    const std::vector<int>& indices,
    const char* name,
    const char* title
) {
    TMultiGraph* mg = new TMultiGraph();
    mg->SetName(name);
    mg->SetTitle(Form("%s;Time JST;1 min mean count rate [counts/sec]", title));

    for (size_t ii = 0; ii < indices.size(); ii++) {
        int idx = indices[ii];

        TGraph* g = make_graph(one_min, idx);
        if (!g) continue;

        mg->Add(g, "L");
    }

    return mg;
}


TCanvas* make_canvas(
    TMultiGraph* mg,
    const char* canvas_name,
    const char* canvas_title
) {
    TCanvas* c = new TCanvas(canvas_name, canvas_title, 1400, 700);
    c->SetGrid();

    mg->Draw("AL");

    mg->GetXaxis()->SetTitle("Time JST");
    mg->GetYaxis()->SetTitle("1 min mean count rate [counts/sec]");

    mg->GetXaxis()->SetTimeDisplay(1);
    mg->GetXaxis()->SetTimeFormat("%m/%d %H:%M");
    mg->GetXaxis()->SetLabelSize(0.035);
    mg->GetXaxis()->SetTitleSize(0.04);
    mg->GetYaxis()->SetTitleSize(0.04);

    TLegend* leg = new TLegend(0.88, 0.68, 0.98, 0.92);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);

    TList* graphs = mg->GetListOfGraphs();

    if (graphs) {
        for (int i = 0; i < graphs->GetSize(); i++) {
            TObject* obj = graphs->At(i);
            if (obj) {
                leg->AddEntry(obj, obj->GetName(), "l");
            }
        }
    }

    leg->Draw();

    c->Update();

    return c;
}


void save_root_file(
    Series one_min[],
    const char* out_root,
    const char* tag
) {
    std::vector<int> ch1_7;
    ch1_7.push_back(0);
    ch1_7.push_back(1);
    ch1_7.push_back(2);
    ch1_7.push_back(3);
    ch1_7.push_back(4);
    ch1_7.push_back(5);
    ch1_7.push_back(6);

    std::vector<int> any234;
    any234.push_back(7);
    any234.push_back(8);
    any234.push_back(9);

    TFile* fout = new TFile(out_root, "RECREATE");

    TMultiGraph* mg_ch1_7 = make_multigraph(
        one_min,
        ch1_7,
        Form("mg_ch1_7_1min_mean_%s", tag),
        Form("ch1-ch7 1 min mean, %s", tag)
    );

    TMultiGraph* mg_any234 = make_multigraph(
        one_min,
        any234,
        Form("mg_any234_1min_mean_%s", tag),
        Form("Any2-Any4 1 min mean, %s", tag)
    );

    TCanvas* c_ch1_7 = make_canvas(
        mg_ch1_7,
        Form("c_ch1_7_1min_mean_%s", tag),
        Form("ch1-ch7 1 min mean, %s", tag)
    );

    TCanvas* c_any234 = make_canvas(
        mg_any234,
        Form("c_any234_1min_mean_%s", tag),
        Form("Any2-Any4 1 min mean, %s", tag)
    );

    fout->cd();

    mg_ch1_7->Write();
    mg_any234->Write();

    c_ch1_7->Write();
    c_any234->Write();

    for (int i = 0; i < N_CH; i++) {
        TGraph* g = make_graph(one_min, i);
        if (!g) continue;
        g->Write();
    }

    fout->Close();

    std::cout << "Saved: " << out_root << std::endl;
}


void write_latest_json(
    const char* out_base,
    const std::string& date_dir,
    const char* data_root,
    time_t latest_epoch_utc
) {
    std::string json_path = std::string(out_base) + "/latest.json";

    time_t now_utc = time(NULL);

    std::ofstream fout(json_path.c_str());

    if (!fout) {
        std::cout << "Failed to write latest.json: " << json_path << std::endl;
        return;
    }

    fout << "{\n";
    fout << "  \"project\": \"AMASA scaler monitor\",\n";
    fout << "  \"version\": 1,\n";
    fout << "  \"status\": \"ok\",\n";
    fout << "\n";
    fout << "  \"date\": \"" << date_dir << "\",\n";
    fout << "\n";
    fout << "  \"updated_at_jst\": \"" << format_time_jst_from_utc(now_utc) << "\",\n";
    fout << "  \"latest_data_time_utc\": \"" << format_time_utc(latest_epoch_utc) << "\",\n";
    fout << "  \"latest_data_time_jst\": \"" << format_time_jst_from_utc(latest_epoch_utc) << "\",\n";
    fout << "\n";
    fout << "  \"time_axis\": {\n";
    fout << "    \"label\": \"JST\",\n";
    fout << "    \"note\": \"Graph x values are stored as UTC epoch seconds. JSROOT/browser time-axis handling displays them in the local timezone.\"\n";
    fout << "  },\n";
    fout << "\n";
    fout << "  \"data\": {\n";
    fout << "    \"source\": \"" << data_root << "\",\n";
    fout << "    \"event_type\": \"S\",\n";
    fout << "    \"aggregation\": \"1min_mean\",\n";
    fout << "    \"unit\": \"counts/sec\"\n";
    fout << "  },\n";
    fout << "\n";
    fout << "  \"periods\": {\n";

    fout << "    \"last1d\": {\n";
    fout << "      \"label\": \"Last 1 day\",\n";
    fout << "      \"days\": 1,\n";
    fout << "      \"root_file\": \"figure/" << date_dir << "/scaler_1min_mean_last1d.root\",\n";
    fout << "      \"objects\": {\n";
    fout << "        \"ch1_7\": {\n";
    fout << "          \"label\": \"ch1-ch7\",\n";
    fout << "          \"type\": \"multigraph\",\n";
    fout << "          \"object\": \"mg_ch1_7_1min_mean_last1d\"\n";
    fout << "        },\n";
    fout << "        \"any234\": {\n";
    fout << "          \"label\": \"Any2-Any4\",\n";
    fout << "          \"type\": \"multigraph\",\n";
    fout << "          \"object\": \"mg_any234_1min_mean_last1d\"\n";
    fout << "        }\n";
    fout << "      }\n";
    fout << "    },\n";
    fout << "\n";
    fout << "    \"last1w\": {\n";
    fout << "      \"label\": \"Last 1 week\",\n";
    fout << "      \"days\": 7,\n";
    fout << "      \"root_file\": \"figure/" << date_dir << "/scaler_1min_mean_last1w.root\",\n";
    fout << "      \"objects\": {\n";
    fout << "        \"ch1_7\": {\n";
    fout << "          \"label\": \"ch1-ch7\",\n";
    fout << "          \"type\": \"multigraph\",\n";
    fout << "          \"object\": \"mg_ch1_7_1min_mean_last1w\"\n";
    fout << "        },\n";
    fout << "        \"any234\": {\n";
    fout << "          \"label\": \"Any2-Any4\",\n";
    fout << "          \"type\": \"multigraph\",\n";
    fout << "          \"object\": \"mg_any234_1min_mean_last1w\"\n";
    fout << "        }\n";
    fout << "      }\n";
    fout << "    }\n";

    fout << "  },\n";
    fout << "\n";
    fout << "  \"single_graphs\": {\n";
    fout << "    \"ch1\":  { \"label\": \"ch1\",  \"group\": \"ch1_7\", \"object\": \"g_ch1_1min_mean\" },\n";
    fout << "    \"ch2\":  { \"label\": \"ch2\",  \"group\": \"ch1_7\", \"object\": \"g_ch2_1min_mean\" },\n";
    fout << "    \"ch3\":  { \"label\": \"ch3\",  \"group\": \"ch1_7\", \"object\": \"g_ch3_1min_mean\" },\n";
    fout << "    \"ch4\":  { \"label\": \"ch4\",  \"group\": \"ch1_7\", \"object\": \"g_ch4_1min_mean\" },\n";
    fout << "    \"ch5\":  { \"label\": \"ch5\",  \"group\": \"ch1_7\", \"object\": \"g_ch5_1min_mean\" },\n";
    fout << "    \"ch6\":  { \"label\": \"ch6\",  \"group\": \"ch1_7\", \"object\": \"g_ch6_1min_mean\" },\n";
    fout << "    \"ch7\":  { \"label\": \"ch7\",  \"group\": \"ch1_7\", \"object\": \"g_ch7_1min_mean\" },\n";
    fout << "    \"Any2\": { \"label\": \"Any2\", \"group\": \"any234\", \"object\": \"g_Any2_1min_mean\" },\n";
    fout << "    \"Any3\": { \"label\": \"Any3\", \"group\": \"any234\", \"object\": \"g_Any3_1min_mean\" },\n";
    fout << "    \"Any4\": { \"label\": \"Any4\", \"group\": \"any234\", \"object\": \"g_Any4_1min_mean\" }\n";
    fout << "  },\n";
    fout << "\n";
    fout << "  \"default_view\": {\n";
    fout << "    \"period\": \"last1d\",\n";
    fout << "    \"group\": \"ch1_7\",\n";
    fout << "    \"display\": \"all\"\n";
    fout << "  }\n";
    fout << "}\n";

    fout.close();

    std::cout << "Saved: " << json_path << std::endl;
}


void last_d_w(
    const char* data_root = "/disk/alpaca/data/Akeno-MiniArray/data",
    const char* out_base = "./figure"
) {
    gStyle->SetTimeOffset(0);

    Series raw[N_CH];

    time_t latest_epoch_utc = 0;
    long total_lines = 0;
    long s_lines = 0;
    long used_lines = 0;

    load_raw_data(
        data_root,
        raw,
        latest_epoch_utc,
        total_lines,
        s_lines,
        used_lines
    );

    if (latest_epoch_utc <= 0) {
        std::cout << "No valid S event data found." << std::endl;
        return;
    }

    std::string date_dir = make_yymmdd_from_utc_as_jst(latest_epoch_utc);
    std::string out_dir = std::string(out_base) + "/" + date_dir;

    gSystem->mkdir(out_base, true);
    gSystem->mkdir(out_dir.c_str(), true);

    double latest_x = (double)latest_epoch_utc;

    double end_x = latest_x;
    double start_1d = end_x - 1.0 * 86400.0;
    double start_1w = end_x - 7.0 * 86400.0;

    Series one_min_1d[N_CH];
    Series one_min_1w[N_CH];

    make_1min_mean_series(raw, one_min_1d, start_1d, end_x);
    make_1min_mean_series(raw, one_min_1w, start_1w, end_x);

    std::string out_1d = out_dir + "/scaler_1min_mean_last1d.root";
    std::string out_1w = out_dir + "/scaler_1min_mean_last1w.root";

    save_root_file(one_min_1d, out_1d.c_str(), "last1d");
    save_root_file(one_min_1w, out_1w.c_str(), "last1w");

    write_latest_json(
        out_base,
        date_dir,
        data_root,
        latest_epoch_utc
    );

    std::cout << std::endl;
    std::cout << "Summary" << std::endl;
    std::cout << "-------" << std::endl;
    std::cout << "Total lines:          " << total_lines << std::endl;
    std::cout << "S lines:              " << s_lines << std::endl;
    std::cout << "Used lines:           " << used_lines << std::endl;
    std::cout << "Latest data UTC:      " << format_time_utc(latest_epoch_utc) << std::endl;
    std::cout << "Latest data JST:      " << format_time_jst_from_utc(latest_epoch_utc) << std::endl;
    std::cout << "Date dir:             " << date_dir << std::endl;
    std::cout << "Output dir:           " << out_dir << std::endl;
    std::cout << "Latest JSON:          " << std::string(out_base) + "/latest.json" << std::endl;
    std::cout << std::endl;
    std::cout << "Done." << std::endl;
}