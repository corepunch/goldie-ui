// Test: Verify space element gets WINDOW_FLEXSPACE from component descriptor
//
// This test verifies that the space window class has WINDOW_FLEXSPACE as a
// default flag, which makes <space> elements expand automatically without
// requiring explicit flags="WINDOW_FLEXSPACE" in .orion files.

#include "../ui.h"
#include "test_framework.h"

void test_space_has_flexspace_by_default(void) {
    TEST("Space element gets WINDOW_FLEXSPACE from component descriptor");
    
    // Look up the space component descriptor
    const fe_component_desc_t *desc = fe_component_by_token("space");
    if (!desc) {
        printf("FAIL: fe_component_by_token(\"space\") returned NULL\n");
        return;
    }
    
    // Verify it has WINDOW_FLEXSPACE in default_flags
    if ((desc->default_flags & WINDOW_FLEXSPACE) == 0) {
        printf("FAIL: space component default_flags=0x%x, missing WINDOW_FLEXSPACE (0x%x)\n",
               desc->default_flags, WINDOW_FLEXSPACE);
        return;
    }
    
    PASS();
}

int main(void) {
    TEST_START("space element flexspace");
    
    test_space_has_flexspace_by_default();
    
    TEST_END();
}
