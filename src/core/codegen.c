#include "core/codegen.h"
#include "core/bus.h"
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <sys/mman.h>
#endif

/* WIN64: rcx=cpu, rdx=bus on entry; non-volatile r15/r14 hold cpu/bus across helper-calls */

static void emit_b(codegen_t* cg, u8 b) {
    if (cg->used < cg->capacity) cg->buffer[cg->used++] = b;
}
static void emit_w16(codegen_t* cg, u16 v) { emit_b(cg, (u8)v); emit_b(cg, (u8)(v >> 8)); }
static void emit_w32(codegen_t* cg, u32 v) {
    emit_b(cg, (u8)v); emit_b(cg, (u8)(v >> 8));
    emit_b(cg, (u8)(v >> 16)); emit_b(cg, (u8)(v >> 24));
}
static void emit_w64(codegen_t* cg, u64 v) {
    for (u32 k = 0; k < 8u; ++k) emit_b(cg, (u8)(v >> (k * 8u)));
}

/* mov eax, [r15 + CG_R_OFF + r*4]  ->  41 8B 87 disp32 */
static void ld_eax(codegen_t* cg, u8 r) {
    emit_b(cg, 0x41); emit_b(cg, 0x8B); emit_b(cg, 0x87);
    emit_w32(cg, CG_R_OFF + (u32)r * 4u);
}
/* mov ecx, [r15 + CG_R_OFF + r*4]  ->  41 8B 8F disp32 */
static void ld_ecx(codegen_t* cg, u8 r) {
    emit_b(cg, 0x41); emit_b(cg, 0x8B); emit_b(cg, 0x8F);
    emit_w32(cg, CG_R_OFF + (u32)r * 4u);
}
/* mov [r15 + CG_R_OFF + r*4], eax  ->  41 89 87 disp32 */
static void st_eax(codegen_t* cg, u8 r) {
    emit_b(cg, 0x41); emit_b(cg, 0x89); emit_b(cg, 0x87);
    emit_w32(cg, CG_R_OFF + (u32)r * 4u);
}
/* mov [r15 + CG_R_OFF + r*4], ecx  ->  41 89 8F disp32 */
static void st_ecx(codegen_t* cg, u8 r) {
    emit_b(cg, 0x41); emit_b(cg, 0x89); emit_b(cg, 0x8F);
    emit_w32(cg, CG_R_OFF + (u32)r * 4u);
}
/* mov dword [r15 + CG_PC_OFF], imm32  ->  41 C7 87 disp32 imm32 */
static void st_pc(codegen_t* cg, u32 pc) {
    emit_b(cg, 0x41); emit_b(cg, 0xC7); emit_b(cg, 0x87);
    emit_w32(cg, CG_PC_OFF);
    emit_w32(cg, pc);
}
static void mov_eax_imm(codegen_t* cg, u32 imm) {
    emit_b(cg, 0xB8); emit_w32(cg, imm);
}
static void op_add_ec(codegen_t* cg) { emit_b(cg, 0x01); emit_b(cg, 0xC8); }
static void op_sub_ec(codegen_t* cg) { emit_b(cg, 0x29); emit_b(cg, 0xC8); }
static void op_and_ec(codegen_t* cg) { emit_b(cg, 0x21); emit_b(cg, 0xC8); }
static void op_or_ec (codegen_t* cg) { emit_b(cg, 0x09); emit_b(cg, 0xC8); }
static void op_xor_ec(codegen_t* cg) { emit_b(cg, 0x31); emit_b(cg, 0xC8); }
static void add_imm(codegen_t* cg, u32 v) { emit_b(cg, 0x05); emit_w32(cg, v); }
static void sub_imm(codegen_t* cg, u32 v) { emit_b(cg, 0x2D); emit_w32(cg, v); }

/* WIN64 helper-call helpers */

