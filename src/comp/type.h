/*
 * How types are internally represented in c script
 *
 * Types are split up into levels to help with cache locality and so that
 * a type can easily be put on the stack if it needs to be temporarily
 */
#ifndef _type_h_
#define _type_h_

#include "common.h"

/* How many pointers/refs can be in a type (including pod) */
#define TYPE_MAX_NESTING 12

/* How many traits a template can have */
#define TYPE_MAX_TRAITS 8

/*
 * Plain Old Data Type IDS
 */
typedef enum
{
    /* 8 Bit boolean and character type */
    TYPE_BOOL, TYPE_CHAR,

    /* Unsigned integer types */
    TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64,
    TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64,
    
    /* Size types */
    TYPE_ISIZE, TYPE_USIZE,

    /* Floating point types */
    TYPE_F32, TYPE_F64,

    /* Pointers and refrences */
    /* SLICEPTR contain a length parameter after the pointer */
    TYPE_PTR, TYPE_SLICEPTR,
    TYPE_REF, TYPE_SLICEREF,

    /* Start of user type IDS */
    TYPE_USER
} typeid_t;

/*
 * A plain type that can be used anywhere
 */
typedef struct
{
    /* The types */
    int         ids[TYPE_MAX_NESTING];

    /* 0 specifies the corresponding id is */
    /* not an array, and non 0 specifies it */
    /* is a an array (with the assosiated length) */
    int         arrlen[TYPE_MAX_NESTING];

    /* How long the nesting is */
    int         nids;

    /* The name of the variable that was given this type */
    /* (leave 0 if not applicable) */
    strview_t   name;
} type_t;

/*
 * Represents a struct definition
 * This also represents structs with templates
 */
typedef struct
{
    /* Templates */
    strview_t  *templates;
    int         ntemplates;
    
    /* Members */
    type_t     *types;
    int         ntypes;

    /* Cached size and alignment */
    /* (leave 0 if this type is templated) */
    /* If this is an enum variant it may be */
    /* permitted to have no members, and thus */
    /* also 0 size and alignment. In that case, */
    /* make sure that templates and ntemplates */
    /* are still 0 */
    int         size;
    int         align;

    /* The name of the struct */
    strview_t   name;
} struct_t;

/*
 * Represents a C-Script tagged union (enum)
 */
typedef struct
{
    /* Name of the enum */
    strview_t   name;

    /* These cached size and alignment will always be active */
    int         size;
    int         align;

    /* Variants of the enum */
    /* Enums and enum variants can not have templates */
    struct_t   *variants;
    int         nvariants;
} enum_t;

#endif

