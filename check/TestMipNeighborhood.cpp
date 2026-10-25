#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "Highs.h"
#include "catch.hpp"
#include "lp_data/HConst.h"
#include "lp_data/HighsCallback.h"
#include "lp_data/HighsLp.h"
#include "lp_data/HighsOptions.h"
#include "util/HighsInt.h"

namespace {
struct Fixture {
  HighsLp lp;
  double optimum = 0;
  std::vector<double> optimal_solution;
};

void Row(HighsLp& lp, std::vector<std::pair<HighsInt, double>> terms,
         double lower, double upper) {
  for (const auto& term : terms) {
    lp.a_matrix_.index_.push_back(term.first);
    lp.a_matrix_.value_.push_back(term.second);
  }
  lp.a_matrix_.start_.push_back(lp.a_matrix_.index_.size());
  lp.row_lower_.push_back(lower);
  lp.row_upper_.push_back(upper);
  ++lp.num_row_;
}

Fixture MakeCut(int groups, int size, int seed, bool wide,
                bool require_one = false) {
  Fixture fixture;
  HighsLp& lp = fixture.lp;
  const int binaries = groups * size;
  lp.num_col_ = binaries;
  lp.col_cost_.assign(binaries, 0);
  lp.col_lower_.assign(binaries, 0);
  lp.col_upper_.assign(binaries, 1);
  lp.integrality_.assign(binaries, HighsVarType::kInteger);
  fixture.optimal_solution.assign(binaries, 0);
  lp.a_matrix_.format_ = MatrixFormat::kRowwise;
  lp.a_matrix_.start_.clear();
  lp.a_matrix_.start_.push_back(0);
  std::mt19937 random(seed);
  for (int group = 0; group < groups; ++group) {
    const HighsInt first_edge_col = lp.num_col_;
    std::vector<std::vector<int>> weights(size, std::vector<int>(size));
    for (int i = 0; i < size; ++i) {
      for (int j = i + 1; j < size; ++j) {
        int weight = 1 + random() % 7;
        weights[i][j] = weight;
        HighsInt z = lp.num_col_++;
        lp.col_cost_.push_back(-weight);
        lp.col_lower_.push_back(0);
        lp.col_upper_.push_back(1);
        lp.integrality_.push_back(HighsVarType::kContinuous);
        fixture.optimal_solution.push_back(0);
        Row(lp, {{z, 1}, {group * size + i, -1}, {group * size + j, -1}},
            -kHighsInf, 0);
        Row(lp, {{z, 1}, {group * size + i, 1}, {group * size + j, 1}},
            -kHighsInf, 2);
      }
    }
    int best = 0;
    std::uint64_t best_mask = 0;
    for (std::uint64_t mask = 0; mask < (std::uint64_t{1} << (size - 1));
         ++mask) {
      int value = 0;
      for (int i = 0; i < size; ++i) {
        for (int j = i + 1; j < size; ++j) {
          if (((mask >> i) & 1) != ((mask >> j) & 1)) {
            value += weights[i][j];
          }
        }
      }
      if (value > best) {
        best = value;
        best_mask = mask;
      }
    }
    int ones = 0;
    for (int i = 0; i < size; ++i) ones += (best_mask >> i) & 1;
    if (ones > size / 2) best_mask ^= (std::uint64_t{1} << size) - 1;
    for (int i = 0; i < size; ++i) {
      fixture.optimal_solution[group * size + i] = (best_mask >> i) & 1;
    }
    HighsInt edge_col = first_edge_col;
    for (int i = 0; i < size; ++i) {
      for (int j = i + 1; j < size; ++j) {
        fixture.optimal_solution[edge_col++] =
            ((best_mask >> i) & 1) != ((best_mask >> j) & 1) ? 1 : 0;
      }
    }
    fixture.optimum -= best;
  }
  if (wide) {
    std::vector<std::pair<HighsInt, double>> terms;
    for (int i = 0; i < binaries; ++i) terms.emplace_back(i, 1);
    // Complement each block's optimum as needed; this bound admits a joint
    // optimum. The row remains in every auxiliary sub-MIP.
    Row(lp, std::move(terms), require_one ? 1 : -kHighsInf, binaries / 2);
  }
  lp.a_matrix_.num_col_ = lp.num_col_;
  lp.a_matrix_.num_row_ = lp.num_row_;
  return fixture;
}

struct NeighborhoodEvents {
  std::mutex mutex;
  int started = 0;
  int released = 0;
  int starts_after_release = 0;
  int tree_after_release = 0;
  int candidates = 0;
  int syncs = 0;
  int yielded = 0;
};
void NeighborhoodCallback(int type, const std::string& message,
                          const HighsCallbackOutput*, HighsCallbackInput*,
                          void* data) {
  if (type != kCallbackLogging) return;
  auto& events = *static_cast<NeighborhoodEvents*>(data);
  std::lock_guard<std::mutex> lock(events.mutex);
  if (message.find("MIP neighborhood worker released:") != std::string::npos) {
    ++events.released;
  }
  if (message.find("MIP neighborhood started:") != std::string::npos) {
    ++events.started;
    if (events.released) ++events.starts_after_release;
  }
  if (message.find("MIP neighborhood candidate:") != std::string::npos) {
    ++events.candidates;
  }
  if (message.find("MIP neighborhood sync:") != std::string::npos) {
    ++events.syncs;
  }
  if (message.find("MIP search yielded for candidate synchronization") !=
      std::string::npos) {
    ++events.yielded;
  }
  int tree = 0, auxiliary = 0;
  if (std::sscanf(message.c_str(),
                  "MIP strategy dispatch: tree=%d auxiliary=%d", &tree,
                  &auxiliary) == 2 &&
      events.released && tree > 1 && auxiliary == 0) {
    ++events.tree_after_release;
  }
}
}  // namespace

