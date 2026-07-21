// make_plotly_json.C
//
// AMASA scaler monitor
// Generate Plotly-ready JSON files from .d scaler data.
//
// Outputs:
//   figure/YYMMDD/json/scaler_10sec_last1d.json
//   figure/YYMMDD/json/scaler_1min_last7d.json
//   figure/YYMMDD/json/scaler_5min_last1m.json
//   figure/latest_plotly.json
//
// Notes:
//   - Input .d file timestamps are UTC.
//   - JSON contains both UTC and JST time strings.
//   - Missing data: mean = null, n = 0
//   - Real zero count: mean = 0.0, n > 0

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>

#include "TSystem.h"
#include "TString.h"

using namespace std;

const int N_CH = 10;
int CH_LIST[N_CH] = {1,2,3,4,5,6,7,17,18,19};
const char* CH_NAME[N_CH] = {
  "ch1","ch2","ch3","ch4","ch5","ch6","ch7","Any2","Any3","Any4"
};

struct Record {
  double t;
  double v[N_CH];
  bool present[N_CH];

  Record() {
    t = 0.0;
    for (int i = 0; i < N_CH; i++) {
      v[i] = 0.0;
      present[i] = false;
    }
  }
};

int channel_index(int ch) {
  for (int i = 0; i < N_CH; i++) {
    if (CH_LIST[i] == ch) return i;
  }
  return -1;
}

bool parse_utc_time_to_epoch(const string& date_s, const string& time_s, double& epoch_out) {
  int y, m, d, hh, mm;
  double ss;

  if (sscanf(date_s.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return false;
  if (sscanf(time_s.c_str(), "%d:%d:%lf", &hh, &mm, &ss) != 3) return false;

  struct tm tm_utc;
  memset(&tm_utc, 0, sizeof(tm_utc));

  tm_utc.tm_year = y - 1900;
  tm_utc.tm_mon  = m - 1;
  tm_utc.tm_mday = d;
  tm_utc.tm_hour = hh;
  tm_utc.tm_min  = mm;
  tm_utc.tm_sec  = (int)floor(ss);

  time_t epoch = timegm(&tm_utc);
  if (epoch < 0) return false;

  epoch_out = (double)epoch + (ss - floor(ss));
  return true;
}

string format_utc_iso(double epoch) {
  time_t t = (time_t)floor(epoch);
  struct tm* gm = gmtime(&t);

  char buf[64];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gm);
  return string(buf);
}

string format_jst_iso(double epoch) {
  time_t t = (time_t)floor(epoch) + 9 * 3600;
  struct tm* gm = gmtime(&t);

  char buf[64];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+09:00", gm);
  return string(buf);
}

string format_jst_human(double epoch) {
  time_t t = (time_t)floor(epoch) + 9 * 3600;
  struct tm* gm = gmtime(&t);

  char buf[64];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S JST", gm);
  return string(buf);
}

string make_yymmdd_from_utc_as_jst(double epoch) {
  time_t t = (time_t)floor(epoch) + 9 * 3600;
  struct tm* gm = gmtime(&t);

  char buf[16];
  strftime(buf, sizeof(buf), "%y%m%d", gm);
  return string(buf);
}

bool find_d_files(const string& data_root, vector<string>& files) {
  files.clear();

  string cmd = "find " + data_root + " -type f -name '*.d' | sort";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) return false;

  char line[4096];
  while (fgets(line, sizeof(line), pipe)) {
    string s(line);
    while (!s.empty() && (s[s.size()-1] == '\n' || s[s.size()-1] == '\r')) {
      s.erase(s.size()-1);
    }
    if (!s.empty()) files.push_back(s);
  }

  int status = pclose(pipe);
  return status == 0;
}

bool load_records(const string& data_root,
                  vector<Record>& records,
                  double& latest_epoch_utc,
                  long& total_lines,
                  long& s_lines,
                  long& used_lines) {
  records.clear();
  latest_epoch_utc = -1.0;
  total_lines = 0;
  s_lines = 0;
  used_lines = 0;

  vector<string> files;
  if (!find_d_files(data_root, files)) {
    cerr << "ERROR: failed to find .d files under " << data_root << endl;
    return false;
  }

  cout << "Found " << files.size() << " .d files under " << data_root << endl;

  for (size_t ifile = 0; ifile < files.size(); ifile++) {
    ifstream fin(files[ifile].c_str());
    if (!fin) {
      cerr << "WARNING: failed to open " << files[ifile] << endl;
      continue;
    }

    string line;
    while (getline(fin, line)) {
      total_lines++;

      if (line.empty()) continue;

      istringstream iss(line);

      string run_s;
      string event_type;
      string event_no_s;
      string date_s;
      string time_s;

      if (!(iss >> run_s >> event_type >> event_no_s >> date_s >> time_s)) continue;

      if (event_type != "S") continue;
      s_lines++;

      double epoch;
      if (!parse_utc_time_to_epoch(date_s, time_s, epoch)) continue;

      Record rec;
      rec.t = epoch;

      int ch;
      double val;

      while (iss >> ch >> val) {
        int idx = channel_index(ch);
        if (idx >= 0) {
          rec.v[idx] = val;
          rec.present[idx] = true;
        }
      }

      records.push_back(rec);
      used_lines++;

      if (epoch > latest_epoch_utc) latest_epoch_utc = epoch;
    }
  }

  return !records.empty();
}

