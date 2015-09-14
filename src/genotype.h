//
//  genotype.h
//  Octopus
//
//  Created by Daniel Cooke on 24/03/2015.
//  Copyright (c) 2015 Oxford University. All rights reserved.
//

#ifndef __Octopus__genotype__
#define __Octopus__genotype__

#include <vector>
#include <unordered_map>
#include <ostream>
#include <iterator>  // std::cbegin etc
#include <initializer_list>
#include <algorithm> // std::std::sort, std::inplace_merge, std::all_of, std::binary_search,
                     // std::equal_range, std::unique_copy, std::equal, std::fill_n
#include <type_traits> // std::enable_if_t, std::is_base_of
#include <boost/functional/hash.hpp> // boost::hash_range
#include <boost/math/special_functions/binomial.hpp>

#include "haplotype.h"
#include "equitable.h"
#include "mappable.h"

// Genotype should only store Haplotype and Alleles
template <typename MappableType, typename = std::enable_if_t<std::is_base_of<Mappable<MappableType>, MappableType>::value>>
class Genotype;

template <typename MappableType>
class Genotype<MappableType> : public Equitable<Genotype<MappableType>>
{
public:
    using Iterator = typename std::vector<MappableType>::const_iterator;
    
    Genotype() = default;
    explicit Genotype(unsigned ploidy);
    explicit Genotype(unsigned ploidy, const MappableType& init);
    explicit Genotype(std::initializer_list<MappableType> elements);
    ~Genotype() = default;
    
    Genotype(const Genotype&)            = default;
    Genotype& operator=(const Genotype&) = default;
    Genotype(Genotype&&)                 = default;
    Genotype& operator=(Genotype&&)      = default;
    
    const MappableType& at(unsigned n) const;
    void emplace(const MappableType& element);
    void emplace(MappableType&& element);
    
    Iterator begin() const noexcept;
    Iterator end() const noexcept ;
    Iterator cbegin() const noexcept ;
    Iterator cend() const noexcept ;
    
    unsigned ploidy() const noexcept;
    bool contains(const MappableType& element) const;
    unsigned num_occurences(const MappableType& element) const;
    bool is_homozygous() const;
    std::vector<MappableType> get_unique() const;
    
private:
    std::vector<MappableType> elements_;
};

template <typename MappableType>
Genotype<MappableType>::Genotype(unsigned ploidy)
:
elements_ {}
{
    elements_.reserve(ploidy);
}

template <typename MappableType>
Genotype<MappableType>::Genotype(unsigned ploidy, const MappableType& init)
:
elements_ {ploidy, init}
{}

template <typename MappableType>
Genotype<MappableType>::Genotype(std::initializer_list<MappableType> elements)
:
elements_ {elements}
{
    std::sort(elements_.begin(), elements_.end());
}

template <typename MappableType>
const MappableType& Genotype<MappableType>::at(unsigned n) const
{
    return elements_.at(n);
}

template <typename MappableType>
void Genotype<MappableType>::emplace(const MappableType& element)
{
    elements_.emplace_back(element);
    std::inplace_merge(std::begin(elements_), std::prev(std::end(elements_)), std::end(elements_));
}

template <typename MappableType>
void Genotype<MappableType>::emplace(MappableType&& element)
{
    elements_.emplace_back(std::move(element));
    std::inplace_merge(std::begin(elements_), std::prev(std::end(elements_)), std::end(elements_));
}

template <typename MappableType>
typename Genotype<MappableType>::Iterator Genotype<MappableType>::begin() const noexcept
{
    return elements_.begin();
}

template <typename MappableType>
typename Genotype<MappableType>::Iterator Genotype<MappableType>::end() const noexcept
{
    return elements_.end();
}

template <typename MappableType>
typename Genotype<MappableType>::Iterator Genotype<MappableType>::cbegin() const noexcept
{
    return elements_.cbegin();
}

template <typename MappableType>
typename Genotype<MappableType>::Iterator Genotype<MappableType>::cend() const noexcept
{
    return elements_.cend();
}

template <typename MappableType>
bool Genotype<MappableType>::is_homozygous() const
{
    if (ploidy() < 2) return true;
    const auto& first_element = elements_.front();
    return std::all_of(std::next(std::cbegin(elements_)), std::cend(elements_),
                       [&first_element] (const auto& element) {
                           return first_element == element;
                       });
}

