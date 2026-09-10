#ifndef _NLFUNCTION_H_
#define _NLFUNCTION_H_

#include "NL/nlFunctionCommon.h"

/* Function<P1> is the one-argument shorthand for Function<void(P1)>. */
#ifndef _NLFUNCTION_PRIMARY_DEFINED_
#define _NLFUNCTION_PRIMARY_DEFINED_
#define NLF_GENERATE_PRIMARY_WRAPPER
#include "NL/detail/nlFunctionPreProcTemplate.h"
#undef NLF_GENERATE_PRIMARY_WRAPPER
#endif

/* Function0<ReturnType> and Function<ReturnType()>. */
#ifndef _NLFUNCTION_ARITY0_DEFINED_
#define _NLFUNCTION_ARITY0_DEFINED_
#define NLF_CLASS Function0
#define NLF_TEMPLATE_PARAMETERS typename ReturnType
#define NLF_TEMPLATE_ARGUMENTS ReturnType
#define NLF_PARAMETER_TYPES
#define NLF_PARAMETER_DECLARATIONS
#define NLF_ARGUMENT_NAMES
#define NLF_COMMA_PARAMETER_DECLARATIONS
#define NLF_COMMA_ARGUMENT_NAMES
#define NLF_ARITY 0
#include "NL/detail/nlFunctionPreProcTemplate.h"
#undef NLF_ARITY
#undef NLF_COMMA_ARGUMENT_NAMES
#undef NLF_COMMA_PARAMETER_DECLARATIONS
#undef NLF_ARGUMENT_NAMES
#undef NLF_PARAMETER_DECLARATIONS
#undef NLF_PARAMETER_TYPES
#undef NLF_TEMPLATE_ARGUMENTS
#undef NLF_TEMPLATE_PARAMETERS
#undef NLF_CLASS
#endif

/* Function1<ReturnType, P1> and Function<ReturnType(P1)>. */
#ifndef _NLFUNCTION_ARITY1_DEFINED_
#define _NLFUNCTION_ARITY1_DEFINED_
#define NLF_CLASS Function1
#define NLF_TEMPLATE_PARAMETERS typename ReturnType, typename P1
#define NLF_TEMPLATE_ARGUMENTS ReturnType, P1
#define NLF_PARAMETER_TYPES P1
#define NLF_PARAMETER_DECLARATIONS P1 p0
#define NLF_ARGUMENT_NAMES p0
#define NLF_COMMA_PARAMETER_DECLARATIONS , P1 p0
#define NLF_COMMA_ARGUMENT_NAMES , p0
#define NLF_ARITY 1
#include "NL/detail/nlFunctionPreProcTemplate.h"
#undef NLF_ARITY
#undef NLF_COMMA_ARGUMENT_NAMES
#undef NLF_COMMA_PARAMETER_DECLARATIONS
#undef NLF_ARGUMENT_NAMES
#undef NLF_PARAMETER_DECLARATIONS
#undef NLF_PARAMETER_TYPES
#undef NLF_TEMPLATE_ARGUMENTS
#undef NLF_TEMPLATE_PARAMETERS
#undef NLF_CLASS
#endif

/* Function3<ReturnType, P1, P2, P3> and the signature wrapper. */
#ifndef _NLFUNCTION_ARITY3_DEFINED_
#define _NLFUNCTION_ARITY3_DEFINED_
#define NLF_CLASS Function3
#define NLF_TEMPLATE_PARAMETERS typename ReturnType, typename P1, typename P2, typename P3
#define NLF_TEMPLATE_ARGUMENTS ReturnType, P1, P2, P3
#define NLF_PARAMETER_TYPES P1, P2, P3
#define NLF_PARAMETER_DECLARATIONS P1 p1, P2 p2, P3 p3
#define NLF_ARGUMENT_NAMES p1, p2, p3
#define NLF_COMMA_PARAMETER_DECLARATIONS , P1 p1, P2 p2, P3 p3
#define NLF_COMMA_ARGUMENT_NAMES , p1, p2, p3
#define NLF_ARITY 3
#include "NL/detail/nlFunctionPreProcTemplate.h"
#undef NLF_ARITY
#undef NLF_COMMA_ARGUMENT_NAMES
#undef NLF_COMMA_PARAMETER_DECLARATIONS
#undef NLF_ARGUMENT_NAMES
#undef NLF_PARAMETER_DECLARATIONS
#undef NLF_PARAMETER_TYPES
#undef NLF_TEMPLATE_ARGUMENTS
#undef NLF_TEMPLATE_PARAMETERS
#undef NLF_CLASS
#endif

#endif // _NLFUNCTION_H_
