// plot_raw_day.C
//
// AMASA scaler monitor diagnostic macro
// Plot raw S-event scaler data for a specific JST day.
//
// Example:
//   root -l -b -q 'plot_raw_day.C("/disk/alpaca/data/Akeno-MiniArray/data", "/home/asuke/amasa_monitor/raw_check", "2026-07-24")'
//
// Outputs:
//   raw_YYYYMMDD.root
//   raw_YYYYMMDD_ch7_full.png
//   raw_YYYYMMDD_ch7_around0900.png
//   raw_YYYYMMDD_ch1_7_full.png
//   raw_YYYYMMDD_ch1_7_around0900.png
//
// Notes:
//   - Input .d timestamps are UTC.
//   - target_date is interpreted as JST date.
//   - X values are UTC epoch seconds.
//   - ROOT time axis displays in local timezone if environment is JST.

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstring>

#include "TSystem.h"
#include "TStyle.h"
#include "TGraph.h"
#include "TMultiGraph.h"
#include "TLegend.h"
#include "TCanvas.h"
#include "TFile.h"
#include "TAxis.h"
#include "TString.h"

using namespace std;

const int N_CH = 10;
int CH_LIST[N_CH] = {1,2,3,4,5,6,7,17,18,19};
const char* CH_NAME[N_CH] = {
  "ch1","ch2","ch3","ch4","ch5","ch6","ch7","Any2","Any3","Any4"
};

struct Series {
  vector<double> x;
  vector<double> y;
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

bool parse_jst_date_start_epoch(const string& date_s, double& start_epoch_utc) {
  int y, m, d;

  if (sscanf(date_s.c_str(), "%d-%d-%d", &y, &m, &d) != 3) {
    cerr << "ERROR: target date must be YYYY-MM-DD, got " << date_s << endl;
    return false;
  }

  /*
    target date is JST 00:00.
    JST = UTC + 9h, so UTC = JST - 9h.
  */
  struct tm tm_utc;
  memset(&tm_utc, 0, sizeof(tm_utc));

  tm_utc.tm_year = y - 1900;
  tm_utc.tm_mon  = m - 1;
  tm_utc.tm_mday = d;
  tm_utc.tm_hour = -9;
  tm_utc.tm_min  = 0;
  tm_utc.tm_sec  = 0;

  time_t epoch = timegm(&tm_utc);
  if (epoch < 0) return false;

  start_epoch_utc = (double)epoch;
  return true;
}

string yyyymmdd_compact(const string& date_s) {
  string out = "";
  for (size_t i = 0; i < date_s.size(); i++) {
    if (date_s[i] >= '0' && date_s[i] <= '9') out += date_s[i];
  }
  return out;
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

void setup_time_axis(TAxis* ax) {
  ax->SetTimeDisplay(1);
  ax->SetTimeFormat("%m/%d %H:%M");
  ax->SetTitle("Time (JST)");
}

TGraph* make_graph(const Series& s, const char* name, const char* title, int color) {
  TGraph* g = new TGraph((int)s.x.size());

  for (size_t i = 0; i < s.x.size(); i++) {
    g->SetPoint((int)i, s.x[i], s.y[i]);
  }

  g->SetName(name);
  g->SetTitle(title);
  g->SetLineColor(color);
  g->SetMarkerColor(color);
  g->SetLineWidth(1);
  g->SetMarkerStyle(20);
  g->SetMarkerSize(0.35);

  return g;
}

TCanvas* draw_single(TGraph* g,
                     const string& canvas_name,
                     const string& title,
                     double xmin,
                     double xmax,
                     const string& png_path) {
  TCanvas* c = new TCanvas(canvas_name.c_str(), title.c_str(), 1200, 650);
  c->SetGrid();

  g->SetTitle(title.c_str());
  g->Draw("ALP");

  setup_time_axis(g->GetXaxis());
  g->GetYaxis()->SetTitle("Raw scaler count");

  g->GetXaxis()->SetLimits(xmin, xmax);
  g->GetYaxis()->SetTitleOffset(1.2);

  c->Modified();
  c->Update();
  c->SaveAs(png_path.c_str());

  return c;
}

TCanvas* draw_multi(vector<TGraph*>& graphs,
                    const string& canvas_name,
                    const string& title,
                    double xmin,
                    double xmax,
                    const string& png_path) {
  TCanvas* c = new TCanvas(canvas_name.c_str(), title.c_str(), 1200, 650);
  c->SetGrid();

  TMultiGraph* mg = new TMultiGraph();
  mg->SetName((canvas_name + "_mg").c_str());
  mg->SetTitle(title.c_str());

  TLegend* leg = new TLegend(0.88, 0.62, 0.98, 0.88);
  leg->SetBorderSize(0);
  leg->SetFillStyle(0);

  for (size_t i = 0; i < graphs.size(); i++) {
    mg->Add(graphs[i], "LP");
    leg->AddEntry(graphs[i], graphs[i]->GetName(), "lp");
  }

  mg->Draw("A");
  setup_time_axis(mg->GetXaxis());
  mg->GetXaxis()->SetLimits(xmin, xmax);
  mg->GetYaxis()->SetTitle("Raw scaler count");
  mg->GetYaxis()->SetTitleOffset(1.2);

  leg->Draw();

  c->Modified();
  c->Update();
  c->SaveAs(png_path.c_str());

  return c;
}

void plot_raw_day(const char* data_root = "/disk/alpaca/data/Akeno-MiniArray/data",
                  const char* out_base  = "/home/asuke/amasa_monitor/raw_check",
                  const char* target_jst_date = "2026-07-24") {
  cout << "AMASA raw day plotter" << endl;
  cout << "Data root:       " << data_root << endl;
  cout << "Out base:        " << out_base << endl;
  cout << "Target JST date: " << target_jst_date << endl;

  gStyle->SetOptStat(0);
  gStyle->SetTimeOffset(0);

  double day_start_utc;
  if (!parse_jst_date_start_epoch(target_jst_date, day_start_utc)) {
    return;
  }

  double day_end_utc = day_start_utc + 24 * 3600;

  double zoom_start_utc = day_start_utc + 9 * 3600;   // JST 09:00
  double zoom_xmin = day_start_utc + 8 * 3600;        // JST 08:00
  double zoom_xmax = day_start_utc + 12 * 3600;       // JST 12:00

  cout << "JST day start UTC: " << format_utc_iso(day_start_utc) << endl;
  cout << "JST day end UTC:   " << format_utc_iso(day_end_utc) << endl;
  cout << "JST day start JST: " << format_jst_iso(day_start_utc) << endl;
  cout << "JST day end JST:   " << format_jst_iso(day_end_utc) << endl;
  cout << "Focus time JST:    " << format_jst_iso(zoom_start_utc) << endl;

  vector<string> files;
  if (!find_d_files(data_root, files)) {
    cerr << "ERROR: failed to find .d files under " << data_root << endl;
    return;
  }

  cout << "Found " << files.size() << " .d files" << endl;

  Series series[N_CH];

  long total_lines = 0;
  long s_lines = 0;
  long used_lines = 0;

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

      if (epoch < day_start_utc || epoch >= day_end_utc) continue;

      int ch;
      double val;

      while (iss >> ch >> val) {
        int idx = channel_index(ch);
        if (idx >= 0) {
          series[idx].x.push_back(epoch);
          series[idx].y.push_back(val);
        }
      }

      used_lines++;
    }
  }

