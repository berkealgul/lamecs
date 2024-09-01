#ifndef LAMECS_H
#define LAMECS_H

#include <algorithm>
#include <bitset>
#include <iostream> 
#include <limits>
#include <memory>
#include <unordered_map>
#include <queue>
#include <typeinfo>
#include <vector>

#ifndef LAMECS_ASSERTS
	#define LAMECS_ASSERT(condition, msg) \
		if (condition) { \
			std::cerr << "[ECS error]: " << msg << std::endl; \
			::abort(); \
		}
#endif
#ifndef LAMECS_INFO
	#ifdef LAMECS_INFO_ENABLED
		#define LAMECS_INFO(msg) std::cout << "[ECS info]: " << msg << "\n";
	#else
		#define LAMECS_INFO(msg);
	#endif
#endif

namespace lamecs
{

constexpr size_t tombstone = std::numeric_limits<size_t>::max();

// registry parameters
constexpr size_t MAX_ENTITY_COUNT    = 100000;
constexpr size_t MAX_COMPONENT_COUNT = 64;

// sparse set parameters
constexpr size_t DENSE_SET_CHUNK_SIZE = 3200;
constexpr size_t SPARSE_PAGINATION_CHUNK_SIZE = 1600;

// custom types
using entity_id = u_int32_t;
using component_type   = const char*; 
using component_bitset = std::bitset<MAX_COMPONENT_COUNT>;

class sparse_set_interface 
{
public:
    virtual ~sparse_set_interface() = default;
    virtual void push() {}
    virtual void remove(entity_id id) { }
    virtual bool contains(entity_id id) { return 0; }
};

template <typename C>
class sparse_set : public sparse_set_interface
{
private:
    std::vector<size_t> dense_to_sparse_arr_;
    std::vector<C> dense_arr_;
    std::vector<std::vector<size_t>> sparse_arr_;

    size_t get_dense_index(entity_id id)
    {
        size_t page = id / SPARSE_PAGINATION_CHUNK_SIZE;
        size_t idx  = id % SPARSE_PAGINATION_CHUNK_SIZE;

        if(page < sparse_arr_.size())
            if(idx < sparse_arr_[page].size())
                return sparse_arr_[page][idx];
        return tombstone;
    }

    void set_dense_index(entity_id id, size_t item)
    {
        size_t page = id / SPARSE_PAGINATION_CHUNK_SIZE;
        size_t idx  = id % SPARSE_PAGINATION_CHUNK_SIZE;

        if(page >= sparse_arr_.size()) 
            sparse_arr_.resize(page + 1); 
        if(idx  >= sparse_arr_[page].size()) 
            sparse_arr_[page].resize(SPARSE_PAGINATION_CHUNK_SIZE, tombstone);

        sparse_arr_[page][idx] = item;
    }

    void push_to_dense(C item) // change to set index style
    {
        if(dense_arr_.capacity() <= dense_arr_.size()) 
        {
            dense_arr_.reserve(dense_arr_.capacity() + DENSE_SET_CHUNK_SIZE);    
            dense_to_sparse_arr_.reserve(dense_arr_.capacity() + DENSE_SET_CHUNK_SIZE);   
        }

        dense_arr_.push_back(item);
    }

public:

    sparse_set() = default;

    C* set(entity_id id, C item)
    {
        size_t index = get_dense_index(id);

        if(index == tombstone)
        {
            push(id, item);
            return &dense_arr_.back();
        }

        dense_arr_[index] = item;
        dense_to_sparse_arr_[index] = id;
        return &dense_arr_[index];
    }

    void push(entity_id id, C item)
    {
        set_dense_index(id, dense_arr_.size());
        push_to_dense(item);  
        dense_to_sparse_arr_.push_back(dense_arr_.size());
    }

