// Copyright (c) 2015-2018 Daniel Cooke
// Use of this source code is governed by the MIT license that can be found in the LICENSE file.

#ifndef haplotype_likelihood_cache_hpp
#define haplotype_likelihood_cache_hpp

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <functional>

#include <boost/optional.hpp>

#include "config/common.hpp"
#include "core/types/haplotype.hpp"
#include "basics/aligned_read.hpp"
#include "utils/kmer_mapper.hpp"
#include "haplotype_likelihood_model.hpp"

namespace octopus {

/*
    HaplotypeLikelihoodCache is essentially a matrix of haplotype likelihoods, i.e.
    p(read | haplotype) for a given set of AlignedReads and Haplotypes.
 
    The matrix can be efficiently populated as the read mapping and alignment are
    done internally which allows minimal memory allocation.
 */
class HaplotypeLikelihoodCache
{
public:
    using FlankState = HaplotypeLikelihoodModel::FlankState;
    
    using LikelihoodVector     = std::vector<double>;
    using LikelihoodVectorRef  = std::reference_wrapper<const LikelihoodVector>;
    using HaplotypeRef         = std::reference_wrapper<const Haplotype>;
    using SampleLikelihoodMap  = std::unordered_map<HaplotypeRef, LikelihoodVectorRef>;
    
    HaplotypeLikelihoodCache() = default;
    
    HaplotypeLikelihoodCache(unsigned max_haplotypes, const std::vector<SampleName>& samples);
    
    HaplotypeLikelihoodCache(HaplotypeLikelihoodModel likelihood_model,
                             unsigned max_haplotypes,
                             const std::vector<SampleName>& samples);
    
    HaplotypeLikelihoodCache(const HaplotypeLikelihoodCache&)            = default;
    HaplotypeLikelihoodCache& operator=(const HaplotypeLikelihoodCache&) = default;
    HaplotypeLikelihoodCache(HaplotypeLikelihoodCache&&)                 = default;
    HaplotypeLikelihoodCache& operator=(HaplotypeLikelihoodCache&&)      = default;
    
    ~HaplotypeLikelihoodCache() = default;
    
    void populate(const ReadMap& reads, const std::vector<Haplotype>& haplotypes,
                  boost::optional<FlankState> flank_state = boost::none);
    
    std::size_t num_likelihoods(const SampleName& sample) const;
    
    const LikelihoodVector& operator()(const SampleName& sample, const Haplotype& haplotype) const;
    const LikelihoodVector& operator[](const Haplotype& haplotype) const; // when primed with a sample
    
    SampleLikelihoodMap extract_sample(const SampleName& sample) const;
    
    bool contains(const Haplotype& haplotype) const noexcept;
    
    template <typename S, typename Container>
    void insert(S&& sample, const Haplotype& haplotype, Container&& likelihoods);
    
    template <typename Container> void erase(const Container& haplotypes);
    
    bool is_empty() const noexcept;
    
    void clear() noexcept;
    
    bool is_primed() const noexcept;
    void prime(const SampleName& sample) const;
    void unprime() const noexcept;
    
private:
    static constexpr unsigned char mapperKmerSize {6};
    static constexpr std::size_t maxMappingPositions {10};
    
    HaplotypeLikelihoodModel likelihood_model_;
    
    struct ReadPacket
    {
        using Iterator = ReadMap::mapped_type::const_iterator;
        ReadPacket(Iterator first, Iterator last);
        Iterator first, last;
        std::size_t num_reads;
    };
    
    std::unordered_map<Haplotype, std::vector<LikelihoodVector>, HaplotypeHash> cache_;
    std::unordered_map<SampleName, std::size_t> sample_indices_;
    
    mutable boost::optional<std::size_t> primed_sample_;
    
    // Just to optimise population
    std::vector<ReadPacket> read_iterators_;
    std::vector<std::size_t> mapping_positions_;
    