bool write_dataset_json(const string& outfile,
                        const string& dataset_key,
                        const string& dataset_label,
                        const vector<Record>& records,
                        double latest_epoch_utc,
                        int duration_sec,
                        int base_bin_sec) {
  double end_epoch = floor(latest_epoch_utc / base_bin_sec) * base_bin_sec;
  double start_epoch = end_epoch - duration_sec;

  double start_bin = floor(start_epoch / base_bin_sec) * base_bin_sec;
  double end_bin = end_epoch;

  int n_bins = (int)((end_bin - start_bin) / base_bin_sec) + 1;
  if (n_bins <= 0) {
    cerr << "ERROR: invalid n_bins for " << dataset_key << endl;
    return false;
  }

  vector< vector<double> > sum(N_CH);
  vector< vector<int> > cnt(N_CH);

  for (int ich = 0; ich < N_CH; ich++) {
    sum[ich].assign(n_bins, 0.0);
    cnt[ich].assign(n_bins, 0);
  }

  for (size_t i = 0; i < records.size(); i++) {
    double t = records[i].t;
    if (t < start_bin || t >= end_bin + base_bin_sec) continue;

    int ibin = (int)floor((t - start_bin) / base_bin_sec);
    if (ibin < 0 || ibin >= n_bins) continue;

    for (int ich = 0; ich < N_CH; ich++) {
      if (!records[i].present[ich]) continue;
      sum[ich][ibin] += records[i].v[ich];
      cnt[ich][ibin] += 1;
    }
  }

  ofstream fout(outfile.c_str());
  if (!fout) {
    cerr << "ERROR: failed to open output JSON: " << outfile << endl;
    return false;
  }

  fout << "{\n";
  fout << "  \"meta\": {\n";
  fout << "    \"project\": \"AMASA scaler monitor\",\n";
  fout << "    \"dataset\": \"" << dataset_key << "\",\n";
  fout << "    \"label\": \"" << dataset_label << "\",\n";
  fout << "    \"duration_sec\": " << duration_sec << ",\n";
  fout << "    \"base_bin_sec\": " << base_bin_sec << ",\n";
  fout << "    \"aggregation\": \"mean\",\n";
  fout << "    \"unit\": \"counts/sec\",\n";
  fout << "    \"time_zone\": \"JST\",\n";
  fout << "    \"start_time_utc\": \"" << format_utc_iso(start_bin) << "\",\n";
  fout << "    \"end_time_utc\": \"" << format_utc_iso(end_bin) << "\",\n";
  fout << "    \"start_time_jst\": \"" << format_jst_iso(start_bin) << "\",\n";
  fout << "    \"end_time_jst\": \"" << format_jst_iso(end_bin) << "\",\n";
  fout << "    \"channels\": [";
  for (int ich = 0; ich < N_CH; ich++) {
    if (ich > 0) fout << ", ";
    fout << "\"" << CH_NAME[ich] << "\"";
  }
  fout << "]\n";
  fout << "  },\n";

  fout << "  \"data\": [\n";

  for (int ibin = 0; ibin < n_bins; ibin++) {
    double t_bin = start_bin + ibin * base_bin_sec;

    fout << "    {\n";
    fout << "      \"t_unix\": " << fixed << setprecision(0) << t_bin << ",\n";
    fout << "      \"time_utc\": \"" << format_utc_iso(t_bin) << "\",\n";
    fout << "      \"time_jst\": \"" << format_jst_iso(t_bin) << "\"";

    for (int ich = 0; ich < N_CH; ich++) {
      fout << ",\n";
      fout << "      \"" << CH_NAME[ich] << "_mean\": ";
      if (cnt[ich][ibin] > 0) {
        double mean = sum[ich][ibin] / cnt[ich][ibin];
        fout << setprecision(10) << mean;
      } else {
        fout << "null";
      }
      fout << ",\n";
      fout << "      \"" << CH_NAME[ich] << "_n\": " << cnt[ich][ibin];
    }

    fout << "\n";
    fout << "    }";
    if (ibin != n_bins - 1) fout << ",";
    fout << "\n";
  }

  fout << "  ]\n";
  fout << "}\n";

  fout.close();

  cout << "Saved: " << outfile << "  bins=" << n_bins << endl;
  return true;
}

