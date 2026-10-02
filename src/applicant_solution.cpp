#include "../include/antworld.h"
#include <map>


namespace {
    const std::vector<Coord> directions = {
        {-1, 0},  // Up
        {0, 1},   // Right
        {1, 0},   // Down
        {0, -1}   // Left
    };
}


// The ant's initial responsibility.
enum class AntRole {
    Scout,
    Collector,
    Combined
};


// Information remembered separately for each ant.
// The structure (AntMemory) stores who the ant is and what it has seen so far.
struct AntMemory {
    AntRole antRole = AntRole::Collector;
    Coord explorationDirection = Coord(0, 0);
    std::vector<std::vector<int>> stepCounts;
    std::vector<std::vector<bool>> coveredCells;
    std::vector<Coord> knownFood;
};


// Calculate distance using up/down/left/right movement.
int gridDistance(Coord start, Coord destination) {
    int rowDistance = std::abs(start.first - destination.first);
    int columnDistance = std::abs(start.second - destination.second);
    return rowDistance + columnDistance;
}


// Use the provided path functions to budget the complete delivery.
int estimateDeliveryCost(Coord position, Coord food, Coord home,
                         const MapTemplate& terrainMap) {
    return calculatePathCost(terrainMap, shortestPath(terrainMap, position, food)) +
           calculatePathCost(terrainMap, shortestPath(terrainMap, food, home));
}


// Choose the nearest observed food whose full delivery is affordable.
// Returns true if a target was found.
// selectedFood receives the chosen coordinates.
bool chooseFoodTarget(const Ant& ant, const std::vector<Coord>& visibleFood, const MapTemplate& terrainMap, Coord& selectedFood) {
    int bestTravelCost = std::numeric_limits<int>::max();
    bool foodTargetFound = false;
    for (const Coord& candidateFood : visibleFood) {
        // Even flat terrain cannot make this delivery affordable.
        if (gridDistance(ant.position, candidateFood) +
            gridDistance(candidateFood, ant.homeCoord) > ant.energy) continue;
        int deliveryCost = estimateDeliveryCost(ant.position, candidateFood, ant.homeCoord, terrainMap);
        int targetCost = calculatePathCost(terrainMap, shortestPath(terrainMap, ant.position, candidateFood));
        if (ant.energy >= deliveryCost && targetCost < bestTravelCost) {
            selectedFood = candidateFood;
            bestTravelCost = targetCost;
            foodTargetFound = true;
        }
    }
    return foodTargetFound;
}


void assignInitialRoles(std::vector<Ant>& ants, std::vector<AntMemory>& memories) {
    int count = static_cast<int>(ants.size());
    if (count == 0) {
        return;
    }
    for (AntMemory& memory : memories) {
        memory.antRole = AntRole::Collector;
    }
    if (count == 1) {
        memories[0].antRole = AntRole::Combined;
        return;
    }
    int minimumCollectors = count >= 5 ? 2 : 1;
    int maximumScouts = std::min(3, count - minimumCollectors);
    std::vector<int> candidates;
    for (int i = 0; i < count; ++i) {
        if (ants[i].energy < 55) {
            candidates.push_back(i);
        }
    }
    if (candidates.empty()) {
        int lowestEnergyAnt = 0;
        for (int i = 1; i < count; ++i) {
            if (ants[i].energy < ants[lowestEnergyAnt].energy) {
                lowestEnergyAnt = i;
            }
        }
        memories[lowestEnergyAnt].antRole = AntRole::Scout;
        return;
    }
    std::stable_sort(candidates.begin(), candidates.end(), [&ants](int first, int second) {
            return ants[first].energy > ants[second].energy;
        }
    );
    int scoutCount = std::min(maximumScouts, static_cast<int>(candidates.size()));
    for (int i = 0; i < scoutCount; ++i) {
        memories[candidates[i]].antRole = AntRole::Scout;
    }
}


