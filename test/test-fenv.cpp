
#include <sysmakeshift/fenv.hpp>

#include <cfenv>
#include <cmath>     // for sqrt()
#include <tuple>
#include <exception>

#if defined(_MSC_VER)
# include <Windows.h>
# include <eh.h>      // for _set_se_translator()
#elif defined(__GNUC__) && defined(__linux__) && !defined(__clang__)
# include <csignal>
#endif

#include <catch2/catch.hpp>


#if defined(_MSC_VER)
class StructuredException : public std::exception
{
private:
    unsigned seNumber_;

public:
    StructuredException(unsigned _seNumber) noexcept
        : seNumber_(_seNumber)
    {
    }
    unsigned seNumber(void) const noexcept
    {
        return seNumber_;
    }
};

class ScopedStructuredExceptionTranslator
{
private:
    _se_translator_function oldTranslator_;

public:
    ScopedStructuredExceptionTranslator(_se_translator_function newTranslator) noexcept
        : oldTranslator_(_set_se_translator(newTranslator))
    {
    }
    ~ScopedStructuredExceptionTranslator(void) noexcept
    {
        _set_se_translator(oldTranslator_);
    }

    ScopedStructuredExceptionTranslator(ScopedStructuredExceptionTranslator&&) = delete;
    ScopedStructuredExceptionTranslator& operator =(ScopedStructuredExceptionTranslator&&) = delete;
};

void translateStructuredExceptionToStdException(unsigned u, EXCEPTION_POINTERS*)
{
    throw StructuredException(u);
}
#elif defined(__GNUC__) && defined(__linux__) && !defined(__clang__)
class FPException : public std::exception
{
};

extern "C"
{

//[[gnu::no_caller_saved_registers]]
void fpSignalHandler([[maybe_unused]] int sig)
{
    asm(".cfi_signal_frame");
    throw FPException();
}

} // extern "C"
#endif


void divBy0(void)
{
    volatile double zero = 0.;
    [[maybe_unused]] volatile double x = 1./zero;
}

void inexact(void)
{
    volatile double ten = 10.;
    [[maybe_unused]] volatile double x = 1./ten;
}

void invalid(void)
{
    volatile double minus1 = -1.;
    [[maybe_unused]] volatile double x = std::sqrt(minus1);
}


TEST_CASE("set_trapping_fe_exceptions")
{
#ifdef _MSC_VER
    auto scopedExcTranslator = ScopedStructuredExceptionTranslator(translateStructuredExceptionToStdException);
#endif // _MSC_VER

    auto [excFunc, excCode] = GENERATE(
        std::tuple{ &divBy0, FE_DIVBYZERO },
        std::tuple{ &inexact, FE_INEXACT },
        std::tuple{ &invalid, FE_INVALID }
    );

    SECTION("Can detect floating-point exceptions")
    {
        CHECK(std::feclearexcept(FE_ALL_EXCEPT) == 0);
        REQUIRE_NOTHROW(excFunc());
        REQUIRE(std::fetestexcept(excCode));
        CHECK(std::feclearexcept(FE_ALL_EXCEPT) == 0);
    }

    SECTION("Does not raise exception if flag is not set")
    {
        CHECK(std::feclearexcept(FE_ALL_EXCEPT) == 0);
        sysmakeshift::set_trapping_fe_exceptions(FE_ALL_EXCEPT & ~excCode);
        CHECK(sysmakeshift::get_trapping_fe_exceptions() == (FE_ALL_EXCEPT & ~excCode));
        CHECK_NOTHROW(excFunc());
        sysmakeshift::set_trapping_fe_exceptions(0);
    }

    SECTION("Raises exception if flag is set")
    {
#if defined(_MSC_VER)
        sysmakeshift::set_trapping_fe_exceptions(excCode);
        CHECK(sysmakeshift::get_trapping_fe_exceptions() == excCode);
        CHECK_THROWS_AS(excFunc(), StructuredException);
        sysmakeshift::set_trapping_fe_exceptions(0);
#elif defined(__GNUC__) && defined(__linux__) && !defined(__clang__)
        if (excCode == FE_DIVBYZERO)
        {
                // GCC has trouble recovering from more than one SIGFPE, so we only test one.
            std::signal(SIGFPE, fpSignalHandler);
            sysmakeshift::set_trapping_fe_exceptions(excCode);
            CHECK(sysmakeshift::get_trapping_fe_exceptions() == excCode);
            CHECK_THROWS_AS(excFunc(), FPException);
            sysmakeshift::set_trapping_fe_exceptions(0);
        }
#endif
    }
    CHECK(std::feclearexcept(FE_ALL_EXCEPT) == 0);
}
