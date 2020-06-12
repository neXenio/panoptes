#ifndef TEST_CATCH_WRAPPER_H
#define TEST_CATCH_WRAPPER_H

//! \note(rmeusel): This is to work around a compile issue with macOS 10.11
//!                 which we are currently using on our CI platform. The C++17
//!                 std::uncaught_exceptions() does not work before macOS 10.12
//!                 See the respective GitHub issue for details:
//!                       https://github.com/catchorg/Catch2/issues/1218
//!
//! \todo(rmeusel): remove this define as soon as we update macOS on the build
//!                 machine to macOS >= 10.12 or Catch2 upstreams the workaround
#ifdef __APPLE__
#include <AvailabilityMacros.h>
#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_11
#warning "Disabling std::uncaught_exceptions() because macOS is too old"
#define CATCH_CONFIG_NO_CPP17_UNCAUGHT_EXCEPTIONS
#endif
#endif

#define CATCH_CONFIG_ENABLE_CHRONO_STRINGMAKER

#include <catch.hpp>

#endif  // TEST_CATCH_WRAPPER_H
