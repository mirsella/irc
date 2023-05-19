#include "../includes/Server.hpp"
#include <asm-generic/socket.h>
#include <cstring>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <utility>
#include <vector>

Server::Server(int port, std::string password) :
		_port(port), _password(password) {
	_createsocket();
	_bindsocket();
	if (listen(_socket_fd, BACKLOG) < 0)
		throw std::runtime_error("Error: Cannot listen");
	_createpoll();
	_init_commands();
};

Server::~Server() {
	std::map<int, Client*>::iterator clit = _vector_clients.begin();
	while (clit != _vector_clients.end()) {
		// TODO: disconnect user, like a /kill
		/* clit->second->send_message() */
		delete clit->second;
		clit++;
	}
	std::vector<Channel *>::iterator chit = _vector_channels.begin();
	while (chit != _vector_channels.end()) {
		delete *chit;
		chit++;
	}
};

void Server::_createsocket() {
	// AF_INET = addr ip v4 || SOCK_STREAM = protocol TCP 
	if ((_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
		throw std::runtime_error("Error: Cannot create socket");
	int optval = 1;
	if (setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR,
				&optval, sizeof(optval)) < 0)
		throw std::runtime_error("Error: Cannot set socket");
};

void Server::_bindsocket() {
	_socket_addr.sin_family = AF_INET;
	_socket_addr.sin_addr.s_addr = INADDR_ANY; // "127.0.0.1"
	_socket_addr.sin_port = htons(_port);
	memset(_socket_addr.sin_zero, 0, sizeof(_socket_addr.sin_zero));
	if (bind(_socket_fd, reinterpret_cast<struct sockaddr *>(&_socket_addr),
				sizeof(struct sockaddr)) < 0)
		throw std::runtime_error("Error: Cannot bind");
}

void Server::_createpoll(){
	if ((_epoll_fd = epoll_create(BACKLOG)) < 0)
		throw std::runtime_error("Error: Cannot create epoll");
	_epoll_event.data.fd = _socket_fd;
	_epoll_event.events = EPOLLIN;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _socket_fd, &_epoll_event) < 0)
		throw std::runtime_error("Error: Cannot control epoll");
};

void Server::run_server() {
	int	new_event_fd;
	while (1) {
		new_event_fd = epoll_wait(_epoll_fd, _epoll_tab_events, MAX_EVENTS, -1);
		for (int i = 0; i < new_event_fd; i++){
			if (_epoll_tab_events[i].data.fd == _socket_fd)
				_handle_connection();					
			else
				_handle_new_msg(i);
		}
		// ping();
	}
};

void Server::_handle_connection() {
	int					client_d;
	struct sockaddr_in	client_addr;
	socklen_t			client_addr_len;

	// accept the connection request by the client
	memset(&client_addr, 0, sizeof(client_addr));
	memset(&client_addr_len, 0, sizeof(client_addr_len));
	if ((client_d = accept(_socket_fd,
					reinterpret_cast<struct sockaddr*>(&client_addr), &client_addr_len)) < 0)
		throw std::runtime_error("Error: Cannot accept connection");

	// fill the socket adress with getsockname() + add new client to vector
	getsockname(client_d, reinterpret_cast<struct sockaddr*>(&client_addr),
			&client_addr_len); 
	// _vector_clients.push_back(new Client(client_d, inet_ntoa(client_addr.sin_addr)));
	_vector_clients.insert(std::make_pair(client_d,
				new Client(client_d, inet_ntoa(client_addr.sin_addr))));

	// add client_d to the poll
	_epoll_event.data.fd = client_d;
	_epoll_event.events = EPOLLIN;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_d, &_epoll_event) < 0)
		throw std::runtime_error("Error: Cannot control epoll");
};

void Server::_handle_new_msg(int i) {
    char	buffer[BUFFER_SIZE];
    std::string input_buf;
    int		ret;
    Client* client;

    std::map<int, Client*>::iterator it = _vector_clients.find(_epoll_tab_events[i].data.fd);
    client = it->second;
    memset(buffer, 0, BUFFER_SIZE);
    if ((ret = recv(_epoll_tab_events[i].data.fd, buffer, BUFFER_SIZE, 0)) < 0)
        throw std::runtime_error("Error: read msg");
    else if (ret == 0) {
        // disconnect
    } else {
		std::string	cmd;
		std::vector<std::string> args;

        input_buf = client->getInput() + buffer;
        client->setInput(input_buf);
        size_t pos = client->getInput().find_first_of("\r\n");
        while (pos != std::string::npos) {
			cmd = client->getInput().substr(0, pos);
            client->setInput(client->getInput().substr(pos + 1));
            // std::cout << "Input: " << cmd << std::endl;
            args = split(cmd, " ");
            if (args.size()) {
                if (_commands.find(args[0]) != _commands.end()) {
                    _commands[args[0]](*this, *client, args);
                } else {
                    /* std::cout << "Error: Invalid command" << std::endl; */
					client->send_message(ERR_UNKNOWNCOMMAND(client->getNickname(), args[0]));
                }
            }
            else {
                /* std::cout << "Error: Invalid command" << std::endl; */
				client->send_message(ERR_UNKNOWNCOMMAND(client->getNickname(), args[0]));
            }
            pos = client->getInput().find_first_of("\r\n");
        }
    }
};

const std::string	Server::getPassword() const { return _password; }

void	Server::_init_commands( void ) {
	// For now, just add NICK, USER and PASS
	_commands.insert(std::pair<std::string, bool (*)(Server&, Client&, std::vector<std::string>&)>("NICK", &cmd_nick));
	_commands.insert(std::pair<std::string, bool (*)(Server&, Client&, std::vector<std::string>&)>("USER", &cmd_user));
	_commands.insert(std::pair<std::string, bool (*)(Server&, Client&, std::vector<std::string>&)>("PASS", &cmd_pass));
}