/* xor ebx, ebx  (31 DB)  — zero failure flag at prologue end */
static void emit_clear_fail(codegen_t* cg) {
    emit_b(cg, 0x31); emit_b(cg, 0xDB);
}
/* mov rcx, r14  (4C 89 F1)  — arg0 = bus */
static void mov_rcx_r14(codegen_t* cg) {
    emit_b(cg, 0x4C); emit_b(cg, 0x89); emit_b(cg, 0xF1);
}
/* mov edx, eax  (89 C2)  — arg1 = addr (zero-extends) */
static void mov_edx_eax(codegen_t* cg) {
    emit_b(cg, 0x89); emit_b(cg, 0xC2);
}
/* mov r8d, imm32  (41 B8 imm32)  — arg2 = size */
static void mov_r8d_imm(codegen_t* cg, u32 v) {
    emit_b(cg, 0x41); emit_b(cg, 0xB8); emit_w32(cg, v);
}
/* lea r9, [rsp+0]  (4C 8D 0C 24)  — arg3 = &out_slot */
static void lea_r9_rsp0(codegen_t* cg) {
    emit_b(cg, 0x4C); emit_b(cg, 0x8D); emit_b(cg, 0x0C); emit_b(cg, 0x24);
}
/* mov r9d, [r15 + CG_R_OFF + r*4]  (45 8B 8F disp32)  — arg3 = r[rd] for STR */
static void mov_r9d_reg(codegen_t* cg, u8 r) {
    emit_b(cg, 0x45); emit_b(cg, 0x8B); emit_b(cg, 0x8F);
    emit_w32(cg, CG_R_OFF + (u32)r * 4u);
}
/* sub rsp, 16  (48 83 EC 10) */
static void sub_rsp_16(codegen_t* cg) {
    emit_b(cg, 0x48); emit_b(cg, 0x83); emit_b(cg, 0xEC); emit_b(cg, 0x10);
}
/* add rsp, 16  (48 83 C4 10) */
static void add_rsp_16(codegen_t* cg) {
    emit_b(cg, 0x48); emit_b(cg, 0x83); emit_b(cg, 0xC4); emit_b(cg, 0x10);
}
/* mov rax, imm64  (48 B8 imm64) */
static void mov_rax_imm64(codegen_t* cg, u64 v) {
    emit_b(cg, 0x48); emit_b(cg, 0xB8); emit_w64(cg, v);
}
/* call rax  (FF D0) */
static void call_rax(codegen_t* cg) {
    emit_b(cg, 0xFF); emit_b(cg, 0xD0);
}
/* mov ecx, [rsp+0]  (8B 0C 24)  — load out_slot result */
static void mov_ecx_rsp0(codegen_t* cg) {
    emit_b(cg, 0x8B); emit_b(cg, 0x0C); emit_b(cg, 0x24);
}
/* test al, al; jnz +skip  (84 C0 75 skip) — skip failure stub on success */
static void test_al_jnz(codegen_t* cg, u8 skip) {
    emit_b(cg, 0x84); emit_b(cg, 0xC0);
    emit_b(cg, 0x75); emit_b(cg, skip);
}
/* or bl, 1  (80 CB 01)  — mark failure */
static void or_bl_1(codegen_t* cg) {
    emit_b(cg, 0x80); emit_b(cg, 0xCB); emit_b(cg, 0x01);
}
/* jmp short +n  (EB NN) */
static void jmp_short(codegen_t* cg, u8 n) { emit_b(cg, 0xEB); emit_b(cg, n); }
/* and r9d, imm32  (41 81 E1 imm32)  — mask STR val for B/H */
static void and_r9d_imm(codegen_t* cg, u32 v) {
    emit_b(cg, 0x41); emit_b(cg, 0x81); emit_b(cg, 0xE1); emit_w32(cg, v);
}

/* WIN64 thunk prologue: save non-volatile r15/r14/rbx/rsi; shadow space; load cpu/bus.
   4 pushes (32B) + sub rsp,32 = 64B total; entry rsp is 8 mod 16 (ret addr on stack),
   after 4 pushes (32B mod 16 = 0) still 8 mod 16, sub 32 (0 mod 16) -> 8 mod 16 restored. */
