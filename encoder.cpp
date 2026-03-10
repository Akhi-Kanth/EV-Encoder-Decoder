#include <iostream>
#include <algorithm>
#include <bitset>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <unordered_map>

using namespace std;

// bit writer to package individual bits into bytes and write them into files
// added functionaty for strings
struct BitWriter {
    ofstream &out;       
    uint8_t buffer = 0;
    int count = 0;       // number of bits written into buffer so far
    
    BitWriter(ofstream &ofs) : out(ofs) { }

    // destructor - flushes any remaining bits
    ~BitWriter() { 
        flush(); 
    }

    // write a single bit
    void writeBit(bool b) {
        buffer |= (uint8_t(b) << count);
        count++;

        if (count == 8) {
            flush();
        }
    }

    // write n bits from a value
    void writeBits(uint32_t value, int n) {
        for (int i = 0; i < n; ++i) {
            bool bit = (value >> i) & 1u;
            writeBit(bit);
        }
    }

    // flush the buffer to the file
    void flush() {
        if (count == 0) {
            return;
        }

        out.put(static_cast<char>(buffer));
        buffer = 0;
        count = 0;
    }

    // write a full byte
    void writeByte(uint8_t b) {
        for (int i = 0; i < 8; ++i) {
            bool bit = (b >> i) & 1u;
            writeBit(bit);
        }
    }

    // wite a string (a little complicated)
    void writeString(const string &s) {
        if (s.size() > 255) {
            throw runtime_error("String too long to encode");
        }

        // write length first (so the )
        writeByte(static_cast<uint8_t>(s.size()));

        // write each character
        for (char c : s) {
            writeByte(static_cast<uint8_t>(c));
        }
    }
};

// these are dictionaries that map the 64 more common words for the the variable-id phase
// Value Start Request (dash.signals.startRequest): 8202
//     ^^^^^^^^^^ this maps this section
unordered_map<string, uint8_t> section1_map = {
    {"Value",0},{"Voltage",1},{"Cell",2},{"Temperature",3},{"Tire",4},{"Temperatures",5},{"Current",6},
    {"Channel",7},{"Thread",8},{"Connected",9},{"CAN",10},{"Time",11},{"Overcurrent",12},{"Undercurrent",13},
    {"Timeout",14},{"over",15},{"Brake",16},{"Loop",17},{"High",18},{"Low",19},{"Fault",20},{"Limit",21},{"Index",22},
    {"Speed",23},{"Sensor",24},{"Power",25},{"Over",26},{"Minimum",27},{"Maximum",28},{"Board",29},{"Temp",30},
    {"Accel",31},{"Start",32},{"Velocity",33},{"X",34},{"Y",35},{"Z",36},{"Update",37},{"Acceleration",38},{"Pressure",39},
    {"Range",40},{"Wheel",41},{"Control",42},{"Threshold",43},{"DC",44},{"Front",45},{"Module",46},{"Sense",47},
    {"Drive",48},{"Command",49},{"Under",50},{"Absolute",51},{"Battery",52},{"Rod",53},{"Regen",54},{"Reset",55},
    {"Precharge",56},{"Angular",57},{"Bus",58},{"Torque",59},{"Max",60},{"Average",61},{"Light",62},{"Limp",63}
};

// Value Start Request (dash.signals.startRequest): 8202
//                             ^^^^^^^^^^ this maps this section
unordered_map<string, uint8_t> section2_map = {
    {"pcm",0},{"ams",1},{"stack",2},{"faults",3},{"pdu",4},{"cells",5},{"dash",6},{"moc",7},
    {"param",8},{"nav",9},{"thermistors",10},{"pedals",11},{"sensors",12},{"mma",13},{"signals",14},{"watchdog",15},
    {"ins",16},{"connections",17},{"sdl",18},{"charger",19},{"threads",20},{"backLeft",21},{"backRight",22},{"frontLeft",23},
    {"frontRight",24},{"limits",25},{"motor",26},{"implausibility",27},{"pack",28},{"x",29},{"y",30},{"z",31},
    {"temp",32},{"cooling",33},{"thresholds",34},{"status",35},{"min",36},{"max",37},{"avg",38},{"minIndex",39},
    {"maxIndex",40},{"resetCause",41},{"regen",42},{"errors",43},{"amsCanConnected",44},{"dashCanConnected",45},
    {"updateTime",46},{"startupTimeout",47},{"updateTimeout",48},{"uncertainty",49},{"brakeForce",50},{"hobble",51},
    {"limp",52},{"cellV",53},{"intRef",54},{"dieTemp",55},{"Vana",56},{"Vdd",57},{"chipCellSum",58},{"pressed",59},
    {"mode",60},{"sdlCanConnected",61},{"pcmCanConnected",62},{"pduCanConnected",63}
};

