//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"
#include <map>

// The ant's initial responsibility.
// enum class: named integer constants for the ant's role in the colony.
enum class AntRole {
    Scout,
    Collector,
    Combined
};

// Information remembered separately for each ant.
// struct: user-defined data type that groups related variables together.
// Each ant has its own AntMemory instance.
// The structure (AntMemory) stores who the ant is, where it is going, its history of movement, and what it has found.
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

    std::vector<Coord> notUsefulPheromone;

    int waitingSteps = 0;
    int movesWithoutNewFood = 0;

    bool pendingScoutObservation = false;
};

/*WE can potentially remove foundfood in target food and target pheromones*/

// Read an ant's remaining energy.
int checkHP(const Ant& ant) {
    return ant.energy;
    printf("Ant Energy: %d\n", ant.energy);
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

void assignScoutDirections(
    const std::vector<Ant>& ants,
    std::vector<AntMemory>& antMemories,
    int mapRows,
    int mapColumns
) {
    // Each coordinate represents a change in row and column:
    // up, right, down, left.
    const std::vector<Coord> directions = {
        Coord(-1, 0),
        Coord(0, 1),
        Coord(1, 0),
        Coord(0, -1)
    };

    int nextDirection = 0;

    for (std::size_t i = 0; i < ants.size(); ++i) {
        if (antMemories[i].antRole == AntRole::Collector) {
            continue;
        }

        // Try each direction, starting after the previous
        // scout's assigned direction.
        for (int attempt = 0; attempt < 4; ++attempt) {
            int directionIndex = (nextDirection + attempt) % 4;

            Coord direction = directions[directionIndex];

            int nextRow =
                ants[i].position.first + direction.first;

            int nextColumn =
                ants[i].position.second + direction.second;

            bool withinMap =
                nextRow >= 0 && nextRow < mapRows &&
                nextColumn >= 0 && nextColumn < mapColumns;

            if (withinMap) {
                antMemories[i].explorationDirection = direction;

                nextDirection = (directionIndex + 1) % 4;
                break;
            }
        }
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

    assignScoutDirections(
        ants,
        antMemories,
        mapRows,
        mapColumns
    );
    return antMemories;
}

bool chooseExploreTarget(
    const Ant& ant,
    const AntMemory& memory,
    const std::vector<Coord>& visibleFood,
    Coord& nextPosition
) {
    int currentHP = checkHP(ant);

    // Two energy guarantees an adjacent move is affordable
    // under the generated terrain's movement-cost limit.
    if (currentHP < 2 || memory.coveredCells.empty()) {
        return false;
    }

    int mapRows =
        static_cast<int>(memory.coveredCells.size());

    int mapColumns =
        static_cast<int>(memory.coveredCells[0].size());

    if (mapColumns == 0) {
        return false;
    }

    const std::vector<Coord> directions = {
        Coord(-1, 0),
        Coord(0, 1),
        Coord(1, 0),
        Coord(0, -1)
    };

    bool targetFound = false;

    int bestNewCoverage = -1;
    int fewestVisits = std::numeric_limits<int>::max();
    bool bestMatchesDirection = false;

    for (const Coord& direction : directions) {
        Coord candidatePosition(
            ant.position.first + direction.first,
            ant.position.second + direction.second
        );

        int candidateRow = candidatePosition.first;
        int candidateColumn = candidatePosition.second;

        // Reject invalid destinations before calling move().
        if (candidateRow < 0 || candidateRow >= mapRows ||
            candidateColumn < 0 || candidateColumn >= mapColumns) {
            continue;
        }

        bool containsVisibleFood =
            std::find(
                visibleFood.begin(),
                visibleFood.end(),
                candidatePosition
            ) != visibleFood.end();

        // Avoid automatically picking up food that
        // this ant cannot afford to deliver.
        if (containsVisibleFood) {
            int deliveryCost = estimateDeliveryCost(
                ant.position,
                candidatePosition,
                ant.homeCoord
            );

            if (!canCollectAndReturn(currentHP, deliveryCost)) {
                continue;
            }
        }

        // Count how many previously unobserved cells
        // would fall within the next food scan.
        // This does not inspect those cells' contents.
        int newCoverage = 0;

        for (int row = candidateRow - ant.foodRadius;
             row <= candidateRow + ant.foodRadius;
             ++row) {

            for (int column = candidateColumn - ant.foodRadius;
                 column <= candidateColumn + ant.foodRadius;
                 ++column) {

                bool withinMap =
                    row >= 0 && row < mapRows &&
                    column >= 0 && column < mapColumns;

                if (withinMap &&
                    !memory.coveredCells[row][column]) {
                    ++newCoverage;
                }
            }
        }

        int visits =
            memory.stepCounts[candidateRow][candidateColumn];

        bool matchesDirection =
            direction == memory.explorationDirection;

        // Prefer:
        // 1. More new observation coverage.
        // 2. Fewer previous visits.
        // 3. Continuing the assigned exploration direction.
        bool betterTarget =
            !targetFound ||
            newCoverage > bestNewCoverage ||
            (newCoverage == bestNewCoverage &&
             visits < fewestVisits) ||
            (newCoverage == bestNewCoverage &&
             visits == fewestVisits &&
             matchesDirection &&
             !bestMatchesDirection);

        if (betterTarget) {
            nextPosition = candidatePosition;
            bestNewCoverage = newCoverage;
            fewestVisits = visits;
            bestMatchesDirection = matchesDirection;
            targetFound = true;
        }
    }

    return targetFound;
}

void updateAntMemory(
    const Ant& ant,
    AntMemory& memory,
    const std::vector<Coord>& visibleFood
) {
    bool discoveredNewFood = false;

    for (const Coord& food : visibleFood) {
        bool alreadyKnown =
            std::find(
                memory.foundFood.begin(),
                memory.foundFood.end(),
                food
            ) != memory.foundFood.end();

        if (!alreadyKnown) {
            discoveredNewFood = true;
        }
    }

    // Remove outdated food observations only where
    // the ant can currently see that food is absent.
    memory.foundFood.erase(
        std::remove_if(
            memory.foundFood.begin(),
            memory.foundFood.end(),
            [&](const Coord& food) {
                bool withinView =
                    std::abs(food.first - ant.position.first)
                        <= ant.foodRadius &&
                    std::abs(food.second - ant.position.second)
                        <= ant.foodRadius;

                bool stillVisible =
                    std::find(
                        visibleFood.begin(),
                        visibleFood.end(),
                        food
                    ) != visibleFood.end();

                return withinView && !stillVisible;
            }
        ),
        memory.foundFood.end()
    );

    // Add newly observed food to this ant's memory.
    for (const Coord& food : visibleFood) {
        if (std::find(
                memory.foundFood.begin(),
                memory.foundFood.end(),
                food
            ) == memory.foundFood.end()) {
            memory.foundFood.push_back(food);
        }
    }

    int mapRows =
        static_cast<int>(memory.coveredCells.size());

    int mapColumns = mapRows == 0
        ? 0
        : static_cast<int>(memory.coveredCells[0].size());

    // Remember the area covered by the current scan.
    for (int row = ant.position.first - ant.foodRadius;
         row <= ant.position.first + ant.foodRadius;
         ++row) {

        for (int column = ant.position.second - ant.foodRadius;
             column <= ant.position.second + ant.foodRadius;
             ++column) {

            if (row >= 0 && row < mapRows &&
                column >= 0 && column < mapColumns) {
                memory.coveredCells[row][column] = true;
            }
        }
    }

    // Evaluate an exploration move using the scan
    // taken after that move.
    if (memory.pendingScoutObservation) {
        if (discoveredNewFood) {
            memory.movesWithoutNewFood = 0;
        } else {
            ++memory.movesWithoutNewFood;
        }

        memory.pendingScoutObservation = false;
    } else if (discoveredNewFood) {
        memory.movesWithoutNewFood = 0;
    }
}

void recordMovement(
    const Ant& ant,
    AntMemory& memory,
    Coord previousPosition,
    bool wasExploration
) {
    // Standing still is not another visit.
    if (ant.position == previousPosition) {
        return;
    }

    int row = ant.position.first;
    int column = ant.position.second;

    int mapRows =
        static_cast<int>(memory.stepCounts.size());

    int mapColumns = mapRows == 0
        ? 0
        : static_cast<int>(memory.stepCounts[0].size());

    if (row >= 0 && row < mapRows &&
        column >= 0 && column < mapColumns) {
        ++memory.stepCounts[row][column];
    }

    if (wasExploration) {
        memory.pendingScoutObservation = true;
    }
}

void changeExplorationDirection(
    const Ant& ant,
    AntMemory& memory
) {
    const std::vector<Coord> directions = {
        Coord(-1, 0),
        Coord(0, 1),
        Coord(1, 0),
        Coord(0, -1)
    };

    int currentDirection = -1;

    for (int i = 0; i < 4; ++i) {
        if (directions[i] == memory.explorationDirection) {
            currentDirection = i;
            break;
        }
    }

    int mapRows =
        static_cast<int>(memory.coveredCells.size());

    int mapColumns = mapRows == 0
        ? 0
        : static_cast<int>(memory.coveredCells[0].size());

    // Prefer another valid direction.
    for (int offset = 1; offset <= 4; ++offset) {
        int directionIndex = (currentDirection + offset) % 4;

        Coord direction = directions[directionIndex];

        int nextRow = ant.position.first + direction.first;
        int nextColumn = ant.position.second + direction.second;

        if (nextRow >= 0 && nextRow < mapRows &&
            nextColumn >= 0 && nextColumn < mapColumns) {
            memory.explorationDirection = direction;
            break;
        }
    }

    memory.movesWithoutNewFood = 0;
}
struct ColonyMemory {
    bool initialized = false;

    Coord home = Coord(-1, -1);
    int mapRows = 0;
    int mapColumns = 0;

    std::vector<AntMemory> antMemories;

    // Expected ant state after the framework's update.
    // These copies are only for keeping memory aligned.
    std::vector<Ant> expectedAnts;
    int expectedScore = 0;
};

bool matchesExpectedWorld(
    const AntWorld& world,
    const ColonyMemory& memory
) {
    int mapRows = static_cast<int>(world.foodMap.size());

    int mapColumns = mapRows == 0
        ? 0
        : static_cast<int>(world.foodMap[0].size());

    if (!memory.initialized ||
        memory.home != world.homeCoordinates ||
        memory.mapRows != mapRows ||
        memory.mapColumns != mapColumns ||
        memory.expectedScore != world.score ||
        memory.expectedAnts.size() != world.ants.size()) {
        return false;
    }

    for (std::size_t i = 0; i < world.ants.size(); ++i) {
        const Ant& actual = world.ants[i];
        const Ant& expected = memory.expectedAnts[i];

        if (actual.energy != expected.energy ||
            actual.position != expected.position ||
            actual.homeCoord != expected.homeCoord ||
            actual.carryingFood != expected.carryingFood ||
            actual.foodRadius != expected.foodRadius) {
            return false;
        }
    }

    return true;
}

void prepareMemoryForNextStep(
    const AntWorld& world,
    ColonyMemory& memory
) {
    std::vector<AntMemory> survivingMemories;
    std::vector<Ant> survivingAnts;

    int expectedScore = world.score;

    for (std::size_t i = 0; i < world.ants.size(); ++i) {
        // This is a COPY, not the live ant.
        Ant expectedAnt = world.ants[i];

        // Match the order used by updateWorld():
        // deposit first, then remove exhausted ants.
        if (expectedAnt.position == world.homeCoordinates &&
            expectedAnt.carryingFood) {
            ++expectedScore;
            expectedAnt.carryingFood = false;
        }

        if (expectedAnt.energy != 0) {
            survivingAnts.push_back(expectedAnt);
            survivingMemories.push_back(memory.antMemories[i]);
        }
    }

    memory.expectedAnts = std::move(survivingAnts);
    memory.antMemories = std::move(survivingMemories);
    memory.expectedScore = expectedScore;
}


// Run the collection-and-return baseline for each ant.
void AntWorld::forage() {
    // Separate solution memory for each world instance.
    static std::map<const AntWorld*, ColonyMemory> gameMemories;

    if (this->ants.empty()) {
        gameMemories.erase(this);
        return;
    }

    ColonyMemory& colonyMemory = gameMemories[this];

    // Initialize once, or reset if the observed world
    // does not match the previous framework update.
    if (!matchesExpectedWorld(*this, colonyMemory)) {
        colonyMemory = ColonyMemory{};

        colonyMemory.home = this->homeCoordinates;

        colonyMemory.mapRows =
            static_cast<int>(this->foodMap.size());

        colonyMemory.mapColumns = this->foodMap.empty()
            ? 0
            : static_cast<int>(this->foodMap[0].size());

        colonyMemory.antMemories =
            detectSpawnArea(this->ants, this->foodMap);

        colonyMemory.initialized = true;
    }

    for (std::size_t i = 0; i < this->ants.size(); ++i) {
        Ant& ant = this->ants[i];
        AntMemory& memory = colonyMemory.antMemories[i];

        int currentHP = checkHP(ant);

        if (currentHP == 0) {
            continue;
        }

        // Carrying food takes priority over every other task.
        if (ant.carryingFood) {
            if (ant.position != ant.homeCoord) {
                Coord previousPosition = ant.position;

                ant.returnHome(
                    this->terrainMap,
                    this->foodMap
                );

                recordMovement(
                    ant,
                    memory,
                    previousPosition,
                    false
                );
            }

            memory.targetFood = Coord(-1, -1);
            memory.waitingSteps = 0;
            continue;
        }

        std::vector<Coord> visibleFood =
            ant.foodScan(this->foodMap);

        updateAntMemory(ant, memory, visibleFood);

        Coord selectedFood = ant.position;

        // Preserve our existing collection baseline.
        if (chooseFoodTarget(ant, visibleFood, selectedFood)) {
            memory.targetFood = selectedFood;
            memory.waitingSteps = 0;

            Coord previousPosition = ant.position;

            ant.move(
                this->terrainMap,
                selectedFood,
                this->foodMap
            );

            recordMovement(
                ant,
                memory,
                previousPosition,
                false
            );

            continue;
        }

        memory.targetFood = Coord(-1, -1);

        // Collectors wait up to eight consecutive
        // unproductive turns before exploring.
        if (memory.antRole == AntRole::Collector &&
            memory.waitingSteps < 8) {

            ++memory.waitingSteps;

            if (memory.waitingSteps < 8) {
                continue;
            }
        }

        if (memory.movesWithoutNewFood >= 5) {
            changeExplorationDirection(ant, memory);
        }

        Coord nextPosition = ant.position;

        if (chooseExploreTarget(
                ant,
                memory,
                visibleFood,
                nextPosition
            )) {

            Coord previousPosition = ant.position;

            ant.move(
                this->terrainMap,
                nextPosition,
                this->foodMap
            );

            recordMovement(
                ant,
                memory,
                previousPosition,
                true
            );
        }

        // Pheromone decisions will be inserted before
        // the waiting/exploration branch in the next stage.
    }

    prepareMemoryForNextStep(*this, colonyMemory);
}
