#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

const char* file_config = "config.ini";


class config {
	private:
		boost::property_tree::ptree pt;
	public:
	
		config() {
			if (!boost::filesystem::exists(file_config)) {
				std::ofstream ofs(file_config);
				ofs << "api_port = 8123\n";
				ofs << "websocket_port = 9002\n";
				ofs << "websocket_threads = 1\n";
				ofs << "request_host = http://127.0.0.1\n";
				ofs << "request_timeout = 10000\n";
				ofs << "tls_enable = false\n";
				ofs << "certificate_chain_file = cert.pem\n";
				ofs << "private_key_file = key.pem\n";
				assert(ofs);
				ofs.close();
			}
			try {
				boost::property_tree::ini_parser::read_ini(file_config, pt);
			} catch(std::exception ex) {
				std::cerr << ex.what() << std::endl;
			}
		}

		std::string get(std::string name) {
			if (pt.count(name)) {
				return pt.get<std::string>(name);
			}
			return "";
		}
};