void assignScoutDirections(std::vector<Ant>& ants, std::vector<AntMemory>& antMemories, int mapRows, int mapColumns) {
    int nextDirection = 0;
    for (std::size_t i = 0; i < ants.size(); ++i) {
        if (antMemories[i].antRole == AntRole::Collector) {
            continue;
        }
        // Try each direction, starting after the previous scout's assigned direction.
        for (int attempt = 0; attempt < 4; ++attempt) {
            int directionIndex = (nextDirection + attempt) % 4;
            Coord direction = directions[directionIndex];
            int nextRow = ants[i].position.first + direction.first;
            int nextColumn = ants[i].position.second + direction.second;
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


std::vector<AntMemory> detectSpawnArea(std::vector<Ant>& ants, MapTemplate& foodMap) {
    int rows = static_cast<int>(foodMap.size());
    int columns = rows == 0 ? 0 : static_cast<int>(foodMap[0].size());
    std::vector<AntMemory> memories(ants.size());
    for (std::size_t i = 0; i < ants.size(); ++i) {
        AntMemory& memory = memories[i];
        memory.stepCounts.assign(rows, std::vector<int>(columns, 0));
        memory.coveredCells.assign(rows, std::vector<bool>(columns, false));
        int row = ants[i].position.first;
        int column = ants[i].position.second;
        if (row >= 0 && row < rows && column >= 0 && column < columns) {
            memory.stepCounts[row][column] = 1;
        }
    }
    assignInitialRoles(ants, memories);
    assignScoutDirections(ants, memories, rows, columns);
    return memories;
}


bool chooseExploreTarget(
    const Ant& ant,
    const AntMemory& memory,
    const std::vector<Coord>& visibleFood,
    const MapTemplate& terrainMap,
    Coord& nextPosition
) {
    if (ant.energy == 0 || memory.coveredCells.empty()) {
        return false;
    }
    int mapRows = static_cast<int>(memory.coveredCells.size());
    int mapColumns = static_cast<int>(memory.coveredCells[0].size());
    if (mapColumns == 0) {
        return false;
    }
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
        int moveCost = 1 + std::abs(terrainMap[candidateRow][candidateColumn] -
            terrainMap[ant.position.first][ant.position.second]);
        if (moveCost > ant.energy) continue;
        // Avoid leaving one energy where every exit costs more than one.
        if (ant.energy - moveCost == 1) {
            bool flatExit = false;
            for (Coord d : directions) {
                int r = candidateRow + d.first, c = candidateColumn + d.second;
                if (r >= 0 && r < mapRows && c >= 0 && c < mapColumns &&
                    terrainMap[r][c] == terrainMap[candidateRow][candidateColumn]) flatExit = true;
            }
            if (!flatExit) continue;
        }
        bool containsVisibleFood = std::find(visibleFood.begin(), visibleFood.end(),
            candidatePosition) != visibleFood.end();
        // Avoid automatically picking up food that
        // this ant cannot afford to deliver.
        if (containsVisibleFood) {
            int deliveryCost = estimateDeliveryCost(
                ant.position, candidatePosition, ant.homeCoord, terrainMap);
            // At one energy, allow a legal final step; the framework handles any pickup/drop.
            if (ant.energy < deliveryCost && ant.energy != 1) {
                continue;
            }
        }
        // Count how many previously unobserved cells
        // would fall within the next food scan.
        // This does not inspect those cells' contents.
        int newCoverage = 0;
        for (int row = candidateRow - ant.foodRadius; row <= candidateRow + ant.foodRadius; ++row) {
            for (int column = candidateColumn - ant.foodRadius; column <= candidateColumn + ant.foodRadius; ++column) {
                bool withinMap =
                    row >= 0 && row < mapRows &&
                    column >= 0 && column < mapColumns;
                if (withinMap &&
                    !memory.coveredCells[row][column]) {
                    ++newCoverage;
                }
            }
        }
        int visits = memory.stepCounts[candidateRow][candidateColumn];
        bool matchesDirection = direction == memory.explorationDirection;
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



// Record the cells covered by the ant's current food scan.
void updateAntMemory(const Ant& ant, AntMemory& memory) {
    int mapRows = static_cast<int>(memory.coveredCells.size());
    int mapColumns = mapRows == 0
        ? 0
        : static_cast<int>(memory.coveredCells[0].size());
    for (int row = ant.position.first - ant.foodRadius; row <= ant.position.first + ant.foodRadius; ++row) {
        for (int column = ant.position.second - ant.foodRadius; column <= ant.position.second + ant.foodRadius; ++column) {
            if (row >= 0 && row < mapRows &&
                column >= 0 && column < mapColumns) {
                memory.coveredCells[row][column] = true;
            }
        }
    }
}


// Refresh only the cells visible to this ant; retain older distant observations.
void rememberFood(const Ant& ant, std::vector<Coord>& knownFood,
                  const std::vector<Coord>& visibleFood) {
    knownFood.erase(std::remove_if(knownFood.begin(), knownFood.end(),
        [&ant](Coord food) {
            return std::abs(food.first - ant.position.first) <= ant.foodRadius &&
                   std::abs(food.second - ant.position.second) <= ant.foodRadius;
        }), knownFood.end());
    knownFood.insert(knownFood.end(), visibleFood.begin(), visibleFood.end());
}


// Record a visit only when the ant actually changes position.
void recordMovement(const Ant& ant, AntMemory& memory, Coord previousPosition) {
    if (ant.position == previousPosition) {
        return;
    }
    int row = ant.position.first;
    int column = ant.position.second;
    int mapRows = static_cast<int>(memory.stepCounts.size());
    int mapColumns = mapRows == 0
        ? 0
        : static_cast<int>(memory.stepCounts[0].size());
    if (row >= 0 && row < mapRows &&
        column >= 0 && column < mapColumns) {
        ++memory.stepCounts[row][column];
    }
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


bool matchesExpectedWorld(const AntWorld& world, const ColonyMemory& memory) {
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


void prepareMemoryForNextStep(AntWorld& world, ColonyMemory& memory) {
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


// Observe, choose food or exploration, then act once per ant.
void AntWorld::forage() {
    static std::map<const AntWorld*, ColonyMemory> gameMemories;
    if (ants.empty()) {
        gameMemories.erase(this);
        return;
    }
    ColonyMemory& colonyMemory = gameMemories[this];
    if (!matchesExpectedWorld(*this, colonyMemory)) {
        colonyMemory = ColonyMemory{};
        colonyMemory.home = homeCoordinates;
        colonyMemory.mapRows = static_cast<int>(foodMap.size());
        colonyMemory.mapColumns =
            foodMap.empty() ? 0 : static_cast<int>(foodMap[0].size());
        colonyMemory.antMemories = detectSpawnArea(ants, foodMap);
        colonyMemory.initialized = true;
    }
    for (std::size_t i = 0; i < ants.size(); ++i) {
        Ant& ant = ants[i];
        AntMemory& memory = colonyMemory.antMemories[i];
        if (ant.energy == 0) {
            continue;
        }
        std::vector<Coord> visibleFood = ant.foodScan(foodMap);
        updateAntMemory(ant, memory);
        rememberFood(ant, memory.knownFood, visibleFood);
        Coord previousPosition = ant.position;
        // Deliver carried food before looking for another target.
        if (ant.carryingFood) {
            if (ant.position != ant.homeCoord) {
                ant.returnHome(terrainMap, foodMap);
            }
        } else {
            Coord target = ant.position;
            // Scouts search locally; collectors also use their OWN past food sightings.
            // No food coordinates are shared between ants.
            const std::vector<Coord>& targets = memory.antRole == AntRole::Scout
                ? visibleFood : memory.knownFood;
            bool targetFound = chooseFoodTarget(ant, visibleFood, terrainMap, target);
            if (!targetFound) targetFound = chooseFoodTarget(ant, targets, terrainMap, target);
            if (!targetFound) {
                targetFound = chooseExploreTarget(
                    ant, memory, visibleFood, terrainMap, target
                );
            }
            if (targetFound) {
                ant.move(terrainMap, target, foodMap);
                if (ant.position == target) {
                    auto& known = memory.knownFood;
                    known.erase(std::remove(known.begin(), known.end(), target), known.end());
                }
            }
        }
        recordMovement(ant, memory, previousPosition);
    }
    prepareMemoryForNextStep(*this, colonyMemory);
}