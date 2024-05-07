/*
 * Common/misc. utilities that every part of the c script compiler uses
 */
#ifndef _common_h_
#define _common_h_

#include <assert.h>
#include <stddef.h>

#ifndef ANSI_STRICT
#include <stdint.h>
#include <stdbool.h>
#endif

/* How many templates can be on a type/function */
#define MAX_TEMPLATES 4

#ifndef NDEBUG

/* Used to turn off certain assertions when in release mode */
# define DBG_ASSERT(COND) (assert(COND))

/* Used to turn off certain assertions when in release mode */
# define DBG_ASSERT2(MSG) (assert(0 && MSG))

#endif

#ifdef ANSI_STRICT
typedef char		bool;
#define true		1
#define false		0
typedef long		uintptr_t;
#endif

typedef enum
{
	CS_OKAY				=  0,	/* An okay error code (you're good) */
	CS_ERROR			= -2,	/* Err/Warn was emmited, break from function */
	CS_COMPILER_CRITICAL= -3	/* Massive critical failure, return immediatly */
} cs_retcode_t;

/*
 * Represents a non-owning string view
 */
typedef struct
{
    /* The string in question */
    const char     *str;

    /* How long the sting is */
    int             len;
} strview_t;

/*
 * The c script compiler is spit into 2 phases of memory management when
 * compiling. Phase 1 is where the program is being compiled. Here is where
 * memory is allocated through the user's allocation function. In phase 2
 * when compilation is done and cleanup is performed is when all the free-ing
 * happens. It's done this way so that if one wanted they could pass an arena
 * allocator to the compiler and have it work that way.
 */
typedef void *(*cs_alloc_t)(void *user, size_t size);
typedef void (cs_free_t)(void *user, void *block);

#endif

