#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "softfp.h"

#define BRIDGE_MATH_UNARY(name)                                                                                        \
    double VITAARC_SOFTFP bridge_softfp_##name(double value)                                                           \
    {                                                                                                                  \
        return name(value);                                                                                            \
    }
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

double VITAARC_SOFTFP bridge_softfp_atof(const char* text)
{
    return atof(text);
}

double VITAARC_SOFTFP bridge_softfp_fmin(double left, double right)
{
    return fmin(left, right);
}

double VITAARC_SOFTFP bridge_softfp_fmax(double left, double right)
{
    return fmax(left, right);
}

float VITAARC_SOFTFP bridge_softfp_logf(float value)
{
    return logf(value);
}

double VITAARC_SOFTFP bridge_softfp_scalbnl(double value, int exponent)
{
    return scalbn(value, exponent);
}

double VITAARC_SOFTFP bridge_softfp_nextafter(double value, double toward)
{
    return nextafter(value, toward);
}

float VITAARC_SOFTFP bridge_softfp_nextafterf(float value, float toward)
{
    return nextafterf(value, toward);
}

double VITAARC_SOFTFP bridge_softfp_hypot(double x, double y)
{
    return hypot(x, y);
}

double VITAARC_SOFTFP bridge_softfp_remainder(double x, double y)
{
    return remainder(x, y);
}

double VITAARC_SOFTFP bridge_softfp_remquo(double x, double y, int* quotient)
{
    return remquo(x, y, quotient);
}

long VITAARC_SOFTFP bridge_softfp_lrint(double value)
{
    return lrint(value);
}

long long VITAARC_SOFTFP bridge_softfp_llrint(double value)
{
    return llrint(value);
}

