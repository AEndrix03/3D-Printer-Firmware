#include "Vertex.h"

Vertex::Vertex(int x, int y, int z) {
    this->x = x;
    this->y = y;
    this->z = z;
}

int Vertex::getX() {
    return this->x;
}

int Vertex::getY() {
    return this->y;
}

int Vertex::getZ() {
    return this->z;
}

void Vertex::setX(int x) {
    this->x = x;
}

void Vertex::setY(int y) {
    this->y = y;
}

void Vertex::setZ(int z) {
    this->z = z;
}

Vertex Vertex::subtract(Vertex vertex) {
    Vertex subtractVertex((this->x - vertex.x), (this->y - vertex.y), (this->z - vertex.z));
    return subtractVertex;
}

Vertex Vertex::add(Vertex vertex) {
    Vertex addVertex((this->x + vertex.x), (this->y + vertex.y), (this->z + vertex.z));
    return addVertex;
}

Vertex Vertex::multiply(Vertex vertex) {
    Vertex addVertex((this->x * vertex.x), (this->y * vertex.y), (this->z * vertex.z));
    return addVertex;
}

Vertex Vertex::divide(Vertex vertex) {
    Vertex addVertex((this->x / vertex.x), (this->y / vertex.y), (this->z / vertex.z));
    return addVertex;
}