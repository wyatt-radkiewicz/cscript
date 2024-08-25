//
// cnm_ir.h
// CNM IR header file provides and intemediary between the actual code that
// transpiles IR to machine code and the code generator found in cnm.c. This
// file is not meant to be included by the end user and is only used so that
// definitions can be shared with the code generator and the architecture
// transpilers.
//
#ifndef _cnm_ir_h_
#define _cnm_ir_h_

// Type class contains POD data types, aggregate types, derived types, and
// other special intermediatary types. Only types that are used in IR are the
// POD types, VOID, BOOL, REF, ANYREF, and PTR
typedef enum typeclass_e {
    // POD Types
    TYPE_CHAR,      TYPE_UCHAR,
    TYPE_SHORT,     TYPE_USHORT,
    TYPE_INT,       TYPE_UINT,
    TYPE_LONG,      TYPE_ULONG,
    TYPE_LLONG,     TYPE_ULLONG,
    TYPE_FLOAT,     TYPE_DOUBLE,
    TYPE_BOOL,

    // Derived POD types
    TYPE_PTR,       TYPE_REF,
    TYPE_ANYREF,

    // Special types
    TYPE_VOID,      TYPE_ARR,
    TYPE_FN,        TYPE_FN_ARG,    

    // User made types
    TYPE_USER,

    // This special type is only used while parsing, and will immediately be
    // expanded once parsing is over. (compilation step will never see this type)
    TYPE_TYPEDEF,
} typeclass_t;

// Intructions in the IR refrence a what is like a linked list of UID ranges 
// and what type the UID will be. UID is what is used to refrence SSA variables
// in the IR, they are basically stand-ins for actual names. Only typeclass_t
// is used here because the type of the psudo-variable here should be the POD
// type only.
typedef struct uidref_s {
    unsigned start, len;
    typeclass_t type;
    struct uidref_s *next;
} uidref_t;

//// Opcodes in a cnm instruction
//typedef enum {
//    
//} opcode_t;
//
//// 1 cnm instruction. They are fixed length to keep things simple
//typedef struct {
//
//} instr_t;
//
//// A chunk of instructions. They are a linked list so they can be expanded
//typedef struct {
//
//} ichunk_t;
//
//// A block of instructions. These take parameters for variables that are inputted
//// into this block.
//typedef struct {
//
//} iblock_t;

#endif

