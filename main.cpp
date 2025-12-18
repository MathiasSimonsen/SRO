#include <ixwebsocket/IXNetSystem.h> // fix required windows befor app.hpp
#include "App.hpp"

// main entry
int main(int, char**) {
    // init winsock
    ix::initNetSystem();

    // start app
    App app;
    app.Run();
    return 0;
}
