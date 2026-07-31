#ifndef __BRIDGES_ABI_SOFTFP_H
#define __BRIDGES_ABI_SOFTFP_H

#include <stddef.h>
#include <vitaGL.h>

#include "calling.h"

/* Additional imports used by the 1.9/2.0 libraries. All Android ARM32 math
 * entry points use base PCS, including integer-returning classifiers. */
#define BRIDGE_MATH_UNARY(name) double VITAARC_SOFTFP bridge_softfp_##name(double value);
BRIDGE_MATH_UNARY(atan)
BRIDGE_MATH_UNARY(cosh)
BRIDGE_MATH_UNARY(exp)
BRIDGE_MATH_UNARY(sinh)
BRIDGE_MATH_UNARY(tanh)
BRIDGE_MATH_UNARY(acosh)
BRIDGE_MATH_UNARY(asinh)
BRIDGE_MATH_UNARY(atanh)
BRIDGE_MATH_UNARY(cbrt)
BRIDGE_MATH_UNARY(erf)
BRIDGE_MATH_UNARY(erfc)
BRIDGE_MATH_UNARY(expm1)
BRIDGE_MATH_UNARY(lgamma)
BRIDGE_MATH_UNARY(log1p)
BRIDGE_MATH_UNARY(logb)
BRIDGE_MATH_UNARY(nearbyint)
BRIDGE_MATH_UNARY(rint)
BRIDGE_MATH_UNARY(tgamma)
BRIDGE_MATH_UNARY(exp2)
#undef BRIDGE_MATH_UNARY
float VITAARC_SOFTFP bridge_softfp_logf(float value);
int VITAARC_SOFTFP bridge_softfp_isnanf(float value);
int VITAARC_SOFTFP bridge_softfp_fpclassifyd(double value);
int VITAARC_SOFTFP bridge_softfp_signbit(double value);
int VITAARC_SOFTFP bridge_softfp_isfinite(double value);
/* Android ARM32 long double is IEEE binary64, even on a wider host. */
double VITAARC_SOFTFP bridge_softfp_scalbnl(double value, int exponent);
double VITAARC_SOFTFP bridge_softfp_nextafter(double value, double toward);
float VITAARC_SOFTFP bridge_softfp_nextafterf(float value, float toward);
double VITAARC_SOFTFP bridge_softfp_hypot(double x, double y);
double VITAARC_SOFTFP bridge_softfp_remainder(double x, double y);
double VITAARC_SOFTFP bridge_softfp_remquo(double x, double y, int* quotient);
long VITAARC_SOFTFP bridge_softfp_lrint(double value);
long long VITAARC_SOFTFP bridge_softfp_llrint(double value);

double VITAARC_SOFTFP bridge_softfp_acos(double value);
float VITAARC_SOFTFP bridge_softfp_acosf(float value);
double VITAARC_SOFTFP bridge_softfp_asin(double value);
float VITAARC_SOFTFP bridge_softfp_asinf(float value);
double VITAARC_SOFTFP bridge_softfp_atan2(double y, double x);
float VITAARC_SOFTFP bridge_softfp_atan2f(float y, float x);
double VITAARC_SOFTFP bridge_softfp_ceil(double value);
float VITAARC_SOFTFP bridge_softfp_ceilf(float value);
double VITAARC_SOFTFP bridge_softfp_cos(double value);
float VITAARC_SOFTFP bridge_softfp_cosf(float value);
float VITAARC_SOFTFP bridge_softfp_exp2f(float value);
double VITAARC_SOFTFP bridge_softfp_floor(double value);
float VITAARC_SOFTFP bridge_softfp_floorf(float value);
double VITAARC_SOFTFP bridge_softfp_fmod(double value, double divisor);
float VITAARC_SOFTFP bridge_softfp_fmodf(float value, float divisor);
double VITAARC_SOFTFP bridge_softfp_frexp(double value, int* exponent);
double VITAARC_SOFTFP bridge_softfp_ldexp(double value, int exponent);
double VITAARC_SOFTFP bridge_softfp_log(double value);
double VITAARC_SOFTFP bridge_softfp_log10(double value);
float VITAARC_SOFTFP bridge_softfp_log10f(float value);
long VITAARC_SOFTFP bridge_softfp_lroundf(float value);
double VITAARC_SOFTFP bridge_softfp_modf(double value, double* integer);
double VITAARC_SOFTFP bridge_softfp_pow(double value, double exponent);
float VITAARC_SOFTFP bridge_softfp_powf(float value, float exponent);
float VITAARC_SOFTFP bridge_softfp_roundf(float value);
double VITAARC_SOFTFP bridge_softfp_scalbn(double value, int exponent);
double VITAARC_SOFTFP bridge_softfp_sin(double value);
float VITAARC_SOFTFP bridge_softfp_sinf(float value);
void VITAARC_SOFTFP bridge_softfp_sincos(double value, double* sine, double* cosine);
void VITAARC_SOFTFP bridge_softfp_sincosf(float value, float* sine, float* cosine);
double VITAARC_SOFTFP bridge_softfp_sqrt(double value);
float VITAARC_SOFTFP bridge_softfp_sqrtf(float value);
double VITAARC_SOFTFP bridge_softfp_strtod(const char* text, char** end);
double VITAARC_SOFTFP bridge_softfp_tan(double value);
float VITAARC_SOFTFP bridge_softfp_tanf(float value);

void VITAARC_SOFTFP bridge_softfp_gl_clear_color(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void VITAARC_SOFTFP bridge_softfp_gl_clear_depth(GLfloat depth);
void VITAARC_SOFTFP bridge_softfp_gl_line_width(GLfloat width);
void VITAARC_SOFTFP bridge_softfp_gl_uniform1f(GLint location, GLfloat first);
void VITAARC_SOFTFP bridge_softfp_gl_uniform2f(GLint location, GLfloat first, GLfloat second);
void VITAARC_SOFTFP bridge_softfp_gl_uniform3f(GLint location, GLfloat first, GLfloat second, GLfloat third);
void VITAARC_SOFTFP bridge_softfp_gl_uniform4f(
    GLint location, GLfloat first, GLfloat second, GLfloat third, GLfloat fourth);

#endif /* __BRIDGES_ABI_SOFTFP_H */
