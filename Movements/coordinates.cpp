#include "../Configurations/geometry.h"

int* calculateMaxCoordinates() {
    int* maxCoords = new int[3];

    maxCoords[0] = (int)((X_OFFSET_MAX - X_OFFSET_MIN) / X_MM_PER_STEP);
    maxCoords[1] = (int)((Y_OFFSET_MAX - Y_OFFSET_MIN) / Y_MM_PER_STEP);
    maxCoords[2] = (int)((Z_OFFSET_MAX - Z_OFFSET_MIN) / Z_MM_PER_STEP);

    return maxCoords;
}

//TODO Sistema per salvare la coordinata attuale

int* stepsToCoord(int coord[], int newCoord[]) {
    int* steps = new int[3];

    for (int i = 0; i<3; i++)
        steps[i] = newCoord[i] - coord[i];

    return steps;
}