bool write_latest_plotly_json(const string& outfile,
                              const string& date_dir,
                              double latest_epoch_utc,
                              int target_total_points,
                              int min_points_per_trace,
                              int max_points_per_trace,
                              int hard_limit_points,
                              int last1d_duration_sec,
                              int last1d_bin_sec,
                              int last7d_duration_sec,
                              int last7d_bin_sec,
                              int last1m_duration_sec,
                              int last1m_bin_sec) {
  time_t now = time(NULL);

  ofstream fout(outfile.c_str());
  if (!fout) {
    cerr << "ERROR: failed to open latest_plotly.json: " << outfile << endl;
    return false;
  }

  fout << "{\n";
  fout << "  \"project\": \"AMASA scaler monitor\",\n";
  fout << "  \"version\": 1,\n";
  fout << "  \"status\": \"ok\",\n";
  fout << "  \"date\": \"" << date_dir << "\",\n";
  fout << "\n";
  fout << "  \"updated_at_jst\": \"" << format_jst_human((double)now) << "\",\n";
  fout << "  \"latest_data_time_utc\": \"" << format_utc_iso(latest_epoch_utc) << "\",\n";
  fout << "  \"latest_data_time_jst\": \"" << format_jst_iso(latest_epoch_utc) << "\",\n";
  fout << "\n";

  fout << "  \"default_view\": {\n";
  fout << "    \"range\": \"last1d\",\n";
  fout << "    \"bin\": \"auto\",\n";
  fout << "    \"layout\": \"split_by_group\",\n";
  fout << "    \"panels\": [\"detectors\", \"any\"]\n";
  fout << "  },\n";
  fout << "\n";

  fout << "  \"bin_policy\": {\n";
  fout << "    \"candidate_bins_sec\": [10, 60, 300, 600, 1200, 1800, 3600],\n";
  fout << "    \"candidate_bins_label\": [\"10 sec\", \"1 min\", \"5 min\", \"10 min\", \"20 min\", \"30 min\", \"1 hour\"],\n";
  fout << "    \"target_total_points\": " << target_total_points << ",\n";
  fout << "    \"min_points_per_trace\": " << min_points_per_trace << ",\n";
  fout << "    \"max_points_per_trace\": " << max_points_per_trace << ",\n";
  fout << "    \"hard_limit_points\": " << hard_limit_points << "\n";
  fout << "  },\n";
  fout << "\n";

  fout << "  \"ranges\": {\n";
  fout << "    \"last1d\": { \"label\": \"Last 1 day\",  \"duration_sec\": 86400 },\n";
  fout << "    \"last3d\": { \"label\": \"Last 3 days\", \"duration_sec\": 259200 },\n";
  fout << "    \"last7d\": { \"label\": \"Last 7 days\", \"duration_sec\": 604800 },\n";
  fout << "    \"last2w\": { \"label\": \"Last 2 weeks\", \"duration_sec\": 1209600 },\n";
  fout << "    \"last1m\": { \"label\": \"Last 1 month\", \"duration_sec\": 2678400 }\n";
  fout << "  },\n";
  fout << "\n";

  fout << "  \"channel_groups\": {\n";
  fout << "    \"detectors\": {\n";
  fout << "      \"label\": \"Detector ch1-ch7\",\n";
  fout << "      \"channels\": [\"ch1\", \"ch2\", \"ch3\", \"ch4\", \"ch5\", \"ch6\", \"ch7\"],\n";
  fout << "      \"default_visible\": true\n";
  fout << "    },\n";
  fout << "    \"any\": {\n";
  fout << "      \"label\": \"Any2-Any4\",\n";
  fout << "      \"channels\": [\"Any2\", \"Any3\", \"Any4\"],\n";
  fout << "      \"default_visible\": true\n";
  fout << "    }\n";
  fout << "  },\n";
  fout << "\n";

  fout << "  \"datasets\": {\n";
  fout << "    \"last1d_10sec\": {\n";
  fout << "      \"label\": \"Last 1 day, 10 sec base\",\n";
  fout << "      \"duration_sec\": " << last1d_duration_sec << ",\n";
  fout << "      \"base_bin_sec\": " << last1d_bin_sec << ",\n";
  fout << "      \"file\": \"figure/" << date_dir << "/json/scaler_10sec_last1d.json\"\n";
  fout << "    },\n";
  fout << "    \"last7d_1min\": {\n";
  fout << "      \"label\": \"Last 7 days, 1 min base\",\n";
  fout << "      \"duration_sec\": " << last7d_duration_sec << ",\n";
  fout << "      \"base_bin_sec\": " << last7d_bin_sec << ",\n";
  fout << "      \"file\": \"figure/" << date_dir << "/json/scaler_1min_last7d.json\"\n";
  fout << "    },\n";
  fout << "    \"last1m_5min\": {\n";
  fout << "      \"label\": \"Last 1 month, 5 min base\",\n";
  fout << "      \"duration_sec\": " << last1m_duration_sec << ",\n";
  fout << "      \"base_bin_sec\": " << last1m_bin_sec << ",\n";
  fout << "      \"file\": \"figure/" << date_dir << "/json/scaler_5min_last1m.json\"\n";
  fout << "    }\n";
  fout << "  }\n";
  fout << "}\n";

  fout.close();

  cout << "Saved: " << outfile << endl;
  return true;
}

