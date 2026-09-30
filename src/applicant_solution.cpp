//
// Created by dusan on 9/15/26.
//

#include "../include/antworld.h"


/** @brief this is where you as the applicant will make use of the above functions to develop your solution.
 * here are some existing examples of how calling these functions works to help get you started!
 */
//void AntWorld::forage() {
//First function: 

    // std::vector<Coord> visibleFood = this->ants[0].foodScan(this->foodMap);
    //
    // Coord desiredDestination = Coord(5, 5);
    // Coord finalPos = this->ants[0].move(this->terrainMap, desiredDestination, this->foodMap);
    // bool destCheck = (desiredDestination == finalPos);
    //
    // this->ants[0].dropPheromone(this->pheromoneMap);
    //
    // this->ants[0].erasePheromone(this->pheromoneMap);
    //
    // this->ants[0].returnHome(this->terrainMap, this->foodMap);

/** You may insert any custom functions below **/

void detectAntSurroundings(Ant&ant, MapTemplate & terrainMap, MapTemplate & foodMap, MapTemplate & phermoneMap){
    if (ant.energy<=50){ //checks if anthealth is below 50
        ant.returnHome(terrainMap,foodMap);//return to home
    }

std::vector<Coord> visibleFood = ant.foodScan(foodMap);
bool isFlat = !visibleFood.empty();

if (isFlat){
    ant.move(terrainMap, visibleFood[0], foodMap);
}
else { 
    std::vector <Coord> trails= ant.pheromoneScan(phermoneMap);
    if (!trails.empty()){
        ant.move(terrainMap, trails[0], foodMap);
    }




    }
}


void AntWorld::forage() {
    for(auto &ant:ants) { //loop through 
        detectAntSurroundings(ant, terrainMap,foodMap,pheromoneMap); //Spawn ant 
        //detectEnergy(ant); //Future energy function
        //detectFood(ant); //Future food function
    }
} 