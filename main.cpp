
#include <iostream>
#include <thread>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

#include "config.hpp"
//#include "logs.hpp"
#include "websocket_server.hpp"
#include "api_server.hpp"

typedef websocketpp::server<websocketpp::config::asio> server_no_tls;
typedef websocketpp::server<websocketpp::config::asio_tls> server_tls;

using namespace std;

int main() {
	config conf;
	//logs();
	
	cout << "Starting..." << endl;
	cout << "Response host:" << conf.get("request_host") << endl;
	
	bool tls_mode = false;
	if (!conf.get("tls_enable").empty()) {
		tls_mode = conf.get("tls_enable") == "true" ? true : false;
	}
	cout << "tls:";
	if (tls_mode) {
		cout << "on" << endl;
		websocket_server<server_tls> websocket(stoi(conf.get("websocket_port")), conf);
		api_server<server_tls> api(stoi(conf.get("api_port")), websocket);
		thread t1([](api_server<server_tls> &api){
			api.run();
		}, ref(api));

		thread t2([](websocket_server<server_tls> &websocket){
			websocket.run();
		}, ref(websocket));
		
		t1.detach();
		t2.join();
	}else{
		cout << "off" << endl;
		websocket_server<server_no_tls> websocket(stoi(conf.get("websocket_port")), conf);
		api_server<server_no_tls> api(stoi(conf.get("api_port")), websocket);
		thread t1([](api_server<server_no_tls> &api){
			api.run();
		}, ref(api));

		thread t2([](websocket_server<server_no_tls> &websocket){
			websocket.run();
		}, ref(websocket));
		
		t1.detach();
		t2.join();
	}
}
