// Offline bitcode library. Reuse OSL's scalar noise implementation and dual numbers.
#include <OSL/dual_vec.h>
#include <OSL/oslconfig.h>
#include <OSL/oslnoise.h>
#include <cmath>

using Dual = OSL::Dual2<float>;
extern "C" float osl_sin_ff(float x) { return std::sin(x); }
extern "C" float osl_cos_ff(float x) { return std::cos(x); }
extern "C" void osl_sin_dfdf(Dual* out, const Dual* x)
{
    const float derivative = std::cos(x->val());
    *out = Dual(std::sin(x->val()), derivative * x->dx(), derivative * x->dy());
}
extern "C" void osl_cos_dfdf(Dual* out, const Dual* x)
{
    const float derivative = -std::sin(x->val());
    *out = Dual(std::cos(x->val()), derivative * x->dx(), derivative * x->dy());
}
extern "C" float osl_filterwidth_fdf(const Dual* x) { return std::sqrt(x->dx() * x->dx() + x->dy() * x->dy()); }
extern "C" float osl_snoise_fv(const OSL::Vec3* x)
{
    float result;
    OSL::pvt::SNoiseScalar{}(result, *x);
    return result;
}
extern "C" void osl_snoise_dfdv(Dual* out, const OSL::Dual2<OSL::Vec3>* x) { OSL::pvt::SNoiseScalar{}(*out, *x); }