    void remove(entity_id id)
    {
        size_t deleted_dense_index = get_dense_index(id);
        
        if(deleted_dense_index == tombstone || dense_arr_.empty()) { return; }
        
        set_dense_index(id, tombstone);
        set_dense_index(dense_to_sparse_arr_.back(), deleted_dense_index);

        std::swap(dense_arr_[deleted_dense_index], dense_arr_.back());
        std::swap(dense_to_sparse_arr_[deleted_dense_index], dense_to_sparse_arr_.back());

        dense_arr_.pop_back();
        dense_to_sparse_arr_.pop_back();
    }

    C& operator[](entity_id id)
    {
        size_t idx = get_dense_index(id);
        LAMECS_ASSERT(idx == tombstone, "Sparse set does not contain type " << typeid(C).name() << " for entity: " << id); 
        return dense_arr_[idx];        
    }

    void clear()
    {
        sparse_arr_.clear();
        dense_arr_.clear();
        dense_to_sparse_arr_.clear();
    }
    
    bool contains(entity_id id) { return get_dense_index(id) != tombstone; }

    bool empty() { return dense_arr_.size() == 0; }

    const std::vector<C>& data() { return dense_arr_; }
};


class registry
{
private:    
    std::queue<entity_id> available_entity_ids_;
    std::vector<std::unique_ptr<sparse_set_interface>> component_pools_;
    std::unordered_map<component_bitset, sparse_set<entity_id>> enitity_groups_;
    std::unordered_map<component_type, size_t> component_bit_positions_;
    sparse_set<component_bitset> component_bitsets_;

    template<typename C>
    size_t get_component_position()
    {
        component_type type = get_component_type<C>();
        if(!component_bit_positions_.contains(type)) { return tombstone; }
        return component_bit_positions_.at(type);
    }

    template<typename C>
    sparse_set<C>& get_component_pool(bool register_when_not_found = true)
    {
        if(get_component_position<C>() == tombstone)
        {
            LAMECS_ASSERT(!register_when_not_found, "registry dont have component type: " << typeid(C).name());
            register_component<C>();
        }

        sparse_set_interface* generic_ptr = component_pools_[get_component_position<C>()].get();
        return *dynamic_cast<sparse_set<C>*>(generic_ptr);
    }

    component_bitset& get_component_bitset(entity_id id, bool create_when_not_found = true)
    {
        if(!component_bitsets_.contains(id))
        {
            if(create_when_not_found) { component_bitsets_.push(id, component_bitset()); }
            else { LAMECS_ASSERT(true, "Entity: " << id << " does not exist"); } // currently we wont hit with this situation but get_component_pool() also does similar stuff so why not adding this ?
        }
        return component_bitsets_[id];
    }

    template<typename C>
    void set_bitset_bit(component_bitset& bitset, bool value) 
    { 
        size_t bitset_position = get_component_position<C>();
        LAMECS_ASSERT(bitset_position == tombstone, "registry dont have component type: " << typeid(C).name());
        bitset[bitset_position] = value;
    }

    template<typename... Components> 
    component_bitset get_component_bitset_mask()
    {
        component_bitset mask;
        (set_bitset_bit<Components>(mask, 1), ...);
        return mask;
    }   

    void remove_entity_from_group(component_bitset bitset, entity_id id)
    {
        enitity_groups_.emplace(std::piecewise_construct,
				std::forward_as_tuple(bitset),
				std::forward_as_tuple());
        sparse_set<entity_id>& group = enitity_groups_.at(bitset);
        group.remove(id);
        if(group.empty()) { enitity_groups_.erase(bitset); }
    }

    void add_entity_to_group(component_bitset bitset, entity_id id)
    {
        enitity_groups_.emplace(std::piecewise_construct,
				std::forward_as_tuple(bitset),
				std::forward_as_tuple());
        enitity_groups_[bitset].push(id, id);
    }

    template<typename C>
    inline component_type get_component_type() { return typeid(C).name(); }

public:
    registry() 
    {
        for(entity_id i = 0; i < MAX_COMPONENT_COUNT; i++) { available_entity_ids_.push(i); }
    }