    void set_read_iterators_and_sample_indices(const ReadMap& reads);
};

template <typename S, typename Container>
void HaplotypeLikelihoodCache::insert(S&& sample, const Haplotype& haplotype,
                                      Container&& likelihoods)
{
    sample_indices_.emplace(std::forward<S>(sample), sample_indices_.size());
    cache_[haplotype].emplace_back(std::forward<Container>(likelihoods));
}

template <typename Container>
void HaplotypeLikelihoodCache::erase(const Container& haplotypes)
{
    for (const auto& haplotype : haplotypes) {
        cache_.erase(haplotype);
    }
}

// non-member methods

HaplotypeLikelihoodCache merge_samples(const std::vector<SampleName>& samples,
                                       const SampleName& new_sample,
                                       const std::vector<Haplotype>& haplotypes,
                                       const HaplotypeLikelihoodCache& haplotype_likelihoods);

namespace debug {

std::vector<std::reference_wrapper<const Haplotype>>
rank_haplotypes(const std::vector<Haplotype>& haplotypes, const SampleName& sample,
                const HaplotypeLikelihoodCache& haplotype_likelihoods);

template <typename S>
void print_read_haplotype_likelihoods(S&& stream,
                                     const std::vector<Haplotype>& haplotypes,
                                     const ReadMap& reads,
                                     const HaplotypeLikelihoodCache& haplotype_likelihoods,
                                     const std::size_t n = 5)
{
    if (n == static_cast<std::size_t>(-1)) {
        stream << "Printing all read likelihoods for each haplotype in ";
    } else {
        stream << "Printing top " << n << " read likelihoods for each haplotype in ";
    }
    const bool is_single_sample {reads.size() == 1};
    if (is_single_sample) {
        stream << "sample " << std::cbegin(reads)->first;
    } else {
        stream << "each sample";
    }
    stream << '\n';
    using ReadReference = std::reference_wrapper<const AlignedRead>;
    for (const auto& sample_reads : reads) {
        const auto& sample = sample_reads.first;
        if (!is_single_sample) {
            stream << "Sample: " << sample << ":" << '\n';
        }
        const auto ranked_haplotypes = rank_haplotypes(haplotypes, sample, haplotype_likelihoods);
        const auto m = std::min(n, sample_reads.second.size());
        for (const auto& haplotype : ranked_haplotypes) {
            if (!is_single_sample) {
                stream << "\t";
            }
            debug::print_variant_alleles(stream, haplotype);
            stream << '\n';
            std::vector<std::pair<ReadReference, double>> likelihoods {};
            likelihoods.reserve(sample_reads.second.size());
            std::transform(std::cbegin(sample_reads.second), std::cend(sample_reads.second),
                           std::cbegin(haplotype_likelihoods(sample, haplotype)),
                           std::back_inserter(likelihoods),
                           [] (const AlignedRead& read, const double likelihood) {
                               return std::make_pair(std::cref(read), likelihood);
                           });
            const auto mth = std::next(std::begin(likelihoods), m);
            std::partial_sort(std::begin(likelihoods), mth, std::end(likelihoods),
                              [] (const auto& lhs, const auto& rhs) {
                                  return lhs.second > rhs.second;
                              });
            std::for_each(std::begin(likelihoods), mth, [&] (const auto& p) {
                if (is_single_sample) {
                    stream << "\t";
                } else {
                    stream << "\t\t";
                }
                const auto& read = p.first.get();
                stream << read.name() << " "
                       << mapped_region(read) << " "
                       << p.first.get().cigar() << ": "
                       << p.second << '\n';
            });
        }
    }
}

void print_read_haplotype_likelihoods(const std::vector<Haplotype>& haplotypes,
                                      const ReadMap& reads,
                                      const HaplotypeLikelihoodCache& haplotype_likelihoods,
                                      std::size_t n = 5);

} // namespace debug

} // namespace octopus

#endif
