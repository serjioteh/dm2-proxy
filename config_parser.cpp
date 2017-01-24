#include "config_parser.h"

ParserConfigFile::ParserConfigFile() {}

void ParserConfigFile::set_config(const std::string &f_name)
{
    std::ifstream config(f_name);

    if (!config.is_open()) {
        std::stringstream msg;
        msg << "There is no config file with such name: \"" << f_name << "\"!" << std::endl;
        throw msg.str();
    }

    parse_config_file(config);

    config.close();
}

void ParserConfigFile::parse_config_file(std::ifstream& config)
{
    std::string str_line, str_port, str_ip;
    unsigned short port = 0;

    while (!config.eof())
    {
        std::getline(config, str_line);
        str_line.erase(std::remove_if(str_line.begin(), str_line.end(), isspace), str_line.end());
        if (str_line.empty())
            continue;

        std::istringstream streamline(str_line);

        std::getline(streamline, str_port, ',');
        port = static_cast<unsigned short>(std::stoi(str_port));
        ports.push_back(port);

        config_map[port] = std::vector<pair_addr>();

        while (!streamline.eof())
        {
            std::getline(streamline, str_ip, ':');
            std::getline(streamline, str_port, ',');
            config_map[port].push_back(pair_addr(str_ip, std::stoi(str_port)));
        }
    }
}