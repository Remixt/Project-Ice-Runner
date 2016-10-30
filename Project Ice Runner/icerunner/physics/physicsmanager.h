#ifndef PHYSICSMANAGER_H
#define PHYSICSMANAGER_H
#include <cstdio>
#include "core/constants.hpp"


namespace ice
{

namespace physics
{

class PhysicsManager
{
/*private:

	float left; // x coordinate of left side of player
	float right; //x coordinate of right side of player
	float top; // y coordinate of top side of player
	float bottom; // y coordinate of bottom side of player
	float velocity; // velocity of player. 
	*/
public:
    bool Init() { return true; }

	//PlayerAABB(float l, float r, float b, float t); //constructor for player AABB;
	//PlayerAABB(); //Default Constructor of player;

	//bool Intersect(const WallAABB& aabb, const Vec2& u, const Vec2& v); //AABB Collision Test

    void Simulate()
    {
    }

	
};

/*class WallAABB {
	friend class PhysicsManager;

private:

	float wLeft; // x coordinate of left side of wall;
	float wRight; //x coordinate of right side of wall;
	float wTop; //y coordinate of top side of wall;
	float wBottom; //y coordinate of bottom side of wall;
	int ID; //ID corresponding to each wall in the wall.

public:

	WallAABB(float l, float r, float b, float t); //constructor for Wall AABB;
	WallAABB(); //Default Constructor of Wall;
};*/

} // namespace physics

} // namespace ice

#endif // PHYSICSMANAGER_H