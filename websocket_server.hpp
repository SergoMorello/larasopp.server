#pragma once

#include <thread>
#include <algorithm>
#include <vector>
#include <mutex>
#include <regex>
#include <set>
#include <functional>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/common/thread.hpp>

#include <fstream>
#include <nlohmann/json.hpp>

#include <boost/thread/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include "HTTPRequest.hpp"

using nlohmann::json;

typedef websocketpp::server<websocketpp::config::asio> server;
typedef websocketpp::server<websocketpp::config::asio_tls> server_tls;

typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using websocketpp::frame::opcode::TEXT;

using websocketpp::lib::thread;

std::mutex m_send_lock;
std::mutex m_response_lock;
std::mutex m_message_lock;

struct connection_data {
    std::string token;
	std::vector<std::string> channels;
};

std::string get_password() {
	return "websocket";
}

template <typename server_type>
class websocket_server {
	private:
		uint16_t port;
		config conf;
	public:
		websocket_server(uint16_t _port, config _conf) : port(_port), conf(_conf) {
			m_server.init_asio(&ios);

			if constexpr(std::is_same_v<server_tls, server_type>) {
				m_server.set_tls_init_handler(bind(&websocket_server::on_tls_init,this,::_1));
			}

			m_server.set_validate_handler(bind(&websocket_server::on_validate,this,::_1));
			m_server.set_open_handler(bind(&websocket_server::on_open,this,::_1));
			m_server.set_close_handler(bind(&websocket_server::on_close,this,::_1));
			m_server.set_message_handler(bind(&websocket_server::on_message,this,::_1,::_2));

			m_server.clear_access_channels(websocketpp::log::alevel::all);
			m_server.clear_error_channels(websocketpp::log::alevel::all);
		}

		~websocket_server() {
			for (auto it : m_connections) {
				m_server.close(it.first, websocketpp::close::status::value(1001), "close");
			}
		}

