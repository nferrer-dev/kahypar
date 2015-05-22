/*
 * RecursiveBisection.h
 *
 *  Created on: Apr 6, 2015
 *      Author: theuer
 */

#ifndef SRC_PARTITION_INITIAL_PARTITIONING_RECURSIVEBISECTION_H_
#define SRC_PARTITION_INITIAL_PARTITIONING_RECURSIVEBISECTION_H_

#include <vector>
#include <cmath>

#include "lib/definitions.h"
#include "lib/io/PartitioningOutput.h"
#include "partition/initial_partitioning/IInitialPartitioner.h"
#include "partition/initial_partitioning/InitialPartitionerBase.h"
#include "tools/RandomFunctions.h"

using defs::Hypergraph;
using defs::HypernodeID;
using defs::HypernodeWeight;
using defs::HyperedgeID;

using partition::InitialStatManager;

namespace partition {

template<class InitialPartitioner = IInitialPartitioner>
class RecursiveBisection: public IInitialPartitioner,
		private InitialPartitionerBase {

public:
	RecursiveBisection(Hypergraph& hypergraph, Configuration& config) :
			InitialPartitionerBase(hypergraph, config), balancer(_hg, _config) {

		for (const HypernodeID hn : _hg.nodes()) {
			total_hypergraph_weight += _hg.nodeWeight(hn);
		}

	}

	~RecursiveBisection() {
	}

private:

	void kwayPartitionImpl() final {
		recursiveBisection(_hg, 0, _config.initial_partitioning.k - 1);
		_config.initial_partitioning.epsilon = _config.partition.epsilon;
		InitialPartitionerBase::eraseConnectedComponents();
		InitialPartitionerBase::recalculateBalanceConstraints(_config.initial_partitioning.epsilon);
		InitialPartitionerBase::performFMRefinement();
	}

	void bisectionPartitionImpl() final {
		performMultipleRunsOnHypergraph(_hg, 2);
		_config.initial_partitioning.epsilon = _config.partition.epsilon;
		InitialPartitionerBase::recalculateBalanceConstraints(_config.initial_partitioning.epsilon);
		InitialPartitionerBase::performFMRefinement();
	}

	double calculateEpsilon(Hypergraph& hyper,
			HypernodeWeight hypergraph_weight, PartitionID k) {

		double base = ((static_cast<double>(k) * total_hypergraph_weight)
				/ (static_cast<double>(_config.initial_partitioning.k)
						* hypergraph_weight)) * (1 + _config.partition.epsilon);

		return std::pow(base,
				1.0
						/ std::ceil(
								(std::log(static_cast<double>(k)) / std::log(2))))
				- 1.0;

	}

	int calculateRuns(double alpha, PartitionID all_runs_k, PartitionID x) {
		int n = _config.initial_partitioning.nruns;
		PartitionID k = _config.initial_partitioning.k;
		if (k <= all_runs_k)
			return n;
		double m = ((1.0 - alpha) * n) / (all_runs_k - k);
		double c = ((alpha * all_runs_k - k) * n) / (all_runs_k - k);
		return std::max(((int) (m * x + c)), 1);
	}

	/*double calculateRecursiveEpsilon(PartitionID curk, PartitionID k,
	 double current_value) {
	 if (curk == 1) {
	 return std::pow(
	 current_value * (1.0 + _config.partition.epsilon)
	 / _config.initial_partitioning.k,
	 1.0 / (std::log(static_cast<double>(k)) / std::log(2.0)))
	 - 1.0;
	 }
	 return std::min(
	 calculateRecursiveEpsilon(
	 std::floor(static_cast<double>(curk) / 2.0), k,
	 current_value * curk
	 / std::floor(static_cast<double>(curk) / 2.0)),
	 calculateRecursiveEpsilon(
	 std::ceil(static_cast<double>(curk) / 2.0), k,
	 current_value * curk
	 / std::ceil(static_cast<double>(curk) / 2.0)));
	 }*/

	void recursiveBisection(Hypergraph& hyper, PartitionID k1, PartitionID k2) {

		//Assign partition id
		if (k2 - k1 == 0) {
			int count = 0;
			for (HypernodeID hn : hyper.nodes()) {
				hyper.setNodePart(hn, k1);
				count++;
			}
			return;
		}

		HypernodeWeight hypergraph_weight = 0;
		for (const HypernodeID hn : hyper.nodes()) {
			hypergraph_weight += hyper.nodeWeight(hn);
		}

		//Calculate balance constraints for partition 0 and 1
		PartitionID k = (k2 - k1 + 1);
		PartitionID km = floor(static_cast<double>(k) / 2.0);

		_config.initial_partitioning.epsilon = calculateEpsilon(hyper,
				hypergraph_weight, k);

		_config.initial_partitioning.perfect_balance_partition_weight[0] =
				static_cast<double>(km) * hypergraph_weight
						/ static_cast<double>(k);
		_config.initial_partitioning.perfect_balance_partition_weight[1] =
				static_cast<double>(k - km) * hypergraph_weight
						/ static_cast<double>(k);
		InitialPartitionerBase::recalculateBalanceConstraints(_config.initial_partitioning.epsilon);

		//Performing bisection
		performMultipleRunsOnHypergraph(hyper, k);

		if (_config.initial_partitioning.stats) {
			InitialStatManager::getInstance().addStat("Recursive Bisection",
					"Hypergraph weight (" + std::to_string(k1) + " - "
							+ std::to_string(k2) + ")", hypergraph_weight);
			InitialStatManager::getInstance().addStat("Recursive Bisection",
					"Epsilon (" + std::to_string(k1) + " - "
							+ std::to_string(k2) + ")",
					_config.initial_partitioning.epsilon);
			InitialStatManager::getInstance().addStat("Recursive Bisection",
					"Hypergraph cut (" + std::to_string(k1) + " - "
							+ std::to_string(k2) + ")",
					metrics::hyperedgeCut(hyper));
		}

		//Extract Hypergraph with partition 0
		HypernodeID num_hypernodes_0;
		HyperedgeID num_hyperedges_0;
		HyperedgeIndexVector index_vector_0;
		HyperedgeVector edge_vector_0;
		HyperedgeWeightVector hyperedge_weights_0;
		HypernodeWeightVector hypernode_weights_0;
		std::vector<HypernodeID> hgToExtractedHypergraphMapper_0;
		InitialPartitionerBase::extractPartitionAsHypergraph(hyper, 0,
				num_hypernodes_0, num_hyperedges_0, index_vector_0,
				edge_vector_0, hyperedge_weights_0, hypernode_weights_0,
				hgToExtractedHypergraphMapper_0);
		Hypergraph partition_0(num_hypernodes_0, num_hyperedges_0,
				index_vector_0, edge_vector_0, _config.initial_partitioning.k,
				&hyperedge_weights_0, &hypernode_weights_0);

		//Recursive bisection on partition 0
		recursiveBisection(partition_0, k1, k1 + km - 1);

		//Extract Hypergraph with partition 1
		HypernodeID num_hypernodes_1;
		HyperedgeID num_hyperedges_1;
		HyperedgeIndexVector index_vector_1;
		HyperedgeVector edge_vector_1;
		HyperedgeWeightVector hyperedge_weights_1;
		HypernodeWeightVector hypernode_weights_1;
		std::vector<HypernodeID> hgToExtractedHypergraphMapper_1;
		InitialPartitionerBase::extractPartitionAsHypergraph(hyper, 1,
				num_hypernodes_1, num_hyperedges_1, index_vector_1,
				edge_vector_1, hyperedge_weights_1, hypernode_weights_1,
				hgToExtractedHypergraphMapper_1);
		Hypergraph partition_1(num_hypernodes_1, num_hyperedges_1,
				index_vector_1, edge_vector_1, _config.initial_partitioning.k,
				&hyperedge_weights_1, &hypernode_weights_1);

		//Recursive bisection on partition 1
		recursiveBisection(partition_1, k1 + km, k2);

		//Assign partition id from partition 0 to the current hypergraph
		for (HypernodeID hn : partition_0.nodes()) {
			if (hyper.partID(hgToExtractedHypergraphMapper_0[hn])
					!= partition_0.partID(hn)) {
				hyper.changeNodePart(hgToExtractedHypergraphMapper_0[hn],
						hyper.partID(hgToExtractedHypergraphMapper_0[hn]),
						partition_0.partID(hn));
			}
		}

		ASSERT(
				[&]() { for(HypernodeID hn : partition_0.nodes()) { if(partition_0.partID(hn) != hyper.partID(hgToExtractedHypergraphMapper_0[hn])) { return false; } } return true; }(),
				"Assignment of a hypernode from a bisection step below failed in partition 0!");

		//Assign partition id from partition 1 to the current hypergraph
		for (HypernodeID hn : partition_1.nodes()) {
			if (hyper.partID(hgToExtractedHypergraphMapper_1[hn])
					!= partition_1.partID(hn)) {
				hyper.changeNodePart(hgToExtractedHypergraphMapper_1[hn],
						hyper.partID(hgToExtractedHypergraphMapper_1[hn]),
						partition_1.partID(hn));
			}
		}

		ASSERT(
				[&]() { for(HypernodeID hn : partition_1.nodes()) { if(partition_1.partID(hn) != hyper.partID(hgToExtractedHypergraphMapper_1[hn])) { return false; } } return true; }(),
				"Assignment of a hypernode from a bisection step below failed in partition 1!");

	}

	void performMultipleRunsOnHypergraph(Hypergraph& hyper, PartitionID k) {
		std::vector<PartitionID> best_partition(hyper.numNodes(), 0);
		HyperedgeWeight best_cut = std::numeric_limits<HyperedgeWeight>::max();

		int runs = calculateRuns(_config.initial_partitioning.alpha, 2, k);
		for (int i = 0; i < runs; i++) {
			// TODO(heuer): In order to improve running time, you really should
			// instantiate the partitioner only _once_ and have the partition
			// method clear the interal state of the partitioner at the beginning.
			// I think this will remove a lot of memory management overhead.
			InitialPartitioner partitioner(hyper, _config);
			partitioner.partition(2);

			HyperedgeWeight current_cut = metrics::hyperedgeCut(hyper);
			if (current_cut < best_cut) {
				best_cut = current_cut;
				for (HypernodeID hn : hyper.nodes()) {
					best_partition[hn] = hyper.partID(hn);
				}
			}
		}

		for (HypernodeID hn : hyper.nodes()) {
			if (hyper.partID(hn) != best_partition[hn]) {
				hyper.changeNodePart(hn, hyper.partID(hn), best_partition[hn]);
			}
		}
	}

	using InitialPartitionerBase::_hg;
	using InitialPartitionerBase::_config;

	HypernodeWeight total_hypergraph_weight = 0;

	HypergraphPartitionBalancer balancer;

};

}

#endif /* SRC_PARTITION_INITIAL_PARTITIONING_RECURSIVEBISECTION_H_ */