// csv helpers
// gpt generated 
bool split_csv_line(const string &line, vector<string> &out) {
    out.clear();
    string cur;
    for (char c : line) {
        if (c == ',') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    if (!cur.empty() || (!line.empty() && line.back() == ',')) out.push_back(cur);
    return !out.empty();
}
static inline string trim(const string &s) {
    size_t a = 0; while (a < s.size() && isspace((unsigned char)s[a])) ++a;
    size_t b = s.size(); while (b > a && isspace((unsigned char)s[b-1])) --b;
    return s.substr(a, b - a);
}

// variable id encoders
void write_word(ofstream &ofs, const unordered_map<string,uint8_t> &map, const string &word) {
    auto it = map.find(word);

    // if the word is in the map, then write the associated code
    if (it != map.end()) {
        ofs.put(it->second);
    } else { 
        // if it is not, then
        ofs.put(0xFF);          // escape
        ofs.put(word.size());   // length
        ofs.write(word.c_str(), word.size());
    }

}

// this is to encode each section in the variable-associations part
void encode_section(ofstream &ofs, const string &s, const unordered_map<string,uint8_t> &map, char sep = ' ') {
    
    istringstream iss(s);
    string word;

    // encode each word in this section
    while (iss >> word) {
        write_word(ofs, map, word);
    }

    // mark the end of this section
    ofs.put(0xFE);

    // add optional separator like '(' or ')'
    if (sep) {
        ofs.put(sep);
    }
}

// encode full variable line 
void encode_line(ofstream &ofs, const string &line, bool is_first_line, int &base_id) {
    
    // find delimiters 
    size_t last_paren_open  = line.rfind('(');
    size_t last_paren_close = line.rfind(')');
    size_t colon_pos        = line.find(':', last_paren_close);

    if (last_paren_open == string::npos || 
        last_paren_close == string::npos || 
        colon_pos == string::npos) 
    {
        throw runtime_error("Invalid line format for encode_line");
    }

    // split into three sections
    string s1 = trim(line.substr(0, last_paren_open));                        
    string s2 = trim(line.substr(last_paren_open + 1, last_paren_close - last_paren_open - 1)); 
    string s3 = trim(line.substr(colon_pos + 1));                             

    // encode both sections
    encode_section(ofs, s1, section1_map, '(');
    encode_section(ofs, s2, section2_map, ')');

    // Only the first line sets the base_id
    if (is_first_line) {
        base_id = static_cast<int>(stoul(s3));       // store as base ID
        ofs.write(reinterpret_cast<char*>(&base_id), sizeof(base_id));  // write to file
    }
}

void encode_data_line(BitWriter &bw, int32_t &last_ts, int &last_id, const string &line, int base_id ,bool is_end_marker = false) {
    if (is_end_marker) {
        // special end-of-data marker
        bw.writeBits(0b11, 2); 
        bw.writeBits(1023, 10);     
        bw.writeBits(0, 32);        // impossible timestamp to mark end
        return;
    }

    // split csv line into parts
    vector<string> parts;
    if (!split_csv_line(line, parts) || parts.size() < 3) return;

    int32_t ts = static_cast<int32_t>(stoll(trim(parts[0]))); // timestamp
    int id = stoi(trim(parts[1]));                            // id
    string val_s = trim(parts[2]);                            // value string

    // encode timestamp
    if (last_ts == -1) {
        bw.writeBits(static_cast<uint32_t>(ts), 32); // first timestamp
    } else {
        int32_t delta = ts - last_ts;
        if (delta == 0) {
            bw.writeBits(0b00, 2);
        } else if (delta == 1) {
            bw.writeBits(0b01, 2);
        } else if (delta >= 2 && delta <= 9) {
            bw.writeBits(0b10, 2);
            bw.writeBits(delta - 2, 3);
        } else {
            if (delta < 0 || delta > 1023) {
                bw.writeBits(0b11, 2);
                bw.writeBits(1023, 10);
                bw.writeBits(static_cast<uint32_t>(ts), 32); // full timestamp
            } else {
                bw.writeBits(0b11, 2);
                bw.writeBits(static_cast<uint32_t>(delta), 10);
            }
        }
    }
    last_ts = ts;

    // encode id
    if (last_id == -1) {
        int off = max(0, min(4095, id - 8192));
        bw.writeBits(static_cast<uint32_t>(off), 12);
    } else if (id == last_id + 1) {
        bw.writeBit(0); // sequential
    } else {
        bw.writeBit(1); // explicit
        int off = max(0, min(4095, id - 8192));
        bw.writeBits(static_cast<uint32_t>(off), 12);
    }
    last_id = id;

    // encode value
    if (val_s == "0" || val_s == "0.0") {
        bw.writeBits(0b00, 2);
    } else if (val_s == "1" || val_s == "1.0") {
        bw.writeBits(0b01, 2);
    } else {
        bw.writeBits(0b10, 2);
        bw.writeString(val_s);
    }
}

int main(int argc, char **argv) {
    
    // check arguments: need exactly 3 (input file, output file)
    if (argc != 3) {
        cerr << "Usage: " << argv[0] 
             << " input_file output_file\n";
        return 1;
    }

    // parse command line arguments
    string in_file  = argv[1];
    string out_file = argv[2];
    int base_id;

    try {
        // open input text file
        ifstream ifs(in_file);
        if (!ifs) throw runtime_error("Failed to open input file");

        // open binary output file
        ofstream ofs(out_file, ios::binary);
        if (!ofs) throw runtime_error("Failed to open output file");

        BitWriter bw(ofs);

        string line;
        bool first_var = true;

        // encode header line
        string header_line;
        if (getline(ifs, header_line)) { 
            bw.writeString(header_line);  // write header as string
        }

        // encode variable definitions 
        while (getline(ifs, line)) {
            line = trim(line);

            if (line.empty()) {
                continue; // skip blanks
            }

            // variable definition lines contain '(' and ':'
            if (line.find('(') != string::npos && 
                line.find(':') != string::npos) 
            {
                encode_line(ofs, line, first_var, base_id);
                first_var = false;
            } 
            else {
                // stop when we hit first data (CSV) line
                break;
            }
        }

        // put divider between variable section and data section
        bw.flush();
        ofs.put(0xFD);

        // encode data lines 
        int32_t last_ts = -1;
        int last_id     = -1;

        // encode first line if we already read one above
        if (!line.empty()) {
            vector<string> parts;
            if (split_csv_line(line, parts) && parts.size() >= 3) {
                encode_data_line(bw, last_ts, last_id, line, base_id);
            }
        }

        // encode the rest of the data lines
        while (getline(ifs, line)) {
            line = trim(line);
            if (line.empty()) continue;

            vector<string> parts;
            if (split_csv_line(line, parts) && parts.size() >= 3) {
                encode_data_line(bw, last_ts, last_id, line, base_id);
            }
        }

        // write end-of-data marker
        encode_data_line(bw, last_ts, last_id, "", base_id, true);

        // flush and close
        bw.flush();
        ofs.close();
        ifs.close();

        cout << "Encoding complete -> " << out_file << endl;
    } 
    catch (const exception &ex) {
        cerr << "Error: " << ex.what() << endl;
        return 1;
    }

    return 0;
}