		context_ptr on_tls_init(connection_hdl hdl) {
			context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
			try {
				ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::no_sslv3 |
                         boost::asio::ssl::context::single_dh_use);

				ctx->set_password_callback(bind(&get_password));
				ctx->use_certificate_chain_file(conf.get("certificate_chain_file").c_str());
				ctx->use_private_key_file(conf.get("private_key_file").c_str(), boost::asio::ssl::context::pem);
			} catch (std::exception& e) {
				std::cout << e.what() << std::endl;
			}
			return ctx;
		}

		bool on_validate(connection_hdl hdl) {
			int limit = 10;
			if (!conf.get("websocket_limit").empty()) {
				limit = atoi(conf.get("websocket_limit").c_str());
			}
			return this->num_host_connected(hdl) <= limit ? true : false;
		}
		
		void on_open(connection_hdl hdl) {
			connection_data data;

			std::smatch params;
			typename server_type::connection_ptr con = m_server.get_con_from_hdl(hdl);
			
			std::string url = con->get_request().get_uri();
			std::regex_match(url, params, std::regex(R"(/token=([^&]+))"));
			data.token = this->urlDecode(params[1].str());
			std::cout << "New connect: " << con->get_raw_socket().remote_endpoint().address() << " - " << data.token << std::endl;
			m_connections[hdl] = data;
		}

		int num_host_connected(connection_hdl hdl) {
			int num = 0;
			try {
				boost::asio::ip::address new_ip = m_server.get_con_from_hdl(hdl)->get_raw_socket().remote_endpoint().address();
				for (auto it : m_connections) {
					boost::asio::ip::address list_ip = m_server.get_con_from_hdl(it.first)->get_raw_socket().remote_endpoint().address();
					if (new_ip == list_ip) {
						++num;
					}
				}
			} catch(std::exception& e) {}
			
			return num;
		}

		void on_close(connection_hdl hdl) {
			std::cout << "Close connect" << std::endl;
			m_connections.erase(hdl);
		}

		std::string urlDecode(std::string SRC) {
			std::string ret;
			char ch;
			int i, ii;
			for (i=0; i<SRC.length(); i++) {
				if (SRC[i]=='%') {
					sscanf(SRC.substr(i+1,2).c_str(), "%x", &ii);
					ch=static_cast<char>(ii);
					ret+=ch;
					i=i+2;
				} else {
					ret+=SRC[i];
				}
			}
			return (ret);
		}

		void send(connection_hdl hdl, std::string data, std::string channel, std::string event = "message") {
			std::lock_guard<std::mutex> lock(m_send_lock);
			m_server.pause_reading(hdl);
			
			json response;
			response["channel"] = channel;
			response["event"] = event;
			try {
				response["message"] = json::parse(data);
			} catch(json::exception e) {
				response["message"] = data;
			}
			
			try {
				m_server.send(hdl, response.dump(), websocketpp::frame::opcode::TEXT);
			} catch (const websocketpp::exception &e) {
				std::cout << "Error send: " << e.what();
			}
			m_server.resume_reading(hdl);
		}

		void on_message(connection_hdl hdl, typename server_type::message_ptr msg) {
			std::lock_guard<std::mutex> lock(m_message_lock);

			std::string raw_msg = msg->get_payload();
			if (raw_msg.empty()) {
				return;
			}

			json json_object;
			std::string subscribe, unsubscribe, channel, event, message, type;
			std::string token = m_connections.at(hdl).token;
			try {
				json_object = json::parse(raw_msg);
				if (json_object.count("subscribe")) {
					subscribe = json_object["subscribe"].get<std::string>();
				}

				if (json_object.count("unsubscribe")) {
					unsubscribe = json_object["unsubscribe"].get<std::string>();
				}
				
				if (json_object.count("message")) {
					message = json_object["message"].get<json>().dump();
				}

				if (json_object.count("type")) {
					type = json_object["type"].get<std::string>();
				}

				if (json_object.count("channel")) {
					channel = json_object["channel"].get<std::string>();
				}

				if (json_object.count("event")) {
					event = json_object["event"].get<std::string>();
				}
			} catch(json::exception e) {
				this->send(hdl, "bad json", "error");
				std::cerr << e.what() << std::endl;
				return;
			}

			std::vector<std::string> &channels = m_connections.at(hdl).channels;
			
			// Subscribe
			if (!subscribe.empty()) {
				if (std::find(channels.begin(), channels.end(), subscribe) == channels.end() ) {
					json json_data;
					json_data["token"] = token;
					json_data["channel"] = subscribe;
					this->request_app("/broadcasting/auth", json_data, [hdl, subscribe, &channels, this](std::string response){
						try {
							if (json::parse(response)["success"].get<bool>() == true) {
								std::cout << "auth: ok!" << std::endl;
								channels.push_back(subscribe);
								this->send(hdl, subscribe, "subscribe");
							}
						} catch(json::exception e) {
							std::cout << "auth: fail!" << std::endl;
							this->send(hdl, subscribe, "fail");
						}
					});
				}
			}

			// Unsubscribe
			if (!unsubscribe.empty()) {
				channels.erase(std::remove(channels.begin(), channels.end(), unsubscribe), channels.end());
				this->send(hdl, unsubscribe, "unsubscribe");
			}

			// Message
			if (!message.empty() && !channel.empty() && !event.empty()) {
				if (std::find(channels.begin(), channels.end(), channel) != channels.end() ) {
					if (type == "private") {
						this->event_response(channel, event, message, token);
					} else if (type == "protected") {
						this->trigger_channel(channel, event, message);
					} else {
						this->trigger_channel(channel, event, message);
						this->event_response(channel, event, message, token);
					}
				}
				
			}
		}

		void trigger_channel(std::string channel, std::string event, std::string message) {
			for (auto it : m_connections) {
				if (std::find(it.second.channels.begin(), it.second.channels.end(), channel) != it.second.channels.end() ) {		
					this->send(it.first, message, channel, event);
				}
			}
		}
		void request_app(std::string path, json data, std::function<void(std::string response)> callback) {
			std::thread t1([](config conf, std::string path, json data, std::function<void(std::string response)> callback){
				int request_timeout = 10000;
				if (!conf.get("request_timeout").empty()) {
					request_timeout = atoi(conf.get("request_timeout").c_str());
				}
				try {
					
					http::Request req(std::string(conf.get("request_host") + path));
					const auto response = req.send("POST", data.dump(), {
						{"Content-Type", "application/json"}
					},std::chrono::milliseconds{request_timeout});
					callback(std::string{response.body.begin(), response.body.end()});
					
				} catch (const std::exception& e) {
					std::cerr << "Request failed, error: " << e.what() << '\n';
				}
			}, std::ref(conf), path, data, callback);
			t1.detach();
		}

		void event_response(std::string channel, std::string event, std::string message, std::string token) {
			json json_object;
			json_object["channel"] = channel;
			json_object["event"] = event;
			json_object["token"] = token;
			try {
				json_object["message"] = json::parse(message);
			}  catch(json::exception e) {
				json_object["message"] = message;
			}
			this->request_app("/broadcasting/trigger/" + channel + "/" + event, json_object, [](std::string response){
				//std::cout << response << '\n';
			});
		}

		int get_connections() {
			return m_connections.size();
		}

		void run() {
			std::cout << "Websocket port:" << this->port << " listen..." << std::endl;
			m_server.listen(boost::asio::ip::tcp::v4(), this->port);
			m_server.start_accept();
			//ios.run();
			
			int num_threads = 1;
			if (!conf.get("websocket_threads").empty()) {
				num_threads = atoi(conf.get("websocket_threads").c_str());
			}

			if (num_threads == 1) {
				m_server.run();
			} else {
				typedef websocketpp::lib::shared_ptr<websocketpp::lib::thread> thread_ptr;
				std::vector<thread_ptr> ts;
				for (size_t i = 0; i < num_threads; i++) {
					ts.push_back(websocketpp::lib::make_shared<websocketpp::lib::thread>(&server_type::run, &m_server));
				}

				for (size_t i = 0; i < num_threads; i++) {
					ts[i]->join();
				}
			}
		}
	private:
		typedef std::map<connection_hdl,connection_data,std::owner_less<connection_hdl>> con_list;

		boost::asio::io_service ios;
		server_type m_server;
		con_list m_connections;
};