//
//  haplotype_tests.cpp
//  Octopus
//
//  Created by Daniel Cooke on 02/04/2015.
//  Copyright (c) 2015 Oxford University. All rights reserved.
//

#include "catch.hpp"

#include <iostream>
#include <string>
#include <cstddef>
#include <set>

#include "test_common.h"
#include "reference_genome.h"
#include "reference_genome_factory.h"
#include "read_manager.h"
#include "variant.h"
#include "variant_utils.h"
#include "variant_candidate_generator.h"
#include "alignment_candidate_variant_generator.h"
#include "haplotype.h"
#include "genotype.h"

TEST_CASE("test_make_haplotypes", "[haplotype]")
{
    ReferenceGenomeFactory a_factory {};
    ReferenceGenome human(a_factory.make(human_reference_fasta));
    
    auto a_region = parse_region("3:1000000-1000010", human);
    
    auto the_reference_sequence = human.get_sequence(a_region); // CCAACAAGCA
    
    Haplotype reference_haplotype {human, a_region};
    
    REQUIRE(reference_haplotype.get_sequence() == the_reference_sequence);
    
    VariantFactory a_variant_factory {};
    
    auto variant_1 = a_variant_factory.make("3", 1000004, "C", "A");
    
    Haplotype haplotype2 {human};
    haplotype2.emplace_back(variant_1);
    
    REQUIRE(haplotype2.get_sequence(a_region) == "CCAAAAAGCA");
    
    Haplotype haplotype3 {human, a_region};
    haplotype3.emplace_back(variant_1);
    
    REQUIRE(haplotype2.get_sequence(a_region) == haplotype3.get_sequence());
    
    auto variant_2 = a_variant_factory.make("3", 1000004, "CA", "");
    auto variant_3 = a_variant_factory.make("3", 1000008, "", "C");
    
    Haplotype haplotype4 {human, a_region};
    haplotype4.emplace_back(variant_2);
    haplotype4.emplace_back(variant_3);
    
    REQUIRE(haplotype4.get_sequence() == "CCAAAGCCA");
    
    Haplotype haplotype5 {human};
    haplotype5.emplace_back(variant_2);
    haplotype5.emplace_back(variant_3);
    
    REQUIRE(haplotype5.get_sequence() == "AGC");
    
    auto variant_4 = a_variant_factory.make("3", 1000004, "CA", "GG");
    
    Haplotype haplotype6 {human, a_region};
    haplotype6.emplace_back(variant_4);
    
    REQUIRE(haplotype6.get_sequence() == "CCAAGGAGCA");
    
    auto variant_5 = a_variant_factory.make("3", 1000004, "C", "G");
    auto variant_6 = a_variant_factory.make("3", 1000005, "A", "G");
    
    Haplotype haplotype7 {human, a_region};
    haplotype7.emplace_back(variant_6);
    haplotype7.emplace_front(variant_5);
    
    REQUIRE(haplotype7.get_sequence() == haplotype6.get_sequence());
}

TEST_CASE("test_make_haplotype_from_candidates", "[haplotype]")
{
    ReferenceGenomeFactory a_factory {};
    ReferenceGenome ecoli(a_factory.make(ecoli_reference_fasta));
    
    ReadManager a_read_manager(std::vector<std::string> {ecoli_bam});
    
    VariantFactory a_variant_factory {};
    VariantCandidateGenerator candidate_generator {};
    candidate_generator.register_generator(
            std::make_unique<AlignmentCandidateVariantGenerator>(ecoli, a_variant_factory, 0));
    
    auto a_region = parse_region("R00000042:99640-99745", ecoli);
    
    auto reference_sequence = ecoli.get_sequence(a_region);
    
    auto sample_ids = a_read_manager.get_sample_ids();
    auto the_sample_id = sample_ids.at(0);
    
    auto some_reads = a_read_manager.fetch_reads(the_sample_id, a_region);
    
    candidate_generator.add_reads(some_reads.begin(), some_reads.end());
    
    auto variants = candidate_generator.get_candidates(a_region);
    
    REQUIRE(variants.size() == 12);
    
    Haplotype haplotype1 {ecoli, a_region};
    for (const auto& variant : variants) {
        if (is_snp(variant)) {
            haplotype1.emplace_back(variant);
        }
    }
    
    REQUIRE(haplotype1.get_sequence() == "AGCGTGGGTAAACAAAGCCATGCTATCAGCACCGCCAGCGGCGTTGGCGAACA"
            "TTTTGCTGATAAAACTGCGTTAATTACGCGTCTTAAATTACTGATTGCTGAG");
}

TEST_CASE("test_make_genotypes", "[genotype]")
{
    ReferenceGenomeFactory a_factory {};
    ReferenceGenome human(a_factory.make(human_reference_fasta));
    
    Haplotype hap1 {human};
    hap1.emplace_back(parse_region("1", human), "");
    
    Haplotype hap2 {human};
    hap2.emplace_back(parse_region("2", human), "");
    
    Haplotype hap3 {human};
    hap3.emplace_back(parse_region("3", human), "");
    
    Haplotype hap4 {human};
    hap4.emplace_back(parse_region("4", human), "");
    
    std::vector<Haplotype> haplotypes {hap1, hap2, hap3, hap4};
    
    unsigned num_haplotypes {4};
    
    auto genotypes_1 = get_all_genotypes(haplotypes, 1);
    
    REQUIRE(genotypes_1.size() == num_genotypes(num_haplotypes, 1));
    
    std::unordered_set<Genotype> unique_1 {genotypes_1.cbegin(), genotypes_1.cend()};
    
    REQUIRE(genotypes_1.size() == unique_1.size());
    
    auto genotypes_2 = get_all_genotypes(haplotypes, 2);
    
    REQUIRE(genotypes_2.size() == num_genotypes(num_haplotypes, 2));
    
    std::unordered_set<Genotype> unique_2 {genotypes_2.cbegin(), genotypes_2.cend()};
    
    REQUIRE(genotypes_2.size() == unique_2.size());
    
    auto genotypes_3 = get_all_genotypes(haplotypes, 3);
    
    REQUIRE(genotypes_3.size() == num_genotypes(num_haplotypes, 3));
    
    std::unordered_set<Genotype> unique_3 {genotypes_3.cbegin(), genotypes_3.cend()};
    
    REQUIRE(genotypes_3.size() == unique_3.size());
    
    auto genotypes_4 = get_all_genotypes(haplotypes, 4);
    
    REQUIRE(genotypes_4.size() == num_genotypes(num_haplotypes, 4));
    
    std::unordered_set<Genotype> unique_4 {genotypes_4.cbegin(), genotypes_4.cend()};
    
    REQUIRE(genotypes_4.size() == unique_4.size());
}
