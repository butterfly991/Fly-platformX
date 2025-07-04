#include <cassert>
#include <iostream>
#include "core/security/SecurityManager.hpp"

using cloud::core::security::SecurityManager;

void smokeTestSecurityManager() {
    SecurityManager sm;
    assert(sm.initialize());
    sm.setPolicy("strict");
    assert(sm.getPolicy() == "strict");
    assert(sm.checkPolicy("strict"));
    sm.auditEvent("login", "user1");
    sm.shutdown();
    std::cout << "[OK] SecurityManager smoke test\n";
}

void stressTestSecurityManager() {
    SecurityManager sm;
    assert(sm.initialize());
    for (int i = 0; i < 10000; ++i) {
        sm.setPolicy("policy" + std::to_string(i % 10));
        sm.auditEvent("event", std::to_string(i));
    }
    sm.shutdown();
    std::cout << "[OK] SecurityManager stress test\n";
}

int main() {
    smokeTestSecurityManager();
    stressTestSecurityManager();
    std::cout << "All SecurityManager tests passed!\n";
    return 0;
} 