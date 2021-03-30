#pragma once

#include "platform/platform.h"

struct au_parser;
struct au_lexer;

AU_PRIVATE int au_parser_exec_statement(struct au_parser *p,
                                        struct au_lexer *l);
AU_PRIVATE int au_parser_exec_block(struct au_parser *p,
                                    struct au_lexer *l);

AU_PRIVATE int au_parser_exec_export_statement(struct au_parser *p,
                                               struct au_lexer *l);
AU_PRIVATE int au_parser_exec_import_statement(struct au_parser *p,
                                               struct au_lexer *l);
AU_PRIVATE int au_parser_exec_class_statement(struct au_parser *p,
                                              struct au_lexer *l,
                                              int exported);
AU_PRIVATE int au_parser_exec_def_statement(struct au_parser *p,
                                            struct au_lexer *l,
                                            int exported);
AU_PRIVATE int au_parser_exec_const_statement(struct au_parser *p,
                                              struct au_lexer *l,
                                              int exported);
AU_PRIVATE int au_parser_exec_while_statement(struct au_parser *p,
                                              struct au_lexer *l);
AU_PRIVATE int au_parser_exec_if_statement(struct au_parser *p,
                                           struct au_lexer *l);
AU_PRIVATE int au_parser_exec_print_statement(struct au_parser *p,
                                              struct au_lexer *l);
AU_PRIVATE int au_parser_exec_return_statement(struct au_parser *p,
                                               struct au_lexer *l);
