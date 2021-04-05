// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "bc.h"
#include "def.h"
#include "expr.h"
#include "regs.h"
#include "stmt.h"

void au_parser_init(struct au_parser *p, struct au_program_data *p_data) {
    memset(p, 0, sizeof(struct au_parser));

    p->bc = (struct au_bc_buf){0};
    au_hm_vars_init(&p->vars);
    au_hm_vars_init(&p->consts);
    p->p_data = p_data;

    p->num_locals = 0;
    p->max_register = -1;
    p->block_level = 0;

    p->local_to_reg = (struct reg_array){0};

    p->self_name = 0;
    p->self_len = 0;
    p->self_fill_call = (struct size_t_array){0};
    p->self_num_args = 0;
    p->self_flags = 0;
    p->func_idx = AU_SM_FUNC_ID_MAIN;

    p->class_interface = 0;
    p->self_keyword = 0;
    p->self_keyword_len = 0;

    au_parser_flush_cached_regs(p);
    au_parser_flush_free_regs(p);

    p->top_level = 0;
}

void au_parser_del(struct au_parser *p) {
    au_data_free(p->bc.data);
    au_hm_vars_del(&p->vars);
    au_data_free(p->local_to_reg.data);
    au_hm_vars_del(&p->consts);
    au_data_free(p->self_fill_call.data);
    memset(p, 0, sizeof(struct au_parser));
}

int au_parser_exec(struct au_parser *p, struct au_lexer *l) {
    while (1) {
        int retval = au_parser_exec_statement(p, l);
        if (retval == 0)
            return 0;
        else if (retval == -1)
            break;
        au_parser_flush_free_regs(p);
    }
    au_parser_emit_bc_u8(p, AU_OP_RET_NULL);
    au_parser_emit_pad8(p);
    au_parser_emit_pad8(p);
    au_parser_emit_pad8(p);
    return 1;
}

struct au_parser_result au_parse(const char *src, size_t len,
                                 struct au_program *program) {
    struct au_program_data p_data;
    au_program_data_init(&p_data);
    au_stdlib_export(&p_data);

    struct au_lexer l;
    au_lexer_init(&l, src, len);

    struct au_parser p;
    au_parser_init(&p, &p_data);
    if (!au_parser_exec(&p, &l)) {
        struct au_parser_result res = p.res;
        au_lexer_del(&l);
        au_parser_del(&p);
        au_program_data_del(&p_data);
        assert(res.type != AU_PARSER_RES_OK);
        return res;
    }

    for (size_t i = 0; i < p_data.fns.len; i++) {
        if (p_data.fns.data[i].type == AU_FN_NONE) {
            struct au_token name_token =
                p_data.fns.data[i].as.none_func.name_token;
            au_lexer_del(&l);
            au_parser_del(&p);
            au_program_data_del(&p_data);
            return (struct au_parser_result){
                .type = AU_PARSER_RES_UNKNOWN_FUNCTION,
                .data.unknown_id.name_token = name_token,
            };
        }
    }

    struct au_bc_storage p_main;
    au_bc_storage_init(&p_main);
    p_main.func_idx = AU_SM_FUNC_ID_MAIN;
    p_main.bc = p.bc;
    p_main.num_locals = p.num_locals;
    p_main.num_registers = p.max_register + 1;
    p_main.num_values = p_main.num_locals + p_main.num_registers;
    p.bc = (struct au_bc_buf){0};

    program->main = p_main;
    program->data = p_data;

    au_lexer_del(&l);
    au_parser_del(&p);
    // p_data is moved
    return (struct au_parser_result){
        .type = AU_PARSER_RES_OK,
    };
}
