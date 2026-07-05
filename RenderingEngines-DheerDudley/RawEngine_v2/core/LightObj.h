//
// Created by dheer on 27-11-2025.
//

#ifndef RAWENGINE_LIGHTOBJ_H
#define RAWENGINE_LIGHTOBJ_H
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


class LightObj {
private:
    glm::vec3 position;

    glm::vec4 colour;
public:
    LightObj(glm::vec3 position, glm::vec4 colour)
        :position(position), colour(colour){}
    ~LightObj() = default;
    LightObj& operator = (const LightObj& anotherLight) {
        if (this!= &anotherLight) {
            this->position = anotherLight.getPos();
            this->colour = anotherLight.getCol();
        }
        return *this;
    }
    glm::vec3 getPos() const;
    glm::vec4 getCol() const;
};


#endif //RAWENGINE_LIGHTOBJ_H