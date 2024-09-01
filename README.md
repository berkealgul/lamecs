# lamecs

lamecs is a header only library for sparse-set based "pure" ecs implementation

## Code example
```cpp
struct pos 
{
    int x;
    int y;
    int z;
};

struct vel
{
    int dx;
    int dy;
    int dz;
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
    
    // callback style iterating
    registry.each<pos>([&registry](lamecs::entity_id id, pos& v)
    {
        //...
    });

    registry.each<vel>([&registry](vel& v)
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
```

It is far from perfect but it was nice learning for me to understand data oriented design and C++ templates


A shout-out to Michele Caini's [EnTT](https://github.com/skypjack/entt) and [ECS back and forth series](https://skypjack.github.io/2019-02-14-ecs-baf-part-1/), and Chris Christakis's [seecs](https://github.com/chrischristakis/seecs) which were awesome learning materials