void make_plotly_json(const char* data_root = "/disk/alpaca/data/Akeno-MiniArray/data",
                      const char* out_base  = "./figure",
                      int target_total_points = 20000,
                      int min_points_per_trace = 300,
                      int max_points_per_trace = 3000,
                      int hard_limit_points = 50000,
                      int last1d_duration_sec = 86400,
                      int last1d_bin_sec = 10,
                      int last7d_duration_sec = 604800,
                      int last7d_bin_sec = 60,
                      int last1m_duration_sec = 2678400,
                      int last1m_bin_sec = 300) {
  cout << "AMASA Plotly JSON generator" << endl;
  cout << "Data root: " << data_root << endl;
  cout << "Out base:  " << out_base << endl;

  vector<Record> records;
  double latest_epoch_utc;
  long total_lines, s_lines, used_lines;

  bool ok = load_records(data_root, records, latest_epoch_utc,
                         total_lines, s_lines, used_lines);

  if (!ok) {
    cerr << "ERROR: no valid S event data found." << endl;
    return;
  }

  string date_dir = make_yymmdd_from_utc_as_jst(latest_epoch_utc);

  string out_dir = string(out_base) + "/" + date_dir;
  string json_dir = out_dir + "/json";

  gSystem->mkdir(out_dir.c_str(), kTRUE);
  gSystem->mkdir(json_dir.c_str(), kTRUE);

  string f_last1d = json_dir + "/scaler_10sec_last1d.json";
  string f_last7d = json_dir + "/scaler_1min_last7d.json";
  string f_last1m = json_dir + "/scaler_5min_last1m.json";
  string f_latest = string(out_base) + "/latest_plotly.json";

  bool ok1 = write_dataset_json(f_last1d, "last1d_10sec",
                                "Last 1 day, 10 sec base",
                                records, latest_epoch_utc,
                                last1d_duration_sec, last1d_bin_sec);

  bool ok2 = write_dataset_json(f_last7d, "last7d_1min",
                                "Last 7 days, 1 min base",
                                records, latest_epoch_utc,
                                last7d_duration_sec, last7d_bin_sec);

  bool ok3 = write_dataset_json(f_last1m, "last1m_5min",
                                "Last 1 month, 5 min base",
                                records, latest_epoch_utc,
                                last1m_duration_sec, last1m_bin_sec);

  bool ok4 = write_latest_plotly_json(f_latest,
                                      date_dir,
                                      latest_epoch_utc,
                                      target_total_points,
                                      min_points_per_trace,
                                      max_points_per_trace,
                                      hard_limit_points,
                                      last1d_duration_sec,
                                      last1d_bin_sec,
                                      last7d_duration_sec,
                                      last7d_bin_sec,
                                      last1m_duration_sec,
                                      last1m_bin_sec);

  cout << endl;
  cout << "Summary" << endl;
  cout << "-------" << endl;
  cout << "Total lines:          " << total_lines << endl;
  cout << "S lines:              " << s_lines << endl;
  cout << "Used lines:           " << used_lines << endl;
  cout << "Records:              " << records.size() << endl;
  cout << "Latest data UTC:      " << format_utc_iso(latest_epoch_utc) << endl;
  cout << "Latest data JST:      " << format_jst_iso(latest_epoch_utc) << endl;
  cout << "Date dir:             " << date_dir << endl;
  cout << "JSON dir:             " << json_dir << endl;
  cout << "Latest Plotly JSON:   " << f_latest << endl;

  if (ok1 && ok2 && ok3 && ok4) {
    cout << "Status: OK" << endl;
  } else {
    cout << "Status: ERROR" << endl;
  }
}