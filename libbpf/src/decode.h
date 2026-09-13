#ifndef BPF_DECODE_H
#define BPF_DECODE_H

#include "types.h"
#include "bpf.h"

#define BPF_NREG 11
#define BPF_INS_BASIC 8u
#define BPF_INS_WIDE 16u

/* Instruction class (opcode low three bits), RFC 9669 section 3.3. */
#define BPF_CLS_LD 0x0u
#define BPF_CLS_LDX 0x1u
#define BPF_CLS_ST 0x2u
#define BPF_CLS_STX 0x3u
#define BPF_CLS_ALU 0x4u
#define BPF_CLS_JMP 0x5u
#define BPF_CLS_JMP32 0x6u
#define BPF_CLS_ALU64 0x7u

/* ALU/JMP source operand (s bit), section 4. */
#define BPF_SRC_K 0x0u
#define BPF_SRC_X 0x1u

/* LD/ST mode modifier (top three bits), section 5. */
#define BPF_MODE_IMM 0x0u
#define BPF_MODE_ABS 0x1u
#define BPF_MODE_IND 0x2u
#define BPF_MODE_MEM 0x3u
#define BPF_MODE_MEMSX 0x4u
#define BPF_MODE_ATOMIC 0x6u

/* LD/ST size modifier, section 5. */
#define BPF_SIZE_W 0x0u
#define BPF_SIZE_H 0x1u
#define BPF_SIZE_B 0x2u
#define BPF_SIZE_DW 0x3u

/* ALU arithmetic codes, section 4.1. */
#define BPF_ALU_ADD 0x0u
#define BPF_ALU_SUB 0x1u
#define BPF_ALU_MUL 0x2u
#define BPF_ALU_DIV 0x3u
#define BPF_ALU_OR 0x4u
#define BPF_ALU_AND 0x5u
#define BPF_ALU_LSH 0x6u
#define BPF_ALU_RSH 0x7u
#define BPF_ALU_NEG 0x8u
#define BPF_ALU_MOD 0x9u
#define BPF_ALU_XOR 0xau
#define BPF_ALU_MOV 0xbu
#define BPF_ALU_MOVSX 0xbu
#define BPF_ALU_ARSH 0xcu
#define BPF_ALU_END 0xdu

/* JMP codes, section 4.3. */
#define BPF_JMP_JA 0x0u
#define BPF_JMP_JEQ 0x1u
#define BPF_JMP_JGT 0x2u
#define BPF_JMP_JGE 0x3u
#define BPF_JMP_JSET 0x4u
#define BPF_JMP_JNE 0x5u
#define BPF_JMP_JSGT 0x6u
#define BPF_JMP_JSGE 0x7u
#define BPF_JMP_CALL 0x8u
#define BPF_JMP_EXIT 0x9u
#define BPF_JMP_JLT 0xau
#define BPF_JMP_JLE 0xbu
#define BPF_JMP_JSLT 0xcu
#define BPF_JMP_JSLE 0xdu

typedef enum bpf_err
{
    BPF_OK = 0,
    BPF_ETRUNC,      /* buffer ends inside a required wide instruction */
    BPF_ECOUNT,      /* output array too small for all instructions */
    BPF_EREG,        /* register number out of range */
    BPF_EDEPRECATED, /* deprecated packet access instruction */
    BPF_EIMM,        /* unsupported host-specific 64-bit immediate */
    BPF_EEND,        /* invalid byte-swap width or source */
    BPF_ENEG,        /* NEG with register source */
    BPF_EMOVSX,      /* MOVSX with immediate source or bad width */
    BPF_ECALL,       /* invalid CALL src_reg */
    BPF_EEXIT,       /* invalid EXIT form */
    BPF_ESIZE,       /* invalid size for mode (e.g. atomic 8/16, MEMSX DW) */
    BPF_EEMPTY,      /* no instructions in the buffer */
    BPF_ERES,        /* reserved continuation/encoding field is non-zero */
    BPF_EUNSUP,      /* unknown or unsupported operation code */
    BPF_ER10,        /* write to the frame pointer r10 */
    BPF_EBRANCH,     /* branch/call target out of range or mid-wide */
    BPF_EPROF,       /* required conformance group not allowed by profile */
    BPF_EADDR,       /* guest address not mapped or interval crosses regions */
    BPF_EPERM,       /* access denied by region permissions */
    BPF_EHANDLE,     /* not a live capability handle */
    BPF_EFULL,       /* capability handle space exhausted */
    BPF_ESTALE,      /* completion for a request that is not pending */
    BPF_ESIG         /* invalid or over-arity FFI signature */
} bpf_err;

/* A single decoded instruction. `class` is always set. For ALU/JMP classes
 * `code` and `source` describe the operation; for LD/ST classes `mode` and
 * `size` describe it. */
typedef struct bpf_insn
{
    bpf_byte opcode;
    bpf_byte class;
    bpf_byte code;
    bpf_byte source;
    bpf_byte mode;
    bpf_byte size;
    bpf_byte src_reg;
    bpf_byte dst_reg;
    int offset;
    int imm;
    bpf_byte is_wide;
    bpf_u32 next_imm;
} bpf_insn;

/* Decode a byte buffer of eBPF instructions. Reads whole instructions into
 * `out` (capacity `cap`) and stores the decoded count in `n`. Wide (IMM LD)
 * instructions consume 16 bytes and set is_wide. */
bpf_err bpf_decode(const bpf_byte *buf, bpf_u32 len, bpf_insn *out, bpf_u32 cap,
                   bpf_u32 *n);

#endif
