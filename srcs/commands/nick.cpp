#include "../../includes/commands.hpp"

bool	parse_nick(std::string &nickname) {
	if (nickname.size() > 9) {
		std::cerr << "[NICK] Error: nickname too long" << std::endl;
		return false;
	}
	if (nickname.find_first_not_of(NICK_VALID_CHARS) != std::string::npos) {
		std::cerr << "[NICK] Error: invalid nickname" << std::endl;
		return false;
	}
	return true;
}

bool	is_nick_taken(Server &server, std::string &nickname) {
	for (std::map<int, Client*>::iterator it = server.getClients().begin();
			it != server.getClients().end(); it++) {
		if (it->second->getNickname() == nickname) {
			std::cerr << "[NICK] Error: nickname already taken" << std::endl;
			return true;
		}
	}
	return false;
}

bool	cmd_nick(Server &server, Client &client, std::vector<std::string> &input) {
	if (input.size() < 2) {
		std::cerr << "[NICK] Error: not enough arguments" << std::endl;
		// TODO: send ERR_NONICKNAMEGIVEN
		return false;
	}
	if (!parse_nick(input[1])) {
		// TODO: send ERR_ERRONEUSNICKNAME
		return false;
	}
	if (is_nick_taken(server, input[1])) {
		// TODO: send ERR_NICKNAMEINUSE
		return false;
	}
	// if (is_nick_collision(server, input[1])) {
	// 	TODO: send ERR_NICKCOLLISION
	// 	return false;
	// }
	client.setNickname(input[1]);
	std::cout << "New nickname: " << client.getNickname() << std::endl;
	return true;
}