static void emit_prologue(codegen_t* cg) {
    emit_b(cg, 0x41); emit_b(cg, 0x57);             /* push r15 */
    emit_b(cg, 0x41); emit_b(cg, 0x56);             /* push r14 */
    emit_b(cg, 0x53);                                /* push rbx */
    emit_b(cg, 0x56);                                /* push rsi */
    emit_b(cg, 0x48); emit_b(cg, 0x83); emit_b(cg, 0xEC); emit_b(cg, 0x20);  /* sub rsp,32 */
    emit_b(cg, 0x49); emit_b(cg, 0x89); emit_b(cg, 0xCF);                    /* mov r15,rcx */
    emit_b(cg, 0x49); emit_b(cg, 0x89); emit_b(cg, 0xD6);                    /* mov r14,rdx */
}

/* Epilogue: check bl failure flag; set halted+return false on fault, else return true.
   Byte layout (must match jz offset = 21):
     84 DB          test bl, bl        (2B)
     74 15          jz .ok             (2B)  ; skip 21 bytes of halt path
     41 C6 87 xx xx xx xx 01  mov byte [r15+CG_HALT_OFF], 1  (8B)
     48 83 C4 20    add rsp, 32        (4B)
     5E             pop rsi            (1B)
     5B             pop rbx            (1B)
     41 5E          pop r14            (2B)
     41 5F          pop r15            (2B)
     30 C0          xor al, al         (2B)
     C3             ret                (1B)  ; total halt path = 21B
   .ok:
     48 83 C4 20    add rsp, 32        (4B)
     5E 5B          pop rsi; pop rbx   (2B)
     41 5E 41 5F    pop r14; pop r15   (4B)
     B0 01          mov al, 1          (2B)
     C3             ret                (1B)
*/
static void emit_epilogue_check(codegen_t* cg) {
    emit_b(cg, 0x84); emit_b(cg, 0xDB);             /* test bl, bl */
    emit_b(cg, 0x74); emit_b(cg, 21u);              /* jz .ok (+21) */
    /* halt path (21 bytes) */
    emit_b(cg, 0x41); emit_b(cg, 0xC6); emit_b(cg, 0x87);  /* mov byte [r15+disp32], imm8 */
    emit_w32(cg, CG_HALT_OFF);
    emit_b(cg, 0x01);
    emit_b(cg, 0x48); emit_b(cg, 0x83); emit_b(cg, 0xC4); emit_b(cg, 0x20); /* add rsp,32 */
    emit_b(cg, 0x5E);                                /* pop rsi */
    emit_b(cg, 0x5B);                                /* pop rbx */
    emit_b(cg, 0x41); emit_b(cg, 0x5E);             /* pop r14 */
    emit_b(cg, 0x41); emit_b(cg, 0x5F);             /* pop r15 */
    emit_b(cg, 0x30); emit_b(cg, 0xC0);             /* xor al, al */
    emit_b(cg, 0xC3);                                /* ret */
    /* .ok: success path */
    emit_b(cg, 0x48); emit_b(cg, 0x83); emit_b(cg, 0xC4); emit_b(cg, 0x20); /* add rsp,32 */
    emit_b(cg, 0x5E); emit_b(cg, 0x5B);             /* pop rsi; pop rbx */
    emit_b(cg, 0x41); emit_b(cg, 0x5E);             /* pop r14 */
    emit_b(cg, 0x41); emit_b(cg, 0x5F);             /* pop r15 */
    emit_b(cg, 0xB0); emit_b(cg, 0x01);             /* mov al, 1 */
    emit_b(cg, 0xC3);                                /* ret */
}

