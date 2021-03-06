#include "moltengamepad.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cctype>
#include "parser.h"


int do_save(moltengamepad* mg, std::vector<token> &command);
int do_print(moltengamepad* mg, std::vector<token> &command);
int do_load(moltengamepad* mg, std::vector<token> &command);
int do_move(moltengamepad* mg, std::vector<token> &command);
int do_alterslot(moltengamepad* mg, std::vector<token> &command);
  
#define HELP_TEXT "available commands:\n"\
"\tprint:\tprint out lists and information\n"\
"\tmove:\tmove a device to a different slot\n"\
"\talterslot:\tchange a slot setting\n"\
"\tsave:\tsave all profiles to a file\n"\
"\tload:\tload profiles from a file\n"\
"\tquit:\tquit this application\n"\
"\t<profile>.<event> = <outevent>\n"\
"\t\tchange the event mapping for <event> to <outevent> in the profile <profile>"
int do_command(moltengamepad* mg, std::vector<token> &command) {
  if (command.empty()) return 0;
  if (command.front().type == TK_ENDL) return 0;
  if (command.front().value == "print") {
    device_delete_lock.lock();
    do_print(mg,command);
    device_delete_lock.unlock();
    return 0;
  }
   if (command.front().value == "move") {
    device_delete_lock.lock();
    do_move(mg,command);
    device_delete_lock.unlock();
    return 0;
  }
    
  if (command.front().value == "save") return do_save(mg,command);
  if (command.front().value == "load") return do_load(mg,command);
  if (command.front().value == "alterslot") return do_alterslot(mg,command);
  if (command.front().value == "help") {std::cout<<HELP_TEXT<<std::endl; return 0;};
  if (command.front().value == "quit") {return 0;};
  std::cout << "Command not recognized. \"help\" for available commands" << std::endl;
  return 0;
}



int shell_loop(moltengamepad* mg, std::istream &in) {
  bool keep_looping = true;
  std::string header = "";
  char* buff = new char [1024];
  while(!QUIT_APPLICATION && keep_looping) {
    in.getline(buff,1024);
    
    auto tokens = tokenize(std::string(buff));
    
    if (!tokens.empty() && tokens.front().value == "quit") {
      keep_looping = false;
    }
    
    parse_line(tokens,header,mg);
    
    
    if (in.eof()) break;
    

  }
  
    delete[] buff;
}

