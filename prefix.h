#pragma once

#ifndef PREFIX
#define PREFIX(name) name
#endif

#ifdef NO_STATIC_INTERNALS
#define _static
#else
#define _static static
#endif