int VITAARC_SOFTFP bridge_softfp_isnanf(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return (bits & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
}

int VITAARC_SOFTFP bridge_softfp_fpclassifyd(double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    uint64_t fraction = bits & UINT64_C(0xfffffffffffff);
    unsigned exponent = (unsigned)((bits >> 52) & 2047);
    /* Bionic's public enum is a bit mask, unlike newlib's FP_* values. */
    if(!exponent)
        return fraction ? 8 : 16;

    if(exponent == 2047)
        return fraction ? 2 : 1;

    return 4;
}

int VITAARC_SOFTFP bridge_softfp_signbit(double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return (int)(bits >> 63);
}

int VITAARC_SOFTFP bridge_softfp_isfinite(double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return ((bits >> 52) & 2047) != 2047;
}

double VITAARC_SOFTFP bridge_softfp_acos(double value)
{
    return acos(value);
}

float VITAARC_SOFTFP bridge_softfp_acosf(float value)
{
    return acosf(value);
}

double VITAARC_SOFTFP bridge_softfp_asin(double value)
{
    return asin(value);
}

float VITAARC_SOFTFP bridge_softfp_asinf(float value)
{
    return asinf(value);
}

double VITAARC_SOFTFP bridge_softfp_atan2(double y, double x)
{
    return atan2(y, x);
}

float VITAARC_SOFTFP bridge_softfp_atan2f(float y, float x)
{
    return atan2f(y, x);
}

double VITAARC_SOFTFP bridge_softfp_ceil(double value)
{
    return ceil(value);
}

float VITAARC_SOFTFP bridge_softfp_ceilf(float value)
{
    return ceilf(value);
}

double VITAARC_SOFTFP bridge_softfp_cos(double value)
{
    return cos(value);
}

float VITAARC_SOFTFP bridge_softfp_cosf(float value)
{
    return cosf(value);
}

float VITAARC_SOFTFP bridge_softfp_exp2f(float value)
{
    return exp2f(value);
}

double VITAARC_SOFTFP bridge_softfp_floor(double value)
{
    return floor(value);
}

float VITAARC_SOFTFP bridge_softfp_floorf(float value)
{
    return floorf(value);
}

double VITAARC_SOFTFP bridge_softfp_fmod(double value, double divisor)
{
    return fmod(value, divisor);
}

float VITAARC_SOFTFP bridge_softfp_fmodf(float value, float divisor)
{
    return fmodf(value, divisor);
}

double VITAARC_SOFTFP bridge_softfp_frexp(double value, int* exponent)
{
    return frexp(value, exponent);
}

double VITAARC_SOFTFP bridge_softfp_ldexp(double value, int exponent)
{
    return ldexp(value, exponent);
}

double VITAARC_SOFTFP bridge_softfp_log(double value)
{
    return log(value);
}

double VITAARC_SOFTFP bridge_softfp_log10(double value)
{
    return log10(value);
}

float VITAARC_SOFTFP bridge_softfp_log10f(float value)
{
    return log10f(value);
}

long VITAARC_SOFTFP bridge_softfp_lroundf(float value)
{
    return lroundf(value);
}

double VITAARC_SOFTFP bridge_softfp_modf(double value, double* integer)
{
    return modf(value, integer);
}

double VITAARC_SOFTFP bridge_softfp_pow(double value, double exponent)
{
    return pow(value, exponent);
}

float VITAARC_SOFTFP bridge_softfp_powf(float value, float exponent)
{
    return powf(value, exponent);
}

float VITAARC_SOFTFP bridge_softfp_roundf(float value)
{
    return roundf(value);
}

double VITAARC_SOFTFP bridge_softfp_scalbn(double value, int exponent)
{
    return scalbn(value, exponent);
}

double VITAARC_SOFTFP bridge_softfp_sin(double value)
{
    return sin(value);
}

float VITAARC_SOFTFP bridge_softfp_sinf(float value)
{
    return sinf(value);
}

void VITAARC_SOFTFP bridge_softfp_sincos(double value, double* sine, double* cosine)
{
    if(sine) {
        *sine = sin(value);
    }

    if(cosine) {
        *cosine = cos(value);
    }
}

void VITAARC_SOFTFP bridge_softfp_sincosf(float value, float* sine, float* cosine)
{
    if(sine) {
        *sine = sinf(value);
    }

    if(cosine) {
        *cosine = cosf(value);
    }
}

double VITAARC_SOFTFP bridge_softfp_sqrt(double value)
{
    return sqrt(value);
}

float VITAARC_SOFTFP bridge_softfp_sqrtf(float value)
{
    return sqrtf(value);
}

double VITAARC_SOFTFP bridge_softfp_strtod(const char* text, char** end)
{
    return strtod(text, end);
}

double VITAARC_SOFTFP bridge_softfp_tan(double value)
{
    return tan(value);
}

float VITAARC_SOFTFP bridge_softfp_tanf(float value)
{
    return tanf(value);
}

void VITAARC_SOFTFP bridge_softfp_gl_clear_color(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    glClearColor(red, green, blue, alpha);
}

void VITAARC_SOFTFP bridge_softfp_gl_clear_depth(GLfloat depth)
{
    glClearDepthf(depth);
}

void VITAARC_SOFTFP bridge_softfp_gl_line_width(GLfloat width)
{
    glLineWidth(width);
}

void VITAARC_SOFTFP bridge_softfp_gl_uniform1f(GLint location, GLfloat first)
{
    glUniform1f(location, first);
}

void VITAARC_SOFTFP bridge_softfp_gl_uniform2f(GLint location, GLfloat first, GLfloat second)
{
    glUniform2f(location, first, second);
}

void VITAARC_SOFTFP bridge_softfp_gl_uniform3f(GLint location, GLfloat first, GLfloat second, GLfloat third)
{
    glUniform3f(location, first, second, third);
}

void VITAARC_SOFTFP bridge_softfp_gl_uniform4f(
    GLint location, GLfloat first, GLfloat second, GLfloat third, GLfloat fourth)
{
    glUniform4f(location, first, second, third, fourth);
}
