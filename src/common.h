#ifndef _common_h_
#define _common_h_

// How many templates can be on a type/function
#define MAX_TEMPLATES 4

//
// Represents a non-owning string view
//
typedef struct
{
    // The string in question
    const char     *str;

    // How long the sting is
    int             len;
} strview_t;

#endif

