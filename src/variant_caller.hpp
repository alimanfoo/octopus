//
//  variant_caller.hpp
//  Octopus
//
//  Created by Daniel Cooke on 26/08/2015.
//  Copyright (c) 2015 Oxford University. All rights reserved.
//

#ifndef Octopus_variant_caller_hpp
#define Octopus_variant_caller_hpp

#include <vector>
#include <string>
#include <iterator>

#include "common.hpp"
#include "reference_genome.hpp"
#include "read_manager.hpp"
#include "read_filter.hpp"
#include "read_transform.hpp"
#include "candidate_variant_generator.hpp"

class GenomicRegion;
class Variant;
class VcfRecord;

namespace Octopus
{
    class VariantCaller
    {
    public:
        enum class RefCallType { Positional, Blocked, None };
        
        using ReadMap = Octopus::ReadMap;
        
        VariantCaller() = delete;
        VariantCaller(ReferenceGenome& reference, CandidateVariantGenerator& candidate_generator,
                      RefCallType refcall_type = RefCallType::None);
        virtual ~VariantCaller() = default;
        
        VariantCaller(const VariantCaller&)            = delete;
        VariantCaller& operator=(const VariantCaller&) = delete;
        VariantCaller(VariantCaller&&)                 = delete;
        VariantCaller& operator=(VariantCaller&&)      = delete;
        
        std::string get_details() const;
        
        std::vector<VcfRecord> call_variants(const GenomicRegion& region, ReadMap reads);
        
    protected:
        ReferenceGenome& reference_;
        
        const RefCallType refcall_type_ = RefCallType::Positional;
        
        bool refcalls_requested() const noexcept;
        
    private:
        CandidateVariantGenerator& candidate_generator_;
        
        bool done_calling(const GenomicRegion& region) const noexcept;
        
        virtual std::string do_get_details() const = 0;
        
        virtual GenomicRegion get_init_region(const GenomicRegion& region, const ReadMap& reads,
                                              const std::vector<Variant>& candidates) = 0;
        virtual GenomicRegion get_next_region(const GenomicRegion& current_region, const ReadMap& reads,
                                              const std::vector<Variant>& candidates) = 0;
        virtual std::vector<VcfRecord> call_variants(const GenomicRegion& region,
                                                     const std::vector<Variant>& candidates,
                                                     const ReadMap& reads) = 0;
    };
    
    std::vector<Allele>
    generate_callable_alleles(const GenomicRegion& region, const std::vector<Variant>& variants,
                              VariantCaller::RefCallType refcall_type, ReferenceGenome& reference);
    
} // namespace Octopus

#endif