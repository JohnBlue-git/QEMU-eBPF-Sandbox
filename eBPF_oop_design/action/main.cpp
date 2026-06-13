#include <iostream>

#include "ActionLoop.hpp"

int main() {
    // Force the ActionLoop to initialize and start its thread immediately
    ActionLoop::getInstance();

    // ... Initialize other subsystems ...
    // ... Start your application logic ...

    std::cout << "ActionLoop (oop) started." << std::endl;
    return 0;
}
