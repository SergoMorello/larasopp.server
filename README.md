# Laravel websocket and api server

### Compile
```
g++ main.cpp -lpthread -lboost_system -lboost_filesystem -lssl -lcrypto
```

### Config
```
api_port = 8123
websocket_port = 9002
websocket_threads = 1
request_host = http://127.0.0.1
request_timeout = 10000
tls_enable = false
certificate_chain_file = cert.pem
private_key_file = key.pem
```