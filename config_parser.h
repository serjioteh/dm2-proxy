#ifndef P3_CONFIG_PARSER_H
#define P3_CONFIG_PARSER_H

#include <iostream>
#include <fstream>
#include <sstream>

#include <algorithm>
#include <vector>
#include <map>

class ParserConfigFile
{
public:
    typedef std::pair<std::string, unsigned short> pair_addr;

public:
    ParserConfigFile();

    void set_config(const std::string &f_name);
    std::vector<pair_addr>& operator[] (unsigned short key) { return config_map[key]; }
    std::vector<unsigned short>& get_ports() { return ports; }

private:
    void parse_config_file(std::ifstream& config);

private:
    std::map<unsigned short, std::vector<pair_addr>> config_map;
    std::vector<unsigned short> ports;
};


#endif //P3_CONFIG_PARSER_H
