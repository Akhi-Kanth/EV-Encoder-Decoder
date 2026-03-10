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

// bit reader to unpackage the bytes back into bits and process them 
struct BitReader {
    ifstream &in;          
    uint8_t buffer = 0; 
    int count = 0;      // how many bits are left in the buffer
    bool eof = false;

    // Constructor
    BitReader(ifstream &ifs) 
        : in(ifs) 
    { 
    }

    // Read a single bit
    bool readBit() {
        // if buffer is empty, refill it
        if (count == 0) {
            int c = in.get();

            if (c == EOF) {
                eof = true;
                throw runtime_error("BitReader: reached EOF");
            }

            buffer = static_cast<uint8_t>(c);
            count = 8;
        }

        // get the the lowest-order bit
        bool b = buffer & 1u;

        // shift the byte down the buffer to remove that bit
        buffer >>= 1;
        count--;

        return b;
    }

    // check if there are more bits to read
    bool hasMore() const {
        // bits remain in buffer or in the stream stream isn't exhausted
        return (count > 0) || !in.eof();
    }

    // Read n bits and return them as an integer
    uint32_t readBits(int n) {
        uint32_t v = 0;

        for (int i = 0; i < n; ++i) {
            if (readBit()) {
                v |= (1u << i);  // set bit at position i
            }
        }

        return v;
    }


    // read a full byte
    uint8_t readByte() {
        return static_cast<uint8_t>(readBits(8));
    }

    // read a string: first length, then characters
    string readString() {
        uint8_t len = readByte();

        string s;
        for (int i = 0; i < len; ++i) {
            char c = static_cast<char>(readByte());
            s.push_back(c);
        }

        return s;
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

unordered_map<uint8_t, string> section1_rev, section2_rev;

void build_reverse_maps() {
    for (auto &p : section1_map) section1_rev[p.second] = p.first;
    for (auto &p : section2_map) section2_rev[p.second] = p.first;
}

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


// variable id decoders
// Read one encoded word 
string read_word(ifstream &ifs, const unordered_map<uint8_t,string> &rev_map) {
    
    int b = ifs.get();
    if (b == EOF) {
        throw runtime_error("Unexpected EOF");
    }

    uint8_t code = static_cast<uint8_t>(b);

    // If it's a literal escape marker (0xFF), read the literal string
    if (code == 0xFF) {
        int len = ifs.get();
        if (len == EOF) {
            throw runtime_error("Unexpected EOF in literal");
        }

        string s(len, '\0');
        ifs.read(&s[0], len);

        return s;
    } 
    else {
        // otherwise, look it up in the dictionary
        auto it = rev_map.find(code);
        if (it == rev_map.end()) {
            throw runtime_error("Unknown code in dictionary");
        }
        return it->second;
    }
}

// this is to decode each section in the variable-associations part
string decode_section(ifstream &ifs, const unordered_map<uint8_t,string> &rev_map) {
    stringstream ss;

    while (true) {
        int b = ifs.peek();
        if (b == EOF) {
            throw runtime_error("Unexpected EOF");
        }

        // stop when hitting end-of-section marker
        if (b == 0xFE) {
            ifs.get(); 
            break;
        }

        // decode one word and append it with a space
        ss << read_word(ifs, rev_map) << " ";
    }

    // remove trailing space 
    string result = ss.str();
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    return result;
}

// decode variable id
// decode variable id
string decode_line(ifstream &ifs, uint32_t &counter, bool is_first_line, int &base_id) {
    
    // Decode both sections
    string s1 = decode_section(ifs, section1_rev);

    if (ifs.get() != '(') {
        throw runtime_error("Expected '('");
    }


    string s2 = decode_section(ifs, section2_rev);

    if (ifs.get() != ')') {
        throw runtime_error("Expected ')'");
    }

    // Handle counter/base_id value
    if (is_first_line) {
        ifs.read(reinterpret_cast<char*>(&counter), sizeof(counter));
        base_id = static_cast<int>(counter);  // store first variable id as base_id
    } else {
        counter++;  // regenerate sequential counter
    }

    return s1 + " (" + s2 + "): " + to_string(counter);
}

// decode full file
void decode_file(const string in_file, const string out_file){
    ifstream ifs(in_file, ios::binary);
    if (!ifs) throw runtime_error("failed to open input binary");

    ofstream ofs(out_file); 
    if (!ofs) throw runtime_error("failed to open output csv");

    BitReader br(ifs);

    // decode header
    string header_line = br.readString();
    ofs << header_line << "\n"; 

    // decode variable section
    uint32_t counter = 0;
    bool first_var = true;
    int base_id = -1; // store first variable id

    while (true) {
        int next_byte = ifs.peek();
        if (next_byte == EOF || next_byte == 0xFD) {
            if (next_byte == 0xFD) ifs.get(); 
            break;
        }
        string decoded = decode_line(ifs, counter, first_var, base_id);
        ofs << decoded << "\n";
        first_var = false;
    }

    // decode data section
    try {
        int32_t cur_ts = static_cast<int32_t>(br.readBits(32));
        int cur_id = base_id + static_cast<int>(br.readBits(12)); // use base_id here

        uint32_t tag = br.readBits(2);
        string cur_val;
        if (tag == 0b00) cur_val = "0";
        else if (tag == 0b01) cur_val = "1";
        else cur_val = br.readString();

        ofs << cur_ts << "," << cur_id << "," << cur_val << "\n";

        while (true) {
            // decode timestamp prefix
            uint32_t prefix = br.readBits(2);
            if (prefix == 0b11) {
                uint32_t small10 = br.readBits(10);
                if (small10 == 1023) break; // end marker
                cur_ts += static_cast<int32_t>(small10);
            } else if (prefix == 0b01) {
                cur_ts += 1;
            } else if (prefix == 0b10) {
                cur_ts += static_cast<int32_t>(br.readBits(3) + 2);
            }

            // decode id
            bool id_flag = br.readBit();
            if (!id_flag) {
                cur_id += 1; // sequential
            } else {
                cur_id = base_id + static_cast<int>(br.readBits(12)); // use base_id
            }

            // decode value
            uint32_t vtag = br.readBits(2);
            if (vtag == 0b00) cur_val = "0";
            else if (vtag == 0b01) cur_val = "1";
            else cur_val = br.readString();

            // add all to a line
            ofs << cur_ts << "," << cur_id << "," << cur_val << "\n";
        }

    } catch (...) { 
        // end of data or error
    }

    ifs.close();
    ofs.close();
    cout << "decoding complete -> " << out_file << endl;
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

    // build reverse maps (used during decoding)
    build_reverse_maps();

    try {
        decode_file(in_file, out_file);
    } 
    catch (const exception &ex) {
        cerr << "Error: " << ex.what() << endl;
        return 1;
    }

    return 0;
}