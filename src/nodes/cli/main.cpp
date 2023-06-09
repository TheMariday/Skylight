#include <iostream>
#include "spdlog/spdlog.h"
#include "skylight_message/user_command.hpp"
#include "skylight_messaging.h"
#include "skylight_time.h"
#include "CLI11.hpp"


int main(int argc, char **argv) {

    CLI::App app{"App description"};

    std::string broadcastChannel = "command";
    app.add_option("-c,--channel", broadcastChannel, "The channel to broadcast the command");

    CLI11_PARSE(app, argc, argv);

    skylight::Messaging messaging;

    if (!messaging.good())
        return 1;

    do {
        std::cout << "Command to send to '" << broadcastChannel << "': ";
        skylight_message::user_command userCommand;
        std::cin >> userCommand.command;
        userCommand.timestamp = skylight::Now();
        messaging.publish(broadcastChannel, &userCommand);
    } while (std::cin.get() == '\n');

    return 0;
}