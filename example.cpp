#define LAMECS_INFO_ENABLED
#include "lamecs.hpp"

struct pos 
{
    int x, y, z;
};

struct vel
{
    int dx, dy, dz;
};

int main()
{
    lamecs::registry registry;

    auto e1 = registry.create_entity();
    auto e2 = registry.create_entity();
    auto e3 = registry.create_entity();

    // you dont have to do this since components are registered during .emplace()  
    registry.register_component<pos>();

    registry.emplace<pos>(e1, {0, 0, 0});
    registry.emplace<pos>(e2, {0, 0, 1});
    registry.emplace<vel>(e1, {1, 0, 0});
    registry.emplace<vel>(e2, {0, 1, 1});
    registry.emplace<vel>(e3, {0, 1, 3});
    
    // remove component from entity
    registry.remove<vel>(e2);
    
    registry.remove_entity(e3);

    // access and modify specific components of an entity
    auto [p, v] = registry.get_entity<pos, vel>(e1);

    // callback style iterating
    registry.each<vel>([&registry](lamecs::entity_id id, vel& v)
    {
        //...
    });

    registry.each<vel, pos>([&registry](vel& v, pos& p)
    {
        //...
    });

    // you can also create "views" to access entities with specific components
    for(auto& [id, pos, vel] : registry.view<pos, vel>())
    {
        //...
    }

    return 0;
}
