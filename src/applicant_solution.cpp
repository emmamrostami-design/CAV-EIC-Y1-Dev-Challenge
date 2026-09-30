//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"
// The ant's initial responsibility.
enum class AntRole {
    Scout,
    Collector,
    Combined
};

// Information remembered separately for each ant.
struct AntMemory {
    AntRole antRole = AntRole::Collector;

    // A direction is a change in row and column.
    Coord explorationDirection = Coord(0, 0);

    // Number of recorded visits to each position.
    std::vector<std::vector<int>> stepCounts;

    // Cells covered by this ant's food observations.
    std::vector<std::vector<bool>> coveredCells;

    std::vector<Coord> foundFood;

    Coord targetFood = Coord(-1, -1);
    Coord targetPheromone = Coord(-1, -1);

    std::vector<Coord> notUsefulPheromone;

    int waitingSteps = 0;
    int movesWithoutNewFood = 0;

    bool pendingScoutObservation = false;
};

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
void assignInitialRoles(
    const std::vector<Ant>& ants,
    std::vector<AntMemory>& antMemories
) {
    int antCount = static_cast<int>(ants.size());

    if (antCount == 0) {
        return;
    }

    // Begin with everyone assigned as a collector.
    for (AntMemory& memory : antMemories) {
        memory.antRole = AntRole::Collector;
    }

    if (antCount == 1) {
        antMemories[0].antRole = AntRole::Combined;
        return;
    }

    int minimumCollectors = (antCount >= 5) ? 2 : 1;

    int maximumScouts = std::min(
        3,
        antCount - minimumCollectors
    );

    // Store the indexes of ants eligible to become scouts.
    std::vector<int> scoutCandidates;

    for (int i = 0; i < antCount; ++i) {
        if (ants[i].energy < 55) {
            scoutCandidates.push_back(i);
        }
    }

    // Among qualifying ants, prefer higher starting energy.
    // Equal-energy ants retain their original order.
    std::stable_sort(
        scoutCandidates.begin(),
        scoutCandidates.end(),
        [&ants](int first, int second) {
            return ants[first].energy > ants[second].energy;
        }
    );

    int scoutCount = std::min(
        maximumScouts,
        static_cast<int>(scoutCandidates.size())
    );

    for (int i = 0; i < scoutCount; ++i) {
        int antIndex = scoutCandidates[i];
        antMemories[antIndex].antRole = AntRole::Scout;
    }

    // If none qualify, designate the lowest-energy ant
    // as the fallback scout.
    if (scoutCandidates.empty()) {
        int fallbackScout = 0;

        for (int i = 1; i < antCount; ++i) {
            if (ants[i].energy < ants[fallbackScout].energy) {
                fallbackScout = i;
            }
        }

        antMemories[fallbackScout].antRole = AntRole::Scout;
    }
}
std::vector<AntMemory> detectSpawnArea(
    std::vector<Ant>& ants,
    MapTemplate& foodMap
) {
    int mapRows = static_cast<int>(foodMap.size());

    int mapColumns = foodMap.empty()
        ? 0
        : static_cast<int>(foodMap[0].size());

    std::vector<AntMemory> antMemories(ants.size());

    for (std::size_t i = 0; i < ants.size(); ++i) {
        Ant& ant = ants[i];
        AntMemory& memory = antMemories[i];

        memory.stepCounts.assign(
            mapRows,
            std::vector<int>(mapColumns, 0)
        );

        memory.coveredCells.assign(
            mapRows,
            std::vector<bool>(mapColumns, false)
        );

        // Discover food through the provided scan.
        memory.foundFood = ant.foodScan(foodMap);

        int antRow = ant.position.first;
        int antColumn = ant.position.second;

        // Record the starting position once.
        if (antRow >= 0 && antRow < mapRows &&
            antColumn >= 0 && antColumn < mapColumns) {
            memory.stepCounts[antRow][antColumn] = 1;
        }

        // Record which cells were covered by that food scan.
        for (int row = antRow - ant.foodRadius;
             row <= antRow + ant.foodRadius;
             ++row) {

            for (int column = antColumn - ant.foodRadius;
                 column <= antColumn + ant.foodRadius;
                 ++column) {

                if (row >= 0 && row < mapRows &&
                    column >= 0 && column < mapColumns) {
                    memory.coveredCells[row][column] = true;
                }
            }
        }
    }

    assignInitialRoles(ants, antMemories);

    return antMemories;
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
