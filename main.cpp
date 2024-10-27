#include <algorithm>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

template<typename Key, typename Value, typename hash=std::hash<Key>>
class threadsafe_lookup_table {
private:
    class bucket_type {
    private:
        typedef std::pair<Key, Value> bucket_value;
        typedef std::list<bucket_value> bucket_list;
        typedef typename bucket_list::iterator bucket_iterator;
        mutable std::shared_mutex mutex;
        bucket_list data;
        typedef typename bucket_list::const_iterator const_bucket_iterator;

        // 非const版本
        bucket_iterator for_entry_key(const Key& key) {
            return std::find_if(data.begin(), data.end(), [&](const bucket_value& x) {
                return x.first == key;
            });
        }

        // const版本
        const_bucket_iterator for_entry_key(const Key& key) const {
            return std::find_if(data.cbegin(), data.cend(), [&](const bucket_value& x) {
                return x.first == key;
            });
        }
    public:
        Value value_for(const Key& key, Value& default_value) const {
            std::shared_lock lock(mutex);
            auto it = for_entry_key(key);
            return it==data.end()?default_value:it->second;
        }

        void add_or_update(const Key& key, const Value& value) {
            std::unique_lock lock(mutex);
            auto it = for_entry_key(key);
            if (it==data.end()) {
                data.emplace_back(key, value);
            }else {
                it->second = value;
            }
        }

        void remove(const Key& key) {
            std::unique_lock lock(mutex);
            auto it = for_entry_key(key);
            if (it!=data.end()) {
                data.erase(it);
            }
        }
    };
    std::vector<std::unique_ptr<bucket_type>> buckets;
    hash hash_;
    [[nodiscard]] bucket_type& get_bucket(const Key& key) const{
        size_t hash_value = hash_(key)%buckets.size();
        return *buckets[hash_value];
    }
public:
    typedef Key key_type;
    typedef Value mapped_type;
    typedef hash hash_type;
    explicit threadsafe_lookup_table(unsigned int bucket_num=19, hash const& hasher_=hash()) :buckets(bucket_num), hash_(hasher_) {
        for (unsigned int i=0;i<bucket_num;i++) {
            buckets[i].reset(new bucket_type);
        }
    }
    threadsafe_lookup_table(const threadsafe_lookup_table&) = delete;
    threadsafe_lookup_table& operator=(const threadsafe_lookup_table&) = delete;
    Value value_for(const Key& key, Value& default_value) const {
        return get_bucket(key).value_for(key, default_value);
    }
    void add_or_update(const Key& key, const Value& value) {
        get_bucket(key).add_or_update(key, value);
    }
    void remove(const Key& key) {
        get_bucket(key).remove(key);
    }
    [[nodiscard]] std::map<Key, Value> get_map() const {
        std::vector<std::unique_lock<std::shared_mutex>> locks;
        for(unsigned int i=0;i<buckets.size();i++) {
            locks.emplace_back(std::unique_lock<std::shared_mutex>(buckets[i]->mutex));
        }
        std::map<Key, Value> result;
        for(unsigned int i=0;i<buckets.size();i++) {
            for(auto it=buckets[i]->data.begin();it!=buckets[i]->data.end();++it) {
                result.insert(*it);
            }
        }
        return result;
    }
};

int main() {
    threadsafe_lookup_table<int,std::string> lookup_table;
    lookup_table.add_or_update(1,"hello");
    lookup_table.add_or_update(2,"world");
    std::string dd="ddw";
    lookup_table.value_for(1,dd);
    std::cout<<lookup_table.value_for(3,dd)<<std::endl;
}