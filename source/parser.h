#ifndef PARSER_H
#define PARSER_H
#include <string>
#include <vector>
#include <iostream>
#include "moltengamepad.h"

enum tokentype { TK_IDENT, TK_DOT, TK_EQUAL, TK_HEADER_OPEN, TK_HEADER_CLOSE, TK_LPAREN, TK_RPAREN, TK_COMMA, TK_VALUE, TK_ENDL};

struct token {
  enum tokentype type;
  std::string value;
};

std::vector<token> tokenize(std::string line);

void parse_line(std::vector<token> &line, std::string &header, moltengamepad* mg);

event_translator* parse_trans(enum entry_type intype, std::vector<token> &rhs,slot_manager* slots);

struct complex_expr {
  std::string ident;
  std::vector<complex_expr*> params;
};

struct complex_expr* read_expr(std::vector<token> &tokens, std::vector<token>::iterator &it);
void free_complex_expr(complex_expr* expr);

event_translator* parse_complex_trans(enum entry_type intype, std::vector<token> &rhs);

int do_command(moltengamepad* mg,std::vector<token> &command);

int shell_loop(moltengamepad* mg, std::istream &in);

bool find_token_type(enum tokentype type, std::vector<token> &tokens);

void do_header_line(std::vector<token> &line, std::string &header);

#endif