#define CATCH_CONFIG_RUNNER

#include "catch_wrapper.h"

int main(int argc, char *argv[])
{
    Catch::Session session;

    int return_code = session.applyCommandLine(argc, argv);
    if (return_code != 0) {
        return return_code;
    }

    return session.run();
}
