//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"

// Read an ant's remaining energy.
int checkHP(const Ant& ant) {
    return ant.energy;
}

// Calculate distance using up/down/left/right movement.
int gridDistance(Coord start, Coord destination) {
    int rowDistance = std::abs(start.first - destination.first);
    int columnDistance = std::abs(start.second - destination.second);

    return rowDistance + columnDistance;
}

// Budget for reaching food AND bringing it home.
// On generated maps, each neighbouring move costs at most 2 energy.
int estimateDeliveryCost(
    Coord position,
    Coord food,
    Coord home
) {
    int distanceToFood = gridDistance(position, food);
    int distanceToHome = gridDistance(food, home);

    return 2 * (distanceToFood + distanceToHome);
}

// Decide whether the ant can afford the complete delivery.
bool canCollectAndReturn(int currentHP, int deliveryCost) {
    return currentHP >= deliveryCost;
}

// Find the affordable visible food with the lowest delivery budget.
//
// Returns true if a target was found.
// selectedFood receives the chosen coordinates.
bool chooseFoodTarget(
    const Ant& ant,
    const std::vector<Coord>& visibleFood,
    Coord& selectedFood
) {
    int currentHP = checkHP(ant);
    int lowestDeliveryCost = std::numeric_limits<int>::max();
    bool foodTargetFound = false;

    for (const Coord& candidateFood : visibleFood) {
        int deliveryCost = estimateDeliveryCost(
            ant.position,
            candidateFood,
            ant.homeCoord
        );

        if (canCollectAndReturn(currentHP, deliveryCost) &&
            deliveryCost < lowestDeliveryCost) {
            selectedFood = candidateFood;
            lowestDeliveryCost = deliveryCost;
            foodTargetFound = true;
        }
    }

    return foodTargetFound;
}

// Run the collection-and-return baseline for each ant.
void AntWorld::forage() {
    for (Ant& ant : this->ants) {
        int currentHP = checkHP(ant);

        // The framework removes exhausted ants after forage().
        if (currentHP == 0) {
            continue;
        }

        // Deliver carried food before looking for another task.
        if (ant.carryingFood) {
            if (ant.position != ant.homeCoord) {
                ant.returnHome(this->terrainMap, this->foodMap);
            }

            // Stay home if already there.
            // updateWorld() will deposit the food.
            continue;
        }

        // Discover food only through this ant's provided scan.
        std::vector<Coord> visibleFood =
            ant.foodScan(this->foodMap);

        Coord selectedFood = ant.position;

        if (chooseFoodTarget(ant, visibleFood, selectedFood)) {
            ant.move(
                this->terrainMap,
                selectedFood,
                this->foodMap
            );

            // One movement call for this ant this turn.
            continue;
        }
    }
}
