// Copyright (c) 2015-2018 Daniel Cooke
// Use of this source code is governed by the MIT license that can be found in the LICENSE file.

#ifndef cnv_model_hpp
#define cnv_model_hpp

#include <vector>
#include <unordered_map>
#include <utility>

#include "config/common.hpp"
#include "core/types/haplotype.hpp"
#include "core/types/genotype.hpp"
#include "genotype_prior_model.hpp"
#include "core/models/haplotype_likelihood_cache.hpp"

namespace octopus { namespace model {

class CNVModel
{
public:
    struct AlgorithmParameters
    {
        unsigned max_iterations = 1000;
        double epsilon          = 0.05;
    };
    
    struct Priors
    {
        using GenotypeMixturesDirichletAlphas   = std::vector<double>;
        using GenotypeMixturesDirichletAlphaMap = std::unordered_map<SampleName, GenotypeMixturesDirichletAlphas>;
        
        const GenotypePriorModel& genotype_prior_model;
        GenotypeMixturesDirichletAlphaMap alphas;
    };
    
    struct Latents
    {
        using GenotypeMixturesDirichletAlphas   = std::vector<double>;
        using GenotypeMixturesDirichletAlphaMap = std::unordered_map<SampleName, GenotypeMixturesDirichletAlphas>;
        using ProbabilityVector                 = std::vector<double>;
        
        ProbabilityVector genotype_probabilities;
        GenotypeMixturesDirichletAlphaMap alphas;
    };
    
    struct InferredLatents
    {
        Latents posteriors;
        double approx_log_evidence;
    };
    
    CNVModel() = delete;
    
    CNVModel(std::vector<SampleName> samples, Priors priors);
    CNVModel(std::vector<SampleName> samples, Priors priors, AlgorithmParameters parameters);
    
    CNVModel(const CNVModel&)            = default;
    CNVModel& operator=(const CNVModel&) = default;
    CNVModel(CNVModel&&)                 = default;
    CNVModel& operator=(CNVModel&&)      = default;
    
    ~CNVModel() = default;
    
    const Priors& priors() const noexcept;
    
    InferredLatents evaluate(const std::vector<Genotype<Haplotype>>& genotypes,
                             const HaplotypeLikelihoodCache& haplotype_likelihoods) const;
    
    InferredLatents evaluate(const std::vector<Genotype<Haplotype>>& genotypes,
                             const std::vector<std::vector<unsigned>>& genotype_indices,
                             const HaplotypeLikelihoodCache& haplotype_likelihoods) const;
    
private:
    std::vector<SampleName> samples_;
    Priors priors_;
    AlgorithmParameters parameters_;
};
    
} // namespace model
} // namespace octopus

#endif