/* LDR Rd, [base_eax + imm]: bus_read(bus, addr, sz, &out) -> r[rd].
   Sequence (~67B):
     ld_eax(rn) + add_imm(imm)     -- addr in eax (skipped for LDR_LIT/LDR_REG callers)
     sub rsp,16                    -- 16B local: [rsp+0..3] = out slot
     WIN64: rcx=bus, rdx=addr, r8d=sz, r9=&out
     mov rax, &bus_read; call rax
     mov ecx, [rsp+0]; add rsp,16
     and ecx, mask (B/H only)
     test al; jnz +5 (skip fail stub)
     or bl,1; jmp +7 (skip store)
     st_ecx(rd)                    -- 7 bytes: success store
*/
static void emit_load_from_eax(codegen_t* cg, u8 rd, u32 sz) {
    sub_rsp_16(cg);                                         /* rsp -= 16; [rsp+0] = out slot */
    mov_rcx_r14(cg);                                        /* rcx = bus */
    mov_edx_eax(cg);                                        /* rdx = addr */
    mov_r8d_imm(cg, sz);                                    /* r8d = size */
    lea_r9_rsp0(cg);                                        /* r9 = &out */
    mov_rax_imm64(cg, (u64)(uintptr_t)bus_read);            /* rax = &bus_read */
    call_rax(cg);                                           /* call bus_read */
    mov_ecx_rsp0(cg);                                       /* ecx = out (before add rsp) */
    add_rsp_16(cg);                                         /* rsp += 16 */
    if (sz == 1u) {
        emit_b(cg, 0x81); emit_b(cg, 0xE1); emit_w32(cg, 0xFFu);     /* and ecx, 0xFF */
    } else if (sz == 2u) {
        emit_b(cg, 0x81); emit_b(cg, 0xE1); emit_w32(cg, 0xFFFFu);   /* and ecx, 0xFFFF */
    }
    /* test al; jnz +5 (skip: or bl,1 (3B) + jmp +7 (2B) = 5B) */
    test_al_jnz(cg, 5u);
    or_bl_1(cg);                                            /* failure: bl |= 1 */
    jmp_short(cg, 7u);                                      /* jump past st_ecx (7B) */
    st_ecx(cg, rd);                                         /* success: r[rd] = ecx (7B) */
}

static void emit_load(codegen_t* cg, u8 rd, u8 rn, u32 imm, u32 sz) {
    ld_eax(cg, rn);
    if (imm) add_imm(cg, imm);
    emit_load_from_eax(cg, rd, sz);
}

/* STR Rd, [base_eax + imm]: bus_write(bus, addr, sz, r[rd]).
   No result store; just check al for fault.
   Sequence (~52B):
     ld_eax(rn) + add_imm(imm)
     sub rsp,16
     WIN64: rcx=bus, rdx=addr, r8d=sz, r9d=r[rd] (masked for B/H)
     mov rax, &bus_write; call rax
     add rsp,16
     test al; jnz +3 (skip or bl,1)
     or bl,1
*/
static void emit_store(codegen_t* cg, u8 rd, u8 rn, u32 imm, u32 sz) {
    ld_eax(cg, rn);
    if (imm) add_imm(cg, imm);
    sub_rsp_16(cg);
    mov_rcx_r14(cg);                                        /* rcx = bus */
    mov_edx_eax(cg);                                        /* rdx = addr */
    mov_r8d_imm(cg, sz);                                    /* r8d = size */
    mov_r9d_reg(cg, rd);                                    /* r9d = r[rd] */
    if (sz == 1u) and_r9d_imm(cg, 0xFFu);
    else if (sz == 2u) and_r9d_imm(cg, 0xFFFFu);
    mov_rax_imm64(cg, (u64)(uintptr_t)bus_write);           /* rax = &bus_write */
    call_rax(cg);
    add_rsp_16(cg);
    /* test al; jnz +3 (skip or bl,1 (3B)) */
    test_al_jnz(cg, 3u);
    or_bl_1(cg);
}

