#include "parser.h"
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

// This uses Equalizer APO's format, except:
// 1. Only filters and preamps are supported
// 2. Filters with roll-off in dB per octave are not supported

FilterConfig::FilterConfig(const std::string& path) {
	if (parse(path) == false) {
		throw runtime_error{"Configuration parsing error"};
	}
	n = types.size();
	for (auto& i : argv) {
		vector<char*> v;
		for (auto& j : i) {
			v.push_back(j.data());
		}
		argv_c.push_back(v);
	}
}

map<string, string> eqapo_to_sox = {
	{"PK", "equalizer"}, {"LP", "lowpass"},	   {"LPQ", "lowpass"},
	{"HP", "highpass"},	 {"HPQ", "highpass"},  {"BP", "bandpass"},
	{"LS", "bass"},		 {"LSC", "bass"},	   {"HS", "treble"},
	{"HSC", "treble"},	 {"NO", "bandreject"}, {"AP", "allpass"},
	{"IIR", "biquad"}};

string substr_to_nearest_space(const string& s, size_t p) {
	size_t space = s.find(' ', p);
	if (space == string::npos)
		return "";
	return s.substr(p, space - p);
}

string substr_to_end(const string& s, size_t p) {
	return s.substr(p, string::npos);
}

bool FilterConfig::parse(const std::string& path) {
	ifstream file(path);
	if (!file)
		return 0;
	string l;
	while (getline(file, l)) {
		string command(l, 0, 6);
		if (command == "Filter") {
			size_t tp = l.find("ON");
			if (tp == string::npos)
				return 0;
			tp += 3;
			string eq_type = substr_to_nearest_space(l, tp);
			types.push_back(eqapo_to_sox[eq_type]);
			vector<string> args;

			if (eq_type != "IIR") {
				tp = l.find("Fc");
				if (tp == string::npos)
					return 0;
				tp += 3;
				string freq = substr_to_nearest_space(l, tp);
				size_t gp = l.find("Gain");
				size_t qp = l.find('Q');
				size_t bwp = l.find("BW Oct");
				string gain = substr_to_nearest_space(l, tp);
				if ((eq_type.substr(0, 2) != "LS") &&
					(eq_type.substr(0, 2) != "HS")) {
					args.push_back(freq);
					if (bwp != string::npos) {
						bwp += 7;
						string bw = substr_to_end(l, bwp) + "o";
						args.push_back(bw);
					} else if (qp != string::npos) {
						qp += 2;
						string q = substr_to_end(l, qp) + "q";
						args.push_back(q);
					} else {
						string q;
						if (eq_type == "BP") {
							q = "0.7071q";
							args.push_back(q);
						} else { // eq_type == "NO"
							q = "30q";
							args.push_back(q);
						}
					}
					if (eq_type == "PK") {
						gp += 5;
						string gain = substr_to_nearest_space(l, gp);
						args.push_back(gain);
					}
				} else {
					gp += 5;
					string gain = substr_to_nearest_space(l, gp);
					args.push_back(gain);
					args.push_back(freq);
					if (qp != string::npos) {
						qp += 2;
						string q = substr_to_end(l, qp) + "q";
						args.push_back(q);
					} else if (bwp != string::npos) {
						bwp += 7;
						string bw = substr_to_end(l, bwp) + "o";
						args.push_back(bw);
					}
				}
			} else {
				tp = l.find("Order");
				if (tp == string::npos)
					return 0;
				tp += 6;
				string order = substr_to_nearest_space(l, tp);
				if (order != "2")
					return 0;
				tp = l.find("Coefficients");
				if (tp == string::npos)
					return 0;
				tp += 13;
				for (size_t i = 0; i < 6; i++) {
					string c;
					if (i < 5)
						c = substr_to_nearest_space(l, tp);
					else
						c = substr_to_end(l, tp);
					if (c == "")
						return 0;
					tp += c.length() + 1;
					args.push_back(c);
				}
			}
			argv.push_back(args);
		} else if (command == "Preamp") {
			types.push_back("gain");
			argv.push_back({substr_to_nearest_space(l, 8)});
		} else
			return 0;
	}
	return 1;
}