  cout << "Total lines: " << total_lines << endl;
  cout << "S lines:     " << s_lines << endl;
  cout << "Used S rows: " << used_lines << endl;

  for (int i = 0; i < N_CH; i++) {
    cout << CH_NAME[i] << " points: " << series[i].x.size() << endl;
  }

  string date_compact = yyyymmdd_compact(target_jst_date);
  string out_dir = string(out_base) + "/" + date_compact;

  gSystem->mkdir(out_base, kTRUE);
  gSystem->mkdir(out_dir.c_str(), kTRUE);

  string root_path = out_dir + "/raw_" + date_compact + ".root";

  TFile* fout = new TFile(root_path.c_str(), "RECREATE");

  int colors[N_CH] = {1,2,4,6,8,9,46,30,38,41};
  vector<TGraph*> graphs;

  for (int i = 0; i < N_CH; i++) {
    string gname = string("g_") + CH_NAME[i] + "_raw";
    string gtitle = string(CH_NAME[i]) + " raw scaler, " + target_jst_date + " JST";
    TGraph* g = make_graph(series[i], gname.c_str(), gtitle.c_str(), colors[i]);
    g->Write();
    graphs.push_back(g);
  }

  vector<TGraph*> ch1_7;
  for (int i = 0; i < 7; i++) ch1_7.push_back(graphs[i]);

  int idx_ch7 = channel_index(7);

  string png_ch7_full = out_dir + "/raw_" + date_compact + "_ch7_full.png";
  string png_ch7_zoom = out_dir + "/raw_" + date_compact + "_ch7_around0900.png";
  string png_ch1_7_full = out_dir + "/raw_" + date_compact + "_ch1_7_full.png";
  string png_ch1_7_zoom = out_dir + "/raw_" + date_compact + "_ch1_7_around0900.png";

  TCanvas* c_ch7_full = draw_single(
    graphs[idx_ch7],
    "c_ch7_full",
    string("ch7 raw scaler, ") + target_jst_date + " JST, full day",
    day_start_utc,
    day_end_utc,
    png_ch7_full
  );
  c_ch7_full->Write();

  TCanvas* c_ch7_zoom = draw_single(
    graphs[idx_ch7],
    "c_ch7_around0900",
    string("ch7 raw scaler, ") + target_jst_date + " JST, 08:00-12:00",
    zoom_xmin,
    zoom_xmax,
    png_ch7_zoom
  );
  c_ch7_zoom->Write();

  TCanvas* c_ch1_7_full = draw_multi(
    ch1_7,
    "c_ch1_7_full",
    string("ch1-ch7 raw scaler, ") + target_jst_date + " JST, full day",
    day_start_utc,
    day_end_utc,
    png_ch1_7_full
  );
  c_ch1_7_full->Write();

  TCanvas* c_ch1_7_zoom = draw_multi(
    ch1_7,
    "c_ch1_7_around0900",
    string("ch1-ch7 raw scaler, ") + target_jst_date + " JST, 08:00-12:00",
    zoom_xmin,
    zoom_xmax,
    png_ch1_7_zoom
  );
  c_ch1_7_zoom->Write();

  fout->Close();

  cout << "Saved ROOT: " << root_path << endl;
  cout << "Saved PNG:  " << png_ch7_full << endl;
  cout << "Saved PNG:  " << png_ch7_zoom << endl;
  cout << "Saved PNG:  " << png_ch1_7_full << endl;
  cout << "Saved PNG:  " << png_ch1_7_zoom << endl;
  cout << "Status: OK" << endl;
}