template <typename MappableType>
bool Genotype<MappableType>::contains(const MappableType& element) const
{
    return std::binary_search(std::cbegin(elements_), std::cend(elements_), element);
}

template <typename MappableType>
unsigned Genotype<MappableType>::num_occurences(const MappableType& element) const
{
    auto equal_range = std::equal_range(std::cbegin(elements_), std::cend(elements_), element);
    return static_cast<unsigned>(std::distance(equal_range.first, equal_range.second));
}

template <typename MappableType>
unsigned Genotype<MappableType>::ploidy() const noexcept
{
    return static_cast<unsigned>(elements_.size());
}

template <typename MappableType>
std::vector<MappableType> Genotype<MappableType>::get_unique() const
{
    std::vector<MappableType> result {};
    result.reserve(ploidy());
    
    std::unique_copy(std::cbegin(elements_), std::cend(elements_), std::back_inserter(result));
    
    return result;
}

template <typename MappableType>
bool operator==(const Genotype<MappableType>& lhs, const Genotype<MappableType>& rhs)
{
    return lhs.ploidy() == rhs.ploidy() && std::equal(std::cbegin(lhs), std::cend(lhs), std::cbegin(rhs));
}

namespace std
{
    template <typename MappableType> struct hash<Genotype<MappableType>>
    {
        size_t operator()(const Genotype<MappableType>& genotype) const
        {
            return boost::hash_range(std::cbegin(genotype), std::cend(genotype));
        }
    };
} // end namespace std

template <typename MappableType1, typename MappableType2>
bool contains(const Genotype<MappableType1>& lhs, const Genotype<MappableType2>& rhs)
{
    return std::all_of(std::cbegin(lhs), std::cend(lhs), [&rhs] (const auto& element) {
        return rhs.contains(element);
    });
}

inline unsigned num_genotypes(unsigned num_elements, unsigned ploidy)
{
    return static_cast<unsigned>(boost::math::binomial_coefficient<double>(num_elements + ploidy - 1, num_elements - 1));
}

namespace detail
{
    template <typename MappableType>
    Genotype<MappableType> generate_genotype(const std::vector<MappableType>& elements,
                                             const std::vector<unsigned>& element_indicies)
    {
        Genotype<MappableType> result {static_cast<unsigned>(element_indicies.size())};
        
        for (auto i : element_indicies) {
            result.emplace(elements[i]);
        }
        
        return result;
    }
} // end namespace detail

// Assumes the input haplotypes are unique
template <typename MappableType>
std::vector<Genotype<MappableType>> generate_all_genotypes(const std::vector<MappableType>& elements, unsigned ploidy)
{
    if (elements.empty()) return {};
    
    auto num_elements = static_cast<unsigned>(elements.size());
    
    if (num_elements == 1) {
        return {Genotype<MappableType> {ploidy, elements.front()}}; // just an optimization
    }
    
    std::vector<Genotype<MappableType>> result {};
    result.reserve(num_genotypes(num_elements, ploidy));
    
    std::vector<unsigned> element_indicies(ploidy, 0);
    
    unsigned i {};
    
    while (true) {
        if (element_indicies[i] == num_elements) {
            do { ++i; } while (i < ploidy && element_indicies[i] == num_elements - 1);
            
            if (i == ploidy) return result;
            
            ++element_indicies[i];
            
            std::fill_n(std::begin(element_indicies), i + 1, element_indicies[i]);
            
            i = 0;
        }
        
        result.push_back(detail::generate_genotype(elements, element_indicies));
        ++element_indicies[i];
    }
}

template <typename MappableType>
std::unordered_map<MappableType, unsigned> get_element_occurence_map(const Genotype<MappableType>& genotype)
{
    std::unordered_map<MappableType, unsigned> result {};
    
    for (unsigned i {}; i < genotype.ploidy(); ++i) {
        ++result[genotype.at(i)];
    }
    
    return result;
}

template <typename MappableType>
std::ostream& operator<<(std::ostream& os, const Genotype<MappableType>& genotype)
{
    auto element_occurences = get_element_occurence_map(genotype);
    std::vector<std::pair<MappableType, unsigned>> p {element_occurences.begin(), element_occurences.end()};
    for (unsigned i {}; i < p.size() - 1; ++i) {
        os << p[i].first << "(" << p[i].second << "),";
    }
    os << p.back().first << "(" << p.back().second << ")";
    return os;
}

#endif /* defined(__Octopus__genotype__) */
