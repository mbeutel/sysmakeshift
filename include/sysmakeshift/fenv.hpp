
#ifndef INCLUDED_SYSMAKESHIFT_FENV_HPP_
#define INCLUDED_SYSMAKESHIFT_FENV_HPP_


#include <gsl-lite/gsl-lite.hpp> // for gsl_Expects(), gsl_NODISCARD


namespace sysmakeshift
{


namespace gsl = ::gsl_lite;


    //
    // Sets hardware exception traps for the floating-point exceptions specified by the given mask value.
    //ᅟ
    // The admissible mask values are defined as `FE_*` in standard header <cfenv>.
    // If an exception flag bit is on, the corresponding exception will be trapped; if the bit is clear, the exception will be masked.
    //
gsl_NODISCARD bool try_set_trapping_fe_exceptions(int excepts) noexcept;

    //
    // Disables hardware exception traps for the floating-point exceptions specified by the given mask value.
    //ᅟ
    // The admissible mask values are defined as `FE_*` in standard header <cfenv>.
    //
inline void set_trapping_fe_exceptions(int excepts)
{
    bool succeeded = try_set_trapping_fe_exceptions(excepts);
    gsl_Expects(succeeded);
}

    //
    // Returns the bitmask of all floating-point exceptions for which trapping is currently enabled.
    //ᅟ
    // The admissible mask values are defined as `FE_*` in standard header <cfenv>.
    //
int get_trapping_fe_exceptions(void) noexcept;


} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_FENV_HPP_
