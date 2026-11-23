#ifndef IRCSERVER_HPP
#define IRCSERVER_HPP

#include "Channels.hpp"
#include "ClientId.hpp"
#include "Clients.hpp"
#include "OutgoingMessage.hpp"
#include "Registration.hpp"

#include <map>
#include <string>
#include <vector>

struct IrcMessage;

class IrcServer {
public:
  explicit IrcServer(const std::string &password);

  void HandleConnected(ClientId client_id,
                       std::vector<ClientId> &client_ids_to_disconnect);
  void HandleLine(ClientId client_id, const std::string &line,
                  std::vector<OutgoingMessage> &outgoing_messages,
                  std::vector<ClientId> &client_ids_to_disconnect);
  void HandleDisconnected(ClientId client_id);

private:
  typedef bool (IrcServer::*CommandHandler)(
      ClientId client_id, const IrcMessage &message,
      std::vector<OutgoingMessage> &outgoing_messages);

  struct Command {
    CommandHandler handler;
    bool requires_registration;
  };

  typedef std::map<std::string, Command> CommandMap;

  Clients clients_;
  Registration registration_;
  Channels channels_;
  CommandMap commands_;

  void AddCommand(const std::string &name, CommandHandler handler,
                  bool requires_registration);

  bool ProcessLine(ClientId client_id, const std::string &line,
                   std::vector<OutgoingMessage> &outgoing_messages);
  const Command *FindCommand(const std::string &name) const;

  bool HandleRegistration(ClientId client_id, const IrcMessage &message,
                          std::vector<OutgoingMessage> &outgoing_messages);
  bool HandleJoin(ClientId client_id, const IrcMessage &message,
                  std::vector<OutgoingMessage> &outgoing_messages);

  IrcServer(const IrcServer &other);
  IrcServer &operator=(const IrcServer &other);
};

#endif