    template <typename C>
    void emplace(entity_id id, C&& component={})
    {
        if(id == tombstone)
        {
            LAMECS_INFO("Entity is not valid");
            return;
        }

        sparse_set<C>& pool = get_component_pool<C>();
        pool.set(id, component);
        component_bitset& bitset = get_component_bitset(id);
        remove_entity_from_group(bitset, id);
        set_bitset_bit<C>(bitset, 1);
        add_entity_to_group(bitset, id);
    }

    template <typename C>
    void remove(entity_id id)
    {
        if(!contains_entity(id))
        {
            LAMECS_INFO("Entity: " << id << " does not exist");
            return;
        }

        sparse_set<C>& pool = get_component_pool<C>();
        pool.remove(id);
        component_bitset& bitset = get_component_bitset(id, false);
        remove_entity_from_group(bitset, id);
        set_bitset_bit<C>(bitset, 0);
        add_entity_to_group(bitset, id);
    }

    void remove_entity(entity_id &id)
    {
        if(!contains_entity(id))
        {
            LAMECS_INFO("Entity: " << id << " does not exist");
            return;
        }

        component_bitset deleted_bitset = get_component_bitset(id);
        component_bitsets_.remove(id);
        available_entity_ids_.push(id);
        remove_entity_from_group(deleted_bitset, id);
        for(size_t i = 0; i < MAX_COMPONENT_COUNT; i++)
            if(deleted_bitset[i] == 1) { component_pools_[i]->remove(id); }
    }

    template<typename C>
    C& get(entity_id id)
    {
        LAMECS_ASSERT(!contains_entity(id), "Entity: " << id << " does not exist during .get() call");
        sparse_set<C>& pool = get_component_pool<C>(false);
        return pool[id];
    }

    template<typename C>
    void register_component()
    {
        LAMECS_ASSERT(component_bit_positions_.size() > MAX_COMPONENT_COUNT, "Maximum component limit reached, cant register component");
        component_bit_positions_[get_component_type<C>()] = component_pools_.size();
        component_pools_.push_back(std::make_unique<sparse_set<C>>()); 
    }

    entity_id create_entity()
    {
        LAMECS_ASSERT(available_entity_ids_.empty(), "Maximum enitity limit reached");
        entity_id id = available_entity_ids_.front();
        available_entity_ids_.pop();
        return id;
    }

    bool contains_entity(entity_id id) { return component_bitsets_.contains(id); }

    template<typename ...Components>
    std::vector<std::tuple<entity_id, Components&...>> view()
    {
        std::vector<std::tuple<entity_id, Components&...>> result;
        const component_bitset& target_mask = get_component_bitset_mask<Components...>();

        for(auto&[mask, group] : enitity_groups_)
        {
            if((mask & target_mask) == target_mask)
            {
                for(auto id : group.data()) { result.emplace_back(id,get<Components>(id)...); }   
            }
        }

        return result;
    }

    template<typename ...Components, typename Func>
    void each(Func&& func)
    {
        const component_bitset& target_mask = get_component_bitset_mask<Components...>();

        for(auto& [mask, group] : enitity_groups_)
        {
            if((mask & target_mask) == target_mask)
            {
                for(entity_id id : group.data())
                {
                    // [](entity_id id, Component c1, Component c2, ...)
                    if constexpr(std::is_invocable_v<Func, entity_id, Components&...>)
                        func(id, get<Components>(id)...);
                    // [](Component c1, Component c2, ...)
                    else if constexpr(std::is_invocable_v<Func, Components&...>)
                        func(get<Components>(id)...);
                    else
                        LAMECS_ASSERT(true, "Bad lambda provided for .each(), parameter pack dosent match to lambda args");
                }
            }
        }
    }
};


}; // namespace lamecs

#endif // LAMECS_H