TEST_CASE("parallel-neighborhood-options", "[mip_neighborhood]") {
  Highs highs;
  highs.setOptionValue("output_flag", false);
  std::string strategy;
  REQUIRE(highs.getOptionValue("mip_parallel_strategy", strategy) ==
          HighsStatus::kOk);
  REQUIRE(strategy == "neighborhood");
  REQUIRE(highs.setOptionValue("mip_parallel_strategy", "standard") ==
          HighsStatus::kOk);
  REQUIRE(highs.setOptionValue("mip_parallel_strategy", "invalid") ==
          HighsStatus::kError);
  REQUIRE(highs.setOptionValue("mip_neighborhood_budget", -1.0) ==
          HighsStatus::kError);
  REQUIRE(highs.setOptionValue("mip_neighborhood_slice", 0.0) ==
          HighsStatus::kError);
  HighsOptions copy = highs.getOptions();
  Highs other;
  REQUIRE(other.passOptions(copy) == HighsStatus::kOk);
  REQUIRE(other.getOptionValue("mip_parallel_strategy", strategy) ==
          HighsStatus::kOk);
  REQUIRE(strategy == "standard");
  other.resetOptions();
  REQUIRE(other.getOptionValue("mip_parallel_strategy", strategy) ==
          HighsStatus::kOk);
  REQUIRE(strategy == "neighborhood");
}

