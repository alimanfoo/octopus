// Copyright (c) 2015-2018 Daniel Cooke
// Use of this source code is governed by the MIT license that can be found in the LICENSE file.

#include "indel_mutation_model.hpp"

#include <utility>
#include <cassert>

#include "utils/maths.hpp"
#include "utils/repeat_finder.hpp"

namespace octopus {

namespace {

double calculate_gap_open_rate(const double base_rate, const unsigned period, const unsigned num_periods)
{
    return base_rate * std::pow(10.0, (3.0 / (6 + std::min(2 * period, 12u))) * period * num_periods);
}

double calculate_gap_extend_rate(const double base_rate, const unsigned period, const unsigned num_periods,
                                 const double gap_open_rate)
{
    return std::max(1'000 * gap_open_rate, 0.7);
}

} // namespace

IndelMutationModel::IndelMutationModel(Parameters params)
: params_ {std::move(params)}
, indel_repeat_model_ {params_.max_period + 1, std::vector<ModelCell>(params_.max_periodicity + 1)}
{
    for (unsigned period {0}; period <= params_.max_period; ++period) {
        for (unsigned n {0}; n <= params_.max_periodicity; ++n) {
            const auto open_rate = calculate_gap_open_rate(params.indel_mutation_rate, period, n);
            indel_repeat_model_[period][n].open = std::min(open_rate, params_.max_open_probability);
            const auto extend_rate = calculate_gap_extend_rate(params.indel_mutation_rate, period, n, open_rate);
            indel_repeat_model_[period][n].extend = std::min(extend_rate, params_.max_extend_probability);
        }
    }
}

namespace {

auto find_short_tandem_repeats(const Haplotype& haplotype)
{
    constexpr unsigned max_repeat_period {5};
    return find_exact_tandem_repeats(haplotype.sequence(), haplotype.mapped_region(), 1, max_repeat_period);
}

template <typename FordwardIt, typename Tp>
auto fill_if_greater(FordwardIt first, FordwardIt last, const Tp& value)
{
    return std::transform(first, last, first, [&] (const auto& x) { return std::max(x, value); });
}

template <typename FordwardIt, typename Tp>
auto fill_n_if_greater(FordwardIt first, std::size_t n, const Tp& value)
{
    return fill_if_greater(first, std::next(first, n), value);
}

} // namespace

IndelMutationModel::ContextIndelModel IndelMutationModel::evaluate(const Haplotype& haplotype) const
{
    const auto repeats = find_short_tandem_repeats(haplotype);
    ContextIndelModel result {};
    const auto haplotype_len = sequence_size(haplotype);
    const auto& base_probabilities = indel_repeat_model_[0][0];
    result.gap_open.resize(haplotype_len, base_probabilities.open);
    result.gap_extend.resize(haplotype_len, base_probabilities.extend);
    for (const auto& repeat : repeats) {
        assert(repeat.period > 0 && repeat.period <= params_.max_period);
        const auto repeat_offset = static_cast<std::size_t>(begin_distance(haplotype, repeat));
        const auto repeat_len = region_size(repeat);
        const auto num_repeats = static_cast<unsigned>(repeat_len / repeat.period);
        assert(num_repeats > 0);
        const auto& repeat_state = indel_repeat_model_[repeat.period][std::min(num_repeats, params_.max_periodicity)];
        assert(repeat_offset + repeat_len <= result.gap_open.size());
        fill_n_if_greater(std::next(std::begin(result.gap_open), repeat_offset), repeat_len, repeat_state.open);
        assert(repeat_offset + repeat_len <= result.gap_extend.size());
        fill_n_if_greater(std::next(std::begin(result.gap_extend), repeat_offset), repeat_len, repeat_state.extend);
    }
    return result;
}

// non-member methods

IndelMutationModel::ContextIndelModel make_indel_model(const Haplotype& context, IndelMutationModel::Parameters params)
{
    IndelMutationModel model {params};
    return model.evaluate(context);
}

} // namespace octopus