/* op -> x86 emit. */
bool codegen_supports(opcode_t op) {
    switch (op) {
        case OP_MOV_IMM:    case OP_MOV_REG:
        case OP_ADD_REG:    case OP_SUB_REG:
        case OP_ADD_IMM3:   case OP_ADD_IMM8:
        case OP_SUB_IMM3:   case OP_SUB_IMM8:
        case OP_AND_REG:    case OP_ORR_REG: case OP_EOR_REG:
        case OP_NOP:        case OP_T32_NOP:
        case OP_T32_MOV_IMM:
        case OP_T32_ADD_IMM:
        case OP_T32_SUB_IMM:
        case OP_T32_AND_IMM:
        case OP_T32_ORR_IMM:
        case OP_T32_EOR_IMM:
        case OP_T32_ADDW: case OP_T32_SUBW:
        case OP_T32_MOVW:
        /* memory ops */
        case OP_LDR_IMM:    case OP_STR_IMM:
        case OP_LDRB_IMM:   case OP_STRB_IMM:
        case OP_LDRH_IMM:   case OP_STRH_IMM:
        case OP_LDR_SP:     case OP_STR_SP:
        case OP_LDR_LIT:
        case OP_LDR_REG:    case OP_STR_REG:
        case OP_T32_LDR_IMM:   case OP_T32_STR_IMM:
        case OP_T32_LDRB_IMM:  case OP_T32_STRB_IMM:
        case OP_T32_LDRH_IMM:  case OP_T32_STRH_IMM:
        case OP_T32_LDR_LIT:
        case OP_T32_LDR_REG:   case OP_T32_STR_REG:
        case OP_T32_LDRD_IMM:  case OP_T32_STRD_IMM:
            return true;
        default:
            return false;
    }
}

static void emit_op(codegen_t* cg, const insn_t* i) {
    switch (i->op) {
        case OP_NOP: case OP_T32_NOP: break;

        case OP_MOV_IMM: case OP_T32_MOV_IMM: case OP_T32_MOVW:
            mov_eax_imm(cg, i->imm); st_eax(cg, i->rd); break;

        case OP_MOV_REG:
            ld_eax(cg, i->rm); st_eax(cg, i->rd); break;

        case OP_ADD_REG:
            ld_eax(cg, i->rn); ld_ecx(cg, i->rm); op_add_ec(cg); st_eax(cg, i->rd); break;

        case OP_SUB_REG:
            ld_eax(cg, i->rn); ld_ecx(cg, i->rm); op_sub_ec(cg); st_eax(cg, i->rd); break;

        case OP_ADD_IMM3: case OP_ADD_IMM8:
        case OP_T32_ADD_IMM: case OP_T32_ADDW:
            ld_eax(cg, i->rn); add_imm(cg, i->imm); st_eax(cg, i->rd); break;

        case OP_SUB_IMM3: case OP_SUB_IMM8:
        case OP_T32_SUB_IMM: case OP_T32_SUBW:
            ld_eax(cg, i->rn); sub_imm(cg, i->imm); st_eax(cg, i->rd); break;

        case OP_AND_REG:
            ld_eax(cg, i->rn); ld_ecx(cg, i->rm); op_and_ec(cg); st_eax(cg, i->rd); break;
        case OP_T32_AND_IMM:
            ld_eax(cg, i->rn);
            emit_b(cg, 0x25); emit_w32(cg, i->imm);
            st_eax(cg, i->rd); break;

        case OP_ORR_REG:
            ld_eax(cg, i->rn); ld_ecx(cg, i->rm); op_or_ec(cg); st_eax(cg, i->rd); break;
        case OP_T32_ORR_IMM:
            ld_eax(cg, i->rn);
            emit_b(cg, 0x0D); emit_w32(cg, i->imm);
            st_eax(cg, i->rd); break;

        case OP_EOR_REG:
            ld_eax(cg, i->rn); ld_ecx(cg, i->rm); op_xor_ec(cg); st_eax(cg, i->rd); break;
        case OP_T32_EOR_IMM:
            ld_eax(cg, i->rn);
            emit_b(cg, 0x35); emit_w32(cg, i->imm);
            st_eax(cg, i->rd); break;

        /* === LDR/STR immediate offset === */
        case OP_LDR_IMM:    case OP_T32_LDR_IMM:
            emit_load(cg, i->rd, i->rn, i->imm, 4u); break;
        case OP_LDRB_IMM:   case OP_T32_LDRB_IMM:
            emit_load(cg, i->rd, i->rn, i->imm, 1u); break;
        case OP_LDRH_IMM:   case OP_T32_LDRH_IMM:
            emit_load(cg, i->rd, i->rn, i->imm, 2u); break;
        case OP_STR_IMM:    case OP_T32_STR_IMM:
            emit_store(cg, i->rd, i->rn, i->imm, 4u); break;
        case OP_STRB_IMM:   case OP_T32_STRB_IMM:
            emit_store(cg, i->rd, i->rn, i->imm, 1u); break;
        case OP_STRH_IMM:   case OP_T32_STRH_IMM:
            emit_store(cg, i->rd, i->rn, i->imm, 2u); break;

        /* SP-relative */
        case OP_LDR_SP:
            emit_load(cg, i->rd, REG_SP, i->imm, 4u); break;
        case OP_STR_SP:
            emit_store(cg, i->rd, REG_SP, i->imm, 4u); break;

        /* LDR_LIT: addr = ((PC+4)&~3) + imm; baked at compile time */
        case OP_LDR_LIT:    case OP_T32_LDR_LIT: {
            u32 a = ((i->pc + 4u) & ~3u) + i->imm;
            mov_eax_imm(cg, a);                             /* eax = literal addr */
            emit_load_from_eax(cg, i->rd, 4u);
            break;
        }

        /* Register-offset LDR/STR */
        case OP_LDR_REG:    case OP_T32_LDR_REG:
            ld_eax(cg, i->rn); ld_ecx(cg, i->rm); op_add_ec(cg);
            emit_load_from_eax(cg, i->rd, 4u);
            break;
        case OP_STR_REG:    case OP_T32_STR_REG: {
            ld_eax(cg, i->rn); ld_ecx(cg, i->rm); op_add_ec(cg);
            sub_rsp_16(cg);
            mov_rcx_r14(cg); mov_edx_eax(cg); mov_r8d_imm(cg, 4u);
            mov_r9d_reg(cg, i->rd);
            mov_rax_imm64(cg, (u64)(uintptr_t)bus_write);
            call_rax(cg);
            add_rsp_16(cg);
            test_al_jnz(cg, 3u); or_bl_1(cg);
            break;
        }

        /* LDRD: two consecutive loads; second reg is i->rs */
        case OP_T32_LDRD_IMM:
            emit_load(cg, i->rd, i->rn, i->imm,      4u);
            emit_load(cg, i->rs, i->rn, i->imm + 4u, 4u);
            break;

        /* STRD: two consecutive stores */
        case OP_T32_STRD_IMM:
            emit_store(cg, i->rd, i->rn, i->imm,      4u);
            emit_store(cg, i->rs, i->rn, i->imm + 4u, 4u);
            break;

        default: break;
    }
}

