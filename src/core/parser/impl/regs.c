#include "def.h"

int au_parser_new_reg(struct au_parser *p, uint8_t *out) {
    if (AU_UNLIKELY(p->rstack_len + 1 > AU_REGS)) {
        return 0;
    }

    uint8_t reg = 0;
    int found = 0;
    for (int i = 0; i < AU_REGS; i++) {
        if (!AU_BA_GET_BIT(p->used_regs, i)) {
            found = 1;
            reg = i;
            AU_BA_SET_BIT(p->used_regs, i);
            break;
        }
    }
    if (!found) {
        return 0;
    }

    p->rstack[p->rstack_len++] = reg;
    if (reg > p->max_register)
        p->max_register = reg;
    *out = reg;
    return 1;
}

/// The caller must NOT call any opcode that changes the result in the last
/// register
uint8_t au_parser_last_reg(struct au_parser *p) {
    if (AU_UNLIKELY(p->rstack_len == 0))
        abort();
    return p->rstack[p->rstack_len - 1];
}

void au_parser_swap_top_regs(struct au_parser *p) {
    if (AU_UNLIKELY(p->rstack_len < 2))
        abort(); // TODO
    const uint8_t top2 = p->rstack[p->rstack_len - 2];
    p->rstack[p->rstack_len - 2] = p->rstack[p->rstack_len - 1];
    p->rstack[p->rstack_len - 1] = top2;
}

void au_parser_push_reg(struct au_parser *p, uint8_t reg) {
    AU_BA_SET_BIT(p->used_regs, reg);
    if (AU_UNLIKELY(p->rstack_len + 1 > AU_REGS))
        abort(); // TODO
    p->rstack[p->rstack_len++] = reg;
    if (reg > p->max_register)
        p->max_register = reg;
}

void au_parser_set_reg_unused(struct au_parser *p, uint8_t reg) {
    if (!AU_BA_GET_BIT(p->pinned_regs, reg)) {
        assert(AU_BA_GET_BIT(p->used_regs, reg));
        AU_BA_RESET_BIT(p->used_regs, reg);
    }
}

uint8_t au_parser_pop_reg(struct au_parser *p) {
    if (AU_UNLIKELY(p->rstack_len == 0))
        abort(); // TODO
    uint8_t reg = p->rstack[--p->rstack_len];
    au_parser_set_reg_unused(p, reg);
    return reg;
}

uint8_t au_parser_pop_reg_no_consume(struct au_parser *p) {
    if (AU_UNLIKELY(p->rstack_len == 0))
        abort(); // TODO
    return p->rstack[--p->rstack_len];
}

void au_parser_flush_cached_regs(struct au_parser *p) {
    for (size_t i = 0; i < p->local_to_reg.len; i++) {
        p->local_to_reg.data[i] = CACHED_REG_NONE;
    }
    for (int i = 0; i < AU_BA_LEN(AU_REGS); i++) {
        p->pinned_regs[i] = 0;
    }
}

void au_parser_flush_free_regs(struct au_parser *p) {
    p->rstack_len = 0;
    for (int i = 0; i < AU_BA_LEN(AU_REGS); i++) {
        p->used_regs[i] = p->pinned_regs[i];
    }
}
