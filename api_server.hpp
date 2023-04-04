#pragma once

#include <algorithm>
#include <vector>
#include <regex>
#include <set>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/common/thread.hpp>

#include <fstream>
#include <nlohmann/json.hpp>

#include <boost/asio.hpp>

#include "websocket_server.hpp"

using json = nlohmann::json;

typedef websocketpp::server<websocketpp::config::asio> server;
typedef websocketpp::connection<websocketpp::config::asio>::request_type request;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using websocketpp::frame::opcode::TEXT;

using websocketpp::lib::thread;

template <typename server_type>
class api_server {
	private:
		uint16_t port;
		websocket_server<server_type>& socket;
	public:
		api_server(uint16_t _port, websocket_server<server_type>& socket) : port(_port), socket(socket) {
			m_server.init_asio();
			m_server.set_http_handler(bind(&api_server::on_http,this,::_1));

			m_server.clear_access_channels(websocketpp::log::alevel::all);
		}

		void on_http(connection_hdl hdl) {
			server::connection_ptr con = m_server.get_con_from_hdl(hdl);
			con->append_header("content-type", "application/json");
			request req = con->get_request();
			std::string method = req.get_method();
			std::string body = req.get_body();

			if (method == "POST") {
				this->send_event(con, body);
			}

			if (method == "GET") {
				this->num_connect(con, body);
			}
		}

		bool num_connect(server::connection_ptr& con, std::string event_data) {
			std::string url = con->get_request().get_uri();
			std::smatch params;
			
			if (std::regex_match(url, params, std::regex(R"(/get_num/([-_a-zA-Z0-9]*))"))) {

				json status;
				status["status"] = true;
				status["value"] = this->socket.get_connections();
				con->set_body(status.dump());

				con->set_status(websocketpp::http::status_code::ok);

				return true;
			}
			return false;
		}

		bool send_event(server::connection_ptr& con, std::string event_data) {
			std::smatch params;
			std::string url = con->get_request().get_uri();
			if (std::regex_match(url, params, std::regex(R"(/channel/([-_a-zA-Z0-9]*))"))) {

				json request_status;
				json request_json;

				try {
					request_json = json::parse(event_data);
					this->socket.trigger_channel(params[1].str(), request_json["event"].get<std::string>(), request_json["message"].get<json>().dump());
					request_status["status"] = true;
				} catch(json::exception e) {
					request_status["status"] = false;
					request_status["message"] = e.what();
				}
				con->set_body(request_status.dump());
				con->set_status(websocketpp::http::status_code::ok);

				return true;
			}
			return false;
		}

		void run() {
			std::cout << "Api port:" << this->port << " listen..." << std::endl;
			m_server.listen(boost::asio::ip::tcp::v4(), this->port);
			m_server.start_accept();
			m_server.run();
		}
	private:
		server m_server;
};