bool codegen_init(codegen_t* cg) {
    cg->capacity = CG_BUFFER_SIZE;
    cg->used = 0;
#ifdef _WIN32
    cg->buffer = (u8*)VirtualAlloc(NULL, CG_BUFFER_SIZE,
                                   MEM_COMMIT | MEM_RESERVE,
                                   PAGE_EXECUTE_READWRITE);
#else
    cg->buffer = (u8*)mmap(NULL, CG_BUFFER_SIZE,
                           PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (cg->buffer == MAP_FAILED) cg->buffer = NULL;
#endif
    return cg->buffer != NULL;
}

void codegen_free(codegen_t* cg) {
    if (!cg->buffer) return;
#ifdef _WIN32
    VirtualFree(cg->buffer, 0, MEM_RELEASE);
#else
    munmap(cg->buffer, cg->capacity);
#endif
    cg->buffer = NULL;
}

cg_thunk_t codegen_emit(codegen_t* cg, const insn_t* ins, u8 n) {
    for (u8 k = 0; k < n; ++k) if (!codegen_supports(ins[k].op)) return NULL;
    if (cg->used + (u32)n * 128u + 128u > cg->capacity) return NULL;
    u8* start = cg->buffer + cg->used;
    emit_prologue(cg);
    emit_clear_fail(cg);                /* xor ebx,ebx — failure flag */
    for (u8 k = 0; k < n; ++k) emit_op(cg, &ins[k]);
    u32 last = ins[n - 1].pc + ins[n - 1].size;
    st_pc(cg, last);
    emit_epilogue_check(cg);            /* bl-check: halt-or-success */
    return (cg_thunk_t)start;
}
