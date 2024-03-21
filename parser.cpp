#include "parser.h"
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
using namespace std;

// This uses Equalizer APO's format, except:
// 1. Only filters and preamps are supported
// 2. Filters with roll-off in dB per octave are not supported

map<string, string> eqapo_to_sox = {
  {"PK", "equalizer"}, {"LP", "lowpass"}, {"LPQ", "lowpass"}, {"HP", "highpass"},
  {"HPQ", "highpass"}, {"BP", "bandpass"}, {"LS", "bass"}, {"LSC", "bass"}, {"HS", "treble"},
  {"HSC", "treble"}, {"NO", "bandreject"}, {"AP", "allpass"}, {"IIR", "biquad"}
};

enum EqType {
  PEAK, LOWPASS, LOWPASS_Q, HIGHPASS, HIGHPASS_Q, BANDPASS, BASS, BASS_C, TREBLE, TREBLE_C, BANDREJECT, ALLPASS, BIQUAD, EQTYPES_COUNT
};

map<string, EqType> eqapo_to_enum = {
  {"PK", PEAK}, {"LP", LOWPASS}, {"LPQ", LOWPASS_Q}, {"HP", HIGHPASS},
  {"HPQ", HIGHPASS_Q}, {"BP", BANDPASS}, {"LS", BASS}, {"LSC", BASS_C}, {"HS", TREBLE},
  {"HSC", TREBLE_C}, {"NO", BANDREJECT}, {"AP", ALLPASS}, {"IIR", BIQUAD}
};

void filterconfig_init(FilterConfig& fc, vector<string>& types, vector<vector<string>>& argv) {
  fc.n = types.size();
  fc.types = new char*[fc.n];
  fc.argc = new size_t[fc.n];
  fc.argv = new char**[fc.n];
  for(size_t i = 0; i < fc.n; i++) {
    fc.types[i] = new char[types[i].length() + 1]();
    strcpy(fc.types[i], types[i].c_str());
    fc.argc[i] = argv[i].size();
    fc.argv[i] = new char*[fc.argc[i]];
    for(size_t j = 0; j < fc.argc[i]; j++) {
      fc.argv[i][j] = new char[argv[i][j].length() + 1];
      strcpy(fc.argv[i][j], argv[i][j].c_str());
    }
  }
}

string substr_to_nearest_space(const string& s, size_t p) {
  size_t space = s.find(' ', p);
  if(space == string::npos) return "";
  return s.substr(p, space - p);
}

string substr_to_end(const string& s, size_t p) {
  return s.substr(p, string::npos);
}

int parse(const char* path, FilterConfig* conf) {
  ifstream file(path);
  vector<string> types;
  vector<vector<string>> argv;
  string l;
  while(getline(file, l)) {
    string command(l, 0, 6);
    if(command == "Filter") {
      size_t tp = l.find("ON");
      if(tp == string::npos) return 0;
      tp += 3;
      string eq_type = substr_to_nearest_space(l, tp);
      types.push_back(eqapo_to_sox[eq_type]);
      vector<string> args;
      
      if(eq_type != "IIR") {
	tp = l.find("Fc");
	if(tp == string::npos) return 0;
	tp += 3;
	string freq = substr_to_nearest_space(l, tp);
	size_t gp = l.find("Gain");
	size_t qp = l.find('Q');
	size_t bwp = l.find("BW Oct");
	string gain = substr_to_nearest_space(l, tp);
	if((eq_type.substr(0, 2) != "LS") && (eq_type.substr(0, 2) != "HS")) {
	  args.push_back(freq);
	  if(qp != string::npos) {
	    qp += 2;
	    string q = substr_to_end(l, qp) + "q";
	    args.push_back(q);
	  }
	  else if(bwp != string::npos) {
	    bwp += 7;
	    string bw = substr_to_end(l, bwp) + "o";
	    args.push_back(bw);
	  }
	  if(eq_type == "PK") {
	    gp += 5;
	    string gain = substr_to_nearest_space(l, gp);
	    args.push_back(gain);
	  }
	}
	else {
	  gp += 5;
	  string gain = substr_to_nearest_space(l, gp);
	  args.push_back(gain);
	  args.push_back(freq);
	  if(qp != string::npos) {
	    qp += 2;
	    string q = substr_to_end(l, qp) + "q";
	    args.push_back(q);
	  }
	  else if(bwp != string::npos) {
	    bwp += 7;
	    string bw = substr_to_end(l, bwp) + "o";
	    args.push_back(bw);
	  }
	}
      }
      else {

      }
      argv.push_back(args);
    }
    else if(command == "Preamp") {
      types.push_back("gain");
      argv.push_back({ substr_to_nearest_space(l, 8) });
    }
  }
  filterconfig_init(*conf, types, argv);
  return 1;
}

void filterconfig_destroy(FilterConfig* fc) {
  for(size_t i = 0; i < fc->n; i++) {
    delete[] fc->types[i];
    for(size_t j = 0; j < fc->argc[i]; j++)
      delete[] fc->argv[i][j];
    delete[] fc->argv[i];
  }
  delete[] fc->types;
  delete[] fc->argv;
  delete[] fc->argc;
}
