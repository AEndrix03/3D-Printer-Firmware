//
// Created by redeg on 17/04/2025.
//

#ifndef ARCHETYPE_APPLICATION_HPP
#define ARCHETYPE_APPLICATION_HPP

class Application {
public:
    void init();
    void loop();

    static void notifyCommandReceived();
};

#endif //ARCHETYPE_APPLICATION_HPP