TEST_CASE("parallel-neighborhood-preserves-proof-and-returns-worker",
          "[mip_neighborhood]") {
  Highs::resetGlobalScheduler(true);
  struct Case {
    int groups;
    int size;
    double budget;
  };
  for (const auto test : {Case{6, 6, 0.0}, Case{6, 6, 0.005}, Case{6, 8, 60.0},
                          Case{2, 8, 60.0}}) {
    const auto fixture = MakeCut(test.groups, test.size, 0, true);
    const double budget = test.budget;
    Highs highs;
    highs.setOptionValue("mip_neighborhood_budget", budget);
    highs.setOptionValue("mip_neighborhood_slice",
                         budget == 0.005 ? 0.001 : 10.0);
    highs.setOptionValue("parallel", "on");
    highs.setOptionValue("threads", 4);
    highs.setOptionValue("log_to_console", false);
    highs.setOptionValue("mip_report_level", 2);
    highs.setOptionValue("mip_rel_gap", 0.0);
    highs.setOptionValue("mip_abs_gap", 0.0);
    highs.setOptionValue("mip_detect_symmetry", false);
    highs.setOptionValue("time_limit", 30.0);
    NeighborhoodEvents events;
    highs.setCallback(NeighborhoodCallback, &events);
    highs.startCallback(kCallbackLogging);
    REQUIRE(highs.passModel(fixture.lp) == HighsStatus::kOk);
    REQUIRE(highs.run() != HighsStatus::kError);
    highs.stopCallback(kCallbackLogging);
    highs.setCallback(nullptr, nullptr);
    REQUIRE(highs.getModelStatus() == HighsModelStatus::kOptimal);
    REQUIRE(std::abs(highs.getInfo().objective_function_value -
                     fixture.optimum) < 1e-6);
    REQUIRE(std::abs(highs.getInfo().mip_dual_bound - fixture.optimum) < 1e-6);
    const auto& x = highs.getSolution().col_value;
    auto rows = fixture.lp.a_matrix_;
    rows.ensureRowwise();
    for (HighsInt row = 0; row < fixture.lp.num_row_; ++row) {
      long double value = 0;
      for (HighsInt k = rows.start_[row]; k < rows.start_[row + 1]; ++k) {
        value += static_cast<long double>(rows.value_[k]) * x[rows.index_[k]];
      }
      REQUIRE(value <= fixture.lp.row_upper_[row] + 1e-6L);
      REQUIRE(value >= fixture.lp.row_lower_[row] - 1e-6L);
    }
    if (budget == 0.0) {
      REQUIRE(events.started == 0);
      REQUIRE(events.released > 0);
      REQUIRE(events.tree_after_release > 0);
    }
    if (budget == 0.005) {
      REQUIRE(events.released > 0);
      REQUIRE(events.tree_after_release > 0);
      REQUIRE(events.starts_after_release == 0);
    }
    if (test.groups == 2) {
      REQUIRE(events.started == 0);
      REQUIRE(events.released > 0);
      REQUIRE(events.tree_after_release > 0);
    }
    if (budget == 60.0 && test.groups == 6) {
      REQUIRE(events.started > 0);
      REQUIRE(events.candidates > 0);
      REQUIRE(events.yielded > 0);
      REQUIRE(events.syncs >= events.candidates);
    }
  }
  Highs::resetGlobalScheduler(true);
}

TEST_CASE("parallel-neighborhood-honors-eligibility-and-standard-fallback",
          "[mip_neighborhood]") {
  struct Case {
    const char* parallel;
    int threads;
    bool standard;
  };
  for (const auto test : {Case{"choose", 4, false}, Case{"off", 4, false},
                          Case{"on", 1, false}, Case{"on", 4, true}}) {
    Highs::resetGlobalScheduler(true);
    const auto fixture = MakeCut(6, 6, 0, true);
    Highs highs;
    if (test.standard) {
      REQUIRE(highs.setOptionValue("mip_parallel_strategy", "standard") ==
              HighsStatus::kOk);
    }
    highs.setOptionValue("parallel", test.parallel);
    highs.setOptionValue("threads", test.threads);
    highs.setOptionValue("log_to_console", false);
    highs.setOptionValue("mip_report_level", 2);
    highs.setOptionValue("mip_rel_gap", 0.0);
    highs.setOptionValue("mip_abs_gap", 0.0);
    highs.setOptionValue("mip_detect_symmetry", false);
    highs.setOptionValue("time_limit", 30.0);
    NeighborhoodEvents events;
    highs.setCallback(NeighborhoodCallback, &events);
    highs.startCallback(kCallbackLogging);
    REQUIRE(highs.passModel(fixture.lp) == HighsStatus::kOk);
    REQUIRE(highs.run() != HighsStatus::kError);
    highs.stopCallback(kCallbackLogging);
    highs.setCallback(nullptr, nullptr);
    REQUIRE(highs.getModelStatus() == HighsModelStatus::kOptimal);
    REQUIRE(std::abs(highs.getInfo().objective_function_value -
                     fixture.optimum) < 1e-6);
    REQUIRE(std::abs(highs.getInfo().mip_dual_bound - fixture.optimum) < 1e-6);
    REQUIRE(events.started == 0);
    REQUIRE(events.yielded == 0);
  }
  Highs::resetGlobalScheduler(true);
}
