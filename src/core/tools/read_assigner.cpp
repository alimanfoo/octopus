// Copyright (c) 2015-2018 Daniel Cooke
// Use of this source code is governed by the MIT license that can be found in the LICENSE file.

#include "read_assigner.hpp"

#include <utility>
#include <iterator>
#include <algorithm>
#include <limits>
#include <random>
#include <stdexcept>
#include <cassert>

#include <boost/optional.hpp>

#include "utils/maths.hpp"
#include "utils/kmer_mapper.hpp"
#include "core/models/haplotype_likelihood_model.hpp"
#include "core/models/error/error_model_factory.hpp"

namespace octopus {

namespace {

using HaplotypeLikelihoods = std::vector<std::vector<double>>;

void find_max_likelihood_haplotypes(const std::vector<Haplotype>& haplotypes, const unsigned read,
                                    const HaplotypeLikelihoods& likelihoods,
                                    std::vector<unsigned>& result)
{
    assert(result.empty());
    auto max_likelihood = std::numeric_limits<double>::lowest();
    for (unsigned k {0}; k < haplotypes.size(); ++k) {
        const auto curr = likelihoods[k][read];
        if (maths::almost_equal(curr, max_likelihood)) {
            result.push_back(k);
        } else if (curr > max_likelihood) {
            result.assign({k});
            max_likelihood = curr;
        }
    }
    if (result.empty()) {
        result.resize(haplotypes.size());
        std::iota(std::begin(result), std::end(result), 0);
    }
}

template <typename ForwardIt, typename RandomGenerator>
ForwardIt random_select(ForwardIt first, ForwardIt last, RandomGenerator& g)
{
    if (first == last) return first;
    const auto max = static_cast<std::size_t>(std::distance(first, last));
    if (max == 1) return first;
    std::uniform_int_distribution<std::size_t> dist {0, max - 1};
    std::advance(first, dist(g));
    return first;
}

template <typename ForwardIt>
ForwardIt random_select(ForwardIt first, ForwardIt last)
{
    static thread_local std::mt19937 generator {42};
    return random_select(first, last, generator);
}

template <typename Range>
decltype(auto) random_select(const Range& values)
{
    assert(!values.empty());
    return *random_select(std::cbegin(values), std::cend(values));
}

auto calculate_support(const std::vector<Haplotype>& haplotypes,
                       const std::vector<AlignedRead>& reads,
                       const HaplotypeLikelihoods& likelihoods,
                       boost::optional<std::deque<AlignedRead>&> ambiguous,
                       AssignmentConfig config)
{
    HaplotypeSupportMap result {};
    std::vector<unsigned> top {};
    top.reserve(haplotypes.size());
    for (unsigned i {0}; i < reads.size(); ++i) {
        find_max_likelihood_haplotypes(haplotypes, i, likelihoods, top);
        if (top.size() == 1) {
            result[haplotypes[top.front()]].push_back(reads[i]);
        } else {
            using UA = AssignmentConfig::AmbiguousAction;
            switch (config.ambiguous_action) {
                case UA::first:
                    result[haplotypes[top.front()]].push_back(reads[i]);
                    break;
                case UA::all: {
                    for (auto idx : top) result[haplotypes[idx]].push_back(reads[i]);
                    break;
                }
                case UA::random: {
                    result[haplotypes[random_select(top)]].push_back(reads[i]);
                    break;
                }
                case UA::drop:
                default:
                    break;
            }
            if (ambiguous) ambiguous->push_back(reads[i]);
        }
        top.clear();
    }
    return result;
}

auto max_deletion_size(const std::vector<Haplotype>& haplotypes)
{
    unsigned result {0};
    for (const auto& haplotype : haplotypes) {
        if (region_size(haplotype) > sequence_size(haplotype)) {
            auto diff = static_cast<unsigned>(region_size(haplotype) - sequence_size(haplotype));
            result = std::max(result, diff);
        }
    }
    return result;
}

auto compute_read_hashes(const std::vector<AlignedRead>& reads)
{
    static constexpr unsigned char mapperKmerSize {6};
    std::vector<KmerPerfectHashes> result {};
    result.reserve(reads.size());
    std::transform(std::cbegin(reads), std::cend(reads), std::back_inserter(result),
                   [=] (const AlignedRead& read) { return compute_kmer_hashes<mapperKmerSize>(read.sequence()); });
    return result;
}

auto calculate_likelihoods(const std::vector<Haplotype>& haplotypes,
                           const std::vector<AlignedRead>& reads,
                           HaplotypeLikelihoodModel& model)
{
    assert(!haplotypes.empty());
    const auto& haplotype_region = mapped_region(haplotypes.front());
    const auto reads_region = encompassing_region(reads);
    const auto min_flank_pad = HaplotypeLikelihoodModel::pad_requirement();
    unsigned min_lhs_expansion{2 * min_flank_pad}, min_rhs_expansion{2 * min_flank_pad};
    if (begins_before(reads_region, haplotype_region)) {
        min_lhs_expansion += begin_distance(reads_region, haplotype_region);
    }
    if (ends_before(haplotype_region, reads_region)) {
        min_rhs_expansion += end_distance(haplotype_region, reads_region);
    }
    const auto min_expansion = std::max(min_lhs_expansion, min_rhs_expansion) + max_deletion_size(haplotypes);
    const auto read_hashes = compute_read_hashes(reads);
    static constexpr unsigned char mapperKmerSize {6};
    auto haplotype_hashes = init_kmer_hash_table<mapperKmerSize>();
    HaplotypeLikelihoods result{};
    result.reserve(haplotypes.size());
    for (const auto& haplotype : haplotypes) {
        const auto expanded_haplotype = expand(haplotype, min_expansion);
        populate_kmer_hash_table<mapperKmerSize>(expanded_haplotype.sequence(), haplotype_hashes);
        auto haplotype_mapping_counts = init_mapping_counts(haplotype_hashes);
        model.reset(expanded_haplotype);
        std::vector<double> likelihoods(reads.size());
        std::transform(std::cbegin(reads), std::cend(reads), std::cbegin(read_hashes), std::begin(likelihoods),
                       [&] (const auto& read, const auto& read_hash) {
                           auto mapping_positions = map_query_to_target(read_hash, haplotype_hashes, haplotype_mapping_counts);
                           reset_mapping_counts(haplotype_mapping_counts);
                           return model.evaluate(read, mapping_positions);
                       });
        clear_kmer_hash_table(haplotype_hashes);
        result.push_back(std::move(likelihoods));
    }
    return result;
}

} // namespace

HaplotypeSupportMap compute_haplotype_support(const Genotype<Haplotype>& genotype,
                                              const std::vector<AlignedRead>& reads,
                                              HaplotypeLikelihoodModel model,
                                              boost::optional<std::deque<AlignedRead>&> ambiguous,
                                              AssignmentConfig config)
{
    if (!genotype.is_homozygous() && !reads.empty()) {
        const auto unique_haplotypes = genotype.copy_unique();
        assert(unique_haplotypes.size() > 1);
        const auto likelihoods = calculate_likelihoods(unique_haplotypes, reads, model);
        return calculate_support(unique_haplotypes, reads, likelihoods, ambiguous, config);
    } else {
        return {};
    }
}

HaplotypeSupportMap compute_haplotype_support(const Genotype<Haplotype>& genotype,
                                              const std::vector<AlignedRead>& reads,
                                              std::deque<AlignedRead>& ambiguous,
                                              AssignmentConfig config)
{
    return compute_haplotype_support(genotype, reads, HaplotypeLikelihoodModel {nullptr, make_indel_error_model(), false}, ambiguous, config);
}

HaplotypeSupportMap compute_haplotype_support(const Genotype<Haplotype>& genotype,
                                              const std::vector<AlignedRead>& reads,
                                              AssignmentConfig config)
{
    return compute_haplotype_support(genotype, reads, HaplotypeLikelihoodModel {nullptr, make_indel_error_model(), false}, config);
}

HaplotypeSupportMap compute_haplotype_support(const Genotype<Haplotype>& genotype,
                                              const std::vector<AlignedRead>& reads,
                                              HaplotypeLikelihoodModel model,
                                              AssignmentConfig config)
{
    return compute_haplotype_support(genotype, reads, std::move(model), boost::none, config);
}

HaplotypeSupportMap compute_haplotype_support(const Genotype<Haplotype>& genotype,
                                              const std::vector<AlignedRead>& reads,
                                              std::deque<AlignedRead>& ambiguous,
                                              HaplotypeLikelihoodModel model,
                                              AssignmentConfig config)
{
    return compute_haplotype_support(genotype, reads, std::move(model), ambiguous, config);
}

AlleleSupportMap compute_allele_support(const std::vector<Allele>& alleles,
                                        const HaplotypeSupportMap& haplotype_support)
{
    return compute_allele_support(alleles, haplotype_support,
                                  [] (const Haplotype& haplotype, const Allele& allele) {
                                      return haplotype.includes(allele);
                                  });
}

} // namespace octopus
