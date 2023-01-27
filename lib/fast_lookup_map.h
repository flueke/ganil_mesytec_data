#ifndef FAST_LOOKUP_MAP_H
#define FAST_LOOKUP_MAP_H

#include <memory>
#include <vector>
#include <cassert>
#include <iostream>

/**
   @class fast_lookup_map

   for fast lookup of objects which are keyed with an integer index (>=0)

   first call add_id() with the id of each object
   then call add_object() for each object with its id
   */

// to do: only valid if Index is of positive integer type
// to do: Object must be copy constructible
template<typename Index, typename Object>
class fast_lookup_map
{
   std::vector<std::unique_ptr<Object>> objects;
   std::vector<Index>  id_list;
   Index maxindex=0;
   bool initialized{false};

   void initialize_object_storage()
   {
      // fill object vector with default-initialized unique_ptr's
      // (no Object is constructed, they all hold nullptr as address)
      for(int i=0; i<maxindex+1; ++i) objects.push_back(std::unique_ptr<Object>());
      initialized=true;
   }

public:
   fast_lookup_map() = default;
   fast_lookup_map(const fast_lookup_map& flm)
   {
      maxindex=flm.maxindex;
      if(!flm.id_list.empty()){
         initialize_object_storage();
         int idx=0; auto max_idx=flm.size();
         for(unsigned int idx=0; idx<max_idx; ++idx)
         {
            auto obj_id=flm.id_list[idx];
            id_list.push_back(obj_id);// essential to increase size of this vector!
            objects[obj_id].reset(new Object(flm.get_object(obj_id)));
         }
      }
   }
   fast_lookup_map& operator=(const fast_lookup_map& flm)
   {
      if(this != &flm)
      {
         maxindex=flm.maxindex;
         id_list.clear();
         objects.clear();
         if(!flm.id_list.empty()){
            initialize_object_storage();
            int idx=0; auto max_idx=flm.size();
            for(unsigned int idx=0; idx<max_idx; ++idx)
            {
               auto obj_id=flm.id_list[idx];
               id_list.push_back(obj_id);// essential to increase size of this vector!
               objects[obj_id].reset(new Object(flm.get_object(obj_id)));
            }
         }
      }
      return *this;
   }

   class iterator
   {
      typename std::vector<Index>::iterator index_iterator;
      fast_lookup_map* flm = nullptr;

   public:
      typedef std::forward_iterator_tag iterator_category;
      typedef Object value_type;
      typedef std::ptrdiff_t difference_type;
      typedef Object* pointer;
      typedef Object& reference;

      iterator() = default;
      iterator(typename std::vector<Index>::iterator it, fast_lookup_map* f) :
         index_iterator{it}, flm{f}
      {}
      iterator(typename std::vector<Index>::iterator it) : iterator(it,nullptr)
      {}
      bool operator!= (const iterator& it) const
      {
         return index_iterator!=it.index_iterator;
      }
      bool operator== (const iterator& it) const
      {
         return index_iterator==it.index_iterator;
      }
      const iterator& operator++ ()
      {
         // Prefix ++ operator
         ++index_iterator;
         return *this;
      }
      iterator operator++ (int)
      {
         // Postfix ++ operator
         iterator tmp(*this);
         operator++();
         return tmp;
      }
      iterator& operator= (const iterator& rhs)
      {
         // copy-assignment operator
         if (this != &rhs) { // check self-assignment based on address of object
            index_iterator=rhs.index_iterator;
            flm=rhs.flm;
         }
         return *this;
      }
      Object& operator* ()
      {
         return flm->get_object(*index_iterator);
      }
      const Object& operator* () const
      {
         return flm->get_object(*index_iterator);
      }
   };
   iterator begin()
   {
      return iterator(id_list.begin(), this);
   }
   iterator end()
   {
      return iterator(id_list.end());
   }

   void add_id(Index id)
   {
      id_list.push_back(id);
      if(id>maxindex){
         maxindex=id;
      }
   }
   void add_object(Index id, const Object& M)
   {
      if(!initialized) {
         initialize_object_storage();
      }
      // create and place copy-constructed Object in correct slot
      objects[id].reset(new Object(M));
   }
   auto size() const
   {
      return id_list.size();
   }
   Index max_index() const
   {
      return maxindex;
   }
   bool has_object(Index id) const { return (bool)objects[id]; }
   Object& get_object(Index id) { return *(objects[id].get()); }
   Object& operator[](Index id) { return get_object(id); }
   const Object& get_object(Index id) const { return *(objects[id].get()); }
   const Object& operator[](Index id) const { return get_object(id); }
};

#endif // FAST_LOOKUP_MAP_H
