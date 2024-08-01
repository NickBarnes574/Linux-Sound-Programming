#include <stdio.h>
#include <stdlib.h>

#include "signal_handler.h"
#include "utilities.h"

int main(void)
{
    int exit_code = E_FAILURE;

    // Initialize signal handler
    exit_code = signal_action_setup();
    if (E_SUCCESS != exit_code)
    {
        print_error("main(): Unable to setup signal handler.");
        goto END;
    }

    exit_code = E_SUCCESS;
END:
    return exit_code;
}
