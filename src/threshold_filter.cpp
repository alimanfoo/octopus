//
//  threshold_filter.cpp
//  Octopus
//
//  Created by Daniel Cooke on 19/07/2016.
//  Copyright © 2016 Oxford University. All rights reserved.
//

#include "threshold_filter.hpp"

#include "vcf_header.hpp"

namespace octopus { namespace CallFiltering
{
    ThresholdVariantCallFilter::ThresholdVariantCallFilter(const ReferenceGenome& reference,
                                                           const ReadPipe& read_pipe,
                                                           std::vector<MeasureWrapper> measures,
                                                           std::size_t max_read_buffer_size)
    :
    VariantCallFilter {reference, read_pipe, std::move(measures), max_read_buffer_size}
    {}
    
    void ThresholdVariantCallFilter::annotate(VcfHeader& header) const
    {
        // TODO
    }
    
    VariantCallFilter::Classification ThresholdVariantCallFilter::classify(const MeasureVector& call_measures) const
    {
        return VariantCallFilter::Classification {};
    }
} // namespace CallFiltering
} // namespace octopus