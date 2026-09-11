// Shared test runner for all suites

#include "greatest.h"

GREATEST_SUITE_EXTERN(test_cli_parsing);
GREATEST_SUITE_EXTERN(test_protocol_suite);
GREATEST_SUITE_EXTERN(test_window_suite);
GREATEST_SUITE_EXTERN(test_e2e_data);
GREATEST_SUITE_EXTERN(test_e2e_network);
GREATEST_SUITE_EXTERN(test_e2e_system);

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    if (greatest_get_verbosity() == 0) {
        greatest_set_verbosity(1);
    }
    RUN_SUITE(test_cli_parsing);
    RUN_SUITE(test_protocol_suite);
    RUN_SUITE(test_window_suite);
    RUN_SUITE(test_e2e_data);
    RUN_SUITE(test_e2e_network);
    RUN_SUITE(test_e2e_system);
    GREATEST_MAIN_END();
}
